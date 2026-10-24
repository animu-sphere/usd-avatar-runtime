#include <avatarMmd/PhysicsAdapter.h>
#include <mmdPhysicsAdapter/BodyProperties.h>
#include <mmdPhysicsAdapter/JointSprings.h>
#include <avatarUsd/SkeletonPoseOutput.h>
#include <avatarPhysics/StageWorld.h>
#include <avatarStageRunner/PhysicsSession.h>
#include <mmdModel/Canonicalize.h>
#include <mmdPmx/Reader.h>
#include <motionVmd/Reader.h>
#include <motionVmd/Motion.h>
#include <usd_physics/jolt/availability.h>
#include <usd_physics/core/kinematic_motion.h>
#include <pxr/external/boost/python.hpp>
#include <pxr/usd/usd/primRange.h>
#include <pxr/usd/usdPhysics/rigidBodyAPI.h>
#include <pxr/usd/usdSkel/skeleton.h>
#include <pxr/usd/usdSkel/bindingAPI.h>
#include <pxr/usd/usdSkel/blendShape.h>
#include <filesystem>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <iostream>
#include <set>
#include <map>

namespace {
namespace py = PXR_BOOST_NAMESPACE::python;
namespace core = usd_physics::core;
using namespace pxr;
using Body = mmd::physics::BodyTransform;
void check(ArStatus status) {
    if (status != AR_OK) throw std::runtime_error("Live MMD runtime status " + std::to_string(status));
}
template<class R> auto take(R result) {
    for (const auto& d : result.diagnostics()) std::cerr << d.code << ": " << d.message << '\n';
    if (!result) throw std::invalid_argument(result.fatal()->code + ": " + result.fatal()->message);
    return std::move(result).value();
}
struct Runtime {
    ArRuntimeApi api{}; ArRuntime handle = 0;
    Runtime() { check(arGetApi(AR_ABI_VERSION, sizeof(api), &api)); check(api.create_runtime(&handle)); }
    ~Runtime() { if (handle) api.destroy_runtime(handle); }
};

// Application host, deliberately outside reusable core/provider targets.
// Runtime/session own fixed ticks; Jolt owns simulation; MMD owns all controls.
class Session {
    // Declaration order keeps provider code alive through runtime destruction.
    std::unique_ptr<avatarMmd::PhysicsAdapter> adapter_;
    Runtime runtime_;
    UsdStageRefPtr stage_;
    avatarUsd::SkeletonBinding binding_;
    SdfPath morphMesh_;
    std::unique_ptr<avatarPhysics::StageWorld> world_;
    std::unique_ptr<avatarStageRunner::PhysicsSession> session_;
    std::unique_ptr<avatarUsd::SkeletonPoseOutput> output_;
    ArInstance instance_ = 0;
    ArInputFrame input_{AR_HEADER(ArInputFrame)};
    std::vector<Body> pending_, result_;
    std::map<std::string, mmd::control::JointTransform> initialBodies_;
    uint64_t pendingFrame_ = 0, resultFrame_ = 0, generation_ = 1, steps_ = 0;
    avatarStageRunner::PhysicsSession::FrameClock::Clock::time_point now_{};
    std::string error_;
    std::vector<std::string> warnings_;
    bool translationSprings_ = false, rotationSprings_ = false;
    size_t translationAxes_ = 0, rotationAxes_ = 0;
    static void AR_CALL diagnostic(void* data, const ArDiagnostic* d) {
        auto& self = *static_cast<Session*>(data);
        // A callback may already hold an owner refusal with a body path/code.
        // Retain it when the runtime adds its generic callback-failure report.
        if (self.error_.empty()) self.error_ = std::string(d->code) + ": " + d->message;
    }
    void status(ArStatus value) {
        if (value != AR_OK) throw std::runtime_error(error_.empty() ?
            "Live MMD runtime status " + std::to_string(value) : error_);
    }
    void buildWorld() {
        if (world_ && !world_->Close()) throw std::runtime_error("Physics resource close refused; retry reset");
        world_.reset(); pending_.clear(); result_.clear(); pendingFrame_ = resultFrame_ = steps_ = 0;
        std::vector<std::string> warnings;
        std::set<std::string> applied;
        size_t translationAxes = 0, rotationAxes = 0;
        // Validate owner declarations even for disabled/format-only joints;
        // only the standard enabled joints below can receive execution.
        for (auto prim : stage_->Traverse()) {
            if (prim.GetAttribute(TfToken("mmd:physics:translationSpring")) ||
                prim.GetAttribute(TfToken("mmd:physics:rotationSpring"))) {
                (void)take(mmd::physics::ReadJointSprings(prim));
                for (const char* name : {"translationSpring", "rotationSpring"})
                    warnings.push_back(prim.GetPath().GetString() + ".mmd:physics:" + name);
            }
        }
        world_ = std::make_unique<avatarPhysics::StageWorld>(stage_, [](const auto& settings) {
            // MMD owner contract section 7.1; immutable policy for this single-format world.
            using Rule = core::ContactCombineRule;
            const core::ContactMixing mixing{Rule::product, Rule::product, 10};
            if (settings) return usd_physics::jolt::createWorld(mixing, settings->gravity);
            return usd_physics::jolt::createWorld(mixing);
        }, [this](const auto& body, auto& descriptor) {
            if (const auto found = initialBodies_.find(body.path); found != initialBodies_.end()) {
                const auto& t = found->second;
                descriptor.initialTransform = {{t.translation[0], t.translation[1], t.translation[2]},
                    {t.rotation[3], t.rotation[0], t.rotation[1], t.rotation[2]}};
            }
            const auto properties = take(mmd::physics::ReadBodyProperties(
                stage_->GetPrimAtPath(SdfPath(body.path))));
            if (properties.filter)
                descriptor.collisionFilter = {properties.filter->category, properties.filter->mask};
            if (properties.contact)
                descriptor.contactMaterial = {properties.contact->friction, properties.contact->restitution};
            if (properties.damping)
                descriptor.damping = {properties.damping->linear, properties.damping->angular};
        }, [&](const auto& joint, auto& descriptor) {
            const auto properties = take(mmd::physics::ReadJointSprings(
                stage_->GetPrimAtPath(SdfPath(joint.path))));
            if (!properties || !properties->spring6Dof) return;
            for (bool rotation : {false, true}) {
                if (!(rotation ? rotationSprings_ : translationSprings_)) continue;
                const auto& values = rotation ? properties->rotation : properties->translation;
                bool complete = true;
                for (int axis = 0; axis < 3; ++axis) {
                    const bool supported = values[axis] > 0 &&
                        descriptor.limits[axis + (rotation ? 3 : 0)].motion != core::AxisMotion::locked;
                    if (supported) {
                        if (rotation) { descriptor.rotationSprings[axis] = {values[axis], 0}; ++rotationAxes; }
                        else { descriptor.translationSprings[axis] = {values[axis], 0, 0}; ++translationAxes; }
                    } else if (values[axis] != 0) complete = false;
                }
                if (complete) applied.insert(joint.path + ".mmd:physics:" +
                    (rotation ? "rotationSpring" : "translationSpring"));
            }
        });
        if (!dynamic_cast<core::KinematicMotion*>(world_->World()))
            throw std::runtime_error("Live MMD preview requires KinematicMotion");
        warnings_.clear();
        for (const auto& path : warnings) if (!applied.count(path)) warnings_.push_back(path);
        translationAxes_ = translationAxes; rotationAxes_ = rotationAxes;
    }
    static ArStatus AR_CALL step(void* data, const ArPhysicsTick* tick, const ArDiagnosticSink*) {
        auto& self = *static_cast<Session*>(data);
        try {
            if (self.pendingFrame_ != tick->tick_id) return AR_INVALID_STATE;
            auto* world = self.world_->World();
            auto* kinematic = dynamic_cast<core::KinematicMotion*>(world);
            const auto& handles = self.world_->Resources()->Bodies();
            for (const auto& row : self.pending_) {
                const auto& t = row.world;
                if (!kinematic->moveKinematic(handles.at(row.body.GetString()),
                    {{t.translation[0], t.translation[1], t.translation[2]},
                     {t.rotation[3], t.rotation[0], t.rotation[1], t.rotation[2]}},
                    core::PhysicsWorld::Duration(tick->delta_seconds)))
                    throw std::runtime_error("Kinematic target refused: " + row.body.GetString());
            }
            world->step(core::PhysicsWorld::Duration(tick->delta_seconds));
            ++self.steps_;
            self.result_.clear();
            for (const auto& [path, handle] : handles) {
                const auto t = world->bodyState(handle).transform;
                self.result_.push_back({SdfPath(path), {{t.translation.x, t.translation.y, t.translation.z},
                    {t.rotation.x, t.rotation.y, t.rotation.z, t.rotation.w}}});
            }
            self.resultFrame_ = tick->tick_id;
            self.pending_.clear(); self.pendingFrame_ = 0;
            return AR_OK;
        } catch (const std::exception& e) { self.error_ = e.what(); return AR_PROVIDER_ERROR; }
    }
    static ArStatus AR_CALL resetWorld(void* data, const ArDiagnosticSink*) {
        auto& self = *static_cast<Session*>(data);
        try { self.buildWorld(); return AR_OK; }
        catch (const std::exception& e) { self.error_ = e.what(); return AR_PROVIDER_ERROR; }
    }
public:
    Session(UsdStageRefPtr stage, const std::string& pmx, const std::string& vmd,
            bool translationSprings = false, bool rotationSprings = false)
        : stage_(std::move(stage)), binding_(stage_,
            {SdfPath("/Asset"), SdfPath("/Asset/skel/Skeleton"), "live-mmd", 1, {}}),
          translationSprings_(translationSprings), rotationSprings_(rotationSprings) {
        auto source = take(mmd::pmx::ReadFile(std::filesystem::u8path(pmx)));
        auto model = take(mmd::Canonicalize(source));
        auto evaluator = take(mmd::control::Evaluator::Prepare(model));
        mmd::binding::BoundMotion motion;
        if (!vmd.empty()) {
            auto bytes = take(motionVmd::ReadFile(std::filesystem::u8path(vmd)));
            motion = take(mmd::binding::Bind(take(motionVmd::BuildMotion(bytes)), model));
        }
        std::vector<UsdPrim> bodies;
        for (auto prim : stage_->Traverse()) {
            if (prim.HasAPI<UsdPhysicsRigidBodyAPI>()) bodies.push_back(prim);
        }
        if (bodies.empty()) throw std::invalid_argument("No MMD rigid bodies in this stage");
        auto coupling = take(mmd::physics::Adapter::Bind(UsdSkelSkeleton::Get(stage_, SdfPath(binding_.SkeletonId())), bodies));
        // World construction and Reset start every associated body at source
        // frame zero, before any moveKinematic/Step. MMD owns the pose/offset math.
        const auto initial = evaluator.BeginPhysics(motion, 0);
        for (const auto& row : take(coupling.InitialBodyTransforms(initial.BeforePhysics())))
            initialBodies_.emplace(row.body.GetString(), row.world);
        avatarMmd::PhysicsAdapterConfig c{evaluator, motion, coupling};
        c.preId = "live.mmd.pre"; c.postId = "live.mmd.post"; c.layoutId = "live-mmd";
        const auto& placement = binding_.RootPlacement();
        c.placement = {{placement.translation[0], placement.translation[1], placement.translation[2]},
            {placement.rotation[0], placement.rotation[1], placement.rotation[2], placement.rotation[3]}};
        const auto& base = binding_.Baseline();
        if (model.skeleton.bones.size() != base.joint_count) throw std::invalid_argument("PMX/stage skeleton mismatch");
        for (uint32_t i = 0; i < base.joint_count; ++i) {
            if (model.skeleton.bones[i].jointPath != base.joints[i].joint_id ||
                model.skeleton.bones[i].parent != base.joints[i].parent_index)
                throw std::invalid_argument("PMX/stage joint order mismatch");
            c.joints.push_back({base.joints[i].skeleton_id, base.joints[i].joint_id, base.joints[i].parent_index});
        }
        // The direct-PMX host uses the owner's canonical single-mesh contract.
        // Only vertex morphs have standard UsdSkel outputs; never map UV,
        // material or impulse channels into a deformation weight by accident.
        const SdfPath meshPath("/Asset/geo/Mesh");
        const UsdSkelBindingAPI mesh(stage_->GetPrimAtPath(meshPath));
        VtTokenArray shapes; SdfPathVector targets;
        if (mesh) {
            mesh.GetBlendShapesAttr().Get(&shapes);
            mesh.GetBlendShapeTargetsRel().GetTargets(&targets);
        }
        size_t vertices = 0;
        for (const auto& morph : model.morphs) {
            if (morph.type != mmd::MorphType::Vertex) continue;
            ++vertices;
            const TfToken name(morph.name.stableId);
            const auto found = std::find(shapes.begin(), shapes.end(), name);
            const auto index = size_t(found - shapes.begin());
            if (found == shapes.end() || shapes.size() != targets.size())
                throw std::invalid_argument("PMX/stage vertex morph binding mismatch");
            const auto prim = stage_->GetPrimAtPath(targets[index]);
            const auto sourceIndex = prim.GetCustomDataByKey(TfToken("mmd:sourceIndex"));
            TfToken type;
            if (!prim.IsA<UsdSkelBlendShape>() ||
                targets[index] != SdfPath("/Asset/morph").AppendChild(name) ||
                !sourceIndex.IsHolding<int>() || sourceIndex.UncheckedGet<int>() != int(morph.sourceIndex) ||
                !prim.GetAttribute(TfToken("mmd:morph:type")).Get(&type) || type != TfToken("vertex"))
                throw std::invalid_argument("PMX/stage vertex morph provenance mismatch");
            c.morphs.push_back({int32_t(morph.sourceIndex), meshPath.GetString(), name.GetString()});
        }
        if (vertices != shapes.size()) throw std::invalid_argument("PMX/stage vertex morph count mismatch");
        if (vertices) morphMesh_ = meshPath;
        std::vector<ArBlendShape> morphs;
        // Keep string storage alive through create_instance's owned baseline copy.
        const auto morphBindings = c.morphs;
        for (const auto& m : morphBindings) morphs.push_back({m.mesh.c_str(), m.target.c_str(), 0});
        c.submit = [this](const auto& context, std::span<const Body> rows) {
            pending_.assign(rows.begin(), rows.end()); pendingFrame_ = context.input->frame_id; return AR_OK;
        };
        c.snapshot = [this](const auto& context, std::vector<Body>& rows) {
            if (context.input->frame_id != resultFrame_) return AR_INVALID_STATE;
            rows = result_; return AR_OK;
        };
        adapter_ = std::make_unique<avatarMmd::PhysicsAdapter>(std::move(c));
        auto pre = adapter_->PreDescriptor(), post = adapter_->PostDescriptor();
        check(runtime_.api.register_evaluator(runtime_.handle, &pre, nullptr));
        check(runtime_.api.register_evaluator(runtime_.handle, &post, nullptr));
        const char* selected[]{pre.id, post.id};
        ArInstanceDesc desc{AR_HEADER(ArInstanceDesc)};
        desc.generation = 1; desc.layout_id = "live-mmd"; desc.layout_version = 1;
        desc.initial_state = base; desc.evaluators = selected; desc.evaluator_count = 2;
        desc.initial_state.blend_shapes = morphs.data();
        desc.initial_state.blend_shape_count = uint32_t(morphs.size());
        check(runtime_.api.create_instance(runtime_.handle, &desc, nullptr, &instance_));
        buildWorld();
        ArPhysicsParticipant participant{instance_, post.id};
        ArPhysicsWorldDesc world{AR_HEADER(ArPhysicsWorldDesc)};
        world.id = "live.mmd.world"; world.participants = &participant; world.participant_count = 1;
        world.fixed_step_seconds = 1. / 120; world.max_catch_up_ticks = 8;
        world.user_data = this; world.step = step; world.reset = resetWorld;
        session_ = std::make_unique<avatarStageRunner::PhysicsSession>(runtime_.handle, world, [this] { return now_; });
        output_ = std::make_unique<avatarUsd::SkeletonPoseOutput>(stage_, binding_, morphMesh_);
    }
    void play() { if (!session_ || !session_->IsOpen()) throw std::runtime_error("Session closed"); session_->Play(); }
    void pause() { if (session_) session_->Pause(); }
    py::dict advance(double elapsed) {
        if (!std::isfinite(elapsed) || elapsed < 0) throw std::invalid_argument("Elapsed time must be finite and nonnegative");
        if (!session_ || !session_->IsOpen()) throw std::runtime_error("Session closed");
        now_ += std::chrono::duration_cast<decltype(now_)::duration>(std::chrono::duration<double>(elapsed));
        const ArPhysicsInput input{instance_, &input_}; ArDiagnosticSink sink{this, diagnostic};
        error_.clear(); status(session_->Display(&input, 1, &sink));
        if (!session_->Snapshots().empty() && session_->Snapshots()[0])
            output_->Apply(runtime_.api, session_->Snapshots()[0]);
        return stats();
    }
    void reset() {
        if (!session_ || !session_->IsOpen()) throw std::runtime_error("Session closed");
        ArDiagnosticSink sink{this, diagnostic}; error_.clear();
        status(session_->Reset(++generation_, &sink));
        output_->Close(); output_ = std::make_unique<avatarUsd::SkeletonPoseOutput>(stage_, binding_, morphMesh_);
    }
    void close() {
        if (session_) status(session_->Close());
        if (output_) output_->Close();
        if (world_ && !world_->Close()) throw std::runtime_error("Physics resource close refused");
    }
    py::dict stats() const {
        py::dict d; d["steps"] = steps_; d["generation"] = generation_;
        d["bodies"] = world_ && world_->Resources() ? world_->Resources()->Bodies().size() : 0;
        d["paused"] = !session_ || session_->IsPaused();
        d["approximateTranslationSpringAxes"] = translationAxes_;
        d["approximateRotationSpringAxes"] = rotationAxes_;
        return d;
    }
    py::list warnings() const { py::list rows; for (const auto& w : warnings_) rows.append(w); return rows; }
    py::dict bodyStates() const {
        py::dict rows;
        if (world_ && world_->Resources()) {
            for (const auto& [path, handle] : world_->Resources()->Bodies()) {
                const auto state = world_->World()->bodyState(handle);
                const auto& t = state.transform;
                const auto& v = state.linearVelocity;
                py::dict row;
                row["translation"] = py::make_tuple(t.translation.x, t.translation.y, t.translation.z);
                row["rotation"] = py::make_tuple(t.rotation.x, t.rotation.y, t.rotation.z, t.rotation.w);
                row["linearVelocity"] = py::make_tuple(v.x, v.y, v.z);
                rows[path] = row;
            }
        }
        return rows;
    }
};
}
PXR_BOOST_PYTHON_MODULE(_native) {
    py::class_<Session, py::noncopyable>("Session", py::init<UsdStageRefPtr, std::string, std::string,
        py::optional<bool, bool>>())
        .def("play", &Session::play).def("pause", &Session::pause).def("advance", &Session::advance)
        .def("reset", &Session::reset).def("close", &Session::close)
        .add_property("stats", &Session::stats).add_property("unappliedMetadata", &Session::warnings)
        .add_property("bodyStates", &Session::bodyStates);
}
