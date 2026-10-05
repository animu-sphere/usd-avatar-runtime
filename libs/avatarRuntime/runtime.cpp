#include "avatarRuntime/api.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace {
struct Failure { ArStatus status; };
thread_local uint32_t callbackDepth = 0;
struct CallbackScope {
    CallbackScope() { ++callbackDepth; }
    ~CallbackScope() { --callbackDepth; }
};

template<class F> ArStatus boundary(F&& f) noexcept {
    if (callbackDepth) return AR_BUSY;
    try { return f(); }
    catch (const Failure& e) { return e.status; }
    catch (const std::bad_alloc&) { return AR_OUT_OF_MEMORY; }
    catch (...) { return AR_PROVIDER_ERROR; }
}
template<class T> bool header(const T* p) {
    return p && p->abi_version == AR_ABI_VERSION && p->struct_size >= sizeof(T);
}
template<class T> bool span(const T* p, uint32_t n) {
    return n <= 1048576u && (!n || p);
}

/* Identifiers are validated UTF-8, without control characters. */
bool identifier(const char* p) {
    if (!p || !*p) return false;
    size_t length = 0;
    while (p[length] && length <= 1024) ++length;
    if (length > 1024) return false;
    for (size_t i = 0; i < length;) {
        const auto c = static_cast<unsigned char>(p[i++]);
        if (c < 0x80) { if (c < 0x20 || c == 0x7f) return false; continue; }
        uint32_t value = 0, count = 0, minimum = 0;
        if (c >= 0xc2 && c <= 0xdf) { value = c & 31; count = 1; minimum = 0x80; }
        else if (c >= 0xe0 && c <= 0xef) { value = c & 15; count = 2; minimum = 0x800; }
        else if (c >= 0xf0 && c <= 0xf4) { value = c & 7; count = 3; minimum = 0x10000; }
        else return false;
        if (i + count > length) return false;
        while (count--) {
            const auto next = static_cast<unsigned char>(p[i++]);
            if ((next & 0xc0) != 0x80) return false;
            value = (value << 6) | (next & 63);
        }
        if (value < minimum || value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff)) return false;
    }
    return true;
}

struct Diagnostics {
    ArDiagnosticSink sink{};
    ArInstance instance = 0;
    uint64_t frame = 0;
    const char* evaluator = "";
    const char* origin = "usd-avatar-runtime";
    ArPhase phase = 0;
    uint32_t count = 0;
    bool overflow = false;
    ArStatus callbackFailure = AR_OK;

    explicit Diagnostics(const ArDiagnosticSink* s) { if (s) sink = *s; }
    void send(const ArDiagnostic& d) {
        if (sink.emit) {
            try { CallbackScope scope; sink.emit(sink.user_data, &d); }
            catch (...) { callbackFailure = AR_PROVIDER_ERROR; }
        }
    }
    void emit(ArStatus status, const char* code, const char* message,
              const char* subject = "", uint32_t severity = AR_SEVERITY_ERROR) {
        if (count >= AR_MAX_DIAGNOSTICS) {
            if (!overflow) {
                overflow = true;
                ArDiagnostic d{AR_HEADER(ArDiagnostic), "runtime.diagnostics.overflow",
                    "usd-avatar-runtime", evaluator, "", "Further diagnostics omitted",
                    AR_SEVERITY_WARNING, AR_OK, instance, frame, phase};
                send(d);
            }
            return;
        }
        ++count;
        ArDiagnostic d{AR_HEADER(ArDiagnostic), code, origin, evaluator, subject,
            message, severity, status, instance, frame, phase};
        send(d);
    }
    [[noreturn]] void fail(ArStatus s, const char* code, const char* msg, const char* subject = "") {
        emit(s, code, msg, subject);
        throw Failure{s};
    }
    static void AR_CALL providerEmit(void* ptr, const ArDiagnostic* d) noexcept {
        auto& self = *static_cast<Diagnostics*>(ptr);
        if (!header(d) || !identifier(d->code) || !identifier(d->origin) ||
            !d->message || d->severity > AR_SEVERITY_ERROR) {
            self.callbackFailure = AR_INVALID_ARGUMENT;
            self.emit(AR_INVALID_ARGUMENT, "runtime.diagnostic.invalid", "Invalid provider diagnostic");
            return;
        }
        const char* saved = self.origin;
        self.origin = d->origin;
        self.emit(d->status, d->code, d->message, d->subject ? d->subject : "", d->severity);
        self.origin = saved;
    }
};

template<class T> void requireHeader(const T* p, Diagnostics& d) {
    if (!p) d.fail(AR_INVALID_ARGUMENT, "runtime.argument.null", "Null descriptor");
    if (!header(p)) d.fail(AR_INCOMPATIBLE_ABI, "runtime.abi.incompatible", "Unsupported descriptor version or size");
}
template<class T> void requireSpan(const T* p, uint32_t n, Diagnostics& d) {
    if (!span(p, n)) d.fail(AR_INVALID_ARGUMENT, "runtime.array.invalid", "Invalid count or array pointer");
}
void requireId(const char* id, Diagnostics& d) {
    if (!identifier(id)) d.fail(AR_INVALID_ARGUMENT, "runtime.identity.invalid", "Invalid UTF-8 identifier");
}
bool transformValid(const ArTransform& t) {
    double norm = 0;
    for (double v : t.translation) if (!std::isfinite(v)) return false;
    for (double v : t.scale) if (!std::isfinite(v)) return false;
    for (double v : t.rotation) { if (!std::isfinite(v)) return false; norm += v * v; }
    return std::isfinite(norm) && std::abs(norm - 1.0) <= 1e-6;
}
bool materialValid(const ArMaterialInput& m) {
    if (m.value_type != AR_VALUE_SCALAR && m.value_type != AR_VALUE_VEC3 && m.value_type != AR_VALUE_VEC4) return false;
    if (m.overridden > 1) return false;
    for (uint32_t i = 0; i < 4; ++i)
        if (!std::isfinite(m.value[i]) || (i >= m.value_type && m.value[i] != 0)) return false;
    return true;
}

struct Layout {
    std::string id;
    uint64_t version;
    std::map<std::string, uint32_t> active;
    std::vector<ArCapability> capabilities;
    Layout(const char* layoutId, uint64_t layoutVersion, std::map<std::string, uint32_t>&& support)
        : id(layoutId), version(layoutVersion), active(std::move(support)) {
        for (const auto& c : active) capabilities.push_back({c.first.c_str(), c.second});
    }
};

struct State {
    ArInstance instance = 0;
    uint64_t frame = 0, generation = 0;
    double seconds = 0;
    uint64_t inputRevision = 0;
    std::shared_ptr<const Layout> layout;
    std::vector<std::string> strings;
    std::vector<ArJoint> joints;
    std::vector<ArBlendShape> shapes;
    std::vector<ArMaterialInput> materials;
    std::vector<ArVisibility> visibility;

    explicit State(const ArStateView& v) : instance(v.instance), frame(v.frame_id),
        generation(v.generation), seconds(v.evaluation_seconds) {
        strings.reserve(size_t(v.joint_count) * 2 + size_t(v.blend_shape_count) * 2 +
            size_t(v.material_count) * 2 + v.visibility_count);
        auto copyId = [&](const char* p) { strings.emplace_back(p); return strings.back().c_str(); };
        for (uint32_t i = 0; i < v.joint_count; ++i) {
            auto j = v.joints[i]; j.skeleton_id = copyId(j.skeleton_id); j.joint_id = copyId(j.joint_id); joints.push_back(j);
        }
        for (uint32_t i = 0; i < v.blend_shape_count; ++i) {
            auto s = v.blend_shapes[i]; s.mesh_id = copyId(s.mesh_id); s.target_id = copyId(s.target_id); shapes.push_back(s);
        }
        for (uint32_t i = 0; i < v.material_count; ++i) {
            auto m = v.materials[i]; m.material_id = copyId(m.material_id); m.input_id = copyId(m.input_id); materials.push_back(m);
        }
        for (uint32_t i = 0; i < v.visibility_count; ++i) {
            auto x = v.visibility[i]; x.target_id = copyId(x.target_id); visibility.push_back(x);
        }
    }
    State(const State& other) : State(other.view()) {
        inputRevision = other.inputRevision; layout = other.layout;
    }
    State(State&&) = default;
    State& operator=(const State&) = delete;
    ArStateView view(ArDomain domains = AR_DOMAIN_ALL) const {
        ArStateView v{AR_HEADER(ArStateView)};
        v.instance = instance; v.frame_id = frame; v.generation = generation; v.evaluation_seconds = seconds;
        v.input_revision = inputRevision;
        if (layout) {
            v.layout_id = layout->id.c_str(); v.layout_version = layout->version;
            v.capabilities = layout->capabilities.data(); v.capability_count = uint32_t(layout->capabilities.size());
        }
        if (domains & AR_DOMAIN_POSE) { v.joints = joints.data(); v.joint_count = uint32_t(joints.size()); }
        if (domains & AR_DOMAIN_DEFORMATION) { v.blend_shapes = shapes.data(); v.blend_shape_count = uint32_t(shapes.size()); }
        if (domains & AR_DOMAIN_MATERIAL) { v.materials = materials.data(); v.material_count = uint32_t(materials.size()); }
        if (domains & AR_DOMAIN_VISIBILITY) { v.visibility = visibility.data(); v.visibility_count = uint32_t(visibility.size()); }
        return v;
    }
};

void validateState(const ArStateView& v, Diagnostics& d) {
    requireHeader(&v, d);
    requireSpan(v.joints, v.joint_count, d); requireSpan(v.blend_shapes, v.blend_shape_count, d);
    requireSpan(v.materials, v.material_count, d); requireSpan(v.visibility, v.visibility_count, d);
    std::set<std::pair<std::string, std::string>> ids;
    for (uint32_t i = 0; i < v.joint_count; ++i) {
        const auto& j = v.joints[i]; requireId(j.skeleton_id, d); requireId(j.joint_id, d);
        if (!ids.emplace(j.skeleton_id, j.joint_id).second)
            d.fail(AR_DUPLICATE_ID, "runtime.layout.duplicate", "Duplicate joint identity", j.joint_id);
        if (j.parent_index < -1 || j.parent_index >= int64_t(i) ||
            (j.parent_index >= 0 && std::strcmp(j.skeleton_id, v.joints[j.parent_index].skeleton_id)))
            d.fail(AR_INVALID_STATE, "runtime.layout.parent", "Invalid joint parent", j.joint_id);
        if (!transformValid(j.local)) d.fail(AR_INVALID_STATE, "runtime.state.transform", "Non-finite transform or non-unit rotation", j.joint_id);
    }
    ids.clear();
    for (uint32_t i = 0; i < v.blend_shape_count; ++i) {
        const auto& s = v.blend_shapes[i]; requireId(s.mesh_id, d); requireId(s.target_id, d);
        if (!ids.emplace(s.mesh_id, s.target_id).second)
            d.fail(AR_DUPLICATE_ID, "runtime.layout.duplicate", "Duplicate blend-shape identity", s.target_id);
        if (!std::isfinite(s.weight)) d.fail(AR_INVALID_STATE, "runtime.state.weight", "Non-finite blend-shape weight", s.target_id);
    }
    ids.clear();
    for (uint32_t i = 0; i < v.material_count; ++i) {
        const auto& m = v.materials[i]; requireId(m.material_id, d); requireId(m.input_id, d);
        if (!ids.emplace(m.material_id, m.input_id).second)
            d.fail(AR_DUPLICATE_ID, "runtime.layout.duplicate", "Duplicate material input", m.input_id);
        if (!materialValid(m)) d.fail(AR_INVALID_STATE, "runtime.state.material", "Invalid typed material value", m.input_id);
    }
    std::set<std::string> targets;
    for (uint32_t i = 0; i < v.visibility_count; ++i) {
        const auto& x = v.visibility[i]; requireId(x.target_id, d);
        if (!targets.emplace(x.target_id).second)
            d.fail(AR_DUPLICATE_ID, "runtime.layout.duplicate", "Duplicate visibility target", x.target_id);
        if (x.visible > 1) d.fail(AR_INVALID_STATE, "runtime.state.visibility", "Visibility must be 0 or 1", x.target_id);
    }
}

using Capabilities = std::map<std::string, uint32_t>;
Capabilities copyCaps(const ArCapability* p, uint32_t n, Diagnostics& d) {
    requireSpan(p, n, d);
    Capabilities result;
    for (uint32_t i = 0; i < n; ++i) {
        requireId(p[i].id, d);
        if (!p[i].version) d.fail(AR_INVALID_ARGUMENT, "runtime.capability.version", "Capability version must be nonzero", p[i].id);
        if (!result.emplace(p[i].id, p[i].version).second)
            d.fail(AR_DUPLICATE_ID, "runtime.capability.duplicate", "Duplicate capability", p[i].id);
    }
    return result;
}
void requireCaps(const Capabilities& need, const Capabilities& have, Diagnostics& d) {
    for (const auto& c : need) {
        auto found = have.find(c.first);
        if (found == have.end() || found->second != c.second)
            d.fail(AR_MISSING_CAPABILITY, "runtime.capability.missing", "Required capability version unavailable", c.first.c_str());
    }
}

struct Evaluator {
    ArEvaluatorDesc callbacks{};
    std::string id, provider, version;
    std::vector<std::string> after;
    Capabilities supplies, required;
};
struct BoundEvaluator { Evaluator* evaluator; void* state = nullptr; bool created = false; };
void cleanupStates(std::vector<BoundEvaluator>& plan) noexcept {
    for (auto i = plan.rbegin(); i != plan.rend(); ++i) {
        if (i->created && i->evaluator->callbacks.destroy_state) {
            try { CallbackScope scope; i->evaluator->callbacks.destroy_state(i->evaluator->callbacks.user_data, i->state); }
            catch (...) { /* Providers contractually cannot throw in destruction. */ }
        }
    }
    plan.clear();
}
struct Instance {
    uint64_t generation;
    State baseline;
    std::vector<BoundEvaluator> plan;
    std::shared_ptr<const State> prior;
    bool poisoned = false;
    Instance(uint64_t g, const ArStateView& v) : generation(g), baseline(v) {}
    ~Instance() { cleanupStates(plan); }
};
struct Runtime {
    /* Instances must be destroyed before descriptor code/metadata. */
    std::map<std::string, Evaluator> evaluators;
    std::map<ArInstance, std::unique_ptr<Instance>> instances;
};
struct Snapshot { std::shared_ptr<const State> state; uint64_t references = 1; };
/* The table tracks handle lifetime only, not a global active avatar. This
   initial direct implementation serializes API calls, including callbacks. */
std::mutex apiMutex;
std::map<ArRuntime, std::unique_ptr<Runtime>> runtimes;
std::map<ArSnapshot, Snapshot> snapshots;
uint64_t nextHandle = 1;
uint64_t newHandle() {
    if (nextHandle == std::numeric_limits<uint64_t>::max()) throw std::bad_alloc();
    return nextHandle++;
}
Runtime& runtime(ArRuntime h) {
    auto i = runtimes.find(h);
    if (i == runtimes.end()) throw Failure{AR_INVALID_HANDLE};
    return *i->second;
}
Instance& instance(Runtime& r, ArInstance h) {
    auto i = r.instances.find(h);
    if (i == r.instances.end()) throw Failure{AR_INVALID_HANDLE};
    return *i->second;
}

std::vector<Evaluator*> makePlan(Runtime& r, const ArInstanceDesc& desc, Diagnostics& d, Capabilities& active) {
    requireSpan(desc.evaluators, desc.evaluator_count, d);
    std::map<std::string, Evaluator*> selected;
    for (uint32_t i = 0; i < desc.evaluator_count; ++i) {
        requireId(desc.evaluators[i], d);
        auto e = r.evaluators.find(desc.evaluators[i]);
        if (e == r.evaluators.end()) d.fail(AR_MISSING_DEPENDENCY, "runtime.evaluator.missing", "Selected evaluator is unregistered", desc.evaluators[i]);
        if (!selected.emplace(e->first, &e->second).second)
            d.fail(AR_DUPLICATE_ID, "runtime.evaluator.duplicate", "Evaluator selected twice", desc.evaluators[i]);
    }
    /* Explicit edges plus phase barriers. No registration-order tie-break. */
    std::map<std::string, std::set<std::string>> edges;
    std::map<std::string, size_t> degree;
    for (const auto& e : selected) degree[e.first] = 0;
    auto add = [&](const std::string& a, const std::string& b) {
        if (edges[a].emplace(b).second) ++degree[b];
    };
    for (const auto& e : selected) {
        for (const auto& dep : e.second->after) {
            auto found = selected.find(dep);
            if (found == selected.end()) d.fail(AR_MISSING_DEPENDENCY, "runtime.dependency.missing", "Required predecessor is not selected", dep.c_str());
            if (found->second->callbacks.phase > e.second->callbacks.phase)
                d.fail(AR_PHASE_ORDER, "runtime.phase.backward", "Dependency crosses a phase backwards", e.first.c_str());
            add(dep, e.first);
        }
        for (const auto& other : selected)
            if (e.second->callbacks.phase < other.second->callbacks.phase) add(e.first, other.first);
    }
    std::vector<Evaluator*> ordered;
    while (ordered.size() < selected.size()) {
        auto best = degree.end();
        for (auto i = degree.begin(); i != degree.end(); ++i)
            if (!i->second && (best == degree.end() ||
                std::make_pair(selected[i->first]->callbacks.phase, i->first) <
                std::make_pair(selected[best->first]->callbacks.phase, best->first))) best = i;
        if (best == degree.end()) d.fail(AR_DEPENDENCY_CYCLE, "runtime.dependency.cycle", "Evaluator dependency cycle");
        std::string id = best->first;
        ordered.push_back(selected[id]); degree.erase(best);
        for (const auto& target : edges[id]) --degree.at(target);
    }
    auto reachable = [&](const std::string& from, const std::string& to) {
        std::vector<std::string> pending{from}; std::set<std::string> seen;
        while (!pending.empty()) {
            auto current = std::move(pending.back()); pending.pop_back();
            if (current == to) return true;
            if (!seen.emplace(current).second) continue;
            for (const auto& child : edges[current]) pending.push_back(child);
        }
        return false;
    };
    for (size_t a = 0; a < ordered.size(); ++a) for (size_t b = a + 1; b < ordered.size(); ++b)
        if ((ordered[a]->callbacks.writes & ordered[b]->callbacks.writes) &&
            !reachable(ordered[a]->id, ordered[b]->id))
            d.fail(AR_WRITE_CONFLICT, "runtime.writer.conflict", "Overlapping state writers require an ordering dependency", ordered[b]->id.c_str());
    Capabilities supplied;
    for (const auto* e : ordered) for (const auto& c : e->supplies) {
        auto found = supplied.emplace(c);
        if (!found.second && found.first->second != c.second)
            d.fail(AR_INCOMPATIBLE_ABI, "runtime.capability.conflict", "Providers supply different capability versions", c.first.c_str());
    }
    auto bound = copyCaps(desc.bound_capabilities, desc.bound_capability_count, d);
    for (const auto& c : bound) {
        auto found = supplied.find(c.first);
        if (found != supplied.end() && found->second == c.second) active.emplace(c);
        else d.emit(AR_OK, "runtime.capability.inactive", "Bound capability has no selected provider", c.first.c_str(), AR_SEVERITY_WARNING);
    }
    requireCaps(copyCaps(desc.required_capabilities, desc.required_capability_count, d), active, d);
    for (const auto* e : ordered) requireCaps(e->required, active, d);
    return ordered;
}

void initializeStates(std::vector<BoundEvaluator>& plan, ArInstance id, uint64_t generation) {
    /* Each pushed state is cleaned even when create_state reports failure. */
    for (auto& b : plan) {
        if (b.evaluator->callbacks.create_state) {
            b.created = true;
            CallbackScope scope;
            ArStatus s = b.evaluator->callbacks.create_state(b.evaluator->callbacks.user_data, id, generation, &b.state);
            if (s != AR_OK) throw Failure{AR_PROVIDER_ERROR};
        }
    }
}

struct Writer {
    State& state;
    ArDomain domains;
    ArStatus failure = AR_OK;
    ArStatus reject(ArStatus s) { if (failure == AR_OK) failure = s; return s; }
    static ArStatus AR_CALL joint(void* ptr, uint32_t n, const ArTransform* t) {
        auto& w = *static_cast<Writer*>(ptr);
        if (!(w.domains & AR_DOMAIN_POSE) || n >= w.state.joints.size() || !t) return w.reject(AR_INVALID_ARGUMENT);
        if (!transformValid(*t)) return w.reject(AR_INVALID_STATE);
        w.state.joints[n].local = *t; return AR_OK;
    }
    static ArStatus AR_CALL shape(void* ptr, uint32_t n, double v) {
        auto& w = *static_cast<Writer*>(ptr);
        if (!(w.domains & AR_DOMAIN_DEFORMATION) || n >= w.state.shapes.size()) return w.reject(AR_INVALID_ARGUMENT);
        if (!std::isfinite(v)) return w.reject(AR_INVALID_STATE);
        w.state.shapes[n].weight = v; return AR_OK;
    }
    static ArStatus AR_CALL material(void* ptr, uint32_t n, uint32_t overridden, const double* v) {
        auto& w = *static_cast<Writer*>(ptr);
        if (!(w.domains & AR_DOMAIN_MATERIAL) || n >= w.state.materials.size() || !v) return w.reject(AR_INVALID_ARGUMENT);
        auto m = w.state.materials[n]; m.overridden = overridden;
        std::copy(v, v + 4, m.value);
        if (!materialValid(m)) return w.reject(AR_INVALID_STATE);
        w.state.materials[n] = m; return AR_OK;
    }
    static ArStatus AR_CALL visibility(void* ptr, uint32_t n, uint32_t v) {
        auto& w = *static_cast<Writer*>(ptr);
        if (!(w.domains & AR_DOMAIN_VISIBILITY) || n >= w.state.visibility.size()) return w.reject(AR_INVALID_ARGUMENT);
        if (v > 1) return w.reject(AR_INVALID_STATE);
        w.state.visibility[n].visible = v; return AR_OK;
    }
    ArStateWriter api() { return {this, joint, shape, material, visibility}; }
};

void validateInput(const ArInputFrame* input, const Instance& inst, Diagnostics& d) {
    requireHeader(input, d);
    if (!input->frame_id || input->generation != inst.generation || !std::isfinite(input->evaluation_seconds) ||
        (inst.prior && (input->frame_id <= inst.prior->frame || input->evaluation_seconds < inst.prior->seconds)))
        d.fail(AR_INVALID_ARGUMENT, "runtime.frame.discontinuity", "Invalid frame identity, generation or time; reset before seeking");
    if (input->has_usd_mapping > 1 || (input->has_usd_mapping &&
        (!std::isfinite(input->usd_time_codes_per_second) || input->usd_time_codes_per_second <= 0 ||
         !std::isfinite(input->usd_time_code_offset) || !std::isfinite(input->evaluation_seconds *
            input->usd_time_codes_per_second + input->usd_time_code_offset))))
        d.fail(AR_INVALID_ARGUMENT, "runtime.clock.usd", "Invalid USD clock mapping");
    requireSpan(input->scalars, input->scalar_count, d);
    std::set<std::tuple<std::string, std::string, std::string>> ids;
    for (uint32_t i = 0; i < input->scalar_count; ++i) {
        const auto& s = input->scalars[i]; requireId(s.source_id, d); requireId(s.actor_id, d); requireId(s.channel_id, d);
        const char* separator = std::strchr(s.channel_id, ':');
        if (!separator || separator == s.channel_id || !separator[1])
            d.fail(AR_INVALID_ARGUMENT, "runtime.channel.namespace", "Scalar channel needs an explicit namespace", s.channel_id);
        if (!ids.emplace(s.source_id, s.actor_id, s.channel_id).second)
            d.fail(AR_DUPLICATE_ID, "runtime.input.duplicate", "Duplicate source/actor/channel input", s.channel_id);
        if (!std::isfinite(s.value) || !std::isfinite(s.source_seconds) || !std::isfinite(s.clock_scale) ||
            s.clock_scale <= 0 || !std::isfinite(s.clock_offset) || !std::isfinite(s.source_seconds * s.clock_scale + s.clock_offset))
            d.fail(AR_INVALID_ARGUMENT, "runtime.input.numeric", "Non-finite input or invalid source clock mapping", s.channel_id);
    }
}

struct Transaction {
    Instance& instance;
    std::vector<BoundEvaluator*> begun;
    bool committed = false;
    explicit Transaction(Instance& i) : instance(i) { begun.reserve(i.plan.size()); }
    ~Transaction() {
        if (!committed) for (auto i = begun.rbegin(); i != begun.rend(); ++i) {
            try { CallbackScope scope; (*i)->evaluator->callbacks.end_frame((*i)->evaluator->callbacks.user_data, (*i)->state, 0); }
            catch (...) { instance.poisoned = true; }
        }
    }
    void commit() {
        for (auto* b : begun) {
            try { CallbackScope scope; b->evaluator->callbacks.end_frame(b->evaluator->callbacks.user_data, b->state, 1); }
            catch (...) { instance.poisoned = true; throw Failure{AR_PROVIDER_ERROR}; }
        }
        committed = true;
    }
};

ArStatus AR_CALL createRuntime(ArRuntime* out) {
    if (out) *out = 0;
    return boundary([&]() -> ArStatus {
        if (!out) return AR_INVALID_ARGUMENT;
        std::lock_guard<std::mutex> lock(apiMutex);
        auto r = std::make_unique<Runtime>(); auto h = newHandle(); runtimes.emplace(h, std::move(r)); *out = h;
        return AR_OK;
    });
}
ArStatus AR_CALL destroyRuntime(ArRuntime h) {
    return boundary([&]() -> ArStatus {
        std::lock_guard<std::mutex> lock(apiMutex);
        runtime(h); runtimes.erase(h); return AR_OK;
    });
}
ArStatus AR_CALL registerEvaluator(ArRuntime h, const ArEvaluatorDesc* desc, const ArDiagnosticSink* sink) {
    return boundary([&]() -> ArStatus {
        std::lock_guard<std::mutex> lock(apiMutex); auto& r = runtime(h); Diagnostics d(sink);
        requireHeader(desc, d); requireId(desc->id, d); requireId(desc->provider_id, d); requireId(desc->provider_version, d);
        if (desc->phase < AR_PHASE_SAMPLE || desc->phase > AR_PHASE_APPEARANCE ||
            ((desc->reads | desc->writes) & ~AR_DOMAIN_ALL) || (desc->flags & ~AR_EVALUATOR_STATEFUL) || !desc->evaluate)
            d.fail(AR_INVALID_ARGUMENT, "runtime.evaluator.descriptor", "Invalid phase, domains, flags or evaluate callback", desc->id);
        const bool stateful = (desc->flags & AR_EVALUATOR_STATEFUL) != 0;
        if ((stateful && (!desc->create_state || !desc->destroy_state || !desc->begin_frame || !desc->end_frame)) ||
            (!stateful && (desc->create_state || desc->destroy_state || desc->begin_frame || desc->end_frame)))
            d.fail(AR_INVALID_ARGUMENT, "runtime.lifecycle.callbacks", "Stateful lifecycle must be complete; stateless callbacks must be absent", desc->id);
        if (r.evaluators.count(desc->id)) d.fail(AR_DUPLICATE_ID, "runtime.evaluator.duplicate", "Duplicate evaluator registration", desc->id);
        Evaluator e; e.id = desc->id; e.provider = desc->provider_id; e.version = desc->provider_version; e.callbacks = *desc;
        /* Descriptor arrays and strings are copied. Only callback data remains borrowed. */
        e.callbacks.id = nullptr; e.callbacks.provider_id = nullptr; e.callbacks.provider_version = nullptr;
        e.callbacks.after = nullptr; e.callbacks.supplies = nullptr; e.callbacks.required = nullptr;
        requireSpan(desc->after, desc->after_count, d);
        std::set<std::string> unique;
        for (uint32_t i = 0; i < desc->after_count; ++i) {
            requireId(desc->after[i], d);
            if (!unique.emplace(desc->after[i]).second) d.fail(AR_DUPLICATE_ID, "runtime.dependency.duplicate", "Duplicate predecessor", desc->after[i]);
            e.after.emplace_back(desc->after[i]);
        }
        e.supplies = copyCaps(desc->supplies, desc->supply_count, d);
        e.required = copyCaps(desc->required, desc->require_count, d);
        r.evaluators.emplace(e.id, std::move(e)); return AR_OK;
    });
}
ArStatus AR_CALL createInstance(ArRuntime h, const ArInstanceDesc* desc, const ArDiagnosticSink* sink, ArInstance* out) {
    if (out) *out = 0;
    return boundary([&]() -> ArStatus {
        std::lock_guard<std::mutex> lock(apiMutex); auto& r = runtime(h); Diagnostics d(sink);
        if (!out) return AR_INVALID_ARGUMENT;
        requireHeader(desc, d);
        if (!desc->generation) d.fail(AR_INVALID_ARGUMENT, "runtime.generation.invalid", "Generation must be nonzero");
        requireId(desc->layout_id, d);
        if (!desc->layout_version) d.fail(AR_INVALID_ARGUMENT, "runtime.layout.version", "Layout version must be nonzero", desc->layout_id);
        validateState(desc->initial_state, d);
        Capabilities active; auto ordered = makePlan(r, *desc, d, active);
        auto inst = std::make_unique<Instance>(desc->generation, desc->initial_state);
        inst->baseline.layout = std::make_shared<const Layout>(desc->layout_id, desc->layout_version, std::move(active));
        const auto id = newHandle(); d.instance = id;
        inst->baseline.instance = id; inst->baseline.generation = desc->generation;
        for (auto* e : ordered) inst->plan.push_back({e, nullptr});
        try { initializeStates(inst->plan, id, desc->generation); }
        catch (...) { d.emit(AR_PROVIDER_ERROR, "runtime.lifecycle.create", "Provider state creation failed"); throw; }
        r.instances.emplace(id, std::move(inst)); *out = id; return AR_OK;
    });
}
ArStatus AR_CALL destroyInstance(ArRuntime h, ArInstance id) {
    return boundary([&]() -> ArStatus {
        std::lock_guard<std::mutex> lock(apiMutex); auto& r = runtime(h); instance(r, id); r.instances.erase(id); return AR_OK;
    });
}
ArStatus AR_CALL resetInstance(ArRuntime h, ArInstance id, uint64_t generation) {
    return boundary([&]() -> ArStatus {
        std::lock_guard<std::mutex> lock(apiMutex); auto& inst = instance(runtime(h), id);
        if (generation <= inst.generation) return AR_INVALID_ARGUMENT;
        std::vector<BoundEvaluator> fresh; fresh.reserve(inst.plan.size());
        for (const auto& b : inst.plan) fresh.push_back({b.evaluator, nullptr});
        try { initializeStates(fresh, id, generation); }
        catch (...) { cleanupStates(fresh); throw; }
        cleanupStates(inst.plan); inst.plan = std::move(fresh);
        inst.generation = generation; inst.baseline.generation = generation;
        inst.prior.reset(); inst.poisoned = false; return AR_OK;
    });
}
ArStatus AR_CALL evaluateFrame(ArRuntime h, ArInstance id, const ArInputFrame* input,
                              const ArDiagnosticSink* sink, ArSnapshot* out) {
    if (out) *out = 0;
    return boundary([&]() -> ArStatus {
        std::lock_guard<std::mutex> lock(apiMutex); auto& inst = instance(runtime(h), id); Diagnostics d(sink);
        d.instance = id; if (header(input)) d.frame = input->frame_id;
        if (!out) return AR_INVALID_ARGUMENT;
        if (inst.poisoned) d.fail(AR_INVALID_STATE, "runtime.lifecycle.poisoned", "Provider violated infallible lifecycle; reset required");
        validateInput(input, inst, d);
        State work(inst.baseline); work.frame = input->frame_id; work.seconds = input->evaluation_seconds;
        work.inputRevision = input->input_revision;
        Transaction transaction(inst);
        for (auto& b : inst.plan) {
            auto& e = *b.evaluator; d.evaluator = e.id.c_str(); d.origin = e.provider.c_str(); d.phase = e.callbacks.phase;
            const ArDomain visible = e.callbacks.reads | e.callbacks.writes;
            auto working = work.view(visible);
            ArStateView prior{}; if (inst.prior) prior = inst.prior->view(visible);
            ArEvaluationContext context{AR_HEADER(ArEvaluationContext), input, inst.prior ? &prior : nullptr,
                &working, {&d, Diagnostics::providerEmit}};
            Writer writer{work, e.callbacks.writes}; auto writerApi = writer.api();
            try {
                if (e.callbacks.begin_frame) {
                    transaction.begun.push_back(&b);
                    CallbackScope scope;
                    if (e.callbacks.begin_frame(e.callbacks.user_data, b.state, &context) != AR_OK)
                        d.fail(AR_PROVIDER_ERROR, "runtime.evaluator.begin", "Provider refused frame preparation");
                }
                ArStatus s;
                { CallbackScope scope; s = e.callbacks.evaluate(e.callbacks.user_data, b.state, &context, &writerApi); }
                if (writer.failure != AR_OK)
                    d.fail(writer.failure, "runtime.writer.invalid", "Invalid value, index or undeclared write domain");
                if (d.callbackFailure != AR_OK)
                    d.fail(d.callbackFailure, "runtime.diagnostic.failed", "Diagnostic callback failed or supplied an invalid record");
                if (s != AR_OK) d.fail(AR_PROVIDER_ERROR, "runtime.evaluator.failed", "Evaluator returned failure");
            } catch (const Failure&) { throw; }
            catch (const std::bad_alloc&) { throw; }
            catch (...) { d.fail(AR_PROVIDER_ERROR, "runtime.evaluator.exception", "Provider threw an exception"); }
        }
        d.evaluator = ""; d.origin = "usd-avatar-runtime"; d.phase = 0;
        validateState(work.view(), d);
        auto published = std::make_shared<State>(std::move(work));
        auto snapshotId = newHandle();
        snapshots.emplace(snapshotId, Snapshot{published, 1});
        try { transaction.commit(); }
        catch (...) { snapshots.erase(snapshotId); d.emit(AR_PROVIDER_ERROR, "runtime.lifecycle.commit", "Provider commit failed; reset required"); throw; }
        inst.prior = std::move(published); *out = snapshotId; return AR_OK;
    });
}
ArStatus AR_CALL getSnapshot(ArSnapshot h, ArStateView* out) {
    return boundary([&]() -> ArStatus {
        if (!header(out)) return out ? AR_INCOMPATIBLE_ABI : AR_INVALID_ARGUMENT;
        std::lock_guard<std::mutex> lock(apiMutex); auto i = snapshots.find(h);
        if (i == snapshots.end()) return AR_INVALID_HANDLE;
        *out = i->second.state->view(); return AR_OK;
    });
}
ArStatus AR_CALL retainSnapshot(ArSnapshot h) {
    return boundary([&]() -> ArStatus {
        std::lock_guard<std::mutex> lock(apiMutex); auto i = snapshots.find(h);
        if (i == snapshots.end()) return AR_INVALID_HANDLE;
        if (i->second.references == std::numeric_limits<uint64_t>::max()) return AR_OUT_OF_MEMORY;
        ++i->second.references; return AR_OK;
    });
}
ArStatus AR_CALL releaseSnapshot(ArSnapshot h) {
    return boundary([&]() -> ArStatus {
        std::lock_guard<std::mutex> lock(apiMutex); auto i = snapshots.find(h);
        if (i == snapshots.end()) return AR_INVALID_HANDLE;
        if (!--i->second.references) snapshots.erase(i);
        return AR_OK;
    });
}
ArStatus AR_CALL getCapabilities(ArRuntime h, ArInstance id, const ArCapability** out, uint32_t* count) {
    if (out) *out = nullptr;
    if (count) *count = 0;
    return boundary([&]() -> ArStatus {
        if (!out || !count) return AR_INVALID_ARGUMENT;
        std::lock_guard<std::mutex> lock(apiMutex); auto& inst = instance(runtime(h), id);
        const auto& capabilities = inst.baseline.layout->capabilities;
        *out = capabilities.data(); *count = uint32_t(capabilities.size()); return AR_OK;
    });
}
} // namespace

extern "C" AR_EXPORT ArStatus AR_CALL arGetApi(uint32_t version, uint32_t size, ArRuntimeApi* out) {
    if (!out) return AR_INVALID_ARGUMENT;
    if (version != AR_ABI_VERSION || size < sizeof(ArRuntimeApi)) return AR_INCOMPATIBLE_ABI;
    const ArRuntimeApi api{AR_HEADER(ArRuntimeApi), createRuntime, destroyRuntime, registerEvaluator,
        createInstance, destroyInstance, resetInstance, evaluateFrame, getSnapshot,
        retainSnapshot, releaseSnapshot, getCapabilities};
    *out = api; return AR_OK;
}
