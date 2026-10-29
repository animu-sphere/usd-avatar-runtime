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
#include <limits>
#include <set>
#ifdef AVATAR_MMD_TOON_SMOKE
#include "ToonCheck.h"
#endif
#ifdef AVATAR_MMD_STAGE_RUNNER_SMOKE
#include <avatarPhysics/SceneResources.h>
#include <avatarPhysics/StageWorld.h>
#include <pxr/usd/usd/editContext.h>
#include <pxr/usd/usdPhysics/joint.h>
#include <pxr/usd/usdPhysics/limitAPI.h>
#include <pxr/usd/usdPhysics/scene.h>
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
                *solver, scene, [this](const auto& body, auto& descriptor) {
                    descriptor.collisionFilter = {1,0};
                    descriptor.damping = {0,0};
                    if (body.motionType == core::MotionType::dynamicBody) {
                        TfToken mode;
                        CHECK(assets.stage->GetPrimAtPath(SdfPath(body.path)).GetAttribute(TfToken("mmd:physics:mode")).Get(&mode));
                        const auto q = GfRotation(GfVec3d(0,0,1), mode == TfToken("dynamic") ? 20 : 60).GetQuat();
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
                TfToken mode;
                CHECK(assets.stage->GetPrimAtPath(SdfPath(body.path)).GetAttribute(TfToken("mmd:physics:mode")).Get(&mode));
                const auto q = GfRotation(GfVec3d(0,0,1), mode == TfToken("dynamic") ? 20 : 60).GetQuat();
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
            bool registerWorld = true, bool constrainedResources = false,
            std::function<void(std::vector<ArMaterialInput>&)> materialLayout = {})
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
        std::vector<ArVertexUv> uvs;
        for (uint32_t i = 0; i < config.baseUvs.size(); ++i)
            uvs.push_back({config.uvMesh.c_str(), i, {config.baseUvs[i][0], config.baseUvs[i][1]}});
        std::reverse(uvs.begin(), uvs.end()); // identity mapping, not array position
        d.initial_state.vertex_uvs = uvs.data(); d.initial_state.vertex_uv_count = uint32_t(uvs.size());
        std::vector<ArMaterialInput> materials;
        if (!config.materials.empty()) materials.push_back({"unrelated", "unchanged", AR_VALUE_SCALAR, 0, {7,0,0,0}});
        const uint32_t types[]{4,3,1,3,4,1,4,4,4};
        for (const auto& b : config.materials) {
            ArMaterialInput value{b.target.c_str(), b.input.c_str(), types[static_cast<uint32_t>(b.field)], 0, {}};
            std::copy(b.base.begin(), b.base.end(), value.value); materials.push_back(value);
        }
        std::reverse(materials.begin(), materials.end());
        if (materialLayout) materialLayout(materials);
        d.initial_state.materials = materials.data(); d.initial_state.material_count = uint32_t(materials.size());
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
    CHECK(std::abs(a.joints[2].rotation[2] - std::sin(-22.5 * 3.141592653589793 / 180)) < 1e-6);
    CHECK(std::abs(b.joints[2].rotation[2] - std::sin(-27.5 * 3.141592653589793 / 180)) < 1e-6);
    CHECK(b.joints[1].translation == (mmd::Double3{0,1,0}));
    CHECK(std::abs(b.joints[0].translation[0] - (7 - std::cos(200 * 3.141592653589793 / 180))) < 1e-6);
    CHECK(std::abs(b.joints[0].translation[1] - (8 - std::sin(200 * 3.141592653589793 / 180))) < 1e-6);
    CHECK(b.joints[0].translation[2] == 9);
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

void
resolvedMorphOutputs()
{
    Assets a;
    a.model.morphs.resize(5);
    a.model.morphs[0].type = mmd::MorphType::Vertex;
    a.model.morphs[1].type = a.model.morphs[2].type = mmd::MorphType::Group;
    a.model.morphs[3].type = mmd::MorphType::Bone;
    a.model.morphs[4].type = mmd::MorphType::Uv;
    a.model.morphs[3].boneOffsets = {{2, {.1f, 0, 0}, {0, 0, 0, 1}}};
    a.model.morphs[1].members = {{0, .5f}, {3, 1.f}};
    a.model.morphs[2].members = {{1, 2.f}, {0, .25f}};
    a.motion.morphs = {{0, {{0, -.5f}}}, {1, {{0, -.25f}}}, {2, {{0, 2.f}}}};
    auto bind = [](auto& c) { c.morphs = {{0, "mesh", "vertex"}}; };
    {
        Fixture f(a, false, bind);
        for (uint64_t generation = 1; generation <= 3; ++generation) {
            if (generation > 1)
                CHECK(f.physics.reset_world(f.runtime, f.world, generation, nullptr) == AR_OK);
            CHECK(f.advance(1.0 / 120) == AR_OK);
            ArStateView view{AR_HEADER(ArStateView)};
            CHECK(f.api.get_snapshot(f.snapshots[0], &view) == AR_OK);
            // Nested diamond plus direct input, resolved once and unclamped.
            CHECK(view.blend_shapes[0].weight == 1.875);
            // Group bone effects are already in the pose and are not applied again.
            CHECK(std::abs(view.joints[3].local.translation[0] - .375) < 1e-7);
            CHECK(view.joints[3].local.translation[1] == 1);
            auto retained = f.snapshots[0];
            CHECK(f.api.retain_snapshot(retained) == AR_OK);
            CHECK(f.advance(.1, true) == AR_OK && f.steps == 1);
            CHECK(view.blend_shapes[0].weight == 1.875);
            CHECK(f.api.release_snapshot(retained) == AR_OK);
        }
    }
    // Negative, zero and absent channels all replace the previous slot value.
    a.motion.morphs = {{0, {{0, -2.f}, {1, 0.f}}}};
    {
        Fixture f(a, false, bind);
        CHECK(f.advance(1.0 / 120) == AR_OK);
        ArStateView view{AR_HEADER(ArStateView)};
        CHECK(f.api.get_snapshot(f.snapshots[0], &view) == AR_OK &&
              view.blend_shapes[0].weight < 0);
        CHECK(f.advance(3.0 / 120) == AR_OK);
        CHECK(f.api.get_snapshot(f.snapshots[0], &view) == AR_OK &&
              view.blend_shapes[0].weight == 0);
    }
    a.motion.morphs.clear();
    {
        Fixture f(a, false, bind);
        CHECK(f.advance(1.0 / 120) == AR_OK);
        ArStateView view{AR_HEADER(ArStateView)};
        CHECK(f.api.get_snapshot(f.snapshots[0], &view) == AR_OK &&
              view.blend_shapes[0].weight == 0);
    }
    // A reached unsupported leaf refuses even at zero and via a zero coefficient.
    for (auto type : {mmd::MorphType::Uv,
                      mmd::MorphType::Material,
                      mmd::MorphType::Flip,
                      mmd::MorphType::Impulse}) {
        a.model.morphs[4].type = type;
        a.model.morphs[1].members.push_back({4, 0.f});
        a.motion.morphs = {{2, {{0, 0.f}}}};
        Fixture f(a, false, bind);
        CHECK(f.advance(1.0 / 120) == AR_PROVIDER_ERROR && !f.snapshots[0] && !f.snapshots[1]);
        CHECK(std::find(f.diagnostics.begin(), f.diagnostics.end(), "MMD_ADAPTER_MORPH") != f.diagnostics.end());
        CHECK(f.advance(0) == AR_INVALID_STATE);
        a.model.morphs[1].members.pop_back();
    }
    a.model.morphs[3].boneOffsets.clear();
    for (float weight :
         {std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()}) {
        a.motion.morphs = {{2, {{0, weight}}}};
        Fixture f(a, false, bind);
        CHECK(f.advance(1.0 / 120) == AR_PROVIDER_ERROR && !f.snapshots[0]);
        CHECK(std::find(f.diagnostics.begin(), f.diagnostics.end(), "MMD_ADAPTER_MORPH") != f.diagnostics.end());
    }
}

void uvMorphOutputs() {
    Assets a;
    a.model.mesh.points.resize(2); a.model.mesh.st = {{{.25f,.75f}}, {{0,1}}};
    a.model.morphs.resize(3);
    a.model.morphs[0].type = mmd::MorphType::Uv;
    a.model.morphs[0].uvOffsets = {{1, {.25f, .5f, 4, 8}}};
    a.model.morphs[1].members = {{0, .5f}};
    a.model.morphs[2].members = {{1, 2.f}, {0, .25f}};
    a.motion.morphs = {{0, {{0, -.5f}}}, {1, {{0, -.25f}}}, {2, {{0, 2.f}}}};
    const auto bind = [](auto& c) { c.uvMorphs = {0}; c.uvMesh = "mesh"; c.baseUvs = {{.25,.75},{0,1}}; };
#ifdef AVATAR_MMD_TOON_SMOKE
    std::unique_ptr<MmdToonCheck> uvConsumers[2];
#endif
    {
        Fixture f(a, false, bind);
        ArSnapshot retained = 0;
        for (uint64_t generation = 1; generation <= 3; ++generation) {
            if (generation > 1) CHECK(f.physics.reset_world(f.runtime, f.world, generation, nullptr) == AR_OK);
            CHECK(f.advance(1. / 120) == AR_OK);
            ArStateView v{AR_HEADER(ArStateView)}; CHECK(f.api.get_snapshot(f.snapshots[0], &v) == AR_OK);
            CHECK(v.vertex_uv_count == 2 && v.vertex_uvs[0].vertex_index == 1);
            CHECK(v.vertex_uvs[0].value[0] == .25 * 1.875 && v.vertex_uvs[0].value[1] == 1 - .5 * 1.875);
            CHECK(v.vertex_uvs[1].value[0] == .25 && v.vertex_uvs[1].value[1] == .75);
#ifdef AVATAR_MMD_TOON_SMOKE
            for (int i = 0; i < 2; ++i) {
                ArStateView state{AR_HEADER(ArStateView)};
                CHECK(f.api.get_snapshot(f.snapshots[i], &state) == AR_OK);
                if (!uvConsumers[i]) uvConsumers[i] = std::make_unique<MmdToonCheck>(state);
                uvConsumers[i]->Apply(f.api, f.snapshots[i]);
            }
#endif
            if (!retained) { retained = f.snapshots[0]; CHECK(f.api.retain_snapshot(retained) == AR_OK); }
        }
        ArStateView old{AR_HEADER(ArStateView)}; CHECK(f.api.get_snapshot(retained, &old) == AR_OK);
        CHECK(old.generation == 1 && old.vertex_uvs[0].value[0] == .25 * 1.875);
        CHECK(f.api.release_snapshot(retained) == AR_OK);
    }
#ifdef AVATAR_MMD_TOON_SMOKE
    for (auto& consumer : uvConsumers) consumer->AfterProducerDestruction();
#endif
    a.motion.morphs = {{0, {{0, -2.f}, {1, 0.f}}}};
    {
        Fixture f(a, false, bind); CHECK(f.advance(1. / 120) == AR_OK);
        CHECK(f.advance(3. / 120) == AR_OK);
        ArStateView v{AR_HEADER(ArStateView)}; CHECK(f.api.get_snapshot(f.snapshots[0], &v) == AR_OK);
        CHECK(v.vertex_uvs[0].value[0] == 0 && v.vertex_uvs[0].value[1] == 1);
    }
    // Owner validation still runs on reached zero-weight UV offsets.
    a.model.morphs[0].uvOffsets[0].delta[0] = std::numeric_limits<float>::infinity();
    a.motion.morphs = {{0, {{0, 0.f}}}};
    {
        Fixture f(a, false, bind); CHECK(f.advance(1. / 120) == AR_PROVIDER_ERROR && !f.snapshots[0]);
        CHECK(std::find(f.diagnostics.begin(), f.diagnostics.end(), "MMD_MOTION_UV_MORPH_INVALID") != f.diagnostics.end());
    }
    a.model.morphs[0].uvOffsets[0].delta[0] = .25f;
    // Owner double deltas may be valid while final renderer float values overflow.
    a.model.morphs[0].uvOffsets[0].delta[0] = std::numeric_limits<float>::max();
    a.motion.morphs = {{0, {{0, 2.f}}}};
    {
        Fixture f(a, false, bind); CHECK(f.advance(1. / 120) == AR_PROVIDER_ERROR && !f.snapshots[0]);
        CHECK(std::find(f.diagnostics.begin(), f.diagnostics.end(), "MMD_ADAPTER_UV") != f.diagnostics.end());
    }
    a.model.morphs[0].uvOffsets[0].delta[0] = .25f;
    a.motion.morphs = {{0, {{0, 0.f}}}};
    a.model.mesh.additionalUvCount = 1; a.model.mesh.additionalUv[0].resize(2);
    a.model.morphs[0].type = mmd::MorphType::AdditionalUv1;
    { Fixture f(a, false, bind); CHECK(f.advance(1. / 120) == AR_PROVIDER_ERROR && !f.snapshots[0]); }
}

void materialMorphOutputs() {
    using Field = avatarMmd::MaterialField;
    const uint32_t sizes[]{4,3,1,3,4,1,4,4,4};
    const char* names[]{"diffuse", "specular", "power", "ambient", "edge", "size", "texture", "sphere", "toon"};
    Assets a;
    a.model.materials.resize(2); a.model.morphs.resize(4);
    a.model.morphs[0].type = a.model.morphs[1].type = mmd::MorphType::Material;
    mmd::MorphMaterialOffset multiply, add;
    multiply.material = mmd::kNone; multiply.operation = mmd::MaterialMorphOperation::Multiply;
    add.material = 0; add.operation = mmd::MaterialMorphOperation::Add;
    // Distinct binary fractions exercise all 28 components independently.
    const auto fill = [](auto& value, float start) {
        value.diffuseColor = {start, start+.03125f, start+.0625f, start+.09375f};
        value.specularColor = {start+.125f, start+.15625f, start+.1875f};
        value.specularPower = start+.21875f;
        value.ambientColor = {start+.25f, start+.28125f, start+.3125f};
        value.edgeColor = {start+.34375f, start+.375f, start+.40625f, start+.4375f};
        value.edgeSize = start+.46875f;
        value.textureTint = {start+.5f, start+.53125f, start+.5625f, start+.59375f};
        value.sphereTint = {start+.625f, start+.65625f, start+.6875f, start+.71875f};
        value.toonTint = {start+.75f, start+.78125f, start+.8125f, start+.84375f};
    };
    fill(multiply, 1.5f); fill(add, .25f);
    a.model.morphs[0].materialOffsets = {multiply};
    a.model.morphs[1].materialOffsets = {add};
    a.model.morphs[2].members = {{0,.5f}};
    a.model.morphs[3].members = {{1,.5f}};
    a.motion.morphs = {{0, {{0,.5f},{1,.5f},{2,0}}}, {2, {{0,1},{1,1},{2,0}}},
        {3, {{0,-1},{1,-1},{2,0}}}};
    const auto bind = [&](auto& c) {
        c.materialMorphs = {0,1};
        for (int material = 0; material < 2; ++material) {
            uint32_t component = 0;
            for (uint32_t field = 0; field < 9; ++field) {
                avatarMmd::MaterialInputBinding b{material, static_cast<Field>(field),
                    "material" + std::to_string(material), names[field], {}};
                for (uint32_t n = 0; n < sizes[field]; ++n)
                    b.base[n] = (1 + component++ + material) / 16.;
                c.materials.push_back(b);
            }
        }
    };
    const auto check = [&](const ArRuntimeApi& api, ArSnapshot snapshot, bool active) {
        ArStateView v{AR_HEADER(ArStateView)}; CHECK(api.get_snapshot(snapshot, &v) == AR_OK);
        CHECK(v.material_count == 19 && v.materials[18].value[0] == 7 && !v.materials[18].overridden);
        for (uint32_t material = 0; material < 2; ++material) {
            uint32_t component = 0;
            for (uint32_t field = 0; field < 9; ++field) {
                // Runtime order is reversed, and includes an unrelated input.
                const auto& value = v.materials[17 - (material * 9 + field)];
                CHECK(std::string(value.material_id) == "material" + std::to_string(material));
                CHECK(std::string(value.input_id) == names[field] && value.value_type == sizes[field] && value.overridden);
                for (uint32_t n = 0; n < 4; ++n) {
                    double expected = 0;
                    if (n < sizes[field]) {
                        const double base = (1 + component + material) / 16.;
                        expected = active ? base * (1.5 + component / 32.) -
                            (material == 0 ? .5 * (.25 + component / 32.) : 0) : base;
                        ++component;
                    }
                    CHECK(value.value[n] == expected);
                }
            }
        }
    };
    ArSnapshot retained = 0; ArRuntimeApi api{};
    {
        Fixture f(a, false, bind); api = f.api;
        CHECK(f.adapter->PostDescriptor().writes & AR_DOMAIN_MATERIAL);
        for (uint64_t generation = 1; generation <= 3; ++generation) {
            if (generation > 1) CHECK(f.physics.reset_world(f.runtime, f.world, generation, nullptr) == AR_OK);
            CHECK(f.advance(1./120) == AR_OK);
            for (auto snapshot : f.snapshots) check(f.api, snapshot, true);
            if (!retained) { retained = f.snapshots[0]; CHECK(api.retain_snapshot(retained) == AR_OK); }
            CHECK(f.advance(3./120) == AR_OK); check(f.api, f.snapshots[0], true);
            CHECK(f.advance(4./120) == AR_OK); check(f.api, f.snapshots[0], false);
        }
    }
    check(api, retained, true); CHECK(api.release_snapshot(retained) == AR_OK);
    {
        Fixture f(a, false, [&](auto& c) { bind(c); c.motion.morphs.clear(); });
        CHECK(f.advance(1./120) == AR_OK); check(f.api, f.snapshots[0], false);
    }
    const auto affine = [&](auto& c) {
        bind(c);
        auto old = std::move(c.materials); c.materials.clear();
        for (auto b : old) {
            if (uint32_t(b.field) < 6) { c.materials.push_back(b); continue; }
            b.base = {};
            const auto input = b.input;
            b.mode = avatarMmd::MaterialValueMode::Multiply; b.input = input + ":multiply"; c.materials.push_back(b);
            b.mode = avatarMmd::MaterialValueMode::Add; b.input = input + ":add"; c.materials.push_back(b);
        }
    };
    {
        Fixture f(a, false, affine);
        for (int tick = 1; tick <= 9; ++tick) {
            CHECK(f.advance(1. / 120.) == AR_OK);
            if (tick != 1 && tick != 9) continue;
            ArStateView v{AR_HEADER(ArStateView)}; CHECK(f.api.get_snapshot(f.snapshots[0], &v) == AR_OK);
            CHECK(v.material_count == 25);
            for (uint32_t i = 0; i < v.material_count; ++i) {
                const auto& m = v.materials[i]; const std::string name(m.input_id);
                const auto sep = name.find(':'); if (sep == std::string::npos) continue;
                const uint32_t tint = name.rfind("texture",0) == 0 ? 0 : name.rfind("sphere",0) == 0 ? 1 : 2;
                for (uint32_t n = 0; n < 4; ++n) {
                    const double component = 16 + tint * 4 + n;
                    const bool multiply = name.substr(sep) == ":multiply";
                    const double expected = tick == 1 ? (multiply ? 1.5 + component / 32. :
                        (std::string(m.material_id) == "material0" ? -.5 * (.25 + component / 32.) : 0)) : (multiply ? 1 : 0);
                    CHECK(m.value[n] == expected);
                }
            }
        }
    }
    // A post-Step arithmetic refusal preserves an earlier retained frame and
    // requires group reset; the first frame after reset uses immutable bases.
    {
        Fixture f(a, false, [&](auto& c) {
            bind(c); c.materials[0].base[0] = std::numeric_limits<float>::max();
            c.frameOffset = -.25; c.motion.morphs = {{0, {{0,0},{1,2}}}};
        });
        CHECK(f.advance(1./120) == AR_OK);
        const auto previous = f.snapshots[0]; CHECK(f.api.retain_snapshot(previous) == AR_OK);
        CHECK(f.advance(1./120) == AR_PROVIDER_ERROR && !f.snapshots[0]);
        ArStateView v{AR_HEADER(ArStateView)}; CHECK(f.api.get_snapshot(previous, &v) == AR_OK);
        CHECK(v.frame_id == 1 && v.materials[17].value[0] == std::numeric_limits<float>::max());
        CHECK(f.physics.reset_world(f.runtime, f.world, 2, nullptr) == AR_OK);
        CHECK(f.advance(1./120) == AR_OK);
        CHECK(f.api.get_snapshot(f.snapshots[0], &v) == AR_OK && v.generation == 2);
        CHECK(v.materials[17].value[0] == std::numeric_limits<float>::max());
        CHECK(f.api.release_snapshot(previous) == AR_OK);
    }
    const auto fail = [&](auto configure, auto layout, const char* diagnostic) {
        Fixture f(a, false, configure, true, false, layout);
        CHECK(f.advance(1./120) == AR_PROVIDER_ERROR && !f.snapshots[0] && !f.snapshots[1]);
        CHECK(std::find(f.diagnostics.begin(), f.diagnostics.end(), diagnostic) != f.diagnostics.end());
    };
    const auto unchanged = [](auto&) {};
    // Missing/mistyped targets are refused before publishing any output.
    fail(bind, [](auto& values) { values.erase(values.begin()); }, "MMD_ADAPTER_MATERIAL");
    fail(bind, [](auto& values) { values[0].value_type = AR_VALUE_VEC3; values[0].value[3] = 0; }, "MMD_ADAPTER_MATERIAL");
    fail(bind, [](auto& values) { values[0].input_id = "wrong"; }, "MMD_ADAPTER_MATERIAL");
    a.model.materials.resize(3); // all-material reaches an unbound target, even at zero
    a.motion.morphs = {{0, {{0,0}}}};
    fail(bind, unchanged, "MMD_ADAPTER_MATERIAL"); a.model.materials.resize(2);
    a.model.morphs[0].materialOffsets[0].toonTint[3] = std::numeric_limits<float>::infinity();
    fail(bind, unchanged, "MMD_MOTION_MATERIAL_MORPH_INVALID");
    a.model.morphs[0].materialOffsets[0] = multiply;
    a.motion.morphs = {{0, {{0,2}}}};
    fail([&](auto& c) { bind(c); c.materials[0].base[0] = std::numeric_limits<float>::max(); },
        unchanged, "MMD_ADAPTER_MATERIAL");
    // Constructor refuses incomplete/duplicate field sets and bad immutable bases.
    const auto rejected = [&](auto change) {
        auto c = a.config(); bind(c);
        c.submit = [](const auto&, auto) { return AR_OK; };
        c.snapshot = [](const auto&, auto&) { return AR_OK; };
        change(c); bool threw = false;
        try { avatarMmd::PhysicsAdapter adapter(std::move(c)); }
        catch (const std::invalid_argument&) { threw = true; }
        CHECK(threw);
    };
    rejected([](auto& c) { c.materials.pop_back(); });
    rejected([](auto& c) { c.materials[1].field = Field::DiffuseColor; });
    rejected([](auto& c) { c.materials[0].field = static_cast<Field>(99); });
    rejected([](auto& c) { c.materials[0].target.clear(); });
    rejected([](auto& c) { c.materials[0].material = -1; });
    rejected([](auto& c) { for (auto& b : c.materials) b.target = "same"; });
    rejected([](auto& c) { c.materials[1].input = c.materials[0].input; });
    rejected([](auto& c) { c.materials[1].base[3] = 1; });
    rejected([](auto& c) { c.materials[0].base[0] = std::numeric_limits<double>::quiet_NaN(); });
    rejected([](auto& c) { c.materialMorphs.push_back(0); });
    rejected([](auto& c) { c.materials.clear(); });
    rejected([](auto& c) { c.materials[0].mode = avatarMmd::MaterialValueMode::Multiply; });
    rejected([](auto& c) { c.materials[6].base = {}; c.materials[6].mode = avatarMmd::MaterialValueMode::Multiply; });
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
    resolvedMorphOutputs();
    uvMorphOutputs();
    materialMorphOutputs();
#ifdef AVATAR_MMD_TOON_SMOKE
    toonConstructed();
#endif
  }
#ifdef AVATAR_MMD_JOLT_SMOKE
  else {
    CHECK(argc == 3);
    Assets assets(argv[1], argv[2]);
#ifdef AVATAR_MMD_STAGE_RUNNER_SMOKE
    stageWorldJolt(assets);
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
          // The associated mode-1 body drives the root position, including gravity.
          CHECK(pose.joints[0].translation[1] <
                continuation.BeforePhysics().joints[0].translation[1]);
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
