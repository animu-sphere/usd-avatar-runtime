#include "avatarMotion/ClipPoseAdapter.h"
#include "motionRetarget/Validation.h"
#include "pxr/base/gf/quatd.h"
#include "pxr/base/gf/rotation.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <stdexcept>

namespace avatarMotion {
namespace {
namespace motion = openstrata::motion;
void require(bool condition, const char* message) {
    if (!condition) throw std::invalid_argument(message);
}
bool identity(const std::string& value) {
    return !value.empty() && value.find('\0') == std::string::npos;
}
void validate(const ClipPoseAdapterConfig& c) {
    require(identity(c.evaluatorId) && identity(c.layoutId) && c.layoutVersion && identity(c.skeletonId),
            "Motion adapter requires evaluator/layout/skeleton identity");
    require(c.after.size() <= std::numeric_limits<uint32_t>::max(), "Too many motion evaluator dependencies");
    for (const auto& id : c.after)
        require(identity(id), "Motion evaluator dependency IDs must be nonempty C strings");
    require(std::isfinite(c.clockScale) && c.clockScale > 0 && std::isfinite(c.clockOffset),
            "Motion clip clock mapping must be finite with positive scale");
    double norm = 0;
    for (double value : c.rootPlacement.rotation) {
        require(std::isfinite(value), "Root placement rotation must be finite");
        norm += value * value;
    }
    require(std::abs(norm - 1) <= 1e-6, "Root placement rotation must be unit length");
    for (int k = 0; k < 3; ++k)
        require(std::isfinite(c.rootPlacement.translation[k]) && c.rootPlacement.scale[k] == 1,
                "Root placement must be finite and rigid");
    const auto& joints = c.skeleton.GetJoints();
    require(!joints.empty() && joints.size() <= size_t(std::numeric_limits<int32_t>::max()) &&
            c.jointIds.size() == joints.size(),
            "Motion adapter requires a nonempty skeleton and complete runtime joint mapping");
    std::set<std::string> ids;
    for (const auto& id : c.jointIds)
        require(!id.empty() && id.find('\0') == std::string::npos && ids.insert(id).second,
                "Motion runtime joint IDs must be distinct, nonempty and contain no NULs");
    // The runtime C boundary cannot represent embedded NULs in owner tokens.
    for (const auto& joint : joints)
        require(joint.token.find('\0') == std::string::npos, "Motion joint tokens must be representable as C strings");
    // Reject an ambiguous many-role-to-one-target binding as before; the owner
    // detects the collision. Its recoverable policy is valid for other hosts.
    require(c.map.FindDuplicateJointIndices().empty(), "Motion humanoid map has duplicate runtime targets");
}
void validateOwner(const ClipPoseAdapterConfig& c, const ArDiagnosticSink& sink) {
    const auto rig = motion::ValidateRetargetConfiguration(c.skeleton, c.map, c.sourceRest, c.options);
    if (!rig.IsValid()) {
        MotionValidationError error("usd-motion-plugins.motionRetarget", AR_MOTION_RETARGET_VERSION, rig.values);
        error.Emit(sink); throw error;
    }
    const auto clip = motion::ValidateMotionClip(c.clip);
    if (!clip.IsValid()) {
        MotionValidationError error("usd-motion-plugins.motionCore", AR_MOTION_CORE_VERSION, clip);
        error.Emit(sink); throw error;
    }
}
void emit(const ArEvaluationContext& c, const char* code, const std::string& subject,
          const char* message, ArStatus status = AR_OK, uint32_t severity = AR_SEVERITY_WARNING,
          const char* origin = "usd-avatar-runtime.avatarMotionAdapter") {
    ArDiagnostic d{AR_HEADER(ArDiagnostic)};
    d.status = status; d.severity = severity; d.code = code;
    d.origin = origin;
    d.subject = subject.c_str(); d.message = message;
    if (c.diagnostics.emit) c.diagnostics.emit(c.diagnostics.user_data, &d);
}
ArStatus failure(const ArEvaluationContext& c, const char* code, const std::string& subject, const char* message) {
    emit(c, code, subject, message, AR_INVALID_STATE, AR_SEVERITY_ERROR);
    return AR_INVALID_STATE;
}
} // namespace

struct ClipPoseAdapter::Impl {
    ClipPoseAdapterConfig config;
    motion::PoseRetargeter retargeter;
    motion::RetargetDiagnostics rigDiagnostics;
    std::vector<const char*> after;
    ArCapability capability{"avatar.motion.clipPose", 1};
    static ClipPoseAdapterConfig checked(ClipPoseAdapterConfig c, const ArDiagnosticSink& sink) {
        validate(c); validateOwner(c, sink); return c;
    }
    explicit Impl(ClipPoseAdapterConfig c, const ArDiagnosticSink& sink)
        : config(checked(std::move(c), sink)), retargeter(config.skeleton, config.map, config.sourceRest, config.options),
          rigDiagnostics(motion::DiagnoseRig(config.skeleton, config.map, config.options)) {
        for (const auto& id : config.after) after.push_back(id.c_str());
    }
    void report(const ArEvaluationContext& c, const motion::RetargetDiagnostics& diagnostics) const {
        for (const auto& d : diagnostics.reported) {
            const std::string code(motion::RetargetDiagnosticCodeString(d.code));
            const uint32_t severity = d.severity == motion::RetargetDiagnosticSeverity::Info ? AR_SEVERITY_INFO :
                d.severity == motion::RetargetDiagnosticSeverity::Error ? AR_SEVERITY_ERROR : AR_SEVERITY_WARNING;
            emit(c, code.c_str(), d.subject, d.detail.c_str(), AR_OK, severity,
                 "usd-motion-plugins.motionRetarget");
        }
    }
    ArStatus evaluate(const ArEvaluationContext& c, const ArStateWriter& writer) const {
        const auto& v = *c.working;
        if (!v.layout_id || config.layoutId != v.layout_id || config.layoutVersion != v.layout_version)
            return failure(c, "MOTION_ADAPTER_LAYOUT", config.layoutId, "Binding layout identity/version mismatch");
        std::vector<uint32_t> indices;
        for (const auto& id : config.jointIds) {
            uint32_t index = 0;
            for (; index < v.joint_count; ++index)
                if (config.skeletonId == v.joints[index].skeleton_id && id == v.joints[index].joint_id) break;
            if (index == v.joint_count)
                return failure(c, "MOTION_ADAPTER_JOINT", id, "Bound motion target joint is missing");
            indices.push_back(index);
        }
        const auto& joints = config.skeleton.GetJoints();
        for (size_t i = 0; i < indices.size(); ++i) {
            const int parent = joints[i].parent;
            const int32_t expected = parent < 0 ? -1 : int32_t(indices[size_t(parent)]);
            if (v.joints[indices[i]].parent_index != expected)
                return failure(c, "MOTION_ADAPTER_PARENT", config.jointIds[i], "Owner/runtime joint parents disagree");
        }
        const double time = (c.input->evaluation_seconds - config.clockOffset) / config.clockScale;
        if (!std::isfinite(time))
            return failure(c, "MOTION_ADAPTER_TIME", config.evaluatorId, "Mapped clip time is not finite");
        report(c, rigDiagnostics);
        const auto sample = motion::SampleClip(config.clip, time);
        if (!sample) {
            emit(c, "MOTION_ADAPTER_UNAVAILABLE", config.evaluatorId, "Empty clip contributes no pose");
            return AR_OK;
        }
        if (sample.status == motion::PoseSampleStatus::Held)
            emit(c, "MOTION_ADAPTER_HELD", config.evaluatorId, "Owner sampler held the nearest clip boundary pose");
        if (!sample.pose->channels.IsEmpty())
            emit(c, "MOTION_ADAPTER_CHANNELS_UNSUPPORTED", config.evaluatorId, "Clip scalar channels require a separate input mapping; only rig pose is published");
        if (sample.pose->lookAtTarget)
            emit(c, "MOTION_ADAPTER_GAZE_UNSUPPORTED", config.evaluatorId, "Clip gaze requires a separate input mapping; only rig pose is published");
        motion::RetargetDiagnostics diagnostics;
        const auto pose = retargeter.Retarget(*sample.pose, &diagnostics);
        report(c, diagnostics);
        for (size_t i = 0; i < indices.size(); ++i) {
            ArTransform local{};
            for (int k = 0; k < 3; ++k) {
                local.translation[k] = pose.translations[i][k];
                local.rotation[k] = pose.rotations[i].GetImaginary()[k];
                local.scale[k] = joints[i].restScale[k];
            }
            local.rotation[3] = pose.rotations[i].GetReal();
            if (joints[i].parent < 0) {
                const auto& p = config.rootPlacement;
                const pxr::GfQuatd placement(p.rotation[3], pxr::GfVec3d(p.rotation[0], p.rotation[1], p.rotation[2]));
                const auto translation = pxr::GfRotation(placement.GetNormalized()).TransformDir(
                    pxr::GfVec3d(local.translation[0], local.translation[1], local.translation[2]));
                const auto rotation = (placement.GetNormalized() * pxr::GfQuatd(
                    local.rotation[3], pxr::GfVec3d(local.rotation[0], local.rotation[1], local.rotation[2]))).GetNormalized();
                for (int k = 0; k < 3; ++k) {
                    local.translation[k] = translation[k] + p.translation[k];
                    local.rotation[k] = rotation.GetImaginary()[k];
                }
                local.rotation[3] = rotation.GetReal();
            }
            const auto status = writer.set_joint(writer.context, indices[i], &local);
            if (status != AR_OK) return status;
        }
        return AR_OK;
    }
    static ArStatus AR_CALL callback(void* user, void*, const ArEvaluationContext* c, const ArStateWriter* w) {
        return static_cast<const Impl*>(user)->evaluate(*c, *w);
    }
};
ClipPoseAdapter::ClipPoseAdapter(ClipPoseAdapterConfig config, ArDiagnosticSink diagnostics)
    : impl_(std::make_unique<Impl>(std::move(config), diagnostics)) {}
ClipPoseAdapter::~ClipPoseAdapter() = default;
ArEvaluatorDesc ClipPoseAdapter::Descriptor() const {
    ArEvaluatorDesc d{AR_HEADER(ArEvaluatorDesc)};
    d.id = impl_->config.evaluatorId.c_str(); d.provider_id = "usd-motion-plugins.motionRetarget";
    d.provider_version = "motionCore/" AR_MOTION_CORE_VERSION ";motionSampling/" AR_MOTION_SAMPLING_VERSION ";motionRetarget/" AR_MOTION_RETARGET_VERSION;
    d.phase = AR_PHASE_RETARGET; d.reads = AR_DOMAIN_POSE; d.writes = AR_DOMAIN_POSE;
    d.after = impl_->after.data(); d.after_count = uint32_t(impl_->after.size());
    d.supplies = &impl_->capability; d.supply_count = 1;
    d.user_data = impl_.get(); d.evaluate = Impl::callback;
    return d;
}
} // namespace avatarMotion
