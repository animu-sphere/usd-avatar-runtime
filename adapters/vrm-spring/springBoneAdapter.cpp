#include "avatarVrm/SpringBoneAdapter.h"
#include "pxr/base/gf/quatd.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <set>
#include <stdexcept>

#if !defined(VRMRIG_SPRING_BONE_API) || VRMRIG_SPRING_BONE_API < 1
#error "Spring adapter requires installed vrmRig SpringBoneSolver"
#endif

namespace avatarVrm {
namespace {
ArStatus fail(const ArEvaluationContext& c, const char* code, const std::string& subject,
              const std::string& detail, bool owner = false) {
    ArDiagnostic d{AR_HEADER(ArDiagnostic)};
    const std::string message = owner ? std::string("vrmRig " AR_VRM_SPRING_OWNER_VERSION ": ") + detail : detail;
    d.code = code; d.subject = subject.c_str(); d.message = message.c_str();
    d.origin = owner ? "usd-vrm-plugins.vrmRig" : "usd-avatar-runtime.avatarVrmSpringAdapter";
    d.severity = AR_SEVERITY_ERROR; d.status = AR_INVALID_STATE;
    if (c.diagnostics.emit) c.diagnostics.emit(c.diagnostics.user_data, &d);
    return AR_INVALID_STATE;
}
pxr::GfQuatd rotation(const ArTransform& t) {
    return {t.rotation[3], pxr::GfVec3d(t.rotation[0], t.rotation[1], t.rotation[2])};
}
bool narrowable(double v) {
    return std::isfinite(v) && std::abs(v) <= std::numeric_limits<float>::max();
}
} // namespace

struct SpringBoneAdapter::Impl {
    SpringBoneAdapterConfig config;
    vrmRig::SpringBoneSolver configured;
    std::vector<const char*> after;
    ArCapability capability{"avatar.vrm.springBone", 1};
    struct History {
        vrmRig::SpringBoneSolver solver;
        std::optional<double> seconds;
    };
    struct State {
        std::unique_ptr<History> committed, trial;
    };

    explicit Impl(SpringBoneAdapterConfig c) : config(std::move(c)) {
        if (config.evaluatorId.empty() || config.layoutId.empty() || !config.layoutVersion ||
            config.skeletonId.empty() || config.jointIds.empty() ||
            config.jointIds.size() != config.rig.parents.size() ||
            !narrowable(config.fixedStepSeconds) || float(config.fixedStepSeconds) <= 0)
            throw std::invalid_argument("VRM_SPRING_ADAPTER_CONFIG: identity, full joint map and positive float tick required");
        std::set<std::string> ids;
        for (const auto& id : config.jointIds)
            if (id.empty() || !ids.insert(id).second)
                throw std::invalid_argument("VRM_SPRING_ADAPTER_CONFIG: joint identities must be nonempty and unique");
        std::string error;
        vrmRig::SpringBoneResult result;
        if (!configured.Configure(config.rig, &error) ||
            !configured.Reset({config.rig.restTransforms, config.placement}, &result, &error))
            throw std::invalid_argument("VRM_SPRING_ADAPTER_OWNER (vrmRig " AR_VRM_SPRING_OWNER_VERSION "): " + error);
        for (const auto& id : config.after) after.push_back(id.c_str());
    }

    ArStatus evaluate(State& state, const ArEvaluationContext& c, const ArStateWriter& writer) const {
        const auto& v = *c.working;
        if (!v.layout_id || config.layoutId != v.layout_id || config.layoutVersion != v.layout_version)
            return fail(c, "VRM_SPRING_ADAPTER_LAYOUT", config.layoutId, "Binding layout identity/version mismatch");
        size_t skeletonCount = 0;
        for (uint32_t i = 0; i < v.joint_count; ++i)
            if (config.skeletonId == v.joints[i].skeleton_id) ++skeletonCount;
        if (skeletonCount != config.jointIds.size())
            return fail(c, "VRM_SPRING_ADAPTER_JOINT", config.skeletonId, "Binding must cover the complete selected skeleton");
        std::vector<uint32_t> indices;
        for (const auto& id : config.jointIds) {
            uint32_t i = 0;
            for (; i < v.joint_count; ++i)
                if (config.skeletonId == v.joints[i].skeleton_id && id == v.joints[i].joint_id) break;
            if (i == v.joint_count)
                return fail(c, "VRM_SPRING_ADAPTER_JOINT", id, "Bound spring joint is missing");
            indices.push_back(i);
        }
        for (size_t i = 0; i < indices.size(); ++i) {
            const int parent = config.rig.parents[i];
            if (v.joints[indices[i]].parent_index != (parent < 0 ? -1 : int32_t(indices[size_t(parent)])))
                return fail(c, "VRM_SPRING_ADAPTER_PARENT", config.jointIds[i], "Owner/runtime joint parents disagree");
        }
        auto& trial = *state.trial;
        const double now = c.input->evaluation_seconds;
        const double tolerance = std::min(config.fixedStepSeconds * 1e-3,
                                         std::max(1e-9, config.fixedStepSeconds * 1e-6));
        if (trial.seconds && std::abs((now - *trial.seconds) - config.fixedStepSeconds) >
                tolerance)
            return fail(c, "VRM_SPRING_ADAPTER_TICK", config.evaluatorId,
                        "Expected one fixed tick; reset before seek, loop wrap or clock discontinuity");

        // Absorb common animated root translation into owner placement, retaining
        // fixed rest translations. Root rotations remain animation. No hierarchy
        // expansion or numerical spring rule is implemented in this adapter.
        const auto root = size_t(std::find(config.rig.parents.begin(), config.rig.parents.end(), -1) - config.rig.parents.begin());
        const pxr::GfQuatd placeRotation(config.placement.rotation);
        const auto& rootLocal = v.joints[indices[root]].local;
        const pxr::GfVec3d placeTranslation =
            pxr::GfVec3d(rootLocal.translation[0], rootLocal.translation[1], rootLocal.translation[2]) -
            placeRotation.Transform(pxr::GfVec3d(config.rig.restTransforms[root].translation));
        for (int k = 0; k < 3; ++k)
            if (!narrowable(placeTranslation[k]))
                return fail(c, "VRM_SPRING_ADAPTER_RANGE", config.skeletonId, "Placement exceeds owner float range");
        vrmRig::SpringBonePose pose;
        pose.placement = {pxr::GfVec3f(placeTranslation), config.placement.rotation};
        for (size_t i = 0; i < indices.size(); ++i) {
            const auto& local = v.joints[indices[i]].local;
            for (double scale : local.scale)
                if (std::abs(scale - 1.0) > 1e-6)
                    return fail(c, "VRM_SPRING_ADAPTER_SCALE", config.jointIds[i], "Spring pose requires unit joint scale");
            pxr::GfVec3d translation(local.translation[0], local.translation[1], local.translation[2]);
            auto q = rotation(local);
            if (config.rig.parents[i] < 0) {
                translation = placeRotation.GetInverse().Transform(translation - placeTranslation);
                q = placeRotation.GetInverse() * q;
            }
            for (int k = 0; k < 3; ++k)
                if (!narrowable(translation[k]))
                    return fail(c, "VRM_SPRING_ADAPTER_RANGE", config.jointIds[i], "Translation exceeds owner float range");
            pose.localTransforms.push_back({pxr::GfVec3f(translation), pxr::GfQuatf(q.GetNormalized())});
        }
        vrmRig::SpringBoneResult result;
        std::string error;
        const bool ok = trial.seconds
            ? trial.solver.Step(float(config.fixedStepSeconds), pose, &result, &error)
            : trial.solver.Reset(pose, &result, &error);
        if (!ok) return fail(c, "VRM_SPRING_ADAPTER_OWNER", config.skeletonId, error, true);
        // The owner's tails identify simulated joints, including expanded roots.
        // Preserve every other channel exactly, avoiding float round-trip loss.
        for (const auto& tail : result.tails) {
            const auto i = size_t(tail.joint);
            auto local = v.joints[indices[i]].local;
            auto q = pxr::GfQuatd(result.localRotations[i]);
            if (config.rig.parents[i] < 0) q = placeRotation * q;
            q.Normalize();
            for (int k = 0; k < 3; ++k) local.rotation[k] = q.GetImaginary()[k];
            local.rotation[3] = q.GetReal();
            const auto status = writer.set_joint(writer.context, indices[i], &local);
            if (status != AR_OK) return status;
        }
        trial.seconds = now;
        return AR_OK;
    }
    static ArStatus AR_CALL create(void* user, ArInstance, uint64_t, void** out) {
        auto state = std::make_unique<State>();
        state->committed = std::make_unique<History>();
        state->committed->solver = static_cast<const Impl*>(user)->configured;
        *out = state.release();
        return AR_OK;
    }
    static void AR_CALL destroy(void*, void* state) { delete static_cast<State*>(state); }
    static ArStatus AR_CALL begin(void*, void* state, const ArEvaluationContext*) {
        auto& s = *static_cast<State*>(state);
        s.trial = std::make_unique<History>(*s.committed);
        return AR_OK;
    }
    static ArStatus AR_CALL evaluateCallback(void* user, void* state, const ArEvaluationContext* c, const ArStateWriter* w) {
        return static_cast<const Impl*>(user)->evaluate(*static_cast<State*>(state), *c, *w);
    }
    static void AR_CALL end(void*, void* state, uint32_t commit) {
        auto& s = *static_cast<State*>(state);
        if (commit) s.committed.swap(s.trial);
        s.trial.reset();
    }
};

SpringBoneAdapter::SpringBoneAdapter(SpringBoneAdapterConfig config) : impl_(std::make_unique<Impl>(std::move(config))) {}
SpringBoneAdapter::~SpringBoneAdapter() = default;
ArEvaluatorDesc SpringBoneAdapter::Descriptor() const {
    ArEvaluatorDesc d{AR_HEADER(ArEvaluatorDesc)};
    d.id = impl_->config.evaluatorId.c_str(); d.provider_id = "usd-vrm-plugins.vrmRig";
    d.provider_version = AR_VRM_SPRING_OWNER_VERSION;
    d.phase = AR_PHASE_SECONDARY; d.flags = AR_EVALUATOR_STATEFUL;
    d.reads = d.writes = AR_DOMAIN_POSE;
    d.after = impl_->after.data(); d.after_count = uint32_t(impl_->after.size());
    d.supplies = &impl_->capability; d.supply_count = 1;
    d.user_data = impl_.get(); d.create_state = Impl::create; d.destroy_state = Impl::destroy;
    d.begin_frame = Impl::begin; d.evaluate = Impl::evaluateCallback; d.end_frame = Impl::end;
    return d;
}
} // namespace avatarVrm
