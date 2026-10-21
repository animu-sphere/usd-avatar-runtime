#include <avatarMmd/PhysicsAdapter.h>
#include <avatarRuntime/physics.h>
#include <mmdModel/Canonicalize.h>
#include <mmdPmx/Reader.h>
#include <motionVmd/Reader.h>
#include <motionVmd/Motion.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usd/primRange.h>
#include <pxr/usd/usdGeom/metrics.h>
#include <pxr/usd/usdGeom/xform.h>
#include <pxr/usd/usdPhysics/rigidBodyAPI.h>
#include <pxr/base/gf/rotation.h>
#ifdef AVATAR_MMD_JOLT_SMOKE
#include <usd_physics/jolt/availability.h>
#include <usd_physics/core/kinematic_motion.h>
#include <usd_physics/usd/box_scene.h>
#endif
#include <cstdlib>
#include <algorithm>
#include <filesystem>
#include <cmath>
#include <iostream>
#include <map>
#include <set>
#ifdef AVATAR_MMD_TOON_SMOKE
#include "ToonCheck.h"
#endif
#ifdef AVATAR_MMD_STAGE_RUNNER_SMOKE
#include <avatarPhysics/SceneResources.h>
#include <pxr/usd/usd/editContext.h>
#include <pxr/usd/usdPhysics/joint.h>
#include <pxr/usd/usdPhysics/limitAPI.h>
#endif

#define CHECK(x) do { if (!(x)) { std::cerr << __LINE__ << ": " << #x << '\n'; std::exit(1); } } while(0)
using namespace pxr;
using Body = mmd::physics::BodyTransform;
using Pose = mmd::control::Pose;

struct Assets {
    mmd::CanonicalDocument model;
    mmd::binding::BoundMotion motion;
    UsdStageRefPtr stage;
    std::vector<UsdPrim> bodies;
    mmd::control::JointTransform placement;
    Assets() {
        stage = UsdStage::CreateInMemory();
        UsdGeomSetStageUpAxis(stage, UsdGeomTokens->y); UsdGeomSetStageMetersPerUnit(stage, 1);
        auto asset = UsdGeomXform::Define(stage, SdfPath("/Asset"));
        asset.AddTranslateOp().Set(GfVec3d(4, 2, 0)); asset.AddRotateZOp().Set(30.0f);
        const auto q = GfRotation(GfVec3d(0,0,1), 30).GetQuat();
        placement = {{4,2,0}, {0,0,q.GetImaginary()[2], q.GetReal()}};
        auto skeleton = UsdSkelSkeleton::Define(stage, SdfPath("/Asset/skel/Skeleton"));
        VtTokenArray names{TfToken("root"), TfToken("root/child"), TfToken("root/child/post")};
        VtMatrix4dArray rest{GfMatrix4d(1), GfMatrix4d(1), GfMatrix4d(1)};
        rest[1].SetTranslate({0,1,0}); rest[2].SetTranslate({0,1,0});
        skeleton.CreateJointsAttr().Set(names); skeleton.CreateRestTransformsAttr().Set(rest);
        model.skeleton.bones.resize(3); model.rig.bones.resize(3);
        for (int i = 0; i < 3; ++i) {
            auto& bone = model.skeleton.bones[i]; bone.parent = i - 1; bone.jointPath = names[i].GetString();
            bone.localTranslation = {0, i ? 1.0 : 0.0, 0};
        }
        model.rig.bones[2].appendSource = 1; model.rig.bones[2].appendRotation = true;
        model.rig.bones[2].appendRatio = .5f; model.rig.bones[2].deformAfterPhysics = true;
        const char* modes[]{"followBone", "dynamic", "dynamicWithBone"};
        for (int i = 0; i < 3; ++i) {
            auto xf = UsdGeomXform::Define(stage, SdfPath("/Asset/body" + std::to_string(i)));
            xf.AddTranslateOp().Set(GfVec3d(1, i == 2 ? 1 : 0, 0));
            auto api = UsdPhysicsRigidBodyAPI::Apply(xf.GetPrim());
            api.CreateKinematicEnabledAttr().Set(i == 0);
            xf.GetPrim().CreateAttribute(TfToken("mmd:physics:bone"), SdfValueTypeNames->Int, true, SdfVariabilityUniform).Set(i == 2 ? 1 : 0);
            xf.GetPrim().CreateAttribute(TfToken("mmd:physics:mode"), SdfValueTypeNames->Token, true, SdfVariabilityUniform).Set(TfToken(modes[i]));
            bodies.push_back(xf.GetPrim());
        }
    }
    Assets(const char* pmx, const char* vmd) {
        auto source = mmd::pmx::ReadFile(std::filesystem::path(pmx)); CHECK(source);
        auto canonical = mmd::Canonicalize(source.value()); CHECK(canonical); model = canonical.value();
        auto bytes = motionVmd::ReadFile(std::filesystem::path(vmd)); CHECK(bytes);
        auto built = motionVmd::BuildMotion(bytes.value()); CHECK(built);
        auto bound = mmd::binding::Bind(built.value(), model); CHECK(bound); motion = bound.value();
        stage = UsdStage::Open(pmx); CHECK(stage);
        for (auto prim : stage->Traverse()) if (prim.HasAPI<UsdPhysicsRigidBodyAPI>()) bodies.push_back(prim);
    }
    avatarMmd::PhysicsAdapterConfig config() {
        auto evaluator = mmd::control::Evaluator::Prepare(model); CHECK(evaluator);
        auto binding = mmd::physics::Adapter::Bind(UsdSkelSkeleton::Get(stage, SdfPath("/Asset/skel/Skeleton")), bodies);
        CHECK(binding);
        avatarMmd::PhysicsAdapterConfig c{evaluator.value(), motion, binding.value()};
        c.preId = "mmd.pre"; c.postId = "mmd.post"; c.layoutId = "mmd.layout"; c.placement = placement;
        for (const auto& bone : model.skeleton.bones) c.joints.push_back({"rig", bone.jointPath, bone.parent});
        return c;
    }
};

struct Fixture {
    Assets& assets;
    ArRuntimeApi api{}; ArPhysicsApi physics{};
    ArRuntime runtime = 0; ArPhysicsWorld world = 0;
    std::unique_ptr<avatarMmd::PhysicsAdapter> adapter;
    ArInstance avatars[2]{}; ArSnapshot snapshots[2]{};
    ArPhysicsParticipant participants[2]{};
    ArInputFrame input{AR_HEADER(ArInputFrame)};
    ArPhysicsInput inputs[2]{};
    std::map<ArInstance, std::vector<Body>> pending, results;
    std::vector<std::string> calls, diagnostics;
    int steps = 0, resets = 0; uint64_t request = 0;
    bool badSnapshot = false, failSubmit = false;
    bool resourceJoints = false;
    ArPhysicsAdvanceResult lastAdvance{AR_HEADER(ArPhysicsAdvanceResult)};
#ifdef AVATAR_MMD_JOLT_SMOKE
    std::unique_ptr<usd_physics::core::PhysicsWorld> solver;
    std::map<ArInstance, std::map<SdfPath, usd_physics::core::BodyHandle>> handles;
    std::map<ArInstance, std::vector<usd_physics::core::ShapeHandle>> shapes;
    std::set<ArInstance> unloaded;
#ifdef AVATAR_MMD_STAGE_RUNNER_SMOKE
    std::map<ArInstance, std::unique_ptr<avatarPhysics::SceneResources>> resources;
    usd_physics::core::BodyHandle sharedBody;
#endif
    void initializeSolver() {
        namespace core = usd_physics::core;
        // Reset rebuilds ALL resources and path/handle maps, never reuses stale handles.
        // Resource sets borrow the old solver and must close before replacing it.
#ifdef AVATAR_MMD_STAGE_RUNNER_SMOKE
        for (auto& [avatar, owned] : resources) CHECK(owned->Close());
        resources.clear();
#endif
        solver = usd_physics::jolt::createWorld(); handles.clear(); shapes.clear();
        auto scene = usd_physics::usd::readPhysicsScene(assets.stage);
        CHECK(scene.bodies.size() == 3 && scene.joints.size() == (resourceJoints ? 1u : 0u));
#ifdef AVATAR_MMD_STAGE_RUNNER_SMOKE
        for (auto avatar : avatars) if (!unloaded.count(avatar)) {
            auto resourcesForAvatar = std::make_unique<avatarPhysics::SceneResources>(
                *solver, scene, [](const auto& body, auto& descriptor) {
                    descriptor.collisionFilter = {1,0};
                    descriptor.damping = {0,0};
                    if (body.motionType == core::MotionType::dynamicBody) {
                        const auto q = GfRotation(GfVec3d(0,0,1), 60).GetQuat();
                        descriptor.initialTransform.rotation = {q.GetReal(),0,0,q.GetImaginary()[2]};
                    }
                });
            for (const auto& body : scene.bodies) {
                const auto handle = resourcesForAvatar->Bodies().at(body.path);
                handles[avatar][SdfPath(body.path)] = handle;
                if (body.motionType == core::MotionType::dynamicBody)
                    CHECK(solver->setLinearVelocity(handle, {.1,0,0}));
            }
            resources.emplace(avatar, std::move(resourcesForAvatar));
        }
#else
        for (auto avatar : avatars) if (!unloaded.count(avatar)) for (const auto& body : scene.bodies) {
            auto shape = solver->createShape(body.shape);
            shapes[avatar].push_back(shape);
            // Generated fixture uses collisionMask=0; no contacts in this coupling smoke.
            auto initial = body.transform;
            // Deliberate initial rotation exercises actual body-to-bone feedback
            // and the generated model's post-physics append without extra APIs.
            if (body.motionType == core::MotionType::dynamicBody) {
                const auto q = GfRotation(GfVec3d(0,0,1), 60).GetQuat();
                initial.rotation = {q.GetReal(),0,0,q.GetImaginary()[2]};
            }
            handles[avatar][SdfPath(body.path)] = solver->createBody(
                {shape, body.motionType, initial, body.mass, {1,0}, {}, {0,0}});
            if (body.motionType == core::MotionType::dynamicBody)
                CHECK(solver->setLinearVelocity(handles[avatar].at(SdfPath(body.path)), {.1,0,0}));
        }
#endif
#ifdef AVATAR_MMD_STAGE_RUNNER_SMOKE
        // A world-owned object outside both avatars must share Step and reset.
        core::BodyDescriptor shared;
        shared.shape = solver->createShape({});
        shared.motionType = core::MotionType::dynamicBody;
        shared.initialTransform.translation = {20,10,0};
        shared.collisionFilter = {1,0};
        shared.damping = {0,0};
        sharedBody = solver->createBody(shared);
        CHECK(solver->setLinearVelocity(sharedBody, {.25,0,0}));
#endif
    }
#endif
    static void AR_CALL diagnostic(void* p, const ArDiagnostic* d) {
        auto& f = *static_cast<Fixture*>(p);
        f.diagnostics.push_back(d->code);
        std::cerr << d->code << ": " << d->message << '\n';
        if (std::string(d->origin) == "usd-mmd-plugins") {
            CHECK(d->instance != 0 && d->frame_id != 0 && std::string(d->evaluator_id) == "mmd.post");
        }
    }
    Fixture(Assets& a, bool jolt = false,
            std::function<void(avatarMmd::PhysicsAdapterConfig&)> configure = {},
            bool registerWorld = true, bool constrainedResources = false)
            : assets(a), resourceJoints(constrainedResources) {
        CHECK(arGetApi(AR_ABI_VERSION, sizeof(api), &api) == AR_OK);
        CHECK(arGetPhysicsApi(1, sizeof(physics), &physics) == AR_OK);
        CHECK(api.create_runtime(&runtime) == AR_OK);
        auto config = assets.config();
        config.submit = [this](const ArEvaluationContext& c, std::span<const Body> bodies) {
            calls.push_back("pre." + std::to_string(c.working->instance));
            CHECK(bodies.size() == 1);
#ifdef AVATAR_MMD_STAGE_RUNNER_SMOKE
            CHECK(c.input->input_revision == input.input_revision);
            CHECK(c.input->scalar_count == input.scalar_count);
            for (uint32_t i = 0; i < input.scalar_count; ++i) {
                const auto &actual = c.input->scalars[i], &expected = input.scalars[i];
                CHECK(std::string(actual.source_id) == expected.source_id);
                CHECK(std::string(actual.actor_id) == expected.actor_id);
                CHECK(std::string(actual.channel_id) == expected.channel_id);
                CHECK(actual.value == expected.value && actual.source_seconds == expected.source_seconds);
                CHECK(actual.clock_scale == expected.clock_scale && actual.clock_offset == expected.clock_offset);
            }
#endif
            if (failSubmit) return AR_PROVIDER_ERROR;
            pending[c.working->instance] = {bodies.begin(), bodies.end()}; return AR_OK;
        };
        config.snapshot = [this](const ArEvaluationContext& c, std::vector<Body>& out) {
            calls.push_back("post." + std::to_string(c.working->instance));
            CHECK(steps == int(c.input->frame_id)); out = results.at(c.working->instance);
            if (badSnapshot) out.clear();
            return AR_OK;
        };
        if (configure) configure(config);
        adapter = std::make_unique<avatarMmd::PhysicsAdapter>(config);
        auto pre = adapter->PreDescriptor(), post = adapter->PostDescriptor();
        CHECK(api.register_evaluator(runtime, &post, nullptr) == AR_OK);
        CHECK(api.register_evaluator(runtime, &pre, nullptr) == AR_OK);
        // Include an unrelated leading joint: mapping is by identity, not array offset.
        std::vector<ArJoint> joints{{"other", "root", -1, {{0,0,0},{0,0,0,1},{1,1,1}}}};
        for (const auto& b : assets.model.skeleton.bones) {
            ArJoint j{"rig", b.jointPath.c_str(), b.parent < 0 ? -1 : b.parent + 1, {{0,0,0},{0,0,0,1},{1,1,1}}};
            joints.push_back(j);
        }
        const char* selected[]{post.id, pre.id};
        ArInstanceDesc d{AR_HEADER(ArInstanceDesc)}; d.generation = 1; d.layout_id = "mmd.layout"; d.layout_version = 1;
        d.initial_state = {AR_HEADER(ArStateView)}; d.initial_state.joints = joints.data(); d.initial_state.joint_count = uint32_t(joints.size());
        ArBlendShape morph{"mesh", "vertex", 0}; ArVisibility visible{"avatar", 1};
        d.initial_state.blend_shapes = &morph; d.initial_state.blend_shape_count = 1;
        d.initial_state.visibility = &visible; d.initial_state.visibility_count = 1;
        d.evaluators = selected; d.evaluator_count = 2;
        for (auto& avatar : avatars) CHECK(api.create_instance(runtime, &d, nullptr, &avatar) == AR_OK);
#ifdef AVATAR_MMD_JOLT_SMOKE
        if (jolt) initializeSolver();
#else
        CHECK(!jolt);
#endif
        participants[0] = {avatars[1], post.id};
        participants[1] = {avatars[0], post.id};
        if (registerWorld) {
            const auto wd = worldDescriptor();
            CHECK(physics.create_world(runtime, &wd, nullptr, &world) == AR_OK);
        }
        inputs[0] = {avatars[1], &input}; inputs[1] = {avatars[0], &input};
    }
    ArPhysicsWorldDesc worldDescriptor() {
        ArPhysicsWorldDesc wd{AR_HEADER(ArPhysicsWorldDesc)}; wd.id = "world"; wd.participants = participants;
        wd.participant_count = 2; wd.fixed_step_seconds = 1.0 / 120; wd.max_catch_up_ticks = 4;
        wd.user_data = this; wd.step = step; wd.reset = reset;
        return wd;
    }
    ~Fixture() {
        release(); CHECK(api.destroy_runtime(runtime) == AR_OK);
    }
    void release() { for (auto& s : snapshots) { if (s) CHECK(api.release_snapshot(s) == AR_OK); s = 0; } }
    static ArStatus AR_CALL step(void* p, const ArPhysicsTick* tick, const ArDiagnosticSink*) {
        auto& f = *static_cast<Fixture*>(p); f.calls.push_back("step");
        size_t expectedMembers = 2;
#ifdef AVATAR_MMD_JOLT_SMOKE
        if (f.solver) expectedMembers = f.handles.size();
#endif
        CHECK(f.pending.size() == expectedMembers); ++f.steps; CHECK(tick->tick_id == uint64_t(f.steps));
#ifdef AVATAR_MMD_JOLT_SMOKE
        if (f.solver) {
            namespace core = usd_physics::core;
            auto* kinematic = dynamic_cast<core::KinematicMotion*>(f.solver.get()); CHECK(kinematic);
            for (const auto& [avatar, commands] : f.pending) for (const auto& command : commands) {
                const auto& t = command.world;
                core::Transform target{{t.translation[0],t.translation[1],t.translation[2]},
                    {t.rotation[3],t.rotation[0],t.rotation[1],t.rotation[2]}};
                CHECK(kinematic->moveKinematic(f.handles.at(avatar).at(command.body), target, core::PhysicsWorld::Duration(tick->delta_seconds)));
            }
            f.solver->step(core::PhysicsWorld::Duration(tick->delta_seconds));
            for (const auto& [avatar, commands] : f.pending) for (const auto& command : commands) {
                const auto actual = f.solver->bodyState(f.handles.at(avatar).at(command.body)).transform;
                const auto& expected = command.world;
                CHECK(std::abs(actual.translation.x - expected.translation[0]) < 1e-6);
                CHECK(std::abs(actual.translation.y - expected.translation[1]) < 1e-6);
                CHECK(std::abs(actual.translation.z - expected.translation[2]) < 1e-6);
                CHECK(std::abs(std::abs(actual.rotation.x*expected.rotation[0] + actual.rotation.y*expected.rotation[1] +
                    actual.rotation.z*expected.rotation[2] + actual.rotation.w*expected.rotation[3]) - 1) < 1e-6);
            }
            for (const auto& [avatar, handles] : f.handles) {
                auto& out = f.results[avatar]; out.clear();
                for (const auto& [path, handle] : handles) {
                    auto state = f.solver->bodyState(handle); const auto& t = state.transform;
                    for (double x : {t.translation.x,t.translation.y,t.translation.z,t.rotation.x,t.rotation.y,t.rotation.z,t.rotation.w}) CHECK(std::isfinite(x));
                    out.push_back({path, {{t.translation.x,t.translation.y,t.translation.z}, {t.rotation.x,t.rotation.y,t.rotation.z,t.rotation.w}}});
                }
            }
        } else
#endif
        {
            for (auto avatar : f.avatars) {
                auto& out = f.results[avatar]; out.clear();
                for (auto body : f.assets.bodies) {
                    TfToken mode; CHECK(body.GetAttribute(TfToken("mmd:physics:mode")).Get(&mode));
                    if (mode == TfToken("followBone")) continue;
                    // Distinct avatar feedback detects cross-instance continuation leakage.
                    double angle = avatar == f.avatars[0] ? 60 : 80;
                    if (mode == TfToken("dynamic")) angle = 170;
                    const auto q = GfRotation(GfVec3d(0,0,1), angle + 30).GetQuat();
                    out.push_back({body.GetPath(), {{7,8,9},{0,0,q.GetImaginary()[2],q.GetReal()}}});
                }
            }
        }
        f.pending.clear(); return AR_OK;
    }
    static ArStatus AR_CALL reset(void* p, const ArDiagnosticSink*) {
        auto& f = *static_cast<Fixture*>(p); f.steps = 0; ++f.resets; f.pending.clear(); f.results.clear();
#ifdef AVATAR_MMD_JOLT_SMOKE
        if (f.solver) f.initializeSolver();
#endif
        return AR_OK;
    }
    ArStatus advance(double seconds, bool pause = false) {
        release(); ArPhysicsAdvanceDesc d{AR_HEADER(ArPhysicsAdvanceDesc)};
        d.request_id = ++request; d.elapsed_seconds = seconds; d.paused = pause; d.inputs = inputs; d.input_count = 2;
        lastAdvance = {AR_HEADER(ArPhysicsAdvanceResult)};
        ArDiagnosticSink sink{this, diagnostic};
        return physics.advance_world(runtime, world, &d, &sink, &lastAdvance, snapshots);
    }
    Pose pose(ArSnapshot s) {
        ArStateView v{AR_HEADER(ArStateView)}; CHECK(api.get_snapshot(s, &v) == AR_OK);
        Pose p;
        for (uint32_t i = 1; i < v.joint_count; ++i) {
            auto& t = v.joints[i].local;
            for (double x : t.translation) CHECK(std::isfinite(x));
            for (double x : t.rotation) CHECK(std::isfinite(x));
            p.joints.push_back({{t.translation[0],t.translation[1],t.translation[2]}, {t.rotation[0],t.rotation[1],t.rotation[2],t.rotation[3]}});
        }
        return p;
    }
};

void constructed() {
    Assets assets; Fixture f(assets);
    CHECK(f.advance(0) == AR_OK && !f.snapshots[0] && f.steps == 0);
    CHECK(f.advance(1.0/120) == AR_OK && f.steps == 1);
    CHECK((f.calls == std::vector<std::string>{"pre."+std::to_string(f.avatars[0]),"pre."+std::to_string(f.avatars[1]),"step",
        "post."+std::to_string(f.avatars[0]),"post."+std::to_string(f.avatars[1])}));
    // Renderer-independent consumer sees FINAL post append, rather than barrier pose.
    const auto a = f.pose(f.snapshots[0]), b = f.pose(f.snapshots[1]);
    CHECK(a != b);
    CHECK(std::abs(a.joints[2].rotation[2] - std::sin(20 * 3.141592653589793 / 180)) < 1e-6);
    CHECK(std::abs(b.joints[2].rotation[2] - std::sin(15 * 3.141592653589793 / 180)) < 1e-6);
    CHECK(b.joints[1].translation == (mmd::Double3{0,1,0}));
    CHECK(b.joints[0].translation == (mmd::Double3{4,2,0}));
    auto retained = f.snapshots[1]; CHECK(f.api.retain_snapshot(retained) == AR_OK);
    CHECK(f.advance(.1, true) == AR_OK && f.steps == 1 && f.pose(retained) == b);
    CHECK(f.advance(2.0/120) == AR_OK && f.steps == 3);
    f.badSnapshot = true;
    CHECK(f.advance(1.0/120) == AR_PROVIDER_ERROR && f.steps == 4 && !f.snapshots[0]);
    CHECK(std::find(f.diagnostics.begin(), f.diagnostics.end(), "MMD_PHYSICS_SNAPSHOT_INVALID") != f.diagnostics.end());
    f.badSnapshot = false; CHECK(f.advance(0) == AR_INVALID_STATE);
    CHECK(f.pose(retained) == b);
    for (uint64_t generation = 2; generation <= 4; ++generation) {
        CHECK(f.physics.reset_world(f.runtime, f.world, generation, nullptr) == AR_OK);
        f.failSubmit = true; CHECK(f.advance(1.0/120) == AR_PROVIDER_ERROR && f.steps == 0);
        f.failSubmit = false; CHECK(f.advance(0) == AR_OK && f.steps == 1 && f.pose(f.snapshots[1]) == b);
    }
    CHECK(f.api.release_snapshot(retained) == AR_OK);
    CHECK(f.physics.destroy_world(f.runtime, f.world) == AR_OK);
    for (auto avatar : f.avatars) {
        CHECK(f.api.reset_instance(f.runtime, avatar, 9) == AR_OK);
        CHECK(f.api.destroy_instance(f.runtime, avatar) == AR_OK);
    }
}

void bindingsAndOutputs() {
    Assets a;
    // Incomplete constructor config is rejected before it can be registered.
    bool rejected = false;
    try { avatarMmd::PhysicsAdapter invalid(a.config()); }
    catch (const std::invalid_argument&) { rejected = true; }
    CHECK(rejected);
    {
        Fixture f(a, false, [](auto& c) { c.layoutVersion = 2; });
        CHECK(f.advance(1.0/120) == AR_PROVIDER_ERROR && f.steps == 0);
        CHECK(f.diagnostics.front() == "MMD_ADAPTER_LAYOUT");
    }
    {
        Fixture f(a, false, [](auto& c) { c.joints[1].parent = -1; });
        CHECK(f.advance(1.0/120) == AR_PROVIDER_ERROR && f.steps == 0);
        CHECK(f.diagnostics.front() == "MMD_ADAPTER_PARENT");
    }
    a.model.morphs.resize(1); a.model.morphs[0].type = mmd::MorphType::Vertex;
    a.motion.morphs = {{0, {{0, .25f}}}}; a.motion.visibility = {{0, false}};
    {
        Fixture f(a);
        CHECK(f.advance(1.0/120) == AR_PROVIDER_ERROR && !f.snapshots[0]);
        CHECK(f.diagnostics.front() == "MMD_ADAPTER_MORPH");
    }
    {
        Fixture f(a, false, [](auto& c) {
            c.morphs = {{0, "mesh", "vertex"}}; c.visibilityTargets = {"avatar"};
        });
        CHECK(f.advance(1.0/120) == AR_OK);
        ArStateView view{AR_HEADER(ArStateView)}; CHECK(f.api.get_snapshot(f.snapshots[0], &view) == AR_OK);
        CHECK(view.blend_shapes[0].weight == .25 && view.visibility[0].visible == 0);
        auto retained = f.snapshots[0]; CHECK(f.api.retain_snapshot(retained) == AR_OK);
        CHECK(f.physics.reset_world(f.runtime, f.world, 2, nullptr) == AR_OK);
        CHECK(f.advance(1.0/120) == AR_OK);
        CHECK(view.blend_shapes[0].weight == .25 && view.visibility[0].visible == 0);
        CHECK(f.api.release_snapshot(retained) == AR_OK);
    }
}

#ifdef AVATAR_MMD_TOON_SMOKE
void publish(Fixture &f, std::unique_ptr<MmdToonCheck> (&consumers)[2]) {
  for (int i = 0; i < 2; ++i) {
    ArStateView state{AR_HEADER(ArStateView)};
    CHECK(f.api.get_snapshot(f.snapshots[i], &state) == AR_OK);
    if (!consumers[i])
      consumers[i] = std::make_unique<MmdToonCheck>(state);
    consumers[i]->Apply(f.api, f.snapshots[i]);
  }
}
void toonConstructed() {
  std::unique_ptr<MmdToonCheck> consumers[2];
  Assets assets;
  assets.model.morphs.resize(1);
  assets.model.morphs[0].type = mmd::MorphType::Vertex;
  assets.motion.morphs = {{0, {{0, .25f}}}};
  assets.motion.visibility = {{0, false}, {1, true}};
  {
    Fixture f(assets, false, [](auto &c) {
      c.morphs = {{0, "mesh", "vertex"}};
      c.visibilityTargets = {"avatar"};
    });
    CHECK(f.advance(1.0 / 120) == AR_OK);
    publish(f, consumers);
    CHECK(f.pose(f.snapshots[0]) != f.pose(f.snapshots[1]));
    CHECK(f.advance(.1, true) == AR_OK);
    publish(f, consumers);
    CHECK(f.advance(4.0 / 120) == AR_OK);
    publish(f, consumers);
    f.badSnapshot = true;
    CHECK(f.advance(1.0 / 120) == AR_PROVIDER_ERROR);
    CHECK(!f.snapshots[0] && !f.snapshots[1]);
    f.badSnapshot = false;
    CHECK(f.physics.reset_world(f.runtime, f.world, 2, nullptr) == AR_OK);
    CHECK(f.advance(1.0 / 120) == AR_OK);
    publish(f, consumers);
  }
  for (auto &consumer : consumers)
    consumer->AfterProducerDestruction();
}
#endif

#ifdef AVATAR_MMD_STAGE_RUNNER_SMOKE
#include "StageRunnerCheck.h"
#endif

int main(int argc, char **argv) {
  if (argc == 1) {
    constructed();
    bindingsAndOutputs();
#ifdef AVATAR_MMD_TOON_SMOKE
    toonConstructed();
#endif
  }
#ifdef AVATAR_MMD_JOLT_SMOKE
  else {
    CHECK(argc == 3);
    Assets assets(argv[1], argv[2]);
#ifdef AVATAR_MMD_STAGE_RUNNER_SMOKE
    stageRunner(assets);
    stageRunnerUnload(assets);
    stageRunnerUnload(assets, true);
#else
#ifdef AVATAR_MMD_TOON_SMOKE
    std::unique_ptr<MmdToonCheck> consumers[2];
#endif
    {
      Fixture f(assets, true);
      std::vector<Pose> first;
      for (uint64_t replay = 0; replay < 3; ++replay) {
        if (replay)
          CHECK(f.physics.reset_world(f.runtime, f.world, replay + 1,
                                      nullptr) == AR_OK);
        for (int tick = 0; tick < 24; ++tick) {
          CHECK(f.advance(1.0 / 120) == AR_OK && f.steps == tick + 1);
          auto pose = f.pose(f.snapshots[1]);
          auto config = assets.config();
          ArStateView view{AR_HEADER(ArStateView)};
          CHECK(f.api.get_snapshot(f.snapshots[1], &view) == AR_OK);
          auto continuation = config.evaluator.BeginPhysics(
              assets.motion, view.evaluation_seconds * 30);
          auto feedback = config.coupling.ApplyPhysicsToPose(
              f.results.at(f.avatars[0]), continuation.BeforePhysics());
          CHECK(feedback);
          auto expected =
              config.evaluator.ResumePhysics(continuation, feedback.value());
          CHECK(expected);
          CHECK(pose == expected.value());
#ifdef AVATAR_MMD_TOON_SMOKE
          publish(f, consumers);
#endif
          CHECK(std::abs(pose.joints[1].rotation[2]) > .1);
          CHECK(std::abs(pose.joints[2].rotation[2]) > .1);
          if (!replay)
            first.push_back(pose);
          else
            CHECK(pose == first[tick]);
          // Free body falls, but its associated root's translation remains
          // animation-owned.
          CHECK(pose.joints[0].translation ==
                continuation.BeforePhysics().joints[0].translation);
        }
      }
      CHECK(f.steps == 24 && f.resets == 2);
    }
#ifdef AVATAR_MMD_TOON_SMOKE
    for (auto &consumer : consumers)
      consumer->AfterProducerDestruction();
#endif
#endif
  }
#endif
  std::cout << "MMD pre/Step/post, final pose, instance isolation and "
               "reset/replay passed\n";
}
