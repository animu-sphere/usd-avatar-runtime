#include "avatarVrm/ExpressionAdapter.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
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
    ArJoint joints[2]{{"rig", "root", -1, {{2,0,0}, {0,0,0,1}, {1,1,1}}},
                     {"rig", "head", 0, {{0,1,0}, {0,0,0,1}, {1,1,1}}}};
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
    ArInstance make(const char* layout = "vrm.test.layout", uint64_t version = 3) {
        const char* selected[] = {"vrm.face"};
        auto evaluator = adapter.Descriptor();
        ArInstanceDesc d{AR_HEADER(ArInstanceDesc)}; d.generation = 1;
        d.layout_id = layout; d.layout_version = version; d.evaluators = selected; d.evaluator_count = 1;
        d.bound_capabilities = evaluator.supplies; d.bound_capability_count = evaluator.supply_count;
        d.required_capabilities = evaluator.supplies; d.required_capability_count = evaluator.supply_count;
        d.initial_state = {AR_HEADER(ArStateView)};
        d.initial_state.joints = joints; d.initial_state.joint_count = 2;
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
    gaze.value[0] = 0; gaze.value[1] = 0; gaze.value[2] = 1; invalid.gazes = &gaze; invalid.gaze_count = 1;
    CHECK(api.evaluate_frame(f.runtime, instance, &invalid, &sink, &baseline) == AR_PROVIDER_ERROR && baseline == 0);
    CHECK(log.has("VRM_ADAPTER_GAZE_SPACE"));
    gaze.kind = AR_GAZE_POINT;
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
    for (const char* defect : {"scale", "missing", "space"}) {
        Fixture f;
        if (std::string(defect) == "scale") f.joints[0].local.scale[0] = 2;
        if (std::string(defect) == "missing") f.joints[1].joint_id = "differentHead";
        auto id = f.make(); ArSnapshot s = 0; Log log; auto sink = log.sink();
        auto observation = gaze;
        if (std::string(defect) == "space") {
            observation.space = AR_GAZE_JOINT_LOCAL; observation.skeleton_id = "rig"; observation.joint_id = "head";
        }
        input.gazes = &observation;
        CHECK(api.evaluate_frame(f.runtime, id, &input, &sink, &s) == AR_PROVIDER_ERROR && s == 0);
        const char* code = std::string(defect) == "scale" ? "VRM_ADAPTER_HEAD_SCALE" :
                           std::string(defect) == "missing" ? "VRM_ADAPTER_HEAD" : "VRM_ADAPTER_GAZE_SPACE";
        CHECK(log.has(code));
    }
}
} // namespace
int main() {
    CHECK(arGetApi(AR_ABI_VERSION, sizeof(api), &api) == AR_OK);
    evaluation(); invalidBindings(); gazeBindings();
    std::cout << "VRM owner expression/LookAt adapter contracts passed\n";
}
