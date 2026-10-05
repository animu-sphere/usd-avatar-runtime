#include "avatarVrmUsd/LookAtBinding.h"
#include "vrmSchema/vrmLookAtAPI.h"
#include "pxr/usd/usd/primRange.h"
#include "pxr/usd/sdf/types.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace avatarVrmUsd {
namespace {
void require(bool condition, const char* code, const std::string& subject) {
    if (!condition) throw std::invalid_argument(std::string(code) + ": " + subject);
}
std::string token(const pxr::UsdAttribute& attr, bool required) {
    pxr::TfToken value;
    if (!required && !attr.HasAuthoredValueOpinion()) return {};
    require(attr.GetTypeName() == pxr::SdfValueTypeNames->Token && attr.Get(&value) && !value.IsEmpty(),
            "VRM_LOOKAT_BINDING_TOKEN", attr.GetPath().GetString());
    return value.GetString();
}
} // namespace
struct LookAtBinding::Impl {
    HumanoidBinding humanoid;
    std::string id, head;
    vrmRig::LookAtRig rig;
    std::vector<avatarVrm::EyeBinding> eyes;
    std::vector<std::string> warnings;
    Impl(const pxr::UsdStagePtr& stage, const LookAtBindingConfig& config)
        : humanoid(stage, config.humanoid) {
        const auto root = stage->GetPrimAtPath(config.humanoid.avatarRoot);
        pxr::UsdPrim prim;
        if (!config.lookAt.IsEmpty()) {
            require(config.lookAt.IsAbsolutePath() && config.lookAt.IsPrimPath() &&
                    config.lookAt.HasPrefix(config.humanoid.avatarRoot),
                    "VRM_LOOKAT_BINDING_PATH", config.lookAt.GetString());
            prim = stage->GetPrimAtPath(config.lookAt);
            require(bool(prim) && prim.IsActive() && prim.IsLoaded() && prim.HasAPI<pxr::UsdVrmLookAtAPI>(),
                    "VRM_LOOKAT_BINDING_SCHEMA", config.lookAt.GetString());
        } else {
            for (const auto& candidate : pxr::UsdPrimRange(root)) {
                if (!candidate.HasAPI<pxr::UsdVrmLookAtAPI>()) continue;
                require(!prim, "VRM_LOOKAT_BINDING_AMBIGUOUS", root.GetPath().GetString());
                prim = candidate;
            }
            require(bool(prim), "VRM_LOOKAT_BINDING_MISSING", root.GetPath().GetString());
        }
        id = prim.GetPath().GetString();
        const auto raw = prim.GetCustomDataByKey(pxr::TfToken("vrm:lookAt:raw"));
        if (!raw.IsEmpty()) {
            require(raw.IsHolding<std::string>() &&
                    vrmRig::ParseLookAtRangeMaps(raw.Get<std::string>(), &rig, &warnings),
                    "VRM_LOOKAT_BINDING_RAW", id);
        }
        pxr::UsdVrmLookAtAPI api(prim);
        const auto type = token(api.GetVrmTypeAttr(), true);
        require(type == "bone" || type == "expression", "VRM_LOOKAT_BINDING_TYPE", id);
        rig.type = type == "bone" ? vrmRig::LookAtType::Bone : vrmRig::LookAtType::Expression;
        const auto& skeleton = humanoid.Skeleton();
        const auto& baseline = skeleton.Baseline();
        const auto rel = api.GetVrmSkeletonRel();
        if (rel.HasAuthoredTargets()) {
            pxr::SdfPathVector targets;
            require(rel.GetForwardedTargets(&targets) && targets.size() == 1 &&
                    targets[0].GetString() == skeleton.SkeletonId(),
                    "VRM_LOOKAT_BINDING_SKELETON", id);
        }
        const auto headIndex = skeleton.HumanoidMap().GetJointIndex(openstrata::motion::HumanJoint::Head);
        require(headIndex >= 0, "VRM_LOOKAT_BINDING_HEAD", humanoid.HumanoidId());
        head = skeleton.JointIds()[size_t(headIndex)];
        for (int32_t i = headIndex; i >= 0; i = baseline.joints[i].parent_index) {
            for (double scale : baseline.joints[i].local.scale)
                require(std::abs(scale - 1) <= 1e-6, "VRM_LOOKAT_BINDING_HEAD_SCALE", skeleton.JointIds()[size_t(i)]);
        }
        rig.leftEyeJoint = token(api.GetVrmLeftEyeAttr(), false);
        rig.rightEyeJoint = token(api.GetVrmRightEyeAttr(), false);
        require(rig.leftEyeJoint.empty() || rig.leftEyeJoint != rig.rightEyeJoint,
                "VRM_LOOKAT_BINDING_EYE_DUPLICATE", id);
        for (const auto& eye : {rig.leftEyeJoint, rig.rightEyeJoint}) {
            if (eye.empty()) continue;
            const auto& joints = skeleton.JointIds();
            const auto found = std::find(joints.begin(), joints.end(), eye);
            require(found != joints.end(), "VRM_LOOKAT_BINDING_EYE", eye);
            const auto index = size_t(found - joints.begin());
            if (rig.type == vrmRig::LookAtType::Bone) {
                require(baseline.joints[index].parent_index == headIndex,
                        "VRM_LOOKAT_BINDING_EYE_PARENT", eye);
                avatarVrm::EyeBinding binding{eye, skeleton.SkeletonId(), eye};
                std::copy_n(baseline.joints[index].local.rotation, 4, binding.restRotation.begin());
                eyes.push_back(std::move(binding));
            }
        }
        require(rig.type != vrmRig::LookAtType::Bone || !eyes.empty(), "VRM_LOOKAT_BINDING_EYES_MISSING", id);
    }
};
LookAtBinding::LookAtBinding(const pxr::UsdStagePtr& stage, LookAtBindingConfig config)
    : impl_(std::make_shared<Impl>(stage, config)) {}
const HumanoidBinding& LookAtBinding::Humanoid() const { return impl_->humanoid; }
const std::string& LookAtBinding::LookAtId() const { return impl_->id; }
const std::vector<std::string>& LookAtBinding::Warnings() const { return impl_->warnings; }
avatarVrm::ExpressionAdapterConfig LookAtBinding::AdapterConfig(
    std::string evaluatorId, avatarVrm::InputIdentity gaze, std::vector<std::string> after) const {
    avatarVrm::ExpressionAdapterConfig result;
    const auto& skeleton = impl_->humanoid.Skeleton();
    result.evaluatorId = std::move(evaluatorId);
    result.layoutId = skeleton.Baseline().layout_id;
    result.layoutVersion = skeleton.Baseline().layout_version;
    result.lookAt = impl_->rig;
    result.gaze = std::move(gaze);
    result.headSkeleton = skeleton.SkeletonId(); result.headJoint = impl_->head;
    result.eyes = impl_->eyes; result.after = std::move(after);
    return result;
}
} // namespace avatarVrmUsd
