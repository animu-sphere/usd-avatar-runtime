#include "avatarRuntime/physics.h"
#include "avatarRuntime/FixedTickSession.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <deque>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
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
bool namespaced(const char* channel) {
    const char* separator = std::strchr(channel, ':');
    return separator && separator != channel && separator[1];
}
bool clockValid(double seconds, double scale, double offset) {
    return std::isfinite(seconds) && std::isfinite(scale) && scale > 0 && std::isfinite(offset) &&
        std::isfinite(seconds * scale + offset);
}
bool materialValid(const ArMaterialInput& m) {
    if (m.value_type != AR_VALUE_SCALAR && m.value_type != AR_VALUE_VEC2 &&
        m.value_type != AR_VALUE_VEC3 && m.value_type != AR_VALUE_VEC4) return false;
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
    std::shared_ptr<const std::vector<std::string>> strings;
    std::vector<ArJoint> joints;
    std::vector<ArBlendShape> shapes;
    std::vector<ArVertexUv> uvs;
    std::vector<ArMaterialInput> materials;
    std::vector<ArVisibility> visibility;
    /* Appended during evaluation; deque keeps copied identities stable. */
    std::deque<std::string> sampleStrings;
    std::vector<ArSourceSample> samples;

    explicit State(const ArStateView& v) : instance(v.instance), frame(v.frame_id),
        generation(v.generation), seconds(v.evaluation_seconds) {
        auto owned = std::make_shared<std::vector<std::string>>();
        owned->reserve(size_t(v.joint_count) * 2 + size_t(v.blend_shape_count) * 2 +
            size_t(v.material_count) * 2 + v.visibility_count + v.vertex_uv_count);
        auto copyId = [&](const char* p) { owned->emplace_back(p); return owned->back().c_str(); };
        for (uint32_t i = 0; i < v.joint_count; ++i) {
            auto j = v.joints[i]; j.skeleton_id = copyId(j.skeleton_id); j.joint_id = copyId(j.joint_id); joints.push_back(j);
        }
        for (uint32_t i = 0; i < v.blend_shape_count; ++i) {
            auto s = v.blend_shapes[i]; s.mesh_id = copyId(s.mesh_id); s.target_id = copyId(s.target_id); shapes.push_back(s);
        }
        for (uint32_t i = 0; i < v.material_count; ++i) {
            auto m = v.materials[i]; m.material_id = copyId(m.material_id); m.input_id = copyId(m.input_id); materials.push_back(m);
        }
        for (uint32_t i = 0; i < v.vertex_uv_count; ++i) {
            auto uv = v.vertex_uvs[i]; uv.mesh_id = copyId(uv.mesh_id); uvs.push_back(uv);
        }
        for (uint32_t i = 0; i < v.visibility_count; ++i) {
            auto x = v.visibility[i]; x.target_id = copyId(x.target_id); visibility.push_back(x);
        }
        strings = std::move(owned);
    }
    // Layout identities are immutable. Value arrays remain independently owned;
    // snapshots share only the string storage that their opaque IDs refer to.
    State(const State& other) : instance(other.instance), frame(other.frame), generation(other.generation),
        seconds(other.seconds), inputRevision(other.inputRevision), layout(other.layout), strings(other.strings),
        joints(other.joints), shapes(other.shapes), uvs(other.uvs), materials(other.materials), visibility(other.visibility) {
        for (const auto& s : other.samples) addSample(s, s.evaluator_id);
    }
    State(State&&) = default;
    State& operator=(const State&) = delete;
    /* Identities are copied; runtime seconds are stamped from the clock mapping. */
    void addSample(ArSourceSample s, const char* evaluator) {
        auto copyId = [&](const char* p) { sampleStrings.emplace_back(p); return sampleStrings.back().c_str(); };
        s.source_id = copyId(s.source_id); s.actor_id = copyId(s.actor_id); s.channel_id = copyId(s.channel_id);
        s.evaluator_id = *evaluator ? copyId(evaluator) : "";
        s.runtime_seconds = s.source_seconds * s.clock_scale + s.clock_offset;
        samples.push_back(s);
    }
    bool hasSample(const char* source, const char* actor, const char* channel) const {
        for (const auto& s : samples)
            if (!std::strcmp(s.source_id, source) && !std::strcmp(s.actor_id, actor) && !std::strcmp(s.channel_id, channel))
                return true;
        return false;
    }
    /* Working views omit samples: provider reports may append during a callback. */
    ArStateView view(ArDomain domains = AR_DOMAIN_ALL, bool provenance = true) const {
        ArStateView v{AR_HEADER(ArStateView)};
        v.instance = instance; v.frame_id = frame; v.generation = generation; v.evaluation_seconds = seconds;
        v.input_revision = inputRevision;
        if (layout) {
            v.layout_id = layout->id.c_str(); v.layout_version = layout->version;
            v.capabilities = layout->capabilities.data(); v.capability_count = uint32_t(layout->capabilities.size());
        }
        if (domains & AR_DOMAIN_POSE) { v.joints = joints.data(); v.joint_count = uint32_t(joints.size()); }
        if (domains & AR_DOMAIN_DEFORMATION) { v.blend_shapes = shapes.data(); v.blend_shape_count = uint32_t(shapes.size()); }
        if (domains & AR_DOMAIN_DEFORMATION) { v.vertex_uvs = uvs.data(); v.vertex_uv_count = uint32_t(uvs.size()); }
        if (domains & AR_DOMAIN_MATERIAL) { v.materials = materials.data(); v.material_count = uint32_t(materials.size()); }
        if (domains & AR_DOMAIN_VISIBILITY) { v.visibility = visibility.data(); v.visibility_count = uint32_t(visibility.size()); }
        if (provenance) { v.samples = samples.data(); v.sample_count = uint32_t(samples.size()); }
        return v;
    }
};

void validateState(const ArStateView& v, Diagnostics& d) {
    requireHeader(&v, d);
    requireSpan(v.joints, v.joint_count, d); requireSpan(v.blend_shapes, v.blend_shape_count, d);
    requireSpan(v.materials, v.material_count, d); requireSpan(v.visibility, v.visibility_count, d);
    requireSpan(v.vertex_uvs, v.vertex_uv_count, d);
    // Dense source layouts normally arrive in identity order. Keep all
    // duplicate checks without allocation for ordered IDs; unordered IDs use
    // one contiguous allocation and no per-vertex string copies.
    using UvIdentity = std::pair<std::string_view, uint32_t>;
    bool orderedUvs = true;
    UvIdentity previousUv;
    for (uint32_t i = 0; i < v.vertex_uv_count; ++i) {
        const auto& uv = v.vertex_uvs[i]; requireId(uv.mesh_id, d);
        const UvIdentity identity{uv.mesh_id, uv.vertex_index};
        if (i && !(previousUv < identity)) orderedUvs = false;
        previousUv = identity;
        for (double x : uv.value) if (!std::isfinite(x))
            d.fail(AR_INVALID_STATE, "runtime.state.uv", "Non-finite vertex UV", uv.mesh_id);
    }
    if (!orderedUvs) {
        std::vector<UvIdentity> uvIds;
        uvIds.reserve(v.vertex_uv_count);
        for (uint32_t i = 0; i < v.vertex_uv_count; ++i)
            uvIds.emplace_back(v.vertex_uvs[i].mesh_id, v.vertex_uvs[i].vertex_index);
        std::sort(uvIds.begin(), uvIds.end());
        if (const auto duplicate = std::adjacent_find(uvIds.begin(), uvIds.end()); duplicate != uvIds.end())
            d.fail(AR_DUPLICATE_ID, "runtime.layout.duplicate", "Duplicate vertex UV identity", duplicate->first.data());
    }
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
    std::set<std::pair<std::string, std::string>> jointIds;
    std::vector<BoundEvaluator> plan;
    std::shared_ptr<const State> prior;
    bool poisoned = false;
    ArPhysicsWorld world = 0;
    const void* fixedSession = nullptr;
    Instance(uint64_t g, const ArStateView& v) : generation(g), baseline(v) {
        for (const auto& j : baseline.joints) jointIds.emplace(j.skeleton_id, j.joint_id);
    }
    ~Instance() { cleanupStates(plan); }
};
struct PhysicsWorld {
    struct Member { ArInstance id; size_t split; };
    std::string id;
    std::vector<Member> members;
    double start = 0, seconds = 0, dt = 0, pending = 0;
    uint32_t maxTicks = 0;
    uint64_t tick = 0, lastRequest = 0;
    bool poisoned = false;
    void* user = nullptr;
    ArStatus (AR_CALL *step)(void*, const ArPhysicsTick*, const ArDiagnosticSink*) = nullptr;
    ArStatus (AR_CALL *reset)(void*, const ArDiagnosticSink*) = nullptr;
};
struct Runtime {
    /* Instances must be destroyed before descriptor code/metadata. */
    std::map<std::string, Evaluator> evaluators;
    std::map<ArInstance, std::unique_ptr<Instance>> instances;
    std::map<ArPhysicsWorld, PhysicsWorld> worlds;
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
    const char* evaluator;
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
    static ArStatus AR_CALL sample(void* ptr, const ArSourceSample* s) {
        auto& w = *static_cast<Writer*>(ptr);
        if (!(w.domains & AR_DOMAIN_POSE) || !s || s->kind != AR_SOURCE_POSE ||
            !identifier(s->source_id) || !identifier(s->actor_id) || !identifier(s->channel_id) ||
            !namespaced(s->channel_id) || s->validity != AR_OBSERVATION_VALID ||
            s->resolution < AR_SAMPLE_INTERPOLATED || s->resolution > AR_SAMPLE_EXTRAPOLATED ||
            !clockValid(s->source_seconds, s->clock_scale, s->clock_offset) || w.state.samples.size() >= 1048576u)
            return w.reject(AR_INVALID_ARGUMENT);
        if (w.state.hasSample(s->source_id, s->actor_id, s->channel_id)) return w.reject(AR_DUPLICATE_ID);
        try { w.state.addSample(*s, w.evaluator); }
        catch (...) { return w.reject(AR_OUT_OF_MEMORY); }
        return AR_OK;
    }
    static ArStatus AR_CALL uv(void* ptr, uint32_t n, const double* v) {
        auto& w = *static_cast<Writer*>(ptr);
        if (!(w.domains & AR_DOMAIN_DEFORMATION) || n >= w.state.uvs.size() || !v)
            return w.reject(AR_INVALID_ARGUMENT);
        if (!std::isfinite(v[0]) || !std::isfinite(v[1])) return w.reject(AR_INVALID_STATE);
        std::copy(v, v + 2, w.state.uvs[n].value); return AR_OK;
    }
    ArStateWriter api() { return {this, joint, shape, material, visibility, sample, uv}; }
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
    requireSpan(input->gazes, input->gaze_count, d);
    std::set<std::tuple<std::string, std::string, std::string>> ids;
    const auto observation = [&](const char* source, const char* actor, const char* channel,
                                 double seconds, double scale, double offset) {
        requireId(source, d); requireId(actor, d); requireId(channel, d);
        if (!namespaced(channel))
            d.fail(AR_INVALID_ARGUMENT, "runtime.channel.namespace", "Input channel needs an explicit namespace", channel);
        if (!ids.emplace(source, actor, channel).second)
            d.fail(AR_DUPLICATE_ID, "runtime.input.duplicate", "Duplicate source/actor/channel input", channel);
        if (!clockValid(seconds, scale, offset))
            d.fail(AR_INVALID_ARGUMENT, "runtime.input.numeric", "Invalid source clock mapping", channel);
    };
    for (uint32_t i = 0; i < input->scalar_count; ++i) {
        const auto& s = input->scalars[i];
        observation(s.source_id, s.actor_id, s.channel_id, s.source_seconds, s.clock_scale, s.clock_offset);
        if (!std::isfinite(s.value))
            d.fail(AR_INVALID_ARGUMENT, "runtime.input.numeric", "Non-finite scalar input", s.channel_id);
    }
    for (uint32_t i = 0; i < input->gaze_count; ++i) {
        const auto& g = input->gazes[i];
        observation(g.source_id, g.actor_id, g.channel_id, g.source_seconds, g.clock_scale, g.clock_offset);
        if (g.kind != AR_GAZE_POINT && g.kind != AR_GAZE_DIRECTION)
            d.fail(AR_INVALID_ARGUMENT, "runtime.gaze.kind", "Unknown gaze point/direction kind", g.channel_id);
        if (g.validity != AR_OBSERVATION_VALID && g.validity != AR_OBSERVATION_UNAVAILABLE && g.validity != AR_OBSERVATION_STALE)
            d.fail(AR_INVALID_ARGUMENT, "runtime.gaze.validity", "Unknown gaze observation validity", g.channel_id);
        if (g.space == AR_GAZE_RUNTIME_WORLD) {
            if (g.skeleton_id || g.joint_id)
                d.fail(AR_INVALID_ARGUMENT, "runtime.gaze.reference", "World gaze must not name a joint reference", g.channel_id);
        } else if (g.space == AR_GAZE_JOINT_LOCAL) {
            requireId(g.skeleton_id, d); requireId(g.joint_id, d);
            if (!inst.jointIds.count({g.skeleton_id, g.joint_id}))
                d.fail(AR_INVALID_ARGUMENT, "runtime.gaze.reference", "Gaze reference is not a bound rig joint", g.channel_id);
        } else {
            d.fail(AR_INVALID_ARGUMENT, "runtime.gaze.space", "Unknown gaze coordinate space", g.channel_id);
        }
        double norm = 0;
        for (double v : g.value) {
            if (!std::isfinite(v))
                d.fail(AR_INVALID_ARGUMENT, "runtime.gaze.numeric", "Non-finite gaze value", g.channel_id);
            if (g.validity == AR_OBSERVATION_UNAVAILABLE && v != 0)
                d.fail(AR_INVALID_ARGUMENT, "runtime.gaze.unavailable", "Unavailable gaze must have a zero payload", g.channel_id);
            if (g.kind == AR_GAZE_DIRECTION) norm += v * v;
        }
        if (g.kind == AR_GAZE_DIRECTION && g.validity != AR_OBSERVATION_UNAVAILABLE &&
            (!std::isfinite(norm) || std::abs(norm - 1.0) > 1e-6))
            d.fail(AR_INVALID_ARGUMENT, "runtime.gaze.direction", "Gaze direction must be a unit vector", g.channel_id);
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
        std::lock_guard<std::mutex> lock(apiMutex); auto& r = runtime(h);
        if (instance(r, id).world || instance(r, id).fixedSession) return AR_INVALID_STATE;
        r.instances.erase(id); return AR_OK;
    });
}
ArStatus AR_CALL resetInstance(ArRuntime h, ArInstance id, uint64_t generation) {
    return boundary([&]() -> ArStatus {
        std::lock_guard<std::mutex> lock(apiMutex); auto& inst = instance(runtime(h), id);
        if (inst.world || inst.fixedSession) return AR_INVALID_STATE;
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
void prepareWork(State& work, const ArInputFrame& input) {
    work.frame = input.frame_id; work.seconds = input.evaluation_seconds;
    work.inputRevision = input.input_revision;
    work.samples.reserve(size_t(input.scalar_count) + input.gaze_count);
    for (uint32_t i = 0; i < input.scalar_count; ++i) {
        const auto& s = input.scalars[i];
        work.addSample({s.source_id, s.actor_id, s.channel_id, "", AR_SOURCE_SCALAR, AR_OBSERVATION_VALID,
            AR_SAMPLE_SELECTED, s.source_seconds, s.clock_scale, s.clock_offset, 0}, "");
    }
    for (uint32_t i = 0; i < input.gaze_count; ++i) {
        const auto& g = input.gazes[i];
        work.addSample({g.source_id, g.actor_id, g.channel_id, "", AR_SOURCE_GAZE, g.validity,
            AR_SAMPLE_SELECTED, g.source_seconds, g.clock_scale, g.clock_offset, 0}, "");
    }
}

void runEvaluators(Instance& inst, const ArInputFrame& input, State& work,
    Transaction& transaction, size_t first, size_t last, Diagnostics& d) {
    d.instance = work.instance; d.frame = work.frame;
    for (size_t n = first; n < last; ++n) {
        auto& b = inst.plan[n];
        auto& e = *b.evaluator; d.evaluator = e.id.c_str(); d.origin = e.provider.c_str(); d.phase = e.callbacks.phase;
        const ArDomain visible = e.callbacks.reads | e.callbacks.writes;
        auto working = work.view(visible, false);
        ArStateView prior{}; if (inst.prior) prior = inst.prior->view(visible);
        ArEvaluationContext context{AR_HEADER(ArEvaluationContext), &input, inst.prior ? &prior : nullptr,
            &working, {&d, Diagnostics::providerEmit}};
        Writer writer{work, e.callbacks.writes, e.id.c_str()}; auto writerApi = writer.api();
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
}

ArStatus evaluateInstance(Instance& inst, const ArInputFrame* input, Diagnostics& d, ArSnapshot* out) {
        if (inst.poisoned) d.fail(AR_INVALID_STATE, "runtime.lifecycle.poisoned", "Provider violated infallible lifecycle; reset required");
        validateInput(input, inst, d);
        State work(inst.baseline); prepareWork(work, *input);
        Transaction transaction(inst);
        runEvaluators(inst, *input, work, transaction, 0, inst.plan.size(), d);
        d.evaluator = ""; d.origin = "usd-avatar-runtime"; d.phase = 0;
        validateState(work.view(), d);
        auto published = std::make_shared<State>(std::move(work));
        auto snapshotId = newHandle();
        snapshots.emplace(snapshotId, Snapshot{published, 1});
        try { transaction.commit(); }
        catch (...) { snapshots.erase(snapshotId); d.emit(AR_PROVIDER_ERROR, "runtime.lifecycle.commit", "Provider commit failed; reset required"); throw; }
        inst.prior = std::move(published); *out = snapshotId; return AR_OK;
}
ArStatus AR_CALL evaluateFrame(ArRuntime h, ArInstance id, const ArInputFrame* input,
                              const ArDiagnosticSink* sink, ArSnapshot* out) {
    if (out) *out = 0;
    return boundary([&]() -> ArStatus {
        std::lock_guard<std::mutex> lock(apiMutex); auto& inst = instance(runtime(h), id); Diagnostics d(sink);
        d.instance = id; if (header(input)) d.frame = input->frame_id;
        if (!out) return AR_INVALID_ARGUMENT;
        if (inst.world || inst.fixedSession)
            d.fail(AR_INVALID_STATE, "runtime.clock.managed", "Managed instances must use their runtime clock");
        return evaluateInstance(inst, input, d, out);
    });
}
/* Shared-world scheduling reuses the same private plans, transactions and
   snapshot storage, without a provider/backend dependency. */
PhysicsWorld& physicsWorld(Runtime& r, ArPhysicsWorld id) {
    auto found = r.worlds.find(id);
    if (found == r.worlds.end()) throw Failure{AR_INVALID_HANDLE};
    return found->second;
}

struct PendingFrame {
    Instance& inst;
    ArInputFrame input;
    State work;
    Transaction transaction;
    PendingFrame(Instance& i, ArInputFrame f, Diagnostics& d)
        : inst(i), input(f), work(i.baseline), transaction(i) {
        validateInput(&input, inst, d);
        prepareWork(work, input);
    }
};

ArStatus AR_CALL createPhysicsWorld(ArRuntime h, const ArPhysicsWorldDesc* desc,
    const ArDiagnosticSink* sink, ArPhysicsWorld* out) {
    if (out) *out = 0;
    return boundary([&]() -> ArStatus {
        std::lock_guard<std::mutex> lock(apiMutex);
        auto& r = runtime(h); Diagnostics d(sink);
        if (!out) return AR_INVALID_ARGUMENT;
        requireHeader(desc, d); requireId(desc->id, d);
        requireSpan(desc->participants, desc->participant_count, d);
        if (!std::isfinite(desc->start_seconds) || !std::isfinite(desc->fixed_step_seconds) ||
            desc->fixed_step_seconds <= 0 || !desc->max_catch_up_ticks ||
            desc->max_catch_up_ticks > 1048576u || !desc->step || !desc->reset)
            d.fail(AR_INVALID_ARGUMENT, "runtime.physics.config", "Invalid fixed clock or world callbacks");
        for (const auto& w : r.worlds) if (w.second.id == desc->id)
            d.fail(AR_DUPLICATE_ID, "runtime.physics.duplicate", "World identity already bound", desc->id);
        PhysicsWorld world;
        world.id = desc->id; world.start = world.seconds = desc->start_seconds;
        world.dt = desc->fixed_step_seconds; world.maxTicks = desc->max_catch_up_ticks;
        world.user = desc->user_data; world.step = desc->step; world.reset = desc->reset;
        std::set<ArInstance> unique;
        for (uint32_t n = 0; n < desc->participant_count; ++n) {
            const auto& member = desc->participants[n]; auto& inst = instance(r, member.instance);
            if (inst.world || inst.fixedSession || inst.prior || inst.poisoned)
                d.fail(AR_INVALID_STATE, "runtime.physics.membership", "Participant requires an unbound, reset instance");
            if (!unique.emplace(member.instance).second)
                d.fail(AR_DUPLICATE_ID, "runtime.physics.participant", "Duplicate world participant");
            requireId(member.post_physics_evaluator, d);
            size_t split = 0;
            while (split < inst.plan.size() && inst.plan[split].evaluator->id != member.post_physics_evaluator) ++split;
            if (split == inst.plan.size())
                d.fail(AR_MISSING_DEPENDENCY, "runtime.physics.boundary", "Post-physics evaluator is absent from participant plan", member.post_physics_evaluator);
            world.members.push_back({member.instance, split});
        }
        std::sort(world.members.begin(), world.members.end(), [](const auto& a, const auto& b) { return a.id < b.id; });
        const auto id = newHandle(); r.worlds.emplace(id, std::move(world));
        for (const auto& member : r.worlds.at(id).members) instance(r, member.id).world = id;
        *out = id; return AR_OK;
    });
}

ArStatus AR_CALL destroyPhysicsWorld(ArRuntime h, ArPhysicsWorld id) {
    return boundary([&]() -> ArStatus {
        std::lock_guard<std::mutex> lock(apiMutex); auto& r = runtime(h); auto& world = physicsWorld(r, id);
        /* Unbinding is a discontinuity. Direct evaluation requires explicit
           instance reset; retained snapshots remain independent values. */
        for (const auto& member : world.members) {
            auto& inst = instance(r, member.id); inst.world = 0; inst.poisoned = true;
        }
        r.worlds.erase(id); return AR_OK;
    });
}

ArStatus AR_CALL removePhysicsParticipant(ArRuntime h, ArPhysicsWorld id, ArInstance member) {
    return boundary([&]() -> ArStatus {
        std::lock_guard<std::mutex> lock(apiMutex);
        auto& r = runtime(h); auto& world = physicsWorld(r, id);
        auto& inst = instance(r, member);
        if (inst.world != id) return AR_INVALID_ARGUMENT;
        // Removing a failed participant must not bypass group recovery.
        if (world.poisoned) return AR_INVALID_STATE;
        for (const auto& item : world.members)
            if (instance(r, item.id).poisoned) return AR_INVALID_STATE;
        const auto found = std::find_if(world.members.begin(), world.members.end(),
            [member](const auto& item) { return item.id == member; });
        if (found == world.members.end()) return AR_INVALID_ARGUMENT;
        world.members.erase(found);
        inst.world = 0; inst.poisoned = true;
        return AR_OK;
    });
}

void worldCallback(PhysicsWorld& world, const ArPhysicsTick* tick, Diagnostics& d) {
    d.instance = 0; d.frame = tick ? tick->tick_id : 0;
    d.evaluator = world.id.c_str(); d.origin = "usd-avatar-runtime"; d.phase = 0;
    const ArDiagnosticSink forwarding{&d, Diagnostics::providerEmit};
    ArStatus status = AR_PROVIDER_ERROR;
    try {
        CallbackScope scope;
        status = tick ? world.step(world.user, tick, &forwarding) : world.reset(world.user, &forwarding);
    } catch (...) {
        d.fail(AR_PROVIDER_ERROR, "runtime.physics.exception", "World callback threw; reset required", world.id.c_str());
    }
    if (status != AR_OK || d.callbackFailure != AR_OK)
        d.fail(AR_PROVIDER_ERROR, "runtime.physics.callback", "World callback failed; reset required", world.id.c_str());
}

void poisonWorld(Runtime& r, PhysicsWorld& world) {
    world.poisoned = true;
    for (const auto& member : world.members) instance(r, member.id).poisoned = true;
}

ArStatus AR_CALL resetPhysicsWorld(ArRuntime h, ArPhysicsWorld id, uint64_t generation,
    const ArDiagnosticSink* sink) {
    return boundary([&]() -> ArStatus {
        std::lock_guard<std::mutex> lock(apiMutex); auto& r = runtime(h); auto& world = physicsWorld(r, id);
        Diagnostics d(sink);
        for (const auto& member : world.members) if (generation <= instance(r, member.id).generation)
            d.fail(AR_INVALID_ARGUMENT, "runtime.physics.generation", "World reset must increase every participant generation");
        std::vector<std::unique_ptr<Instance>> fresh;
        for (const auto& member : world.members) {
            const auto& old = instance(r, member.id);
            auto next = std::make_unique<Instance>(generation, old.baseline.view());
            next->world = id; next->baseline.layout = old.baseline.layout;
            next->baseline.instance = member.id; next->baseline.generation = generation;
            for (const auto& b : old.plan) next->plan.push_back({b.evaluator, nullptr});
            initializeStates(next->plan, member.id, generation);
            fresh.push_back(std::move(next));
        }
        try { worldCallback(world, nullptr, d); }
        catch (...) { poisonWorld(r, world); throw; }
        for (size_t n = 0; n < world.members.size(); ++n)
            r.instances.at(world.members[n].id).swap(fresh[n]);
        world.tick = world.lastRequest = 0; world.seconds = world.start;
        world.pending = 0; world.poisoned = false;
        return AR_OK;
    });
}

ArStatus AR_CALL advancePhysicsWorld(ArRuntime h, ArPhysicsWorld id,
    const ArPhysicsAdvanceDesc* desc, const ArDiagnosticSink* sink,
    ArPhysicsAdvanceResult* out, ArSnapshot* output) {
    /* Only touch count/slots after descriptor header/span validation. */
    if (header(desc) && span(desc->inputs, desc->input_count) && output)
        std::fill(output, output + desc->input_count, ArSnapshot{0});
    return boundary([&]() -> ArStatus {
        std::lock_guard<std::mutex> lock(apiMutex); auto& r = runtime(h); auto& world = physicsWorld(r, id);
        Diagnostics d(sink); requireHeader(desc, d); requireHeader(out, d);
        requireSpan(desc->inputs, desc->input_count, d);
        if (desc->input_count != world.members.size() || (desc->input_count && !output) ||
            !desc->request_id || desc->request_id <= world.lastRequest || desc->paused > 1 ||
            !std::isfinite(desc->elapsed_seconds) || desc->elapsed_seconds < 0)
            d.fail(AR_INVALID_ARGUMENT, "runtime.physics.request", "Invalid request identity, elapsed time or participant count");
        if (world.poisoned)
            d.fail(AR_INVALID_STATE, "runtime.physics.poisoned", "Shared world requires reset");
        std::map<ArInstance, const ArInputFrame*> inputs;
        for (uint32_t n = 0; n < desc->input_count; ++n) {
            const auto& item = desc->inputs[n];
            auto& inst = instance(r, item.instance);
            if (inst.world != id || !inputs.emplace(item.instance, item.input).second)
                d.fail(AR_INVALID_ARGUMENT, "runtime.physics.inputs", "Inputs must cover world membership exactly once");
            if (inst.poisoned) {
                poisonWorld(r, world);
                d.fail(AR_INVALID_STATE, "runtime.physics.poisoned", "Participant lifecycle requires shared-world reset");
            }
            requireHeader(item.input, d);
        }
        const auto nextInput = [&](ArInstance member, uint64_t tick, double seconds) {
            ArInputFrame input = *inputs.at(member);
            input.frame_id = tick; input.generation = instance(r, member).generation; input.evaluation_seconds = seconds;
            return input;
        };
        // Validate observations before accepting any elapsed time or callbacks.
        for (const auto& member : world.members) {
            auto input = nextInput(member.id, world.tick + 1, world.seconds);
            d.instance = member.id; d.frame = input.frame_id; validateInput(&input, instance(r, member.id), d);
        }
        const double pending = world.pending + (desc->paused ? 0 : desc->elapsed_seconds);
        if (!std::isfinite(pending))
            d.fail(AR_INVALID_ARGUMENT, "runtime.physics.clock", "Elapsed time overflow");
        world.pending = pending; world.lastRequest = desc->request_id;
        uint32_t completed = 0;
        // Allow only timestep-relative floating-point subtraction roundoff.
        const double clockTolerance = world.dt * (8 * std::numeric_limits<double>::epsilon());
        while (!desc->paused && completed < world.maxTicks &&
            (world.pending >= world.dt || world.dt - world.pending <= clockTolerance)) {
            const auto tick = world.tick + 1;
            const double seconds = world.start + double(tick) * world.dt;
            if (!tick || !std::isfinite(seconds) || seconds <= world.seconds)
                d.fail(AR_INVALID_STATE, "runtime.physics.clock", "Fixed clock exhausted; reset required");
            std::vector<std::unique_ptr<PendingFrame>> frames;
            for (const auto& member : world.members) {
                d.instance = member.id; d.frame = tick;
                frames.push_back(std::make_unique<PendingFrame>(instance(r, member.id), nextInput(member.id, tick, seconds), d));
            }
            bool stepped = false;
            try {
                for (size_t n = 0; n < frames.size(); ++n) {
                    auto& f = *frames[n];
                    runEvaluators(f.inst, f.input, f.work, f.transaction, 0, world.members[n].split, d);
                }
                stepped = true; // Even a failed Step may already have mutated the solver.
                const ArPhysicsTick context{tick, seconds, world.dt}; worldCallback(world, &context, d);
                std::vector<std::shared_ptr<const State>> published;
                for (size_t n = 0; n < frames.size(); ++n) {
                    auto& f = *frames[n];
                    runEvaluators(f.inst, f.input, f.work, f.transaction, world.members[n].split, f.inst.plan.size(), d);
                    d.evaluator = ""; d.origin = "usd-avatar-runtime"; d.phase = 0;
                    validateState(f.work.view(), d);
                    published.push_back(std::make_shared<State>(std::move(f.work)));
                }
                for (auto& f : frames) f->transaction.commit();
                for (size_t n = 0; n < frames.size(); ++n) frames[n]->inst.prior = std::move(published[n]);
            } catch (...) {
                if (stepped) {
                    poisonWorld(r, world);
                    d.emit(AR_INVALID_STATE, "runtime.physics.invalidated", "Failure after Step began; reset the entire shared world");
                }
                throw;
            }
            world.tick = tick; world.seconds = seconds;
            world.pending = std::max(0.0, world.pending - world.dt); ++completed;
        }
        std::vector<ArSnapshot> retained;
        retained.reserve(desc->input_count);
        try {
            for (uint32_t n = 0; n < desc->input_count; ++n) {
                const auto& prior = instance(r, desc->inputs[n].instance).prior;
                ArSnapshot handle = 0;
                if (prior) { handle = newHandle(); snapshots.emplace(handle, Snapshot{prior, 1}); }
                retained.push_back(handle);
            }
        } catch (...) { for (auto handle : retained) snapshots.erase(handle); throw; }
        for (size_t n = 0; n < retained.size(); ++n) output[n] = retained[n];
        *out = {AR_HEADER(ArPhysicsAdvanceResult), completed, world.tick, world.seconds, world.pending};
        return AR_OK;
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

namespace avatarRuntime {
struct FixedTickSession::Impl {
    ArRuntime runtime;
    ArInstance instance;
    FixedTickConfig config;
    FixedTickStats stats;
    uint64_t request = 0;
    ArSnapshot snapshot = 0;
    void release() {
        if (snapshot) {
            auto found = snapshots.find(snapshot);
            if (found != snapshots.end() && !--found->second.references) snapshots.erase(found);
            snapshot = 0;
        }
    }
};
FixedTickSession::FixedTickSession(ArRuntime h, ArInstance id, FixedTickConfig config)
    : impl_(std::make_unique<Impl>()) {
    const auto status = boundary([&]() -> ArStatus {
        std::lock_guard<std::mutex> lock(apiMutex);
        auto& inst = instance(runtime(h), id);
        if (!std::isfinite(config.startSeconds) || !std::isfinite(config.fixedStepSeconds) ||
            config.fixedStepSeconds <= 0 || !config.maxCatchUpTicks ||
            !std::isfinite(config.startSeconds + config.fixedStepSeconds) ||
            config.startSeconds + config.fixedStepSeconds <= config.startSeconds)
            return AR_INVALID_ARGUMENT;
        if (inst.world || inst.fixedSession || inst.prior || inst.poisoned) return AR_INVALID_STATE;
        impl_->runtime = h; impl_->instance = id; impl_->config = config;
        impl_->stats.simulationSeconds = config.startSeconds;
        inst.fixedSession = impl_.get();
        return AR_OK;
    });
    if (status != AR_OK) throw std::invalid_argument("FixedTickSession binding failed: " + std::to_string(status));
}
FixedTickSession::~FixedTickSession() { Close(); }
ArStatus FixedTickSession::Advance(uint64_t requestId, double elapsed, bool paused,
    const ArInputFrame& observation, const ArDiagnosticSink* sink) {
    return boundary([&]() -> ArStatus {
        std::lock_guard<std::mutex> lock(apiMutex);
        auto& s = *impl_;
        if (!s.instance) return AR_INVALID_STATE;
        auto& inst = instance(runtime(s.runtime), s.instance);
        Diagnostics d(sink); d.instance = s.instance;
        if (inst.fixedSession != impl_.get() || inst.poisoned) return AR_INVALID_STATE;
        if (!requestId || requestId <= s.request || !std::isfinite(elapsed) || elapsed < 0)
            return AR_INVALID_ARGUMENT;
        requireHeader(&observation, d);
        ArInputFrame input = observation;
        input.frame_id = s.stats.tick + (s.stats.initialized ? 2 : 1);
        input.generation = inst.generation;
        input.evaluation_seconds = s.stats.simulationSeconds;
        validateInput(&input, inst, d); // also on zero-tick/paused requests
        const double pending = s.stats.pendingSeconds + (paused ? 0 : elapsed);
        if (!std::isfinite(pending)) return AR_INVALID_ARGUMENT;
        s.request = requestId; s.stats.pendingSeconds = pending; s.stats.tickCount = 0;
        const auto commit = [&](uint64_t frame, double seconds) {
            input.frame_id = frame; input.evaluation_seconds = seconds; d.frame = frame;
            ArSnapshot next = 0;
            evaluateInstance(inst, &input, d, &next);
            s.release(); s.snapshot = next;
        };
        if (!s.stats.initialized) {
            commit(1, s.config.startSeconds);
            s.stats.initialized = true;
        }
        const double tolerance = s.config.fixedStepSeconds * (8 * std::numeric_limits<double>::epsilon());
        while (!paused && s.stats.tickCount < s.config.maxCatchUpTicks &&
            (s.stats.pendingSeconds >= s.config.fixedStepSeconds ||
             s.config.fixedStepSeconds - s.stats.pendingSeconds <= tolerance)) {
            const auto tick = s.stats.tick + 1;
            const double seconds = s.config.startSeconds + double(tick) * s.config.fixedStepSeconds;
            if (tick >= std::numeric_limits<uint64_t>::max() - 1 ||
                !std::isfinite(seconds) || seconds <= s.stats.simulationSeconds)
                return AR_INVALID_STATE;
            commit(tick + 1, seconds);
            s.stats.tick = tick; s.stats.simulationSeconds = seconds;
            s.stats.pendingSeconds = std::max(0.0, s.stats.pendingSeconds - s.config.fixedStepSeconds);
            ++s.stats.tickCount;
        }
        return AR_OK;
    });
}
ArStatus FixedTickSession::Reset(uint64_t generation, double start) {
    return boundary([&]() -> ArStatus {
        std::lock_guard<std::mutex> lock(apiMutex);
        auto& s = *impl_;
        if (!s.instance) return AR_INVALID_STATE;
        auto& inst = instance(runtime(s.runtime), s.instance);
        if (!std::isfinite(start) || !std::isfinite(start + s.config.fixedStepSeconds) ||
            start + s.config.fixedStepSeconds <= start || generation <= inst.generation)
            return AR_INVALID_ARGUMENT;
        std::vector<BoundEvaluator> fresh; fresh.reserve(inst.plan.size());
        for (const auto& b : inst.plan) fresh.push_back({b.evaluator, nullptr});
        try { initializeStates(fresh, s.instance, generation); }
        catch (...) { cleanupStates(fresh); throw; }
        cleanupStates(inst.plan); inst.plan = std::move(fresh);
        inst.generation = generation; inst.baseline.generation = generation;
        inst.prior.reset(); inst.poisoned = false;
        s.release(); s.config.startSeconds = start; s.request = 0;
        s.stats = {}; s.stats.simulationSeconds = start;
        return AR_OK;
    });
}
ArStatus FixedTickSession::Close() {
    return boundary([&]() -> ArStatus {
        std::lock_guard<std::mutex> lock(apiMutex);
        auto& s = *impl_; s.release();
        if (!s.instance) return AR_OK;
        // Runtime destruction is also safe: retained snapshots are independent.
        auto r = runtimes.find(s.runtime);
        if (r != runtimes.end()) {
            auto& inst = instance(*r->second, s.instance);
            inst.fixedSession = nullptr; inst.poisoned = true;
        }
        s.instance = 0;
        return AR_OK;
    });
}
const FixedTickStats& FixedTickSession::Stats() const { return impl_->stats; }
ArSnapshot FixedTickSession::Snapshot() const { return impl_->snapshot; }
} // namespace avatarRuntime

extern "C" AR_EXPORT ArStatus AR_CALL arGetApi(uint32_t version, uint32_t size, ArRuntimeApi* out) {
    if (!out) return AR_INVALID_ARGUMENT;
    if (version != AR_ABI_VERSION || size < sizeof(ArRuntimeApi)) return AR_INCOMPATIBLE_ABI;
    const ArRuntimeApi api{AR_HEADER(ArRuntimeApi), createRuntime, destroyRuntime, registerEvaluator,
        createInstance, destroyInstance, resetInstance, evaluateFrame, getSnapshot,
        retainSnapshot, releaseSnapshot, getCapabilities};
    *out = api; return AR_OK;
}

extern "C" AR_EXPORT ArStatus AR_CALL arGetPhysicsApi(uint32_t version, uint32_t size, ArPhysicsApi* out) {
    if (!out) return AR_INVALID_ARGUMENT;
    if (version != AR_PHYSICS_API_VERSION || size < sizeof(ArPhysicsApi)) return AR_INCOMPATIBLE_ABI;
    *out = {AR_HEADER(ArPhysicsApi), createPhysicsWorld, destroyPhysicsWorld, resetPhysicsWorld, advancePhysicsWorld};
    return AR_OK;
}

extern "C" AR_EXPORT ArStatus AR_CALL arGetPhysicsMembershipApi(uint32_t version,
    uint32_t size, ArPhysicsMembershipApi* out) {
    if (!out) return AR_INVALID_ARGUMENT;
    if (version != AR_PHYSICS_MEMBERSHIP_API_VERSION || size < sizeof(ArPhysicsMembershipApi))
        return AR_INCOMPATIBLE_ABI;
    *out = {AR_HEADER(ArPhysicsMembershipApi), removePhysicsParticipant};
    return AR_OK;
}
