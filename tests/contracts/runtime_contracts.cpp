#include "avatarRuntime/api.h"
#include <cstddef>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

#define CHECK(x) do { if (!(x)) { std::cerr << "Line " << __LINE__ << ": " << #x << '\n'; std::exit(1); } } while (0)
ArRuntimeApi api{};
struct Log {
    std::vector<std::string> codes, origins, evaluators, subjects;
    std::vector<uint64_t> frames;
    std::vector<ArStatus> statuses;
    static void AR_CALL emit(void* p, const ArDiagnostic* d) {
        auto& log = *static_cast<Log*>(p);
        log.codes.emplace_back(d->code); log.origins.emplace_back(d->origin);
        log.evaluators.emplace_back(d->evaluator_id); log.subjects.emplace_back(d->subject);
        log.frames.push_back(d->frame_id); log.statuses.push_back(d->status);
    }
    ArDiagnosticSink sink() { return {this, emit}; }
};
struct Fixture {
    ArRuntime runtime = 0;
    ArJoint joints[2]{{"rig", "root", -1, {{0,0,0}, {0,0,0,1}, {1,1,1}}},
        {"rig", "auxiliary", 0, {{0,1,0}, {0,0,0,1}, {2,3,4}}}};
    ArBlendShape shape{"mesh", "face", 0};
    ArMaterialInput material{"material", "test:color", AR_VALUE_VEC4, 0, {1,1,1,1}};
    ArVisibility visibility{"mesh", 1};
    Fixture() { CHECK(api.create_runtime(&runtime) == AR_OK); }
    ~Fixture() { if (runtime) CHECK(api.destroy_runtime(runtime) == AR_OK); }
    ArInstanceDesc desc(const std::vector<const char*>& selected) {
        ArInstanceDesc d{AR_HEADER(ArInstanceDesc)};
        d.generation = 1; d.evaluators = selected.data(); d.evaluator_count = uint32_t(selected.size());
        d.layout_id = "test.layout"; d.layout_version = 1;
        d.initial_state = {AR_HEADER(ArStateView)};
        d.initial_state.joints = joints; d.initial_state.joint_count = 2;
        d.initial_state.blend_shapes = &shape; d.initial_state.blend_shape_count = 1;
        d.initial_state.materials = &material; d.initial_state.material_count = 1;
        d.initial_state.visibility = &visibility; d.initial_state.visibility_count = 1;
        return d;
    }
    ArInstance make(const std::vector<const char*>& selected) {
        auto d = desc(selected); ArInstance id = 0;
        CHECK(api.create_instance(runtime, &d, nullptr, &id) == AR_OK); return id;
    }
};
ArInputFrame frame(uint64_t id, double seconds = 0, uint64_t generation = 1) {
    ArInputFrame f{AR_HEADER(ArInputFrame)}; f.frame_id = id; f.generation = generation;
    f.evaluation_seconds = seconds; return f;
}
ArStateView view(ArSnapshot s) {
    ArStateView v{AR_HEADER(ArStateView)}; CHECK(api.get_snapshot(s, &v) == AR_OK); return v;
}
ArStatus AR_CALL noop(void*, void*, const ArEvaluationContext*, const ArStateWriter*) { return AR_OK; }
ArEvaluatorDesc evaluator(const char* id, ArPhase phase = AR_PHASE_BASE_POSE, ArDomain writes = 0) {
    ArEvaluatorDesc d{AR_HEADER(ArEvaluatorDesc)};
    d.id = id; d.provider_id = "test.provider"; d.provider_version = "1";
    d.phase = phase; d.writes = writes; d.evaluate = noop; return d;
}
void add(Fixture& f, const ArEvaluatorDesc& d) { CHECK(api.register_evaluator(f.runtime, &d, nullptr) == AR_OK); }

void negotiation() {
    ArRuntimeApi invalid{};
    CHECK(arGetApi(1, sizeof(invalid), &invalid) == AR_INCOMPATIBLE_ABI);
    CHECK(arGetApi(2, sizeof(invalid), &invalid) == AR_INCOMPATIBLE_ABI);
    CHECK(arGetApi(3, sizeof(invalid), &invalid) == AR_INCOMPATIBLE_ABI);
    CHECK(arGetApi(AR_ABI_VERSION + 1, sizeof(invalid), &invalid) == AR_INCOMPATIBLE_ABI);
    CHECK(arGetApi(AR_ABI_VERSION, sizeof(invalid) - 1, &invalid) == AR_INCOMPATIBLE_ABI);
    struct Extended { ArRuntimeApi table; uint64_t tail; } extended{};
    extended.tail = 0x12345678;
    CHECK(arGetApi(AR_ABI_VERSION, sizeof(extended), &extended.table) == AR_OK && extended.tail == 0x12345678);
    Fixture f; auto d = evaluator("a"); d.struct_size -= 1;
    CHECK(api.register_evaluator(f.runtime, &d, nullptr) == AR_INCOMPATIBLE_ABI);
    d = evaluator("a"); add(f, d);
    CHECK(api.register_evaluator(f.runtime, &d, nullptr) == AR_DUPLICATE_ID);
    auto instance = f.make({"a"});
    auto revisionTwo = evaluator("revision.two"); revisionTwo.abi_version = 2;
    CHECK(api.register_evaluator(f.runtime, &revisionTwo, nullptr) == AR_INCOMPATIBLE_ABI);
    auto legacy = f.desc({}); legacy.abi_version = 1; ArInstance rejected = 999;
    CHECK(api.create_instance(f.runtime, &legacy, nullptr, &rejected) == AR_INCOMPATIBLE_ABI && rejected == 0);
    legacy.abi_version = 2;
    CHECK(api.create_instance(f.runtime, &legacy, nullptr, &rejected) == AR_INCOMPATIBLE_ABI && rejected == 0);
    legacy.abi_version = AR_ABI_VERSION; legacy.struct_size = uint32_t(offsetof(ArInstanceDesc, layout_id));
    CHECK(api.create_instance(f.runtime, &legacy, nullptr, &rejected) == AR_INCOMPATIBLE_ABI && rejected == 0);
    Fixture other; auto input = frame(1); ArSnapshot snapshot = 999;
    auto oldInput = input; oldInput.abi_version = 1;
    CHECK(api.evaluate_frame(f.runtime, instance, &oldInput, nullptr, &snapshot) == AR_INCOMPATIBLE_ABI && snapshot == 0);
    oldInput.abi_version = 2;
    CHECK(api.evaluate_frame(f.runtime, instance, &oldInput, nullptr, &snapshot) == AR_INCOMPATIBLE_ABI && snapshot == 0);
    oldInput.abi_version = AR_ABI_VERSION; oldInput.struct_size = uint32_t(offsetof(ArInputFrame, gazes));
    CHECK(api.evaluate_frame(f.runtime, instance, &oldInput, nullptr, &snapshot) == AR_INCOMPATIBLE_ABI && snapshot == 0);
    oldInput.abi_version = AR_ABI_VERSION; oldInput.struct_size = uint32_t(offsetof(ArInputFrame, input_revision));
    CHECK(api.evaluate_frame(f.runtime, instance, &oldInput, nullptr, &snapshot) == AR_INCOMPATIBLE_ABI && snapshot == 0);
    CHECK(api.evaluate_frame(f.runtime, instance, &input, nullptr, &snapshot) == AR_OK);
    ArStateView oldView{AR_HEADER(ArStateView)}; oldView.abi_version = 1;
    CHECK(api.get_snapshot(snapshot, &oldView) == AR_INCOMPATIBLE_ABI);
    oldView.abi_version = 2;
    CHECK(api.get_snapshot(snapshot, &oldView) == AR_INCOMPATIBLE_ABI);
    oldView.abi_version = AR_ABI_VERSION; oldView.struct_size = uint32_t(offsetof(ArStateView, layout_id));
    CHECK(api.get_snapshot(snapshot, &oldView) == AR_INCOMPATIBLE_ABI);
    oldView.abi_version = 3; oldView.struct_size = sizeof(ArStateView);
    CHECK(api.get_snapshot(snapshot, &oldView) == AR_INCOMPATIBLE_ABI);
    oldView.abi_version = AR_ABI_VERSION; oldView.struct_size = uint32_t(offsetof(ArStateView, samples));
    CHECK(api.get_snapshot(snapshot, &oldView) == AR_INCOMPATIBLE_ABI); // revision-3 view size
    CHECK(api.release_snapshot(snapshot) == AR_OK);
    CHECK(api.evaluate_frame(other.runtime, instance, &input, nullptr, &snapshot) == AR_INVALID_HANDLE && snapshot == 0);
    CHECK(api.destroy_instance(f.runtime, instance) == AR_OK);
    CHECK(api.destroy_instance(f.runtime, instance) == AR_INVALID_HANDLE);
    CHECK(api.evaluate_frame(f.runtime, instance, &input, nullptr, &snapshot) == AR_INVALID_HANDLE);
}

struct Order {
    std::vector<std::string> calls;
    static ArStatus AR_CALL evaluate(void* p, void*, const ArEvaluationContext*, const ArStateWriter*) {
        auto& pair = *static_cast<std::pair<Order*, const char*>*>(p);
        pair.first->calls.emplace_back(pair.second); return AR_OK;
    }
};
void planning() {
    {
        Fixture f; auto a = evaluator("a", AR_PHASE_CONSTRAINTS, AR_DOMAIN_POSE);
        auto b = evaluator("b", AR_PHASE_CONSTRAINTS, AR_DOMAIN_POSE);
        add(f, b); add(f, a);
        auto desc = f.desc({}); std::vector<const char*> selected{"b", "a"};
        desc.evaluators = selected.data(); desc.evaluator_count = 2; ArInstance id = 99;
        CHECK(api.create_instance(f.runtime, &desc, nullptr, &id) == AR_WRITE_CONFLICT && id == 0);
    }
    {
        Fixture f; auto a = evaluator("a"); auto b = evaluator("b");
        const char* afterA[] = {"b"}; const char* afterB[] = {"a"};
        a.after = afterA; a.after_count = 1; b.after = afterB; b.after_count = 1;
        add(f, a); add(f, b);
        std::vector<const char*> selected{"a", "b"}; auto desc = f.desc(selected); ArInstance id;
        Log log; auto sink = log.sink();
        CHECK(api.create_instance(f.runtime, &desc, &sink, &id) == AR_DEPENDENCY_CYCLE);
        CHECK(log.codes.back() == "runtime.dependency.cycle");
        selected = {"a"}; desc = f.desc(selected);
        CHECK(api.create_instance(f.runtime, &desc, nullptr, &id) == AR_MISSING_DEPENDENCY);
    }
    {
        Fixture f; auto a = evaluator("a", AR_PHASE_CONSTRAINTS); auto b = evaluator("b", AR_PHASE_EXPRESSIONS);
        const char* after[] = {"b"}; a.after = after; a.after_count = 1; add(f, a); add(f, b);
        std::vector<const char*> selected{"a", "b"}; auto desc = f.desc(selected); ArInstance id;
        CHECK(api.create_instance(f.runtime, &desc, nullptr, &id) == AR_PHASE_ORDER);
    }
    {
        Fixture f; Order order;
        std::pair<Order*, const char*> data[]{{&order, "a"}, {&order, "b"}, {&order, "c"}};
        auto a = evaluator("a", AR_PHASE_CONSTRAINTS, AR_DOMAIN_POSE);
        auto b = evaluator("b", AR_PHASE_CONSTRAINTS, AR_DOMAIN_POSE);
        auto c = evaluator("c", AR_PHASE_SAMPLE);
        a.user_data = &data[0]; b.user_data = &data[1]; c.user_data = &data[2];
        a.evaluate = b.evaluate = c.evaluate = Order::evaluate;
        const char* after[] = {"a"}; b.after = after; b.after_count = 1;
        add(f, b); add(f, a); add(f, c);
        auto instance = f.make({"b", "c", "a"}); auto input = frame(1); ArSnapshot s;
        CHECK(api.evaluate_frame(f.runtime, instance, &input, nullptr, &s) == AR_OK);
        CHECK(order.calls == std::vector<std::string>({"c", "a", "b"})); CHECK(api.release_snapshot(s) == AR_OK);
    }
}

void capabilities() {
    Fixture f; auto e = evaluator("a"); ArCapability support{"test.feature", 1};
    e.supplies = &support; e.supply_count = 1; add(f, e);
    std::vector<const char*> selected{"a"}; auto d = f.desc(selected);
    d.required_capabilities = &support; d.required_capability_count = 1; ArInstance id;
    CHECK(api.create_instance(f.runtime, &d, nullptr, &id) == AR_MISSING_CAPABILITY);
    d.bound_capabilities = &support; d.bound_capability_count = 1;
    CHECK(api.create_instance(f.runtime, &d, nullptr, &id) == AR_OK);
    const ArCapability* active; uint32_t count;
    CHECK(api.get_capabilities(f.runtime, id, &active, &count) == AR_OK);
    CHECK(count == 1 && std::string(active[0].id) == "test.feature" && active[0].version == 1);
    support.version = 2;
    CHECK(api.create_instance(f.runtime, &d, nullptr, &id) == AR_MISSING_CAPABILITY);
}

struct Control {
    int mode = 0, creates = 0, destroys = 0, commits = 0, aborts = 0;
    bool priorSeen = false;
    ArRuntime runtime = 0;
    std::vector<std::string>* events = nullptr;
    const char* label = "";
    struct State { int committed = 0, pending = 0; };
    static ArStatus AR_CALL create(void* p, ArInstance, uint64_t, void** out) {
        auto& c = *static_cast<Control*>(p); ++c.creates;
        *out = new State(); return c.mode == 7 ? AR_PROVIDER_ERROR : AR_OK;
    }
    static void AR_CALL destroy(void* p, void* state) { ++static_cast<Control*>(p)->destroys; delete static_cast<State*>(state); }
    static ArStatus AR_CALL begin(void* p, void* state, const ArEvaluationContext* ctx) {
        auto& c = *static_cast<Control*>(p); auto& s = *static_cast<State*>(state);
        if (c.events) c.events->push_back(std::string("begin:") + c.label);
        s.pending = s.committed + 1; c.priorSeen = ctx->prior != nullptr;
        return c.mode == 6 ? AR_PROVIDER_ERROR : AR_OK;
    }
    static ArStatus AR_CALL evaluate(void* p, void* state, const ArEvaluationContext* ctx, const ArStateWriter* writer) {
        auto& c = *static_cast<Control*>(p); auto& s = *static_cast<State*>(state);
        if (c.events) c.events->push_back(std::string("evaluate:") + c.label);
        CHECK(ctx->working->joint_count == 2 && ctx->working->material_count == 0);
        auto t = ctx->working->joints[0].local;
        t.translation[0] = s.pending;
        CHECK(writer->set_joint(writer->context, 0, &t) == AR_OK);
        if (c.mode == 1 || c.mode == 10) return AR_PROVIDER_ERROR;
        if (c.mode == 2) writer->set_blend_shape(writer->context, 0, 1); // undeclared, ignored by provider
        if (c.mode == 3) { t.translation[0] = std::numeric_limits<double>::infinity(); writer->set_joint(writer->context, 0, &t); }
        if (c.mode == 4) throw 42;
        if (c.mode == 5) CHECK(api.destroy_runtime(c.runtime) == AR_BUSY);
        const uint32_t diagnostics = c.mode == 8 ? 300 : 1;
        for (uint32_t i = 0; i < diagnostics; ++i) {
            ArDiagnostic d{AR_HEADER(ArDiagnostic), "owner.warning", "original.owner", "wrong", "test:channel", "Test warning",
                AR_SEVERITY_WARNING, AR_OK, 0, 999, 0};
            if (c.mode == 11) d.abi_version = 99;
            ctx->diagnostics.emit(ctx->diagnostics.user_data, &d);
        }
        return AR_OK;
    }
    static void AR_CALL finish(void* p, void* state, uint32_t commit) {
        auto& c = *static_cast<Control*>(p); auto& s = *static_cast<State*>(state);
        if (c.events) c.events->push_back(std::string(commit ? "commit:" : "abort:") + c.label);
        if ((commit && c.mode == 9) || (!commit && c.mode == 10)) throw 42;
        if (commit) { ++c.commits; s.committed = s.pending; }
        else { ++c.aborts; s.pending = s.committed; }
    }
    ArEvaluatorDesc descriptor() {
        auto e = evaluator("counter", AR_PHASE_BASE_POSE, AR_DOMAIN_POSE); e.user_data = this;
        e.flags = AR_EVALUATOR_STATEFUL; e.create_state = create; e.destroy_state = destroy;
        e.begin_frame = begin; e.evaluate = evaluate; e.end_frame = finish; return e;
    }
};

void transactions() {
    Control c; Fixture f; c.runtime = f.runtime; add(f, c.descriptor());
    auto a = f.make({"counter"}); auto b = f.make({"counter"}); CHECK(c.creates == 2);
    ArSnapshot first, second, s; auto input = frame(1, .25);
    CHECK(api.evaluate_frame(f.runtime, a, &input, nullptr, &first) == AR_OK);
    CHECK(view(first).joints[0].local.translation[0] == 1 && !c.priorSeen);
    input = frame(2, .5);
    for (int mode : {1, 2, 3, 4, 6, 11}) {
        c.mode = mode; s = 99;
        const ArStatus expected = mode == 2 || mode == 11 ? AR_INVALID_ARGUMENT : mode == 3 ? AR_INVALID_STATE : AR_PROVIDER_ERROR;
        CHECK(api.evaluate_frame(f.runtime, a, &input, nullptr, &s) == expected && s == 0);
        CHECK(view(first).frame_id == 1 && view(first).joints[0].local.translation[0] == 1);
    }
    CHECK(c.commits == 1 && c.aborts == 6);
    c.mode = 5; Log log; auto sink = log.sink();
    CHECK(api.evaluate_frame(f.runtime, a, &input, &sink, &second) == AR_OK);
    CHECK(view(second).joints[0].local.translation[0] == 2 && c.priorSeen);
    CHECK(log.origins[0] == "original.owner" && log.evaluators[0] == "counter" && log.frames[0] == 2);
    CHECK(log.statuses[0] == AR_OK && log.subjects[0] == "test:channel");
    c.mode = 0; input = frame(1);
    CHECK(api.evaluate_frame(f.runtime, b, &input, nullptr, &s) == AR_OK);
    CHECK(view(s).joints[0].local.translation[0] == 1); CHECK(api.release_snapshot(s) == AR_OK);
    input = frame(2, .5);
    CHECK(api.evaluate_frame(f.runtime, a, &input, nullptr, &s) == AR_INVALID_ARGUMENT);
    input = frame(3, .1);
    CHECK(api.evaluate_frame(f.runtime, a, &input, nullptr, &s) == AR_INVALID_ARGUMENT);
    c.mode = 7;
    CHECK(api.reset_instance(f.runtime, a, 2) == AR_PROVIDER_ERROR);
    c.mode = 0; input = frame(3, .75);
    CHECK(api.evaluate_frame(f.runtime, a, &input, nullptr, &s) == AR_OK);
    CHECK(view(s).joints[0].local.translation[0] == 3); CHECK(api.release_snapshot(s) == AR_OK);
    CHECK(api.reset_instance(f.runtime, a, 2) == AR_OK);
    CHECK(api.reset_instance(f.runtime, a, 2) == AR_INVALID_ARGUMENT);
    input = frame(1, -1, 2);
    c.mode = 8; Log overflow; auto overflowSink = overflow.sink();
    CHECK(api.evaluate_frame(f.runtime, a, &input, &overflowSink, &s) == AR_OK);
    CHECK(view(s).joints[0].local.translation[0] == 1 && !c.priorSeen);
    CHECK(overflow.codes.size() == AR_MAX_DIAGNOSTICS + 1);
    CHECK(overflow.codes.back() == "runtime.diagnostics.overflow");
    CHECK(api.release_snapshot(first) == AR_OK); CHECK(api.release_snapshot(second) == AR_OK);
    CHECK(api.destroy_instance(f.runtime, a) == AR_OK); CHECK(api.destroy_instance(f.runtime, b) == AR_OK);
    CHECK(c.creates == c.destroys);
    CHECK(view(s).generation == 2); CHECK(api.release_snapshot(s) == AR_OK);
}

void orderedRollbackAndPoisoning() {
    std::vector<std::string> events;
    Control a, b, c; Fixture f;
    a.events = b.events = c.events = &events; a.label = "a"; b.label = "b"; c.label = "c";
    auto da = a.descriptor(); da.id = "a";
    auto db = b.descriptor(); db.id = "b"; db.phase = AR_PHASE_CONSTRAINTS;
    auto dc = c.descriptor(); dc.id = "c"; dc.phase = AR_PHASE_FINAL_POSE;
    add(f, dc); add(f, db); add(f, da);
    auto id = f.make({"c", "b", "a"}); c.mode = 6;
    auto input = frame(1); ArSnapshot s;
    CHECK(api.evaluate_frame(f.runtime, id, &input, nullptr, &s) == AR_PROVIDER_ERROR);
    CHECK(events == std::vector<std::string>({"begin:a", "evaluate:a", "begin:b", "evaluate:b",
        "begin:c", "abort:c", "abort:b", "abort:a"}));
    events.clear(); c.mode = 0;
    CHECK(api.evaluate_frame(f.runtime, id, &input, nullptr, &s) == AR_OK);
    CHECK(view(s).joints[0].local.translation[0] == 1);
    CHECK(events == std::vector<std::string>({"begin:a", "evaluate:a", "begin:b", "evaluate:b",
        "begin:c", "evaluate:c", "commit:a", "commit:b", "commit:c"}));
    CHECK(api.release_snapshot(s) == AR_OK);
    /* A contract-violating infallible callback cannot leave the instance live. */
    c.mode = 9; input = frame(2);
    CHECK(api.evaluate_frame(f.runtime, id, &input, nullptr, &s) == AR_PROVIDER_ERROR && s == 0);
    c.mode = 0;
    CHECK(api.evaluate_frame(f.runtime, id, &input, nullptr, &s) == AR_INVALID_STATE);
    CHECK(api.reset_instance(f.runtime, id, 2) == AR_OK);
    input = frame(1, 0, 2);
    CHECK(api.evaluate_frame(f.runtime, id, &input, nullptr, &s) == AR_OK);
    CHECK(view(s).joints[0].local.translation[0] == 1); CHECK(api.release_snapshot(s) == AR_OK);
    c.mode = 10; input = frame(2, 0, 2);
    CHECK(api.evaluate_frame(f.runtime, id, &input, nullptr, &s) == AR_PROVIDER_ERROR);
    c.mode = 0;
    CHECK(api.evaluate_frame(f.runtime, id, &input, nullptr, &s) == AR_INVALID_STATE);
    CHECK(api.destroy_instance(f.runtime, id) == AR_OK);
    CHECK(a.creates == a.destroys && b.creates == b.destroys && c.creates == c.destroys);
}

ArStatus AR_CALL resolve(void*, void*, const ArEvaluationContext* ctx, const ArStateWriter* writer) {
    if (ctx->input->scalar_count) {
        const double v = ctx->input->scalars[0].value;
        const double color[4]{v, 0, 0, 1};
        CHECK(writer->set_blend_shape(writer->context, 0, v) == AR_OK);
        CHECK(writer->set_material(writer->context, 0, 1, color) == AR_OK);
        CHECK(writer->set_visibility(writer->context, 0, 0) == AR_OK);
    }
    return AR_OK;
}
void inputsAndSnapshots() {
    Fixture f; auto e = evaluator("resolve", AR_PHASE_EXPRESSIONS, AR_DOMAIN_DEFORMATION | AR_DOMAIN_MATERIAL | AR_DOMAIN_VISIBILITY);
    e.evaluate = resolve; add(f, e); auto id = f.make({"resolve"});
    ArScalarInput scalar{"source", "actor", "intent:custom", .75, 10, 1, -10};
    auto input = frame(1); input.scalars = &scalar; input.scalar_count = 1;
    input.has_usd_mapping = 1; input.usd_time_codes_per_second = 24; input.usd_time_code_offset = 100;
    ArSnapshot a, b;
    CHECK(api.evaluate_frame(f.runtime, id, &input, nullptr, &a) == AR_OK);
    auto v = view(a); CHECK(v.blend_shapes[0].weight == .75 && v.materials[0].overridden == 1 && v.visibility[0].visible == 0);
    CHECK(v.joints[1].local.scale[2] == 4 && std::string(v.joints[1].joint_id) == "auxiliary");
    input = frame(2);
    CHECK(api.evaluate_frame(f.runtime, id, &input, nullptr, &b) == AR_OK);
    v = view(b); CHECK(v.blend_shapes[0].weight == 0 && v.materials[0].overridden == 0 && v.visibility[0].visible == 1);
    CHECK(view(a).blend_shapes[0].weight == .75); CHECK(api.release_snapshot(b) == AR_OK);
    input = frame(3); input.scalars = &scalar; input.scalar_count = 1; scalar.value = 0;
    CHECK(api.evaluate_frame(f.runtime, id, &input, nullptr, &b) == AR_OK);
    CHECK(view(b).blend_shapes[0].weight == 0 && view(b).materials[0].overridden == 1);
    CHECK(api.release_snapshot(b) == AR_OK);
    input = frame(4); input.scalars = &scalar; input.scalar_count = 1;
    scalar.value = std::numeric_limits<double>::quiet_NaN();
    CHECK(api.evaluate_frame(f.runtime, id, &input, nullptr, &b) == AR_INVALID_ARGUMENT);
    scalar.value = 0; scalar.clock_scale = 0;
    CHECK(api.evaluate_frame(f.runtime, id, &input, nullptr, &b) == AR_INVALID_ARGUMENT);
    scalar.clock_scale = 1; scalar.channel_id = "unqualified";
    CHECK(api.evaluate_frame(f.runtime, id, &input, nullptr, &b) == AR_INVALID_ARGUMENT);
    scalar.channel_id = "intent:custom"; ArScalarInput duplicate[2]{scalar, scalar};
    input.scalars = duplicate; input.scalar_count = 2;
    CHECK(api.evaluate_frame(f.runtime, id, &input, nullptr, &b) == AR_DUPLICATE_ID);
    CHECK(api.retain_snapshot(a) == AR_OK); CHECK(api.destroy_instance(f.runtime, id) == AR_OK);
    CHECK(view(a).frame_id == 1); CHECK(api.release_snapshot(a) == AR_OK); CHECK(api.release_snapshot(a) == AR_OK);
    CHECK(api.release_snapshot(a) == AR_INVALID_HANDLE);
}

void deterministicFrames() {
    Fixture f; auto e = evaluator("resolve", AR_PHASE_EXPRESSIONS,
        AR_DOMAIN_DEFORMATION | AR_DOMAIN_MATERIAL | AR_DOMAIN_VISIBILITY);
    e.evaluate = resolve; add(f, e);
    auto a = f.make({"resolve"}); auto b = f.make({"resolve"});
    ArScalarInput scalar{"source", "actor", "intent:custom", 0, 0, 1, 0};
    for (uint64_t n = 1; n <= 3; ++n) {
        auto input = frame(n, double(n) * .25); scalar.value = double(n) * .2;
        input.scalars = &scalar; input.scalar_count = 1;
        ArSnapshot sa, sb;
        CHECK(api.evaluate_frame(f.runtime, a, &input, nullptr, &sa) == AR_OK);
        CHECK(api.evaluate_frame(f.runtime, b, &input, nullptr, &sb) == AR_OK);
        const auto va = view(sa), vb = view(sb);
        CHECK(va.frame_id == vb.frame_id && va.generation == vb.generation && va.evaluation_seconds == vb.evaluation_seconds);
        CHECK(va.joint_count == vb.joint_count && va.blend_shape_count == vb.blend_shape_count &&
            va.material_count == vb.material_count && va.visibility_count == vb.visibility_count);
        for (uint32_t j = 0; j < va.joint_count; ++j) {
            CHECK(std::string(va.joints[j].joint_id) == vb.joints[j].joint_id);
            for (int k = 0; k < 3; ++k) CHECK(va.joints[j].local.translation[k] == vb.joints[j].local.translation[k] &&
                va.joints[j].local.scale[k] == vb.joints[j].local.scale[k]);
            for (int k = 0; k < 4; ++k) CHECK(va.joints[j].local.rotation[k] == vb.joints[j].local.rotation[k]);
        }
        CHECK(va.blend_shapes[0].weight == vb.blend_shapes[0].weight);
        CHECK(va.materials[0].overridden == vb.materials[0].overridden && va.materials[0].value_type == vb.materials[0].value_type);
        for (int k = 0; k < 4; ++k) CHECK(va.materials[0].value[k] == vb.materials[0].value[k]);
        CHECK(va.visibility[0].visible == vb.visibility[0].visible);
        CHECK(api.release_snapshot(sa) == AR_OK && api.release_snapshot(sb) == AR_OK);
    }
}

ArGazeInput gaze() {
    return {"source", "actor", "intent:gaze", AR_GAZE_POINT, AR_GAZE_RUNTIME_WORLD,
        AR_OBSERVATION_VALID, nullptr, nullptr, {0, 0, 0}, 2, 2, -1};
}

struct GazeProbe {
    const ArInputFrame* expected = nullptr;
    std::vector<ArGazeInput> observations;
    int calls = 0;
    static ArStatus AR_CALL evaluate(void* p, void*, const ArEvaluationContext* ctx, const ArStateWriter*) {
        auto& probe = *static_cast<GazeProbe*>(p); ++probe.calls;
        CHECK(ctx->input == probe.expected);
        CHECK(ctx->working->joint_count == 0); // input references are independent of state read domains
        const auto& input = *ctx->input;
        CHECK(input.gaze_count == probe.observations.size());
        for (uint32_t i = 0; i < input.gaze_count; ++i) {
            const auto& actual = input.gazes[i]; const auto& expected = probe.observations[i];
            CHECK(std::string(actual.source_id) == expected.source_id && std::string(actual.actor_id) == expected.actor_id);
            CHECK(std::string(actual.channel_id) == expected.channel_id);
            CHECK(actual.kind == expected.kind && actual.space == expected.space && actual.validity == expected.validity);
            CHECK(actual.skeleton_id == expected.skeleton_id && actual.joint_id == expected.joint_id);
            for (int k = 0; k < 3; ++k) CHECK(actual.value[k] == expected.value[k]);
            CHECK(actual.source_seconds == expected.source_seconds && actual.clock_scale == expected.clock_scale &&
                actual.clock_offset == expected.clock_offset);
        }
        if (input.gaze_count) {
            const auto& g = input.gazes[0];
            CHECK(std::string(g.source_id) == "source" && std::string(g.actor_id) == "actor");
            CHECK(std::string(g.channel_id) == "intent:gaze");
            CHECK(g.source_seconds == 2 && g.clock_scale == 2 && g.clock_offset == -1);
            CHECK(input.evaluation_seconds == 10); // mapped sample time 3 remains distinct
        }
        return AR_OK;
    }
};

void gazeInputs() {
    Fixture f; GazeProbe probe;
    auto e = evaluator("gaze", AR_PHASE_GAZE); e.user_data = &probe; e.evaluate = GazeProbe::evaluate;
    add(f, e); auto id = f.make({"gaze"});
    auto input = frame(1, 10); probe.expected = &input;
    ArSnapshot snapshot;
    const auto accept = [&]() {
        const int before = probe.calls;
        probe.observations.clear();
        if (input.gaze_count) probe.observations.assign(input.gazes, input.gazes + input.gaze_count);
        CHECK(api.evaluate_frame(f.runtime, id, &input, nullptr, &snapshot) == AR_OK);
        CHECK(probe.calls == before + 1 && view(snapshot).frame_id == input.frame_id);
        CHECK(api.release_snapshot(snapshot) == AR_OK); ++input.frame_id;
    };
    accept(); // absent observation
    auto g = gaze(); input.gazes = &g; input.gaze_count = 1;
    accept(); // the origin is a valid target point
    g.kind = AR_GAZE_DIRECTION; g.value[2] = 1; accept();
    g.validity = AR_OBSERVATION_STALE; accept(); // forwarded without dropping/holding
    g.validity = AR_OBSERVATION_UNAVAILABLE; g.value[2] = 0; accept();
    g = gaze(); g.space = AR_GAZE_JOINT_LOCAL; g.skeleton_id = "rig"; g.joint_id = "auxiliary";
    g.value[1] = 2; accept(); // opaque bound joints, no new head/eye vocabulary
    g.kind = AR_GAZE_DIRECTION; g.value[1] = -1; accept();
    ArGazeInput multiple[]{g, g, g};
    multiple[1].source_id = "other-source"; multiple[2].actor_id = "other-actor";
    input.gazes = multiple; input.gaze_count = 3; accept(); // no implicit arbitration
}

void gazeValidation() {
    Control control; Fixture f; std::vector<std::string> events; control.events = &events;
    add(f, control.descriptor()); auto id = f.make({"counter"});
    auto input = frame(1); auto g = gaze(); input.gazes = &g; input.gaze_count = 1;
    ArSnapshot first, retry;
    CHECK(api.evaluate_frame(f.runtime, id, &input, nullptr, &first) == AR_OK);
    input.frame_id = 2; input.input_revision = 9; events.clear();
    const auto reject = [&](ArStatus status, const char* code, const char* subject = "intent:gaze") {
        Log log; auto sink = log.sink(); retry = 999;
        CHECK(api.evaluate_frame(f.runtime, id, &input, &sink, &retry) == status && retry == 0);
        CHECK(log.codes.size() == 1 && log.codes[0] == code && log.subjects[0] == subject);
        CHECK(log.origins[0] == "usd-avatar-runtime" && log.frames[0] == 2);
        CHECK(events.empty() && control.commits == 1 && control.aborts == 0);
        CHECK(view(first).frame_id == 1 && view(first).input_revision == 0);
    };
    input.gazes = nullptr; reject(AR_INVALID_ARGUMENT, "runtime.array.invalid", "");
    input.gazes = &g; input.gaze_count = 1048577; reject(AR_INVALID_ARGUMENT, "runtime.array.invalid", "");
    input.gaze_count = 1;
    for (uint32_t unknown : {0u, 99u}) {
        g.kind = unknown; reject(AR_INVALID_ARGUMENT, "runtime.gaze.kind"); g = gaze();
        g.space = unknown; reject(AR_INVALID_ARGUMENT, "runtime.gaze.space"); g = gaze();
        g.validity = unknown; reject(AR_INVALID_ARGUMENT, "runtime.gaze.validity"); g = gaze();
    }
    g.channel_id = "gaze"; reject(AR_INVALID_ARGUMENT, "runtime.channel.namespace", "gaze"); g = gaze();
    g.source_id = ""; reject(AR_INVALID_ARGUMENT, "runtime.identity.invalid", ""); g = gaze();
    g.actor_id = nullptr; reject(AR_INVALID_ARGUMENT, "runtime.identity.invalid", ""); g = gaze();
    g.skeleton_id = "rig"; reject(AR_INVALID_ARGUMENT, "runtime.gaze.reference"); g = gaze();
    g.joint_id = "root"; reject(AR_INVALID_ARGUMENT, "runtime.gaze.reference"); g = gaze();
    g.space = AR_GAZE_JOINT_LOCAL; g.skeleton_id = "rig"; g.joint_id = "missing";
    reject(AR_INVALID_ARGUMENT, "runtime.gaze.reference");
    g.skeleton_id = "other-rig"; g.joint_id = "root"; reject(AR_INVALID_ARGUMENT, "runtime.gaze.reference");
    g.skeleton_id = nullptr; reject(AR_INVALID_ARGUMENT, "runtime.identity.invalid", ""); g = gaze();
    for (double invalid : {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity()}) {
        g.value[1] = invalid; reject(AR_INVALID_ARGUMENT, "runtime.gaze.numeric"); g = gaze();
        g.source_seconds = invalid; reject(AR_INVALID_ARGUMENT, "runtime.input.numeric"); g = gaze();
        g.clock_scale = invalid; reject(AR_INVALID_ARGUMENT, "runtime.input.numeric"); g = gaze();
        g.clock_offset = invalid; reject(AR_INVALID_ARGUMENT, "runtime.input.numeric"); g = gaze();
    }
    for (double invalid : {0.0, -1.0}) {
        g.clock_scale = invalid; reject(AR_INVALID_ARGUMENT, "runtime.input.numeric"); g = gaze();
    }
    g.source_seconds = std::numeric_limits<double>::max(); g.clock_scale = 2;
    reject(AR_INVALID_ARGUMENT, "runtime.input.numeric"); g = gaze();
    g.kind = AR_GAZE_DIRECTION; reject(AR_INVALID_ARGUMENT, "runtime.gaze.direction");
    g.value[2] = 2; reject(AR_INVALID_ARGUMENT, "runtime.gaze.direction");
    g.value[2] = std::numeric_limits<double>::max(); reject(AR_INVALID_ARGUMENT, "runtime.gaze.direction");
    g.validity = AR_OBSERVATION_STALE; g.value[2] = 0; reject(AR_INVALID_ARGUMENT, "runtime.gaze.direction");
    g = gaze(); g.validity = AR_OBSERVATION_UNAVAILABLE; g.value[0] = 1;
    reject(AR_INVALID_ARGUMENT, "runtime.gaze.unavailable"); g = gaze();
    ArGazeInput duplicate[]{g, g}; input.gazes = duplicate; input.gaze_count = 2;
    reject(AR_DUPLICATE_ID, "runtime.input.duplicate"); input.gazes = &g; input.gaze_count = 1;
    ArScalarInput scalar{"source", "actor", "intent:gaze", 0, 0, 1, 0};
    input.scalars = &scalar; input.scalar_count = 1; reject(AR_DUPLICATE_ID, "runtime.input.duplicate");
    input.scalars = nullptr; input.scalar_count = 0;
    CHECK(api.evaluate_frame(f.runtime, id, &input, nullptr, &retry) == AR_OK);
    CHECK(control.commits == 2 && control.aborts == 0 && control.priorSeen);
    CHECK(view(retry).joints[0].local.translation[0] == 2 && view(retry).input_revision == 9);
    CHECK(api.release_snapshot(first) == AR_OK && api.release_snapshot(retry) == AR_OK);
}

struct MetadataProbe {
    uint64_t priorRevision = 0;
    bool hasPrior = false;
    bool fail = false;
    static ArStatus AR_CALL evaluate(void* p, void*, const ArEvaluationContext* ctx, const ArStateWriter*) {
        const auto& probe = *static_cast<MetadataProbe*>(p);
        CHECK(ctx->working->input_revision == ctx->input->input_revision);
        CHECK(std::string(ctx->working->layout_id) == "test.layout" && ctx->working->layout_version == 7);
        CHECK(ctx->working->capability_count == 2 && ctx->working->joint_count == 0);
        CHECK((ctx->prior != nullptr) == probe.hasPrior);
        if (ctx->prior) CHECK(ctx->prior->input_revision == probe.priorRevision && ctx->prior->layout_version == 7);
        return probe.fail ? AR_PROVIDER_ERROR : AR_OK;
    }
};

void snapshotIdentity() {
    Fixture f; MetadataProbe probe;
    auto e = evaluator("metadata"); e.user_data = &probe; e.evaluate = MetadataProbe::evaluate;
    char capabilityId[] = "test.z";
    ArCapability supplied[]{{capabilityId, 3}, {"test.a", 2}};
    e.supplies = supplied; e.supply_count = 2; add(f, e);
    std::vector<const char*> selected{"metadata"}; auto d = f.desc(selected);
    char layoutId[] = "test.layout";
    d.layout_id = layoutId; d.layout_version = 7;
    ArCapability bound[]{{"test.optional", 1}, supplied[0], supplied[1]};
    d.bound_capabilities = bound; d.bound_capability_count = 3;
    /* Only descriptor metadata is authoritative. Initial-state metadata is ignored. */
    d.initial_state.layout_id = reinterpret_cast<const char*>(1);
    d.initial_state.capabilities = reinterpret_cast<const ArCapability*>(1);
    d.initial_state.capability_count = 999; d.initial_state.layout_version = 99;
    d.initial_state.input_revision = 999;
    ArInstance id = 0; Log log; auto sink = log.sink();
    CHECK(api.create_instance(f.runtime, &d, &sink, &id) == AR_OK);
    CHECK(log.codes == std::vector<std::string>{"runtime.capability.inactive"});
    layoutId[0] = 'X'; capabilityId[0] = 'X'; bound[1].version = 99;
    auto input = frame(1); input.input_revision = 77;
    ArSnapshot first, second, reset, retry, rebound;
    CHECK(api.evaluate_frame(f.runtime, id, &input, nullptr, &first) == AR_OK);
    const auto original = view(first);
    CHECK(std::string(original.layout_id) == "test.layout" && original.layout_version == 7);
    CHECK(original.input_revision == 77 && original.capability_count == 2);
    CHECK(std::string(original.capabilities[0].id) == "test.a" && original.capabilities[0].version == 2);
    CHECK(std::string(original.capabilities[1].id) == "test.z" && original.capabilities[1].version == 3);
    const ArCapability* active; uint32_t count;
    CHECK(api.get_capabilities(f.runtime, id, &active, &count) == AR_OK && count == 2);
    CHECK(std::string(active[1].id) == "test.z" && active[1].version == 3);
    probe.hasPrior = true; probe.priorRevision = 77;
    input.frame_id = 2;
    CHECK(api.evaluate_frame(f.runtime, id, &input, nullptr, &second) == AR_OK);
    CHECK(view(second).input_revision == 77 && view(second).generation == 1);
    CHECK(api.reset_instance(f.runtime, id, 2) == AR_OK);
    probe.hasPrior = false; input = frame(1, 0, 2); input.input_revision = 3;
    CHECK(api.evaluate_frame(f.runtime, id, &input, nullptr, &reset) == AR_OK);
    CHECK(view(reset).generation == 2 && view(reset).layout_version == 7 && view(reset).input_revision == 3);
    probe.hasPrior = true; probe.priorRevision = 3; probe.fail = true;
    input.frame_id = 2; input.input_revision = 88; retry = 999;
    CHECK(api.evaluate_frame(f.runtime, id, &input, nullptr, &retry) == AR_PROVIDER_ERROR && retry == 0);
    probe.fail = false; input.input_revision = 0;
    CHECK(api.evaluate_frame(f.runtime, id, &input, nullptr, &retry) == AR_OK);
    CHECK(view(retry).input_revision == 0 && view(reset).input_revision == 3);
    /* Structure change uses a new instance/version; reset alone preserved layout. */
    d = f.desc({}); d.layout_version = 8; f.joints[1].joint_id = "newAuxiliary";
    ArInstance other;
    CHECK(api.create_instance(f.runtime, &d, nullptr, &other) == AR_OK);
    input = frame(1); input.input_revision = 4;
    CHECK(api.evaluate_frame(f.runtime, other, &input, nullptr, &rebound) == AR_OK);
    CHECK(view(rebound).layout_version == 8 && view(rebound).instance != original.instance);
    CHECK(std::string(view(rebound).joints[1].joint_id) == "newAuxiliary" && view(rebound).capability_count == 0);
    CHECK(api.destroy_runtime(f.runtime) == AR_OK); f.runtime = 0;
    /* Previously returned pointers, not only fresh views, remain readable. */
    CHECK(std::string(original.layout_id) == "test.layout" && original.input_revision == 77);
    CHECK(std::string(original.capabilities[1].id) == "test.z" && original.capabilities[1].version == 3);
    CHECK(std::string(original.joints[1].joint_id) == "auxiliary");
    for (auto snapshot : {first, second, reset, retry, rebound}) CHECK(api.release_snapshot(snapshot) == AR_OK);
}

ArStatus AR_CALL badOffset(void*, void*, const ArEvaluationContext*, const ArStateWriter* writer) {
    const double value[4]{.25, .5, 1, 0};
    CHECK(writer->set_material(writer->context, 0, 1, value) == AR_INVALID_STATE);
    return AR_OK; // the rejected write still latches frame failure
}
ArStatus AR_CALL goodOffset(void*, void*, const ArEvaluationContext*, const ArStateWriter* writer) {
    const double value[4]{.25, .5, 0, 0};
    return writer->set_material(writer->context, 0, 1, value);
}
void vec2Materials() {
    Fixture f; f.material = {"material", "test:baseTextureOffset", AR_VALUE_VEC2, 0, {0, 0, 0, 0}};
    auto e = evaluator("offset.bad", AR_PHASE_APPEARANCE, AR_DOMAIN_MATERIAL); e.evaluate = badOffset; add(f, e);
    e = evaluator("offset.good", AR_PHASE_APPEARANCE, AR_DOMAIN_MATERIAL); e.evaluate = goodOffset; add(f, e);
    auto bad = f.make({"offset.bad"}), good = f.make({"offset.good"});
    auto input = frame(1); ArSnapshot s = 999;
    CHECK(api.evaluate_frame(f.runtime, bad, &input, nullptr, &s) == AR_INVALID_STATE && s == 0);
    CHECK(api.evaluate_frame(f.runtime, good, &input, nullptr, &s) == AR_OK);
    const auto v = view(s);
    CHECK(v.materials[0].value_type == AR_VALUE_VEC2 && v.materials[0].overridden == 1);
    CHECK(v.materials[0].value[0] == .25 && v.materials[0].value[1] == .5 && v.materials[0].value[2] == 0);
    CHECK(api.release_snapshot(s) == AR_OK);
}

struct SampleProbe {
    int mode = 0;
    bool hasPrior = false;
    static ArStatus AR_CALL evaluate(void* p, void*, const ArEvaluationContext* ctx, const ArStateWriter* writer) {
        const auto& probe = *static_cast<SampleProbe*>(p);
        /* Working views never expose provenance that a report could reallocate. */
        CHECK(ctx->working->samples == nullptr && ctx->working->sample_count == 0);
        CHECK((ctx->prior != nullptr) == probe.hasPrior);
        if (ctx->prior) CHECK(ctx->prior->sample_count == 3 && ctx->prior->samples[2].kind == AR_SOURCE_POSE);
        ArSourceSample s{"clip", "actor", "motion:pose", "ignored", AR_SOURCE_POSE, AR_OBSERVATION_VALID,
            AR_SAMPLE_HELD, 4, 2, 1, 99};
        switch (probe.mode) {
        case 1: s.source_id = "source"; s.channel_id = "intent:custom"; break; // collides with an input
        case 2: s.kind = AR_SOURCE_SCALAR; break;
        case 3: s.resolution = AR_SAMPLE_SELECTED; break;
        case 4: s.resolution = 5; break;
        case 5: s.validity = AR_OBSERVATION_STALE; break;
        case 6: s.clock_scale = 0; break;
        case 7: s.source_seconds = std::numeric_limits<double>::quiet_NaN(); break;
        case 8: s.channel_id = "pose"; break;
        case 9: s.actor_id = ""; break;
        case 10: CHECK(writer->report_sample(writer->context, nullptr) == AR_INVALID_ARGUMENT); return AR_OK;
        case 11: CHECK(writer->report_sample(writer->context, &s) == AR_OK); break; // then reported twice
        default: break;
        }
        const ArStatus expected = probe.mode == 0 ? AR_OK :
            probe.mode == 1 || probe.mode == 11 ? AR_DUPLICATE_ID : AR_INVALID_ARGUMENT;
        CHECK(writer->report_sample(writer->context, &s) == expected);
        return AR_OK; // a rejected report still latches frame failure
    }
};
ArStatus AR_CALL reportWithoutPose(void*, void*, const ArEvaluationContext*, const ArStateWriter* writer) {
    ArSourceSample s{"clip", "actor", "motion:other", "", AR_SOURCE_POSE, AR_OBSERVATION_VALID,
        AR_SAMPLE_INTERPOLATED, 0, 1, 0, 0};
    CHECK(writer->report_sample(writer->context, &s) == AR_INVALID_ARGUMENT);
    return AR_OK;
}
void sourceSamples() {
    Fixture f; SampleProbe probe;
    auto e = evaluator("pose", AR_PHASE_RETARGET, AR_DOMAIN_POSE); e.user_data = &probe; e.evaluate = SampleProbe::evaluate;
    add(f, e); auto id = f.make({"pose"});
    ArScalarInput scalar{"source", "actor", "intent:custom", .5, 10, 1, -10};
    auto g = gaze(); g.validity = AR_OBSERVATION_STALE; g.value[0] = 1; // a held gaze keeps its source time
    auto input = frame(1, 5); input.scalars = &scalar; input.scalar_count = 1; input.gazes = &g; input.gaze_count = 1;
    ArSnapshot first, next;
    CHECK(api.evaluate_frame(f.runtime, id, &input, nullptr, &first) == AR_OK);
    const auto v = view(first);
    CHECK(v.sample_count == 3 && v.evaluation_seconds == 5);
    const auto& sc = v.samples[0]; const auto& gz = v.samples[1]; const auto& pose = v.samples[2];
    CHECK(sc.kind == AR_SOURCE_SCALAR && sc.validity == AR_OBSERVATION_VALID && sc.resolution == AR_SAMPLE_SELECTED);
    CHECK(std::string(sc.channel_id) == "intent:custom" && std::string(sc.evaluator_id).empty());
    CHECK(sc.source_seconds == 10 && sc.runtime_seconds == 0);
    CHECK(gz.kind == AR_SOURCE_GAZE && gz.validity == AR_OBSERVATION_STALE && gz.resolution == AR_SAMPLE_SELECTED);
    CHECK(gz.source_seconds == 2 && gz.clock_scale == 2 && gz.clock_offset == -1 && gz.runtime_seconds == 3);
    CHECK(pose.kind == AR_SOURCE_POSE && pose.resolution == AR_SAMPLE_HELD && pose.validity == AR_OBSERVATION_VALID);
    CHECK(std::string(pose.source_id) == "clip" && std::string(pose.evaluator_id) == "pose");
    CHECK(pose.source_seconds == 4 && pose.runtime_seconds == 9); // supplied stamps are replaced
    probe.hasPrior = true;
    for (int mode = 1; mode <= 11; ++mode) {
        probe.mode = mode; input.frame_id = 2; next = 999; Log log; auto sink = log.sink();
        const ArStatus expected = mode == 1 || mode == 11 ? AR_DUPLICATE_ID : AR_INVALID_ARGUMENT;
        CHECK(api.evaluate_frame(f.runtime, id, &input, &sink, &next) == expected && next == 0);
        CHECK(!log.codes.empty() && log.codes.back() == "runtime.writer.invalid" && log.evaluators.back() == "pose");
    }
    probe.mode = 0; input.scalars = nullptr; input.scalar_count = 0; input.gazes = nullptr; input.gaze_count = 0;
    CHECK(api.evaluate_frame(f.runtime, id, &input, nullptr, &next) == AR_OK);
    CHECK(view(next).sample_count == 1 && view(next).samples[0].kind == AR_SOURCE_POSE); // absent inputs leave no record
    CHECK(api.release_snapshot(next) == AR_OK);
    auto other = evaluator("material.only", AR_PHASE_APPEARANCE, AR_DOMAIN_MATERIAL); other.evaluate = reportWithoutPose;
    add(f, other); auto materialOnly = f.make({"material.only"}); input = frame(1);
    CHECK(api.evaluate_frame(f.runtime, materialOnly, &input, nullptr, &next) == AR_INVALID_ARGUMENT);
    CHECK(api.destroy_runtime(f.runtime) == AR_OK); f.runtime = 0;
    /* Provenance strings are copied and outlive the runtime with the snapshot. */
    CHECK(std::string(v.samples[1].source_id) == "source" && std::string(v.samples[2].evaluator_id) == "pose");
    CHECK(api.release_snapshot(first) == AR_OK);
}

void validation() {
    Fixture f; ArInstance id; auto d = f.desc({});
    d.layout_id = nullptr;
    CHECK(api.create_instance(f.runtime, &d, nullptr, &id) == AR_INVALID_ARGUMENT);
    d.layout_id = "";
    CHECK(api.create_instance(f.runtime, &d, nullptr, &id) == AR_INVALID_ARGUMENT);
    d.layout_id = "test.layout"; d.layout_version = 0;
    CHECK(api.create_instance(f.runtime, &d, nullptr, &id) == AR_INVALID_ARGUMENT);
    d.layout_version = 1;
    f.joints[0].local.rotation[3] = 0;
    CHECK(api.create_instance(f.runtime, &d, nullptr, &id) == AR_INVALID_STATE);
    f.joints[0].local.rotation[3] = 1; f.joints[1].parent_index = 1;
    CHECK(api.create_instance(f.runtime, &d, nullptr, &id) == AR_INVALID_STATE);
    f.joints[1].parent_index = 0; f.material.value_type = AR_VALUE_VEC2; // unused z/w must be zero
    CHECK(api.create_instance(f.runtime, &d, nullptr, &id) == AR_INVALID_STATE);
    f.material.value_type = 5;
    CHECK(api.create_instance(f.runtime, &d, nullptr, &id) == AR_INVALID_STATE);
    f.material.value_type = AR_VALUE_VEC4;
    CHECK(api.create_instance(f.runtime, &d, nullptr, &id) == AR_OK);
    auto input = frame(1); input.has_usd_mapping = 1; input.usd_time_codes_per_second = 0; ArSnapshot s;
    CHECK(api.evaluate_frame(f.runtime, id, &input, nullptr, &s) == AR_INVALID_ARGUMENT);
}

int main() {
    CHECK(arGetApi(AR_ABI_VERSION, sizeof(api), &api) == AR_OK);
    negotiation(); planning(); capabilities(); transactions(); orderedRollbackAndPoisoning();
    inputsAndSnapshots(); deterministicFrames(); gazeInputs(); gazeValidation(); snapshotIdentity(); validation();
    vec2Materials(); sourceSamples();
    std::cout << "Runtime contract checks passed\n";
}
