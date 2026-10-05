#include "avatarMotion/ClipPoseAdapter.h"
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
bool vectorValid(const pxr::GfVec3f& v) {
    return std::isfinite(v[0]) && std::isfinite(v[1]) && std::isfinite(v[2]);
}
bool rotationValid(const pxr::GfQuatf& q) {
    const auto& v = q.GetImaginary();
    const double w = q.GetReal();
    return vectorValid(v) && std::isfinite(w) &&
        std::abs(w*w + double(v[0])*v[0] + double(v[1])*v[1] + double(v[2])*v[2] - 1.0) <= 1e-6;
}
void require(bool condition, const char* message) {
    if (!condition) throw std::invalid_argument(message);
}
void validate(const ClipPoseAdapterConfig& c) {
    require(!c.evaluatorId.empty() && !c.layoutId.empty() && c.layoutVersion && !c.skeletonId.empty(),
            "Motion adapter requires evaluator/layout/skeleton identity");
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
            c.jointIds.size() == joints.size() && c.skeleton.IsTopologicallyOrdered(),
            "Motion adapter requires a nonempty ordered skeleton and complete joint mapping");
    std::set<std::string> tokens, ids;
    for (size_t i = 0; i < joints.size(); ++i) {
        const auto& j = joints[i];
        require(!j.token.empty() && tokens.insert(j.token).second && !c.jointIds[i].empty() &&
                ids.insert(c.jointIds[i]).second, "Motion joint bindings must be distinct and nonempty");
        require(rotationValid(j.restRotation) && vectorValid(j.restTranslation) && vectorValid(j.restScale),
                "Motion skeleton rest transforms must be finite with unit rotations");
    }
    require(c.map.FindDuplicateJointIndices().empty(), "Motion humanoid map has duplicate targets");
    for (size_t i = 0; i < motion::HumanJointCount; ++i) {
        const auto index = c.map.GetJointIndex(static_cast<motion::HumanJoint>(i));
        require(index == motion::RetargetMap::kUnmapped || (index >= 0 && size_t(index) < joints.size()),
                "Motion humanoid mapping names an invalid target index");
        require(rotationValid(c.sourceRest.localRotations[i]) && vectorValid(c.sourceRest.localTranslations[i]),
                "Motion source rest must be finite with unit rotations");
        size_t walk = i, length = 0;
        while (walk != motion::SourceRestPose::kNoParent) {
            require(walk < motion::HumanJointCount && length++ < motion::HumanJointCount,
                    "Motion source rest hierarchy is invalid or cyclic");
            walk = c.sourceRest.parents[walk];
        }
    }
    require(c.options.targetRest.localRotations.size() <= joints.size(), "Motion target rest exceeds the skeleton");
    for (const auto& q : c.options.targetRest.localRotations)
        require(!q || rotationValid(*q), "Motion target reference rest must have unit finite rotations");
    const auto mode = c.options.rootMotion.mode;
    require(mode == motion::RootMotionMode::Ignore || mode == motion::RootMotionMode::Hips ||
            mode == motion::RootMotionMode::RootJoint, "Unknown motion root policy");
    require(std::isfinite(c.options.rootMotion.translationScale), "Motion root translation scale must be finite");
    for (auto bone : c.options.requiredBones)
        require(motion::IsValidHumanJoint(bone), "Motion required bone is outside the owner vocabulary");
    double previous = -std::numeric_limits<double>::infinity();
    for (const auto& pose : c.clip.samples) {
        require(std::isfinite(pose.timestamp) && pose.timestamp >= previous,
                "Motion clip timestamps must be finite and nondecreasing");
        require(!std::isfinite(previous) || std::isfinite(pose.timestamp - previous),
                "Motion clip interpolation spans must be finite");
        previous = pose.timestamp;
        for (size_t i = 0; i < motion::HumanJointCount; ++i)
            require(!pose.validRotations[i] || rotationValid(pose.localRotations[i]),
                    "Driven motion rotations must be finite unit quaternions");
        require(!pose.root.hasPosition || vectorValid(pose.root.worldPosition), "Motion root position must be finite");
        require(!pose.root.hasOrientation || rotationValid(pose.root.worldOrientation), "Motion root rotation must be finite and unit");
        // The sampler also interpolates these fields even though the adapter
        // publishes only pose. Validate present values before calling it.
        require(!pose.root.hasLinearVelocity || vectorValid(pose.root.linearVelocity), "Motion velocity must be finite");
        require(!pose.root.hasAngularVelocity || vectorValid(pose.root.angularVelocity), "Motion velocity must be finite");
        require(!pose.lookAtTarget || vectorValid(*pose.lookAtTarget), "Motion gaze point must be finite");
        for (const auto& channel : pose.channels.entries)
            require(!channel.name.empty() && std::isfinite(channel.value), "Motion channels must be named and finite");
        if (pose.confidence)
            for (float confidence : *pose.confidence)
                require(std::isfinite(confidence) && confidence >= 0 && confidence <= 1,
                        "Motion confidence must lie in [0,1]");
    }
}
void emit(const ArEvaluationContext& c, const char* code, const std::string& subject,
          const char* message, ArStatus status = AR_OK, uint32_t severity = AR_SEVERITY_WARNING) {
    ArDiagnostic d{AR_HEADER(ArDiagnostic)};
    d.status = status; d.severity = severity; d.code = code;
    d.origin = "usd-motion-plugins.motionRetarget";
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
    static ClipPoseAdapterConfig checked(ClipPoseAdapterConfig c) { validate(c); return c; }
    explicit Impl(ClipPoseAdapterConfig c)
        : config(checked(std::move(c))), retargeter(config.skeleton, config.map, config.sourceRest, config.options),
          rigDiagnostics(motion::DiagnoseRig(config.skeleton, config.map, config.options)) {
        for (const auto& id : config.after) after.push_back(id.c_str());
    }
    void report(const ArEvaluationContext& c, const motion::RetargetDiagnostics& diagnostics) const {
        for (const auto& d : diagnostics.reported) {
            const std::string code(motion::RetargetDiagnosticCodeString(d.code));
            const uint32_t severity = d.severity == motion::RetargetDiagnosticSeverity::Info ? AR_SEVERITY_INFO :
                d.severity == motion::RetargetDiagnosticSeverity::Error ? AR_SEVERITY_ERROR : AR_SEVERITY_WARNING;
            emit(c, code.c_str(), d.subject, d.detail.c_str(), AR_OK, severity);
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
ClipPoseAdapter::ClipPoseAdapter(ClipPoseAdapterConfig config) : impl_(std::make_unique<Impl>(std::move(config))) {}
ClipPoseAdapter::~ClipPoseAdapter() = default;
ArEvaluatorDesc ClipPoseAdapter::Descriptor() const {
    ArEvaluatorDesc d{AR_HEADER(ArEvaluatorDesc)};
    d.id = impl_->config.evaluatorId.c_str(); d.provider_id = "usd-motion-plugins.motionRetarget";
    d.provider_version = "motionSampling/" AR_MOTION_SAMPLING_VERSION ";motionRetarget/" AR_MOTION_RETARGET_VERSION;
    d.phase = AR_PHASE_RETARGET; d.reads = AR_DOMAIN_POSE; d.writes = AR_DOMAIN_POSE;
    d.after = impl_->after.data(); d.after_count = uint32_t(impl_->after.size());
    d.supplies = &impl_->capability; d.supply_count = 1;
    d.user_data = impl_.get(); d.evaluate = Impl::callback;
    return d;
}
} // namespace avatarMotion
