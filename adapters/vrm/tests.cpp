#include "avatarVrm/ExpressionAdapter.h"
#include "pxr/base/gf/quatd.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>

#define CHECK(x) do { if (!(x)) { std::cerr << "Line " << __LINE__ << ": " << #x << '\n'; std::exit(1); } } while (0)
namespace {
ArRuntimeApi api{};
bool near(double a, double b) { return std::abs(a - b) < 1e-6; }
struct Log {
    std::vector<std::string> codes, origins, evaluators;
    static void AR_CALL emit(void* user, const ArDiagnostic* d) {
        auto& l = *static_cast<Log*>(user);
        l.codes.emplace_back(d->code); l.origins.emplace_back(d->origin); l.evaluators.emplace_back(d->evaluator_id);
    }
    bool has(const char* code) const { return std::find(codes.begin(), codes.end(), code) != codes.end(); }
    ArDiagnosticSink sink() { return {this, emit}; }
};
avatarVrm::ExpressionAdapterConfig config() {
    avatarVrm::ExpressionAdapterConfig c;
    c.evaluatorId = "vrm.face"; c.layoutId = "vrm.test.layout"; c.layoutVersion = 3;
    vrmRig::ExpressionDefinition happy; happy.name = "happy";
    happy.overrideBlink = vrmRig::ExpressionOverride::Blend;
    happy.overrideLookAt = vrmRig::ExpressionOverride::Blend;
    happy.morphTargets.push_back({"owner.smile", 0.8f});
    happy.materialColors.push_back({"face", "color", pxr::GfVec4f(1, 0, 0, 0.5f)});
    CHECK(c.expressions.Add(happy));
    vrmRig::ExpressionDefinition blink; blink.name = "blink"; blink.isBinary = true;
    blink.morphTargets.push_back({"owner.blink", 1}); CHECK(c.expressions.Add(blink));
    for (const char* name : {"lookLeft", "lookRight", "lookUp", "lookDown"}) {
        vrmRig::ExpressionDefinition look; look.name = name;
        look.morphTargets.push_back({std::string("owner.") + name, 1}); CHECK(c.expressions.Add(look));
    }
    c.inputs = {{{"tracker", "actor", "face:smile"}, "happy"},
                {{"tracker", "actor", "face:blink"}, "blink"},
                {{"tracker", "actor", "face:unknown"}, "unknown"},
                {{"tracker", "actor", "face:look"}, "lookLeft"}};
    c.morphs = {{"owner.smile", "mesh", "smile"}, {"owner.blink", "mesh", "blink"}};
    for (const char* name : {"lookLeft", "lookRight", "lookUp", "lookDown"})
        c.morphs.push_back({std::string("owner.") + name, "mesh", name});
    vrmRig::LookAtRig gaze; gaze.type = vrmRig::LookAtType::Expression;
    gaze.offsetFromHeadBone = pxr::GfVec3f(0, 0.1f, 0);
    gaze.horizontalInner.outputScale = gaze.horizontalOuter.outputScale = 1;
    gaze.verticalDown.outputScale = gaze.verticalUp.outputScale = 1;
    c.lookAt = gaze; c.gaze = {"tracker", "actor", "eyes:target"};
    c.headSkeleton = "rig"; c.headJoint = "head";
    return c;
}
struct Fixture {
    avatarVrm::ExpressionAdapter adapter;
    ArRuntime runtime = 0;
    ArJoint joints[4]{{"rig", "root", -1, {{2,0,0}, {0,0,0,1}, {1,1,1}}},
                     {"rig", "head", 0, {{0,1,0}, {0,0,0,1}, {1,1,1}}},
                     {"rig", "eye.L", 1, {{0.03,0.1,0}, {0,0,0,1}, {1,1,1}}},
                     {"rig", "eye.R", 1, {{-0.03,0.1,0}, {0,0,0,1}, {1,1,1}}}};
    ArBlendShape morphs[6]{{"mesh", "smile", 0.12}, {"mesh", "blink", 0.3},
                          {"mesh", "lookLeft", 0}, {"mesh", "lookRight", 0},
                          {"mesh", "lookUp", 0}, {"mesh", "lookDown", 0}};
    ArMaterialInput materials[2]{{"face", "inputs:vrm:material:baseColorFactor", AR_VALUE_VEC3, 0, {0.2,0.4,0.6,0}},
                                 {"face", "inputs:vrm:material:baseColorAlphaFactor", AR_VALUE_SCALAR, 0, {0.8,0,0,0}}};
    explicit Fixture(avatarVrm::ExpressionAdapterConfig c = config()) : adapter(std::move(c)) {
        CHECK(api.create_runtime(&runtime) == AR_OK);
        auto d = adapter.Descriptor(); CHECK(api.register_evaluator(runtime, &d, nullptr) == AR_OK);
    }
    ~Fixture() { if (runtime) CHECK(api.destroy_runtime(runtime) == AR_OK); }
    ArInstance make(const char* layout = "vrm.test.layout", uint64_t version = 3, const char* predecessor = nullptr) {
        const char* selected[] = {"vrm.face", predecessor};
        auto evaluator = adapter.Descriptor();
        ArInstanceDesc d{AR_HEADER(ArInstanceDesc)}; d.generation = 1;
        d.layout_id = layout; d.layout_version = version; d.evaluators = selected; d.evaluator_count = predecessor ? 2 : 1;
        d.bound_capabilities = evaluator.supplies; d.bound_capability_count = evaluator.supply_count;
        d.required_capabilities = evaluator.supplies; d.required_capability_count = evaluator.supply_count;
        d.initial_state = {AR_HEADER(ArStateView)};
        d.initial_state.joints = joints; d.initial_state.joint_count = 4;
        d.initial_state.blend_shapes = morphs; d.initial_state.blend_shape_count = 6;
        d.initial_state.materials = materials; d.initial_state.material_count = 2;
        ArInstance instance = 0; CHECK(api.create_instance(runtime, &d, nullptr, &instance) == AR_OK); return instance;
    }
};
ArInputFrame frame(uint64_t id) {
    ArInputFrame f{AR_HEADER(ArInputFrame)}; f.frame_id = id; f.generation = 1;
    f.evaluation_seconds = 0.25 * double(id); f.input_revision = 7; return f;
}
ArStateView view(ArSnapshot s) {
    ArStateView v{AR_HEADER(ArStateView)}; CHECK(api.get_snapshot(s, &v) == AR_OK); return v;
}
void evaluation() {
    Fixture f; auto instance = f.make(); auto other = f.make();
    ArScalarInput scalars[]{{"tracker", "actor", "face:smile", 0.25, 0, 1, 0},
                            {"tracker", "actor", "face:blink", 1, 0, 1, 0},
                            {"tracker", "actor", "face:unknown", 1, 0, 1, 0},
                            {"tracker", "actor", "face:look", 0.9, 0, 1, 0},
                            {"tracker", "otherActor", "face:smile", 1, 0, 1, 0},
                            {"otherTracker", "actor", "face:smile", 1, 0, 1, 0}};
    ArGazeInput gaze{"tracker", "actor", "eyes:target", AR_GAZE_POINT, AR_GAZE_RUNTIME_WORLD,
                     AR_OBSERVATION_VALID, nullptr, nullptr, {3,1.1,1}, 0, 1, 0};
    auto input = frame(1); input.scalars = scalars; input.scalar_count = 6; input.gazes = &gaze; input.gaze_count = 1;
    Log log; auto sink = log.sink(); ArSnapshot snapshot = 0;
    CHECK(api.evaluate_frame(f.runtime, instance, &input, &sink, &snapshot) == AR_OK);
    auto output = view(snapshot);
    // Numeric parity with independently invoked owner LookAt -> resolver.
    auto c = config(); vrmRig::LookAtInput ownerInput;
    ownerInput.timestamp = input.evaluation_seconds; ownerInput.head.position = pxr::GfVec3f(2,1,0);
    ownerInput.target = pxr::GfVec3f(3,1.1f,1);
    auto looked = vrmRig::LookAtEvaluator(*c.lookAt).Evaluate(ownerInput);
    openstrata::motion::MotionChannelSet weights; weights.Set("happy", 0.25f); weights.Set("blink", 1);
    weights.Set("unknown", 1); for (const auto& w : looked.expressions.entries) weights.Set(w.name, w.value);
    auto expected = vrmRig::ExpressionResolver(c.expressions).Resolve(weights);
    for (const auto& target : expected.morphTargets) {
        auto b = std::find_if(c.morphs.begin(), c.morphs.end(), [&](const auto& binding) { return binding.ownerTarget == target.target; });
        CHECK(near(output.blend_shapes[size_t(b - c.morphs.begin())].weight, target.weight));
    }
    auto color = expected.materialColors[0].Apply(pxr::GfVec4f(0.2f,0.4f,0.6f,0.8f));
    CHECK(output.materials[0].overridden == 1 && output.materials[1].overridden == 1);
    for (int i = 0; i < 3; ++i) CHECK(near(output.materials[0].value[i], color[i]));
    CHECK(near(output.materials[1].value[0], color[3]));
    CHECK(output.input_revision == 7 && output.layout_version == 3 && output.capability_count == 2);
    CHECK(log.has("VRM_EXPRESSION_UNRESOLVED") && log.has("VRM_EXPRESSION_SUPPRESSED") && log.has("VRM_ADAPTER_GAZE_PRECEDENCE"));
    for (size_t i = 0; i < log.codes.size(); ++i) {
        CHECK(log.origins[i] == "usd-vrm-plugins.vrmRig" && log.evaluators[i] == "vrm.face");
    }
    auto absent = frame(2); ArSnapshot baseline = 0;
    CHECK(api.evaluate_frame(f.runtime, instance, &absent, nullptr, &baseline) == AR_OK);
    auto base = view(baseline); CHECK(near(base.blend_shapes[0].weight, 0.12) && base.materials[0].overridden == 0);
    CHECK(near(output.blend_shapes[0].weight, 0.2)); // retained first output
    CHECK(api.release_snapshot(baseline) == AR_OK);
    auto zero = frame(1); scalars[0].value = 0; zero.scalars = scalars; zero.scalar_count = 1;
    CHECK(api.evaluate_frame(f.runtime, other, &zero, nullptr, &baseline) == AR_OK);
    CHECK(view(baseline).blend_shapes[0].weight == 0 && view(baseline).materials[0].overridden == 1);
    CHECK(api.release_snapshot(baseline) == AR_OK);
    auto stale = frame(3); gaze.validity = AR_OBSERVATION_STALE; stale.gazes = &gaze; stale.gaze_count = 1;
    CHECK(api.evaluate_frame(f.runtime, instance, &stale, &sink, &baseline) == AR_OK);
    CHECK(log.has("VRM_ADAPTER_GAZE_UNAVAILABLE") && near(view(baseline).blend_shapes[0].weight, 0.12));
    CHECK(api.release_snapshot(baseline) == AR_OK);
    auto invalid = frame(4); gaze.validity = AR_OBSERVATION_VALID; gaze.kind = AR_GAZE_DIRECTION;
    gaze.value[0] = 0; gaze.value[1] = 0; gaze.value[2] = 2; invalid.gazes = &gaze; invalid.gaze_count = 1;
    CHECK(api.evaluate_frame(f.runtime, instance, &invalid, &sink, &baseline) == AR_INVALID_ARGUMENT && baseline == 0);
    gaze.value[2] = 1;
    CHECK(api.evaluate_frame(f.runtime, instance, &invalid, nullptr, &baseline) == AR_OK); // same-frame retry
    CHECK(api.release_snapshot(baseline) == AR_OK);
    auto clamped = frame(5); scalars[0].value = 1e100; clamped.scalars = scalars; clamped.scalar_count = 1;
    CHECK(api.evaluate_frame(f.runtime, instance, &clamped, &sink, &baseline) == AR_OK);
    CHECK(log.has("VRM_EXPRESSION_CLAMPED") && near(view(baseline).blend_shapes[0].weight, 0.8));
    CHECK(api.release_snapshot(baseline) == AR_OK);
    CHECK(api.reset_instance(f.runtime, instance, 2) == AR_OK);
    auto reset = frame(1); reset.generation = 2;
    CHECK(api.evaluate_frame(f.runtime, instance, &reset, nullptr, &baseline) == AR_OK);
    CHECK(view(baseline).generation == 2 && near(view(baseline).blend_shapes[0].weight, 0.12));
    CHECK(api.release_snapshot(baseline) == AR_OK);
    CHECK(api.destroy_runtime(f.runtime) == AR_OK); f.runtime = 0;
    CHECK(view(snapshot).layout_version == 3 && near(view(snapshot).materials[1].value[0], color[3]));
    CHECK(api.release_snapshot(snapshot) == AR_OK);
}
void invalidBindings() {
    auto c = config(); c.inputs.push_back(c.inputs[0]);
    bool threw = false;
    try { avatarVrm::ExpressionAdapter adapter(c); } catch (const std::invalid_argument&) { threw = true; }
    CHECK(threw);
    c = config(); c.lookAt->type = vrmRig::LookAtType::Bone; threw = false;
    try { avatarVrm::ExpressionAdapter adapter(c); } catch (const std::invalid_argument&) { threw = true; }
    CHECK(threw);
    {
        Fixture f; auto id = f.make("different", 3); auto input = frame(1); ArSnapshot s = 0;
        Log log; auto sink = log.sink();
        CHECK(api.evaluate_frame(f.runtime, id, &input, &sink, &s) == AR_PROVIDER_ERROR && s == 0);
        CHECK(log.has("VRM_ADAPTER_LAYOUT"));
        id = f.make("vrm.test.layout", 4);
        CHECK(api.evaluate_frame(f.runtime, id, &input, &sink, &s) == AR_PROVIDER_ERROR && s == 0);
    }
    {
        c = config(); c.morphs.pop_back(); Fixture f(c); auto id = f.make();
        auto input = frame(1); ArSnapshot s = 0; Log log; auto sink = log.sink();
        CHECK(api.evaluate_frame(f.runtime, id, &input, &sink, &s) == AR_PROVIDER_ERROR && s == 0);
        CHECK(log.has("VRM_ADAPTER_TARGET"));
    }
    {
        Fixture f; f.materials[0].input_id = "noncanonical"; auto id = f.make();
        auto input = frame(1); ArSnapshot s = 0; Log log; auto sink = log.sink();
        CHECK(api.evaluate_frame(f.runtime, id, &input, &sink, &s) == AR_PROVIDER_ERROR && s == 0);
        CHECK(log.has("VRM_ADAPTER_TARGET"));
    }
    {
        c = config(); vrmRig::ExpressionDefinition unsupported; unsupported.name = "unsupported";
        unsupported.materialColors.push_back({"face", "private.shader.slot", pxr::GfVec4f(1)});
        CHECK(c.expressions.Add(unsupported)); Fixture f(c); auto id = f.make();
        auto input = frame(1); ArSnapshot s = 0; Log log; auto sink = log.sink();
        CHECK(api.evaluate_frame(f.runtime, id, &input, &sink, &s) == AR_PROVIDER_ERROR && s == 0);
        CHECK(log.has("VRM_ADAPTER_OUTPUT"));
    }
    {
        c = config(); c.lookAt.reset(); Fixture f(c); auto id = f.make();
        auto input = frame(1); ArSnapshot s = 0;
        CHECK(api.evaluate_frame(f.runtime, id, &input, nullptr, &s) == AR_OK);
        CHECK(view(s).capability_count == 1 && near(view(s).blend_shapes[0].weight, 0.12));
        CHECK(api.release_snapshot(s) == AR_OK);
    }
    {
        Fixture f; f.materials[0].value[0] = 1e100; auto id = f.make();
        ArScalarInput scalar{"tracker", "actor", "face:smile", 1, 0, 1, 0};
        auto input = frame(1); input.scalars = &scalar; input.scalar_count = 1;
        ArSnapshot s = 0; Log log; auto sink = log.sink();
        // The owner morph result was already written before material narrowing
        // fails. Retry must begin at baseline, not that partial morph state.
        CHECK(api.evaluate_frame(f.runtime, id, &input, &sink, &s) == AR_PROVIDER_ERROR && s == 0);
        CHECK(log.has("VRM_ADAPTER_RANGE"));
        input.scalars = nullptr; input.scalar_count = 0;
        CHECK(api.evaluate_frame(f.runtime, id, &input, nullptr, &s) == AR_OK);
        CHECK(near(view(s).blend_shapes[0].weight, 0.12) && view(s).materials[0].overridden == 0);
        CHECK(api.release_snapshot(s) == AR_OK);
    }
}
avatarVrm::ExpressionAdapterConfig boneConfig() {
    auto c = config(); c.lookAt->type = vrmRig::LookAtType::Bone;
    c.lookAt->leftEyeJoint = "owner.left"; c.lookAt->rightEyeJoint = "owner.right";
    c.lookAt->horizontalInner.outputScale = 12; c.lookAt->horizontalOuter.outputScale = 30;
    c.lookAt->verticalUp.outputScale = 20; c.lookAt->verticalDown.outputScale = 10;
    c.eyes = {{"owner.left", "rig", "eye.L", {std::sin(0.15),0,0,std::cos(0.15)}},
              {"owner.right", "rig", "eye.R", {0,0,std::sin(0.2),std::cos(0.2)}}};
    return c;
}
ArStatus AR_CALL moveHead(void*, void*, const ArEvaluationContext* c, const ArStateWriter* w) {
    auto head = c->working->joints[1].local;
    head.translation[0] = 1;
    head.rotation[1] = std::sqrt(0.5); head.rotation[3] = std::sqrt(0.5);
    return w->set_joint(w->context, 1, &head);
}
void boneLookAt() {
    auto c = boneConfig();
    Fixture f(c);
    // An earlier pose writer changes the head. Gaze must consume this frame's
    // working pose, and replace animated eye rotation using the bound rest.
    ArEvaluatorDesc pose{AR_HEADER(ArEvaluatorDesc)};
    pose.id = "pose"; pose.provider_id = "test.pose"; pose.provider_version = "1";
    pose.phase = AR_PHASE_BASE_POSE; pose.reads = pose.writes = AR_DOMAIN_POSE; pose.evaluate = moveHead;
    CHECK(api.register_evaluator(f.runtime, &pose, nullptr) == AR_OK);
    f.joints[2].local.rotation[1] = std::sin(0.4); f.joints[2].local.rotation[3] = std::cos(0.4);
    f.joints[2].local.scale[0] = 1.2;
    const char* selected[]{"vrm.face", "pose"};
    auto descriptor = f.adapter.Descriptor();
    CHECK(descriptor.phase == AR_PHASE_EXPRESSIONS && (descriptor.writes & AR_DOMAIN_POSE));
    CHECK(std::string(descriptor.supplies[1].id) == "avatar.vrm.lookAt.bone");
    ArInstanceDesc d{AR_HEADER(ArInstanceDesc)};
    d.generation = 1; d.layout_id = c.layoutId.c_str(); d.layout_version = c.layoutVersion;
    d.evaluators = selected; d.evaluator_count = 2;
    d.bound_capabilities = descriptor.supplies; d.bound_capability_count = descriptor.supply_count;
    d.required_capabilities = descriptor.supplies; d.required_capability_count = descriptor.supply_count;
    d.initial_state = {AR_HEADER(ArStateView)};
    d.initial_state.joints = f.joints; d.initial_state.joint_count = 4;
    d.initial_state.blend_shapes = f.morphs; d.initial_state.blend_shape_count = 6;
    d.initial_state.materials = f.materials; d.initial_state.material_count = 2;
    ArInstance id = 0; CHECK(api.create_instance(f.runtime, &d, nullptr, &id) == AR_OK);
    pose.id = "pose.conflict"; pose.phase = AR_PHASE_EXPRESSIONS;
    CHECK(api.register_evaluator(f.runtime, &pose, nullptr) == AR_OK);
    selected[1] = "pose.conflict";
    ArInstance conflict = 0;
    CHECK(api.create_instance(f.runtime, &d, nullptr, &conflict) == AR_WRITE_CONFLICT && conflict == 0);
    selected[1] = "pose";
    ArGazeInput gaze{"tracker", "actor", "eyes:target", AR_GAZE_POINT, AR_GAZE_RUNTIME_WORLD,
                     AR_OBSERVATION_VALID, nullptr, nullptr, {4,2.1,-1}, 0, 1, 0};
    ArSnapshot retained = 0;
    for (uint64_t frameId = 1; frameId <= 3; ++frameId) {
        gaze.value[2] = frameId == 2 ? 1 : -1; // both yaw signs, then repeat
        gaze.value[1] = frameId == 2 ? 0.1 : 2.1;
        auto input = frame(frameId); input.gazes = &gaze; input.gaze_count = 1;
        ArScalarInput scalar{"tracker", "actor", "face:smile", 0.25, 0, 1, 0};
        input.scalars = &scalar; input.scalar_count = 1;
        ArSnapshot s = 0;
        CHECK(api.evaluate_frame(f.runtime, id, &input, nullptr, &s) == AR_OK);
        auto output = view(s);
        vrmRig::LookAtInput owner;
        owner.head.position = pxr::GfVec3f(3,1,0);
        owner.head.orientation = pxr::GfQuatf(float(std::sqrt(0.5)), pxr::GfVec3f(0,float(std::sqrt(0.5)),0));
        owner.target = pxr::GfVec3f(float(gaze.value[0]),float(gaze.value[1]),float(gaze.value[2]));
        auto result = vrmRig::LookAtEvaluator(*c.lookAt).Evaluate(owner);
        CHECK(result.eyeRotations.size() == 2);
        for (const auto& eye : result.eyeRotations) {
            auto b = std::find_if(c.eyes.begin(), c.eyes.end(), [&](const auto& binding) { return binding.ownerJoint == eye.joint; });
            const auto slot = size_t(b - c.eyes.begin()) + 2;
            const auto& rest = b->restRotation;
            auto expected = (pxr::GfQuatd(eye.rotation) * pxr::GfQuatd(rest[3], pxr::GfVec3d(rest[0],rest[1],rest[2]))).GetNormalized();
            for (int i = 0; i < 3; ++i) {
                CHECK(near(output.joints[slot].local.rotation[i], expected.GetImaginary()[i]));
                CHECK(near(output.joints[slot].local.translation[i], f.joints[slot].local.translation[i]));
                CHECK(near(output.joints[slot].local.scale[i], f.joints[slot].local.scale[i]));
            }
            CHECK(near(output.joints[slot].local.rotation[3], expected.GetReal()));
            if (frameId == 3) for (int i = 0; i < 4; ++i)
                CHECK(near(output.joints[slot].local.rotation[i], view(retained).joints[slot].local.rotation[i]));
        }
        CHECK(near(output.blend_shapes[0].weight, 0.2)); // expressions still run
        CHECK(output.blend_shapes[2].weight == 0); // bone gaze adds no look expression
        if (frameId == 1) retained = s; else CHECK(api.release_snapshot(s) == AR_OK);
    }
    for (uint64_t frameId = 4; frameId <= 7; ++frameId) {
        auto input = frame(frameId);
        if (frameId != 4) {
            input.gazes = &gaze; input.gaze_count = 1;
            gaze.validity = frameId == 5 ? AR_OBSERVATION_STALE :
                            frameId == 6 ? AR_OBSERVATION_UNAVAILABLE : AR_OBSERVATION_VALID;
            // Last frame is a valid point at the current eye origin.
            if (frameId >= 6) {
                gaze.value[0] = frameId == 6 ? 0 : 3;
                gaze.value[1] = frameId == 6 ? 0 : double(1.1f);
                gaze.value[2] = 0;
            }
        }
        Log log; auto sink = log.sink(); ArSnapshot s = 0;
        CHECK(api.evaluate_frame(f.runtime, id, &input, &sink, &s) == AR_OK);
        auto output = view(s);
        for (int i = 0; i < 4; ++i) CHECK(output.joints[2].local.rotation[i] == f.joints[2].local.rotation[i]);
        if (frameId == 5 || frameId == 6) CHECK(log.has("VRM_ADAPTER_GAZE_UNAVAILABLE"));
        if (frameId == 7) CHECK(log.has("VRM_LOOKAT_WARNING"));
        CHECK(api.release_snapshot(s) == AR_OK);
    }
    CHECK(api.reset_instance(f.runtime, id, 2) == AR_OK);
    auto input = frame(1); input.generation = 2;
    ArSnapshot s = 0; CHECK(api.evaluate_frame(f.runtime, id, &input, nullptr, &s) == AR_OK);
    CHECK(view(s).generation == 2 && view(s).joints[2].local.rotation[1] == f.joints[2].local.rotation[1]);
    CHECK(api.release_snapshot(s) == AR_OK);
    CHECK(api.destroy_runtime(f.runtime) == AR_OK); f.runtime = 0;
    CHECK(view(retained).joint_count == 4 && view(retained).layout_version == 3);
    CHECK(api.release_snapshot(retained) == AR_OK);
}
void boneBindings() {
    for (const char* defect : {"unnamed", "duplicateOwner", "missingMap", "extraMap", "duplicateTarget", "skeleton", "head", "norm", "nan", "expression"}) {
        auto c = boneConfig(); const std::string name(defect);
        if (name == "unnamed") c.lookAt->leftEyeJoint = c.lookAt->rightEyeJoint = "";
        if (name == "duplicateOwner") c.lookAt->rightEyeJoint = c.lookAt->leftEyeJoint;
        if (name == "missingMap") c.eyes.pop_back();
        if (name == "extraMap") c.eyes.push_back({"extra", "rig", "extra"});
        if (name == "duplicateTarget") c.eyes[1].joint = c.eyes[0].joint;
        if (name == "skeleton") c.eyes[0].skeleton = "other";
        if (name == "head") c.eyes[0].joint = "head";
        if (name == "norm") c.eyes[0].restRotation[3] = 2;
        if (name == "nan") c.eyes[0].restRotation[0] = std::numeric_limits<double>::quiet_NaN();
        if (name == "expression") c.lookAt->type = vrmRig::LookAtType::Expression;
        bool threw = false;
        try { avatarVrm::ExpressionAdapter adapter(c); } catch (const std::invalid_argument&) { threw = true; }
        CHECK(threw);
    }
    for (const char* defect : {"missing", "parent", "head"}) {
        Fixture f(boneConfig()); const std::string name(defect);
        if (name == "missing") f.joints[2].joint_id = "other";
        if (name == "parent") f.joints[2].parent_index = 0;
        if (name == "head") f.joints[1].joint_id = "other";
        auto id = f.make(); auto input = frame(1); ArSnapshot s = 0; Log log; auto sink = log.sink();
        // Validate output layout even with no gaze this frame.
        CHECK(api.evaluate_frame(f.runtime, id, &input, &sink, &s) == AR_PROVIDER_ERROR && s == 0);
        CHECK(log.has(name == "missing" ? "VRM_ADAPTER_EYE" : name == "parent" ? "VRM_ADAPTER_EYE_PARENT" : "VRM_ADAPTER_HEAD"));
    }
    {
        auto c = boneConfig(); c.lookAt->rightEyeJoint.clear(); c.eyes.pop_back();
        Fixture f(c); auto id = f.make(); auto other = f.make();
        ArGazeInput gaze{"tracker", "actor", "eyes:target", AR_GAZE_POINT, AR_GAZE_RUNTIME_WORLD,
                         AR_OBSERVATION_VALID, nullptr, nullptr, {3,1.1,1}, 0, 1, 0};
        auto input = frame(1); input.gazes = &gaze; input.gaze_count = 1;
        Log log; auto sink = log.sink(); ArSnapshot s = 0;
        CHECK(api.evaluate_frame(f.runtime, id, &input, &sink, &s) == AR_OK);
        CHECK(log.has("VRM_LOOKAT_WARNING") && view(s).joints[2].local.rotation[1] != 0);
        CHECK(view(s).joints[3].local.rotation[1] == 0); CHECK(api.release_snapshot(s) == AR_OK);
        input.gazes = nullptr; input.gaze_count = 0;
        CHECK(api.evaluate_frame(f.runtime, other, &input, nullptr, &s) == AR_OK);
        CHECK(view(s).joints[2].local.rotation[1] == 0); CHECK(api.release_snapshot(s) == AR_OK);
    }
    {
        Fixture f(boneConfig()); f.materials[0].value[0] = 1e100;
        auto id = f.make();
        ArGazeInput gaze{"tracker", "actor", "eyes:target", AR_GAZE_POINT, AR_GAZE_RUNTIME_WORLD,
                         AR_OBSERVATION_VALID, nullptr, nullptr, {3,1.1,1}, 0, 1, 0};
        ArScalarInput scalar{"tracker", "actor", "face:smile", 1, 0, 1, 0};
        auto input = frame(1); input.gazes = &gaze; input.gaze_count = 1;
        input.scalars = &scalar; input.scalar_count = 1;
        Log log; auto sink = log.sink(); ArSnapshot s = 0;
        CHECK(api.evaluate_frame(f.runtime, id, &input, &sink, &s) == AR_PROVIDER_ERROR && s == 0);
        CHECK(log.has("VRM_ADAPTER_RANGE")); // eye writes preceded failed material write
        input.gazes = nullptr; input.gaze_count = 0; input.scalars = nullptr; input.scalar_count = 0;
        CHECK(api.evaluate_frame(f.runtime, id, &input, nullptr, &s) == AR_OK);
        CHECK(view(s).joints[2].local.rotation[1] == 0 && near(view(s).blend_shapes[0].weight, 0.12));
        CHECK(api.release_snapshot(s) == AR_OK);
    }
}
void gazeBindings() {
    ArGazeInput gaze{"tracker", "actor", "eyes:target", AR_GAZE_POINT, AR_GAZE_RUNTIME_WORLD,
                     AR_OBSERVATION_VALID, nullptr, nullptr, {4,1.1,-1}, 0, 1, 0};
    auto input = frame(1); input.gazes = &gaze; input.gaze_count = 1;
    {
        Fixture f;
        // Rotate the root ninety degrees around +Y. Its head moves from
        // world (2,1,0) to (2,1,0), but forward becomes +X.
        f.joints[0].local.rotation[1] = std::sqrt(0.5);
        f.joints[0].local.rotation[3] = std::sqrt(0.5);
        auto id = f.make(); ArSnapshot s = 0;
        CHECK(api.evaluate_frame(f.runtime, id, &input, nullptr, &s) == AR_OK);
        auto c = config(); vrmRig::LookAtInput owner;
        owner.head.position = pxr::GfVec3f(2,1,0);
        owner.head.orientation = pxr::GfQuatf(float(std::sqrt(0.5)), pxr::GfVec3f(0,float(std::sqrt(0.5)),0));
        owner.target = pxr::GfVec3f(4,1.1f,-1);
        auto result = vrmRig::LookAtEvaluator(*c.lookAt).Evaluate(owner);
        auto expected = vrmRig::ExpressionResolver(c.expressions).Resolve(result.expressions);
        for (const auto& target : expected.morphTargets) {
            auto b = std::find_if(c.morphs.begin(), c.morphs.end(), [&](const auto& binding) { return binding.ownerTarget == target.target; });
            CHECK(near(view(s).blend_shapes[size_t(b - c.morphs.begin())].weight, target.weight));
        }
        CHECK(api.release_snapshot(s) == AR_OK);
    }
    for (const char* defect : {"scale", "missing", "referenceScale"}) {
        Fixture f;
        if (std::string(defect) == "scale") f.joints[0].local.scale[0] = 2;
        if (std::string(defect) == "missing") f.joints[1].joint_id = "differentHead";
        if (std::string(defect) == "referenceScale") f.joints[2].local.scale[0] = 2;
        auto id = f.make(); ArSnapshot s = 0; Log log; auto sink = log.sink();
        auto observation = gaze;
        if (std::string(defect) == "referenceScale") {
            observation.space = AR_GAZE_JOINT_LOCAL; observation.skeleton_id = "rig"; observation.joint_id = "eye.L";
        }
        input.gazes = &observation;
        CHECK(api.evaluate_frame(f.runtime, id, &input, &sink, &s) == AR_PROVIDER_ERROR && s == 0);
        const char* code = std::string(defect) == "scale" ? "VRM_ADAPTER_HEAD_SCALE" :
                           std::string(defect) == "missing" ? "VRM_ADAPTER_HEAD" : "VRM_ADAPTER_GAZE_SCALE";
        CHECK(log.has(code));
        // Failed evaluation must not commit frame identity. A world point
        // avoids the unsupported reference scale and can retry the same frame.
        if (std::string(defect) == "referenceScale") {
            input.gazes = &gaze;
            CHECK(api.evaluate_frame(f.runtime, id, &input, nullptr, &s) == AR_OK);
            CHECK(api.release_snapshot(s) == AR_OK);
        }
    }
}
void mappedGaze() {
    for (bool bone : {false, true}) for (bool offset : {false, true}) {
        auto c = bone ? boneConfig() : config();
        if (!offset) c.lookAt->offsetFromHeadBone.reset();
        Fixture f(c);
        // Rotate the reference eye around Z; the pose provider rotates/moves
        // its head around Y. Reference observations must see both rotations.
        f.joints[2].local.rotation[2] = std::sqrt(0.5);
        f.joints[2].local.rotation[3] = std::sqrt(0.5);
        ArEvaluatorDesc pose{AR_HEADER(ArEvaluatorDesc)};
        pose.id = "pose"; pose.provider_id = "test.pose"; pose.provider_version = "1";
        pose.phase = AR_PHASE_BASE_POSE; pose.reads = pose.writes = AR_DOMAIN_POSE; pose.evaluate = moveHead;
        CHECK(api.register_evaluator(f.runtime, &pose, nullptr) == AR_OK);
        auto id = f.make(c.layoutId.c_str(), c.layoutVersion, "pose");
        uint64_t frameId = 0;
        for (uint32_t kind : {AR_GAZE_POINT, AR_GAZE_DIRECTION})
            for (const char* reference : {"world", "root", "head", "eye.L"}) {
                const bool direction = kind == AR_GAZE_DIRECTION;
                ArGazeInput observation{"tracker", "actor", "eyes:target", kind, AR_GAZE_RUNTIME_WORLD,
                    AR_OBSERVATION_VALID, nullptr, nullptr, {0.6, direction ? 0.0 : 0.4, direction ? 0.8 : 2.0}, 0, 1, 0};
                const std::string name(reference);
                if (name != "world") {
                    observation.space = AR_GAZE_JOINT_LOCAL;
                    observation.skeleton_id = "rig"; observation.joint_id = reference;
                }
                // Independently stated world coordinates for the constructed
                // transforms above, rather than repeating adapter traversal.
                pxr::GfVec3f target(0.6f, direction ? 0.0f : 0.4f, direction ? 0.8f : 2.0f);
                if (name == "root" && !direction) target = pxr::GfVec3f(2.6f,0.4f,2);
                if (name == "head") target = direction ? pxr::GfVec3f(0.8f,0,-0.6f) : pxr::GfVec3f(5,1.4f,-0.6f);
                if (name == "eye.L") target = direction ? pxr::GfVec3f(0.8f,0.6f,0) : pxr::GfVec3f(5,1.7f,0.37f);
                vrmRig::LookAtInput owner;
                owner.head.position = direction ? pxr::GfVec3f(0) : pxr::GfVec3f(3,1,0);
                owner.head.orientation = pxr::GfQuatf(float(std::sqrt(0.5)), pxr::GfVec3f(0,float(std::sqrt(0.5)),0));
                owner.target = target;
                const auto evaluator = vrmRig::LookAtEvaluator(*c.lookAt);
                const auto expected = direction ? evaluator.EvaluateDirection(target, owner.head, 0) : evaluator.Evaluate(owner);
                CHECK(expected.hasGaze);
                auto input = frame(++frameId); input.gazes = &observation; input.gaze_count = 1;
                ArSnapshot s = 0;
                CHECK(api.evaluate_frame(f.runtime, id, &input, nullptr, &s) == AR_OK);
                auto output = view(s);
                if (bone) {
                    for (size_t i = 0; i < expected.eyeRotations.size(); ++i) {
                        const auto& rest = c.eyes[i].restRotation;
                        auto q = (pxr::GfQuatd(expected.eyeRotations[i].rotation) *
                            pxr::GfQuatd(rest[3], pxr::GfVec3d(rest[0],rest[1],rest[2]))).GetNormalized();
                        for (int j = 0; j < 3; ++j) CHECK(near(output.joints[i+2].local.rotation[j], q.GetImaginary()[j]));
                        CHECK(near(output.joints[i+2].local.rotation[3], q.GetReal()));
                    }
                } else {
                    auto resolved = vrmRig::ExpressionResolver(c.expressions).Resolve(expected.expressions);
                    for (const auto& morph : resolved.morphTargets) {
                        auto binding = std::find_if(c.morphs.begin(), c.morphs.end(), [&](const auto& b) { return b.ownerTarget == morph.target; });
                        CHECK(near(output.blend_shapes[size_t(binding-c.morphs.begin())].weight, morph.weight));
                    }
                }
                CHECK(api.release_snapshot(s) == AR_OK);
            }
    }
    // A direction has no positional origin: even placement outside owner float
    // range must preserve its result. A point at that placement fails visibly.
    Fixture f; f.joints[0].local.translation[0] = 1e100;
    auto id = f.make();
    ArGazeInput observation{"tracker", "actor", "eyes:target", AR_GAZE_POINT, AR_GAZE_JOINT_LOCAL,
        AR_OBSERVATION_VALID, "rig", "head", {0.6,0,0.8}, 0, 1, 0};
    auto input = frame(1); input.gazes = &observation; input.gaze_count = 1;
    Log log; auto sink = log.sink(); ArSnapshot s = 0;
    CHECK(api.evaluate_frame(f.runtime, id, &input, &sink, &s) == AR_PROVIDER_ERROR && s == 0);
    CHECK(log.has("VRM_ADAPTER_RANGE"));
    observation.kind = AR_GAZE_DIRECTION;
    CHECK(api.evaluate_frame(f.runtime, id, &input, nullptr, &s) == AR_OK);
    CHECK(near(view(s).blend_shapes[2].weight, std::atan2(0.6,0.8) * 180 / (std::acos(-1.0) * 90)));
    CHECK(near(view(s).blend_shapes[4].weight, 0)); // eye offset does not induce pitch
    CHECK(api.release_snapshot(s) == AR_OK);
    // Values at the runtime's allowed double norm boundary still marshal to
    // valid owner float vectors without dropping the gaze after rounding.
    const double unitSlack = std::sqrt(1 + 9.9e-7);
    observation.value[0] = 0.6 * unitSlack; observation.value[2] = 0.8 * unitSlack;
    input.frame_id = 2;
    CHECK(api.evaluate_frame(f.runtime, id, &input, nullptr, &s) == AR_OK);
    CHECK(near(view(s).blend_shapes[2].weight, std::atan2(0.6,0.8) * 180 / (std::acos(-1.0) * 90)));
    CHECK(api.release_snapshot(s) == AR_OK);
    for (uint32_t validity : {AR_OBSERVATION_STALE, AR_OBSERVATION_UNAVAILABLE}) {
        input.frame_id++;
        observation.validity = validity;
        if (validity == AR_OBSERVATION_UNAVAILABLE)
            for (double& value : observation.value) value = 0;
        Log skipped; auto skippedSink = skipped.sink();
        CHECK(api.evaluate_frame(f.runtime, id, &input, &skippedSink, &s) == AR_OK);
        CHECK(skipped.has("VRM_ADAPTER_GAZE_UNAVAILABLE"));
        CHECK(view(s).blend_shapes[2].weight == 0 && near(view(s).blend_shapes[0].weight, 0.12));
        CHECK(api.release_snapshot(s) == AR_OK);
    }
    Fixture scaled; scaled.joints[2].local.scale[0] = 2;
    auto scaledId = scaled.make();
    observation.validity = AR_OBSERVATION_VALID;
    observation.joint_id = "eye.L";
    observation.value[0] = 0.6; observation.value[2] = 0.8;
    input.frame_id = 1;
    CHECK(api.evaluate_frame(scaled.runtime, scaledId, &input, &sink, &s) == AR_PROVIDER_ERROR && s == 0);
    CHECK(log.has("VRM_ADAPTER_GAZE_SCALE"));
    observation.space = AR_GAZE_RUNTIME_WORLD;
    observation.skeleton_id = observation.joint_id = nullptr;
    CHECK(api.evaluate_frame(scaled.runtime, scaledId, &input, nullptr, &s) == AR_OK);
    CHECK(api.release_snapshot(s) == AR_OK);
}
} // namespace
int main() {
    CHECK(arGetApi(AR_ABI_VERSION, sizeof(api), &api) == AR_OK);
    evaluation(); invalidBindings(); gazeBindings(); boneLookAt(); boneBindings(); mappedGaze();
    std::cout << "VRM owner expression/LookAt adapter contracts passed\n";
}
