#include "avatarUsd/SkeletonBinding.h"
#include "avatarUsd/MotionUsdReadError.h"
#include "pxr/base/gf/rotation.h"
#include "pxr/usd/usd/prim.h"
#include <cmath>
#include <set>
#include <stdexcept>

namespace avatarUsd {
namespace {
namespace motion = openstrata::motion;
void require(bool condition, const char* code, const std::string& subject) {
    if (!condition) throw std::invalid_argument(std::string(code) + ": " + subject);
}
ArTransform transform(const motion::SkeletonJoint& joint) {
    ArTransform t{};
    for (int k = 0; k < 3; ++k) {
        t.translation[k] = joint.restTranslation[k];
        t.rotation[k] = joint.restRotation.GetImaginary()[k];
        t.scale[k] = joint.restScale[k];
        require(std::isfinite(t.translation[k]) && std::isfinite(t.scale[k]) && t.scale[k] > 0,
                "USD_BINDING_FLOAT_RANGE", joint.token);
    }
    const auto q = joint.restRotation.GetNormalized();
    require(std::isfinite(q.GetReal()) && std::isfinite(q.GetImaginary().GetLength()),
            "USD_BINDING_FLOAT_RANGE", joint.token);
    for (int k = 0; k < 3; ++k) t.rotation[k] = q.GetImaginary()[k];
    t.rotation[3] = q.GetReal();
    return t;
}
} // namespace

struct SkeletonBinding::Impl {
    SkeletonBindingConfig config;
    std::string skeletonId;
    std::vector<std::string> ids;
    motion::SkeletonDescriptor skeleton;
    motion::RetargetMap map;
    ArTransform placement{{0,0,0},{0,0,0,1},{1,1,1}};
    std::vector<ArJoint> joints;
    ArStateView baseline{AR_HEADER(ArStateView)};

    Impl(const pxr::UsdStagePtr& stage, SkeletonBindingConfig c) : config(std::move(c)) {
        require(bool(stage), "USD_BINDING_STAGE", "null stage");
        require(!config.layoutId.empty() && config.layoutVersion, "USD_BINDING_LAYOUT", config.layoutId);
        require(config.avatarRoot.IsAbsolutePath() && config.avatarRoot.IsPrimPath() &&
                bool(stage->GetPrimAtPath(config.avatarRoot)), "USD_BINDING_AVATAR_ROOT", config.avatarRoot.GetString());
        require(config.skeleton.IsAbsolutePath() && config.skeleton.IsPrimPath() &&
                config.skeleton.HasPrefix(config.avatarRoot), "USD_BINDING_SKELETON_PATH", config.skeleton.GetString());
        motion::SkeletonStageRead read;
        motion::SkeletonReadDiagnostic diagnostic;
        if (!motion::ReadSkeleton(stage, config.skeleton, &read, &diagnostic))
            throw MotionUsdReadError(std::move(diagnostic), AR_MOTION_USD_VERSION);
        ids = read.skeleton.jointTokens;
        auto built = motion::BuildSkeletonDescriptor(ids, read.skeleton.restTransforms);
        require(bool(built.skeleton), "USD_BINDING_OWNER_SKELETON", config.skeleton.GetString());
        skeleton = std::move(*built.skeleton);
        // The owner readers must agree on the joint order used by the runtime.
        for (size_t i = 0; i < ids.size(); ++i)
            require(read.parents[i] == skeleton.GetJoints()[i].parent,
                    "USD_BINDING_PARENT_MAPPING", ids[i]);
        // Preserve owner float rotations, normalized for the runtime tolerance.
        std::vector<motion::SkeletonJoint> normalized = skeleton.GetJoints();
        for (auto& joint : normalized) {
            const auto t = transform(joint);
            joint.restRotation = pxr::GfQuatf(float(t.rotation[3]), pxr::GfVec3f(
                float(t.rotation[0]), float(t.rotation[1]), float(t.rotation[2])));
        }
        skeleton = motion::SkeletonDescriptor(std::move(normalized));
        std::set<motion::HumanJoint> bones;
        std::set<std::string> targets;
        for (const auto& binding : config.humanoid) {
            require(motion::IsValidHumanJoint(binding.bone) && bones.insert(binding.bone).second &&
                    targets.insert(binding.joint).second, "USD_BINDING_HUMANOID_DUPLICATE", binding.joint);
            require(map.SetJointToken(binding.bone, binding.joint, skeleton),
                    "USD_BINDING_HUMANOID_JOINT", binding.joint);
        }
        const auto& rotation = read.worldRotation;
        for (int k = 0; k < 3; ++k) {
            placement.translation[k] = read.worldTranslation[k];
            placement.rotation[k] = rotation.GetImaginary()[k];
            require(std::isfinite(placement.translation[k]), "USD_BINDING_PLACEMENT_RANGE", config.skeleton.GetString());
        }
        placement.rotation[3] = rotation.GetReal();
        skeletonId = config.skeleton.GetString();
        for (size_t i = 0; i < ids.size(); ++i) {
            auto local = transform(skeleton.GetJoints()[i]);
            if (skeleton.GetJoints()[i].parent < 0) {
                const auto position = pxr::GfRotation(rotation).TransformDir(pxr::GfVec3d(
                    local.translation[0],local.translation[1],local.translation[2]));
                const auto q = (rotation * pxr::GfQuatd(local.rotation[3], pxr::GfVec3d(
                    local.rotation[0],local.rotation[1],local.rotation[2]))).GetNormalized();
                for (int k = 0; k < 3; ++k) {
                    local.translation[k] = position[k] + placement.translation[k];
                    local.rotation[k] = q.GetImaginary()[k];
                    require(std::isfinite(local.translation[k]), "USD_BINDING_PLACEMENT_RANGE", ids[i]);
                }
                local.rotation[3] = q.GetReal();
            }
            joints.push_back({skeletonId.c_str(), ids[i].c_str(), skeleton.GetJoints()[i].parent, local});
        }
        baseline.joints = joints.data(); baseline.joint_count = uint32_t(joints.size());
        baseline.layout_id = config.layoutId.c_str(); baseline.layout_version = config.layoutVersion;
    }
};
SkeletonBinding::SkeletonBinding(const pxr::UsdStagePtr& stage, SkeletonBindingConfig config)
    : impl_(std::make_shared<Impl>(stage, std::move(config))) {}
const ArStateView& SkeletonBinding::Baseline() const { return impl_->baseline; }
const motion::SkeletonDescriptor& SkeletonBinding::Skeleton() const { return impl_->skeleton; }
const motion::RetargetMap& SkeletonBinding::HumanoidMap() const { return impl_->map; }
const std::vector<std::string>& SkeletonBinding::JointIds() const { return impl_->ids; }
const ArTransform& SkeletonBinding::RootPlacement() const { return impl_->placement; }
const std::string& SkeletonBinding::SkeletonId() const { return impl_->skeletonId; }
} // namespace avatarUsd
