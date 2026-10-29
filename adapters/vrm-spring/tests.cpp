#include "avatarVrm/SpringBoneAdapter.h"
#include "pxr/base/gf/quatd.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

#define CHECK(x) do { if (!(x)) { std::cerr << "Line " << __LINE__ << ": " << #x << '\n'; std::exit(1); } } while (0)
namespace {
ArRuntimeApi api{};
constexpr double tick = 1.0 / 60;
pxr::GfQuatf turn(float angle, pxr::GfVec3f axis) {
    return {std::cos(angle / 2), axis * std::sin(angle / 2)};
}
avatarVrm::SpringBoneAdapterConfig config(bool roots, bool simulatedRoot = false, double duration = tick) {
    avatarVrm::SpringBoneAdapterConfig c;
    c.evaluatorId = "vrm.spring"; c.layoutId = "spring.layout"; c.layoutVersion = 2;
    c.skeletonId = "rig"; c.jointIds = {"root", "hair", "tip", "side"};
    c.fixedStepSeconds = duration; c.after = {"animation"};
    c.placement = {pxr::GfVec3f(3,4,5), turn(0.5f, {0,1,0})};
    c.rig.parents = {-1,0,1,1};
    c.rig.restTransforms = {{{0,0.8f,0}, pxr::GfQuatf(1)}, {{0,0.2f,0}, pxr::GfQuatf(1)},
                            {{0.1f,0.3f,0}, pxr::GfQuatf(1)}, {{-0.1f,0.15f,0}, pxr::GfQuatf(1)}};
    vrmRig::SpringBoneSpring s; s.center = 0;
    if (roots) s.rootJoints = {1}; else s.joints = {1,2};
    s.parameters.resize(roots ? 1 : 2);
    for (auto& p : s.parameters) { p.stiffness = 0.7f; p.gravityPower = 0.8f; p.hitRadius = 0.02f; }
    c.rig.colliders.push_back({0, vrmRig::SpringBoneColliderShape::Sphere, {0.1f,0.4f,0.05f}, {}, 0.05f});
    c.rig.colliderGroups = {{0}}; s.colliderGroups = {0};
    if (simulatedRoot) {
        s.rootJoints = {0}; s.center = -1; s.colliderGroups.clear();
        c.rig.colliderGroups.clear(); c.rig.colliders.clear();
    }
    c.rig.springs.push_back(s);
    return c;
}
vrmRig::SpringBonePose animated(const avatarVrm::SpringBoneAdapterConfig& c, double time) {
    vrmRig::SpringBonePose p{c.rig.restTransforms, c.placement};
    p.placement.translation[0] += float(time * 0.2);
    p.localTransforms[0].rotation = turn(float(time * 0.3), {0,0,1});
    p.localTransforms[1].rotation = turn(float(0.1 + time * 0.5), {1,0,0});
    return p;
}
ArTransform local(const vrmRig::SpringBonePose& p, size_t i, bool root) {
    auto t = p.localTransforms[i];
    if (root) {
        t.translation = p.placement.translation + p.placement.rotation.Transform(t.translation);
        t.rotation = (p.placement.rotation * t.rotation).GetNormalized();
    }
    const auto q = pxr::GfQuatd(t.rotation).GetNormalized();
    ArTransform out{{t.translation[0],t.translation[1],t.translation[2]},
                    {q.GetImaginary()[0],q.GetImaginary()[1],q.GetImaginary()[2],q.GetReal()}, {1,1,1}};
    return out;
}
struct Log {
    std::string code, origin, detail, evaluator;
    static void AR_CALL emit(void* user, const ArDiagnostic* d) {
        auto& l = *static_cast<Log*>(user);
        if (std::string(d->code).find("VRM_SPRING_ADAPTER_") != 0) return;
        l.code = d->code; l.origin = d->origin; l.detail = d->message;
        l.evaluator = d->evaluator_id;
    }
    ArDiagnosticSink sink() { return {this, emit}; }
};
struct Fixture {
    avatarVrm::SpringBoneAdapterConfig c;
    avatarVrm::SpringBoneAdapter adapter;
    ArRuntime runtime = 0;
    int failure = 0, badPose = 0;
    ArJoint joints[5]{{"unrelated", "other", -1, {{9,8,7}, {0,0,0,1}, {1,1,1}}}};
    ArBlendShape morph{"mesh", "smile", 0.123456789};
    ArMaterialInput material{"mat", "surface:color", AR_VALUE_VEC3, 0, {0.1,0.2,0.3,0}};
    ArVisibility visibility{"mesh", 1};
    explicit Fixture(bool roots, bool simulatedRoot = false, double duration = tick)
        : c(config(roots, simulatedRoot, duration)), adapter(c) {
        CHECK(api.create_runtime(&runtime) == AR_OK);
        auto d = adapter.Descriptor();
        CHECK(d.phase == AR_PHASE_SECONDARY && d.flags == AR_EVALUATOR_STATEFUL);
        CHECK(api.register_evaluator(runtime, &d, nullptr) == AR_OK);
        ArEvaluatorDesc a{AR_HEADER(ArEvaluatorDesc)};
        a.id = "animation"; a.provider_id = "test.animation"; a.provider_version = "1";
        a.phase = AR_PHASE_EXPRESSIONS; a.writes = AR_DOMAIN_POSE;
        a.user_data = this; a.evaluate = animate;
        CHECK(api.register_evaluator(runtime, &a, nullptr) == AR_OK);
        a.id = "downstream"; a.phase = AR_PHASE_FINAL_POSE; a.evaluate = later;
        a.reads = a.writes = AR_DOMAIN_ALL;
        CHECK(api.register_evaluator(runtime, &a, nullptr) == AR_OK);
        const auto p = animated(c, 0);
        for (size_t i = 0; i < 4; ++i)
            joints[i+1] = {"rig", c.jointIds[i].c_str(), c.rig.parents[i] < 0 ? -1 : c.rig.parents[i]+1,
                           local(p, i, c.rig.parents[i] < 0)};
    }
    ~Fixture() { if (runtime) CHECK(api.destroy_runtime(runtime) == AR_OK); }
    static ArStatus AR_CALL animate(void* user, void*, const ArEvaluationContext* ctx, const ArStateWriter* w) {
        auto& f = *static_cast<Fixture*>(user);
        auto p = animated(f.c, ctx->input->evaluation_seconds);
        if (f.badPose == 1) p.localTransforms[1].translation[1] += 0.01f;
        for (size_t i = 0; i < 4; ++i) {
            auto t = local(p, i, f.c.rig.parents[i] < 0);
            if (f.badPose == 2 && i == 1) t.scale[0] = 2;
            const auto status = w->set_joint(w->context, uint32_t(i+1), &t);
            if (status != AR_OK) return status;
        }
        return AR_OK;
    }
    static ArStatus AR_CALL later(void* user, void*, const ArEvaluationContext* ctx, const ArStateWriter* w) {
        auto& f = *static_cast<Fixture*>(user);
        if (f.failure == 1) return AR_PROVIDER_ERROR;
        if (f.failure == 2) {
            auto t = ctx->working->joints[2].local; t.rotation[3] = 10;
            w->set_joint(w->context, 2, &t); // ignored writer failure must abort spring history
        }
        return AR_OK;
    }
    ArStatus make(ArInstance* out, const char* layout = "spring.layout", const char* extra = nullptr) {
        const char* selected[]{"downstream", "vrm.spring", "animation", extra};
        const auto d = adapter.Descriptor();
        ArInstanceDesc desc{AR_HEADER(ArInstanceDesc)};
        desc.generation = 1; desc.layout_id = layout; desc.layout_version = 2;
        desc.evaluators = selected; desc.evaluator_count = extra ? 4 : 3;
        desc.bound_capabilities = d.supplies; desc.bound_capability_count = d.supply_count;
        desc.required_capabilities = d.supplies; desc.required_capability_count = d.supply_count;
        desc.initial_state = {AR_HEADER(ArStateView)};
        desc.initial_state.joints = joints; desc.initial_state.joint_count = 5;
        desc.initial_state.blend_shapes = &morph; desc.initial_state.blend_shape_count = 1;
        desc.initial_state.materials = &material; desc.initial_state.material_count = 1;
        desc.initial_state.visibility = &visibility; desc.initial_state.visibility_count = 1;
        return api.create_instance(runtime, &desc, nullptr, out);
    }
    ArStatus evaluate(ArInstance instance, uint64_t id, double time, ArSnapshot* out,
                      uint64_t generation = 1, const ArDiagnosticSink* sink = nullptr) {
        ArInputFrame input{AR_HEADER(ArInputFrame)};
        input.frame_id = id; input.generation = generation; input.evaluation_seconds = time;
        input.input_revision = id + 100;
        return api.evaluate_frame(runtime, instance, &input, sink, out);
    }
};
ArStateView view(ArSnapshot snapshot) {
    ArStateView v{AR_HEADER(ArStateView)}; CHECK(api.get_snapshot(snapshot, &v) == AR_OK); return v;
}
void equal(const ArStateView& a, const ArStateView& b) {
    CHECK(a.joint_count == b.joint_count);
    for (uint32_t i = 0; i < a.joint_count; ++i) {
        for (int k = 0; k < 3; ++k) {
            CHECK(a.joints[i].local.translation[k] == b.joints[i].local.translation[k]);
            CHECK(a.joints[i].local.scale[k] == b.joints[i].local.scale[k]);
        }
        for (int k = 0; k < 4; ++k) CHECK(a.joints[i].local.rotation[k] == b.joints[i].local.rotation[k]);
    }
}
void parity(bool roots, bool simulatedRoot = false) {
    Fixture f(roots, simulatedRoot); ArInstance a = 0, control = 0, isolated = 0;
    CHECK(f.make(&a) == AR_OK && f.make(&control) == AR_OK && f.make(&isolated) == AR_OK);
    vrmRig::SpringBoneSolver direct; CHECK(direct.Configure(f.c.rig));
    std::vector<ArSnapshot> history;
    for (uint64_t id = 1; id <= 25; ++id) {
        const double time = double(id-1) * tick;
        ArSnapshot result = 0, clean = 0;
        if (id == 1 || id == 4 || id == 8) {
            f.failure = id == 8 ? 2 : 1;
            CHECK(f.evaluate(a, id, time, &result) != AR_OK && result == 0);
            f.failure = 0;
        }
        CHECK(f.evaluate(a, id, time, &result) == AR_OK);
        CHECK(f.evaluate(control, id, time, &clean) == AR_OK);
        auto actual = view(result); equal(actual, view(clean));
        const auto pose = animated(f.c, time);
        vrmRig::SpringBoneResult expected;
        CHECK(id == 1 ? direct.Reset(pose, &expected) : direct.Step(float(tick), pose, &expected));
        bool moved = false;
        for (size_t i = 0; i < 4; ++i) {
            auto q = pxr::GfQuatd(expected.localRotations[i]);
            if (f.c.rig.parents[i] < 0) q = pxr::GfQuatd(pose.placement.rotation) * q;
            q.Normalize();
            const auto& t = actual.joints[i+1].local;
            const pxr::GfQuatd got(t.rotation[3], {t.rotation[0],t.rotation[1],t.rotation[2]});
            CHECK(std::abs(pxr::GfDot(q, got)) > 1 - 1e-8);
            const auto input = local(pose, i, f.c.rig.parents[i] < 0);
            for (int k = 0; k < 3; ++k) CHECK(t.translation[k] == input.translation[k] && t.scale[k] == 1);
            if (i == 1) moved = std::abs(t.rotation[1] - input.rotation[1]) > 1e-6 ||
                                std::abs(t.rotation[2] - input.rotation[2]) > 1e-6;
            if (!roots && i != 1)
                for (int k = 0; k < 4; ++k) CHECK(t.rotation[k] == input.rotation[k]);
        }
        if (id > 5) CHECK(moved);
        CHECK(actual.joints[0].local.translation[0] == 9);
        CHECK(actual.blend_shapes[0].weight == f.morph.weight && actual.materials[0].overridden == 0);
        CHECK(actual.materials[0].value[1] == 0.2 && actual.visibility[0].visible == 1);
        CHECK(actual.input_revision == id + 100 && actual.layout_version == 2);
        history.push_back(result);
        CHECK(api.release_snapshot(clean) == AR_OK);
    }
    ArSnapshot isolatedResult = 0;
    CHECK(f.evaluate(isolated, 1, 0, &isolatedResult) == AR_OK);
    equal(view(history.front()), view(isolatedResult));
    CHECK(api.release_snapshot(isolatedResult) == AR_OK);
    // Reset replays every tick identically, while held snapshots remain independent.
    CHECK(api.reset_instance(f.runtime, a, 2) == AR_OK);
    for (uint64_t id = 1; id <= 25; ++id) {
        ArSnapshot replay = 0;
        CHECK(f.evaluate(a, id, double(id-1)*tick, &replay, 2) == AR_OK);
        equal(view(history[size_t(id-1)]), view(replay));
        CHECK(api.release_snapshot(replay) == AR_OK);
    }
    // A seek restarts at that instant's animated pose, not the bound rest pose.
    CHECK(api.reset_instance(f.runtime, a, 3) == AR_OK);
    ArInstance fresh = 0; CHECK(f.make(&fresh) == AR_OK);
    ArSnapshot seek = 0, seeded = 0;
    CHECK(f.evaluate(a, 1, 5, &seek, 3) == AR_OK);
    CHECK(f.evaluate(fresh, 1, 5, &seeded) == AR_OK);
    equal(view(seek), view(seeded));
    CHECK(api.release_snapshot(seek) == AR_OK && api.release_snapshot(seeded) == AR_OK);
    CHECK(api.destroy_runtime(f.runtime) == AR_OK); f.runtime = 0;
    CHECK(view(history.front()).generation == 1 && view(history.front()).joints[2].joint_id == std::string("hair"));
    for (auto snapshot : history) CHECK(api.release_snapshot(snapshot) == AR_OK);
}
void refusals() {
    Fixture f(false); ArInstance a = 0; CHECK(f.make(&a) == AR_OK);
    Log log; auto sink = log.sink(); ArSnapshot s = 0;
    CHECK(f.evaluate(a, 1, 0, &s) == AR_OK); CHECK(api.release_snapshot(s) == AR_OK);
    for (double time : {0.0, tick*2, 1.0}) {
        s = 0; CHECK(f.evaluate(a, 2, time, &s, 1, &sink) != AR_OK && !s);
        CHECK(log.code == "VRM_SPRING_ADAPTER_TICK");
    }
    f.badPose = 1; s = 0;
    CHECK(f.evaluate(a, 2, tick, &s, 1, &sink) != AR_OK && !s);
    CHECK(log.code == "VRM_SPRING_ADAPTER_OWNER" && log.origin == "usd-vrm-plugins.vrmRig");
    CHECK(log.detail.find("rest translations") != std::string::npos &&
          log.detail.find(std::string("vrmRig ") + f.adapter.Descriptor().provider_version + ": ") == 0 &&
          log.evaluator == "vrm.spring");
    f.badPose = 2;
    CHECK(f.evaluate(a, 2, tick, &s, 1, &sink) != AR_OK && !s);
    CHECK(log.code == "VRM_SPRING_ADAPTER_SCALE");
    f.badPose = 0;
    CHECK(f.evaluate(a, 2, tick, &s) == AR_OK); CHECK(api.release_snapshot(s) == AR_OK);
    ArInstance wrong = 0; CHECK(f.make(&wrong, "wrong.layout") == AR_OK);
    CHECK(f.evaluate(wrong, 1, 0, &s, 1, &sink) != AR_OK && !s);
    CHECK(log.code == "VRM_SPRING_ADAPTER_LAYOUT");
    f.joints[2].joint_id = "missing";
    CHECK(f.make(&wrong) == AR_OK);
    CHECK(f.evaluate(wrong, 1, 0, &s, 1, &sink) != AR_OK && !s);
    CHECK(log.code == "VRM_SPRING_ADAPTER_JOINT");
    f.joints[2].joint_id = "hair"; f.joints[3].parent_index = 1;
    CHECK(f.make(&wrong) == AR_OK);
    CHECK(f.evaluate(wrong, 1, 0, &s, 1, &sink) != AR_OK && !s);
    CHECK(log.code == "VRM_SPRING_ADAPTER_PARENT");
    // Same-phase conflicting pose writers still require explicit dependency.
    auto other = f.adapter.Descriptor(); other.id = "other.spring";
    CHECK(api.register_evaluator(f.runtime, &other, nullptr) == AR_OK);
    CHECK(f.make(&wrong, "spring.layout", "other.spring") == AR_WRITE_CONFLICT);
    auto c = config(false); c.fixedStepSeconds = 0;
    try { avatarVrm::SpringBoneAdapter invalid(c); CHECK(false); } catch (const std::invalid_argument&) {}
    c = config(false); c.jointIds[1] = c.jointIds[0];
    try { avatarVrm::SpringBoneAdapter invalid(c); CHECK(false); } catch (const std::invalid_argument&) {}
    c = config(false); c.rig.springs[0].parameters[0].dragForce = 2;
    try { avatarVrm::SpringBoneAdapter invalid(c); CHECK(false); } catch (const std::invalid_argument& e) {
        CHECK(std::string(e.what()).find("VRM_SPRING_ADAPTER_OWNER") != std::string::npos);
    }
    Fixture tiny(false, false, 1e-12); ArInstance instance = 0;
    CHECK(tiny.make(&instance) == AR_OK);
    CHECK(tiny.evaluate(instance, 1, 0, &s) == AR_OK); CHECK(api.release_snapshot(s) == AR_OK);
    CHECK(tiny.evaluate(instance, 2, 0, &s) != AR_OK && !s);
    CHECK(tiny.evaluate(instance, 2, 2e-12, &s) != AR_OK && !s);
    CHECK(tiny.evaluate(instance, 2, 1e-12, &s) == AR_OK); CHECK(api.release_snapshot(s) == AR_OK);
}
} // namespace
int main() {
    CHECK(arGetApi(AR_ABI_VERSION, sizeof(api), &api) == AR_OK);
    parity(false); parity(true); parity(true, true); refusals();
    std::cout << "VRM spring owner parity, transaction, reset, isolation and mapping contracts passed\n";
}
