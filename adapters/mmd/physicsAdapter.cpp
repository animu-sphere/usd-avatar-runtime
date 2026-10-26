#include <avatarMmd/PhysicsAdapter.h>
#include <pxr/base/gf/quatd.h>
#include <cmath>
#include <map>
#include <optional>
#include <limits>
#include <set>
#include <stdexcept>

namespace avatarMmd {
namespace {
ArStatus emit(const ArEvaluationContext& c, const char* code, const char* origin,
              const std::string& subject, const std::string& message,
              ArStatus status = AR_PROVIDER_ERROR, uint32_t severity = AR_SEVERITY_ERROR) {
    if (c.diagnostics.emit) {
        ArDiagnostic d{AR_HEADER(ArDiagnostic)};
        d.code = code; d.origin = origin; d.subject = subject.c_str();
        d.message = message.c_str(); d.status = status; d.severity = severity;
        c.diagnostics.emit(c.diagnostics.user_data, &d);
    }
    return status;
}
ArStatus failure(const ArEvaluationContext& c, const char* code, const std::string& message) {
    return emit(c, code, "usd-avatar-runtime.avatarMmd", c.working->layout_id ? c.working->layout_id : "", message);
}
template<class T> bool forward(const ArEvaluationContext& c, const mmd::Result<T>& result) {
    auto send = [&](const mmd::Diagnostic& d, ArStatus status) {
        emit(c, d.code.c_str(), "usd-mmd-plugins", mmd::ToString(d.location), d.message, status,
            d.severity == mmd::Severity::Info ? AR_SEVERITY_INFO :
            d.severity == mmd::Severity::Warning ? AR_SEVERITY_WARNING : AR_SEVERITY_ERROR);
    };
    for (const auto& d : result.diagnostics()) send(d, AR_OK);
    if (result.fatal()) send(*result.fatal(), AR_PROVIDER_ERROR);
    return bool(result);
}
using MaterialValues = std::array<std::array<double, 4>, 9>;
constexpr uint32_t materialTypes[]{AR_VALUE_VEC4, AR_VALUE_VEC3, AR_VALUE_SCALAR,
    AR_VALUE_VEC3, AR_VALUE_VEC4, AR_VALUE_SCALAR, AR_VALUE_VEC4, AR_VALUE_VEC4, AR_VALUE_VEC4};
MaterialValues fields(const mmd::control::MaterialMorphValues& v) {
    return {v.diffuseColor, {v.specularColor[0], v.specularColor[1], v.specularColor[2], 0},
        {v.specularPower, 0, 0, 0}, {v.ambientColor[0], v.ambientColor[1], v.ambientColor[2], 0},
        v.edgeColor, {v.edgeSize, 0, 0, 0}, v.textureTint, v.sphereTint, v.toonTint};
}
}
struct PhysicsAdapter::Impl {
    struct State {
        ArInstance instance;
        uint64_t generation;
        std::optional<mmd::control::Evaluator::Continuation> continuation;
        uint64_t frame = 0;
        double time = 0;
    };
    PhysicsAdapterConfig config;
    std::vector<const char*> after;
    const char* postAfter;
    std::map<std::pair<ArInstance, uint64_t>, State*> states;

    explicit Impl(PhysicsAdapterConfig c) : config(std::move(c)) {
        if (config.preId.empty() || config.postId.empty() || config.preId == config.postId ||
            config.layoutId.empty() || !config.layoutVersion || !config.submit || !config.snapshot ||
            !std::isfinite(config.framesPerSecond) || config.framesPerSecond <= 0 ||
            !std::isfinite(config.frameOffset) || config.joints.size() != config.evaluator.JointCount())
            throw std::invalid_argument("Incomplete MMD control/coupling configuration");
        double norm = 0;
        for (double x : config.placement.translation)
            if (!std::isfinite(x)) throw std::invalid_argument("Nonfinite MMD placement");
        for (double x : config.placement.rotation) {
            if (!std::isfinite(x)) throw std::invalid_argument("Nonfinite MMD placement");
            norm += x * x;
        }
        if (std::abs(norm - 1) > 1e-6) throw std::invalid_argument("MMD placement must be rigid");
        std::set<std::pair<std::string, std::string>> joints, morphs;
        std::set<int32_t> morphIndices;
        for (size_t i = 0; i < config.joints.size(); ++i) {
            const auto& j = config.joints[i];
            if (j.skeleton.empty() || j.joint.empty() || !joints.emplace(j.skeleton, j.joint).second ||
                j.parent < -1 || j.parent >= int64_t(i) ||
                (j.parent >= 0 && config.joints[j.parent].skeleton != j.skeleton))
                throw std::invalid_argument("MMD canonical joint binding is incomplete or ambiguous");
        }
        for (const auto& m : config.morphs)
            if (m.morph < 0 || m.mesh.empty() || m.target.empty() ||
                !morphIndices.insert(m.morph).second || !morphs.emplace(m.mesh, m.target).second)
                throw std::invalid_argument("MMD morph binding is incomplete or ambiguous");
        if (config.uvMesh.empty() != config.baseUvs.empty() ||
            (!config.uvMorphs.empty() && config.baseUvs.empty()))
            throw std::invalid_argument("Incomplete MMD UV binding");
        for (auto id : config.uvMorphs)
            if (id < 0 || !morphIndices.insert(id).second)
                throw std::invalid_argument("Ambiguous MMD UV morph binding");
        for (const auto& uv : config.baseUvs) for (double x : uv)
            if (!std::isfinite(x) || std::abs(x) > std::numeric_limits<float>::max())
                throw std::invalid_argument("Invalid MMD base UV");
        for (auto id : config.materialMorphs)
            if (id < 0 || !morphIndices.insert(id).second)
                throw std::invalid_argument("Ambiguous MMD material morph binding");
        std::set<std::pair<std::string, std::string>> materialInputs;
        std::map<int32_t, std::pair<std::string, uint32_t>> materialFields;
        std::map<std::string, int32_t> materialTargets;
        for (const auto& b : config.materials) {
            const auto field = static_cast<uint32_t>(b.field);
            if (b.material < 0 || field >= 9 || b.target.empty() || b.input.empty() ||
                !materialInputs.emplace(b.target, b.input).second)
                throw std::invalid_argument("Invalid or ambiguous MMD material input binding");
            auto& [target, mask] = materialFields[b.material];
            if ((!target.empty() && target != b.target) || (mask & (1u << field)))
                throw std::invalid_argument("Ambiguous MMD material field binding");
            target = b.target; mask |= 1u << field;
            const auto [it, inserted] = materialTargets.emplace(b.target, b.material);
            if (!inserted && it->second != b.material)
                throw std::invalid_argument("MMD materials must have distinct runtime targets");
            for (uint32_t a = 0; a < 4; ++a)
                if (!std::isfinite(b.base[a]) || std::abs(b.base[a]) > std::numeric_limits<float>::max() ||
                    (a >= materialTypes[field] && b.base[a] != 0))
                    throw std::invalid_argument("Invalid MMD material base value");
        }
        for (const auto& [material, binding] : materialFields)
            if (binding.second != 0x1ffu)
                throw std::invalid_argument("MMD material binding requires all nine fields");
        if (!config.materialMorphs.empty() && config.materials.empty())
            throw std::invalid_argument("Missing MMD material output bindings");
        std::set<std::string> visibility;
        for (const auto& id : config.visibilityTargets)
            if (id.empty() || !visibility.insert(id).second)
                throw std::invalid_argument("MMD visibility binding is incomplete or ambiguous");
        for (const auto& id : config.after) after.push_back(id.c_str());
        postAfter = config.preId.c_str();
    }

    ArStatus indices(const ArEvaluationContext& c, std::vector<uint32_t>& out) const {
        const auto& v = *c.working;
        if (!v.layout_id || config.layoutId != v.layout_id || config.layoutVersion != v.layout_version)
            return failure(c, "MMD_ADAPTER_LAYOUT", "Bound layout identity/version mismatch");
        for (const auto& b : config.joints) {
            uint32_t i = 0;
            for (; i < v.joint_count; ++i)
                if (b.skeleton == v.joints[i].skeleton_id && b.joint == v.joints[i].joint_id) break;
            if (i == v.joint_count)
                return failure(c, "MMD_ADAPTER_JOINT", "Canonical joint binding is absent");
            const auto parent = b.parent < 0 ? -1 : int32_t(out[b.parent]);
            if (v.joints[i].parent_index != parent)
                return failure(c, "MMD_ADAPTER_PARENT", "Canonical parent binding mismatch");
            for (double scale : v.joints[i].local.scale)
                if (std::abs(scale - 1) > 1e-6)
                    return failure(c, "MMD_ADAPTER_SCALE", "MMD coupling requires unit-scale local poses");
            out.push_back(i);
        }
        return AR_OK;
    }
    ArStatus write(const ArEvaluationContext& c, const ArStateWriter& w,
                   const mmd::control::Pose& pose, bool final) const {
        std::vector<uint32_t> jointIndices;
        auto status = indices(c, jointIndices);
        if (status != AR_OK) return status;
        const auto& v = *c.working;
        // Validate every output before calling any writer/host operation.
        std::vector<uint32_t> morphIndices, visibilityIndices;
        std::vector<mmd::control::MorphChannel> channels;
        std::vector<uint32_t> uvIndices;
        auto uvs = config.baseUvs;
        std::vector<uint32_t> materialIndices;
        std::vector<std::array<double, 4>> materialValues;
        if (final) {
            channels = config.evaluator.ResolveMorphChannels(pose.channels);
            for (const auto& channel : channels) {
                if (!std::isfinite(channel.weight))
                    return failure(c, "MMD_ADAPTER_MORPH", "Owner resolved morph weight is not finite");
                bool mapped = false;
                for (const auto& m : config.morphs) mapped |= m.morph == channel.morph;
                for (auto id : config.uvMorphs) mapped |= id == channel.morph;
                for (auto id : config.materialMorphs) mapped |= id == channel.morph;
                if (!mapped) return failure(c, "MMD_ADAPTER_MORPH", "Owner non-bone morph has no output binding");
            }
            if (!config.materials.empty()) {
                auto modulation = config.evaluator.EvaluateMaterialMorphs(pose.channels);
                if (!forward(c, modulation)) return AR_PROVIDER_ERROR;
                for (const auto& row : modulation.value()) {
                    bool bound = false;
                    for (const auto& b : config.materials) bound |= b.material == row.material;
                    if (!bound)
                        return failure(c, "MMD_ADAPTER_MATERIAL", "Reached material has no complete output binding");
                }
                for (const auto& b : config.materials) {
                    const auto field = static_cast<uint32_t>(b.field);
                    auto value = b.base;
                    for (const auto& row : modulation.value()) if (row.material == b.material) {
                        const auto multiply = fields(row.multiply), add = fields(row.add);
                        for (uint32_t a = 0; a < materialTypes[field]; ++a)
                            value[a] = b.base[a] * multiply[field][a] + add[field][a];
                    }
                    for (double x : value)
                        if (!std::isfinite(x) || std::abs(x) > std::numeric_limits<float>::max())
                            return failure(c, "MMD_ADAPTER_MATERIAL", "Final material value is nonfinite or exceeds float range");
                    uint32_t i = 0;
                    for (; i < v.material_count; ++i)
                        if (b.target == v.materials[i].material_id && b.input == v.materials[i].input_id) break;
                    if (i == v.material_count || v.materials[i].value_type != materialTypes[field])
                        return failure(c, "MMD_ADAPTER_MATERIAL", "Material output identity or type mismatch");
                    materialIndices.push_back(i); materialValues.push_back(value);
                }
            }
            if (!config.baseUvs.empty()) {
                auto deltas = config.evaluator.EvaluateUvMorphs(pose.channels);
                if (!forward(c, deltas)) return AR_PROVIDER_ERROR;
                for (const auto& delta : deltas.value()) {
                    if (delta.channel || delta.vertex < 0 || size_t(delta.vertex) >= uvs.size())
                        return failure(c, "MMD_ADAPTER_UV", "Unsupported UV channel or vertex");
                    for (int a = 0; a < 2; ++a) uvs[delta.vertex][a] += delta.delta[a];
                }
                uvIndices.assign(uvs.size(), UINT32_MAX);
                for (uint32_t i = 0; i < v.vertex_uv_count; ++i) {
                    const auto& uv = v.vertex_uvs[i];
                    if (config.uvMesh != uv.mesh_id) continue;
                    if (uv.vertex_index >= uvs.size() || uvIndices[uv.vertex_index] != UINT32_MAX)
                        return failure(c, "MMD_ADAPTER_UV", "Changed UV vertex layout");
                    uvIndices[uv.vertex_index] = i;
                }
                for (size_t i = 0; i < uvs.size(); ++i) {
                    if (uvIndices[i] == UINT32_MAX)
                        return failure(c, "MMD_ADAPTER_UV", "Bound UV output is absent");
                    for (double x : uvs[i]) if (!std::isfinite(x) || std::abs(x) > std::numeric_limits<float>::max())
                        return failure(c, "MMD_ADAPTER_UV", "Final UV is nonfinite or exceeds float range");
                }
            }
            for (const auto& m : config.morphs) {
                uint32_t i = 0;
                for (; i < v.blend_shape_count; ++i)
                    if (m.mesh == v.blend_shapes[i].mesh_id && m.target == v.blend_shapes[i].target_id) break;
                if (i == v.blend_shape_count)
                    return failure(c, "MMD_ADAPTER_MORPH", "Bound morph output is absent");
                morphIndices.push_back(i);
            }
            if (!pose.visible && config.visibilityTargets.empty())
                return failure(c, "MMD_ADAPTER_VISIBILITY", "Owner hidden state has no visibility binding");
            for (const auto& target : config.visibilityTargets) {
                uint32_t i = 0;
                for (; i < v.visibility_count; ++i) if (target == v.visibility[i].target_id) break;
                if (i == v.visibility_count)
                    return failure(c, "MMD_ADAPTER_VISIBILITY", "Bound visibility output is absent");
                visibilityIndices.push_back(i);
            }
        }
        for (size_t i = 0; i < jointIndices.size(); ++i) {
            ArTransform t{};
            for (int a = 0; a < 3; ++a) { t.translation[a] = pose.joints[i].translation[a]; t.scale[a] = 1; }
            for (int a = 0; a < 4; ++a) t.rotation[a] = pose.joints[i].rotation[a];
            if (config.joints[i].parent < 0) {
                const auto& p = config.placement;
                const pxr::GfQuatd q(p.rotation[3], p.rotation[0], p.rotation[1], p.rotation[2]);
                auto position = q.Transform(pxr::GfVec3d(t.translation[0], t.translation[1], t.translation[2]));
                const auto rotation = q * pxr::GfQuatd(t.rotation[3], t.rotation[0], t.rotation[1], t.rotation[2]);
                for (int a = 0; a < 3; ++a) {
                    t.translation[a] = position[a] + p.translation[a];
                    t.rotation[a] = rotation.GetImaginary()[a];
                }
                t.rotation[3] = rotation.GetReal();
            }
            if ((status = w.set_joint(w.context, jointIndices[i], &t)) != AR_OK) return status;
        }
        for (size_t i = 0; i < morphIndices.size(); ++i) {
            double weight = 0;
            for (const auto& ch : channels) if (ch.morph == config.morphs[i].morph) weight = ch.weight;
            if ((status = w.set_blend_shape(w.context, morphIndices[i], weight)) != AR_OK) return status;
        }
        for (size_t i = 0; i < uvIndices.size(); ++i)
            if ((status = w.set_vertex_uv(w.context, uvIndices[i], uvs[i].data())) != AR_OK) return status;
        for (size_t i = 0; i < materialIndices.size(); ++i)
            if ((status = w.set_material(w.context, materialIndices[i], 1, materialValues[i].data())) != AR_OK) return status;
        for (auto i : visibilityIndices)
            if ((status = w.set_visibility(w.context, i, pose.visible)) != AR_OK) return status;
        return AR_OK;
    }
    static ArStatus AR_CALL create(void* p, ArInstance instance, uint64_t generation, void** out) {
        auto& self = *static_cast<Impl*>(p);
        auto state = std::make_unique<State>(); state->instance = instance; state->generation = generation;
        if (!self.states.emplace(std::make_pair(instance, generation), state.get()).second) return AR_INVALID_STATE;
        *out = state.release(); return AR_OK;
    }
    static void AR_CALL destroy(void* p, void* s) {
        auto& self = *static_cast<Impl*>(p); auto* state = static_cast<State*>(s);
        if (state) { self.states.erase({state->instance, state->generation}); delete state; }
    }
    static ArStatus AR_CALL begin(void*, void* s, const ArEvaluationContext*) {
        static_cast<State*>(s)->continuation.reset(); return AR_OK;
    }
    static void AR_CALL end(void*, void* s, uint32_t) {
        static_cast<State*>(s)->continuation.reset();
    }
    static ArStatus AR_CALL pre(void* p, void* s, const ArEvaluationContext* c, const ArStateWriter* w) {
        auto& self = *static_cast<Impl*>(p); auto& state = *static_cast<State*>(s);
        const double frame = c->input->evaluation_seconds * self.config.framesPerSecond + self.config.frameOffset;
        if (!std::isfinite(frame)) return failure(*c, "MMD_ADAPTER_TIME", "Mapped MMD frame is not finite");
        auto continuation = self.config.evaluator.BeginPhysics(self.config.motion, frame);
        auto targets = self.config.coupling.UpdateKinematicBodies(continuation.BeforePhysics());
        if (!forward(*c, targets)) return AR_PROVIDER_ERROR;
        auto status = self.write(*c, *w, continuation.BeforePhysics(), false);
        if (status != AR_OK) return status;
        status = self.config.submit(*c, targets.value());
        if (status != AR_OK) return status;
        state.frame = c->input->frame_id; state.time = c->input->evaluation_seconds;
        state.continuation = std::move(continuation); return AR_OK;
    }
    static ArStatus AR_CALL post(void* p, void*, const ArEvaluationContext* c, const ArStateWriter* w) {
        auto& self = *static_cast<Impl*>(p);
        const auto found = self.states.find({c->working->instance, c->input->generation});
        if (found == self.states.end() || !found->second->continuation ||
            found->second->frame != c->input->frame_id || found->second->time != c->input->evaluation_seconds)
            return failure(*c, "MMD_ADAPTER_CONTINUATION", "Post callback requires the matching pre-physics tick");
        const auto& continuation = *found->second->continuation;
        std::vector<mmd::physics::BodyTransform> bodies;
        auto status = self.config.snapshot(*c, bodies);
        if (status != AR_OK) return status;
        auto feedback = self.config.coupling.ApplyPhysicsToPose(bodies, continuation.BeforePhysics());
        if (!forward(*c, feedback)) return AR_PROVIDER_ERROR;
        auto final = self.config.evaluator.ResumePhysics(continuation, feedback.value());
        if (!forward(*c, final)) return AR_PROVIDER_ERROR;
        return self.write(*c, *w, final.value(), true);
    }
};
PhysicsAdapter::PhysicsAdapter(PhysicsAdapterConfig c) : impl_(std::make_unique<Impl>(std::move(c))) {}
PhysicsAdapter::~PhysicsAdapter() = default;
ArEvaluatorDesc PhysicsAdapter::PreDescriptor() const {
    ArEvaluatorDesc d{AR_HEADER(ArEvaluatorDesc)};
    d.id = impl_->config.preId.c_str(); d.provider_id = "usd-mmd-plugins.mmdControl";
    d.provider_version = AR_MMD_PROVIDER_VERSION; d.phase = AR_PHASE_CONSTRAINTS;
    d.reads = d.writes = AR_DOMAIN_POSE; d.flags = AR_EVALUATOR_STATEFUL;
    d.user_data = impl_.get(); d.after = impl_->after.data(); d.after_count = uint32_t(impl_->after.size());
    d.create_state = Impl::create; d.destroy_state = Impl::destroy; d.begin_frame = Impl::begin;
    d.end_frame = Impl::end; d.evaluate = Impl::pre; return d;
}
ArEvaluatorDesc PhysicsAdapter::PostDescriptor() const {
    ArEvaluatorDesc d{AR_HEADER(ArEvaluatorDesc)};
    d.id = impl_->config.postId.c_str(); d.provider_id = "usd-mmd-plugins.mmdControl";
    d.provider_version = AR_MMD_PROVIDER_VERSION; d.phase = AR_PHASE_CONSTRAINTS;
    d.reads = d.writes = AR_DOMAIN_POSE;
    if (!impl_->config.morphs.empty() || !impl_->config.baseUvs.empty()) d.writes |= AR_DOMAIN_DEFORMATION;
    if (!impl_->config.materials.empty()) d.writes |= AR_DOMAIN_MATERIAL;
    if (!impl_->config.visibilityTargets.empty()) d.writes |= AR_DOMAIN_VISIBILITY;
    d.user_data = impl_.get(); d.after = &impl_->postAfter; d.after_count = 1;
    d.evaluate = Impl::post; return d;
}
} // namespace avatarMmd
