#include "avatarUsd/SkeletonBinding.h"
#include "pxr/base/gf/rotation.h"
#include "pxr/usd/usdGeom/metrics.h"
#include "pxr/usd/usdGeom/tokens.h"
#include "pxr/usd/usdGeom/xformCache.h"
#include "pxr/usd/usdSkel/skeleton.h"
#include "pxr/usd/usdSkel/topology.h"
#include <cmath>
#include <limits>
#include <set>
#include <stdexcept>

namespace avatarUsd {
namespace {
namespace motion = openstrata::motion;
void require(bool condition, const char* code, const std::string& subject) {
    if (!condition) throw std::invalid_argument(std::string(code) + ": " + subject);
}
// The owner decomposition intentionally removes shear. Reject anything the
// dense TRS boundary cannot reproduce before calling the owner.
void validateMatrix(const pxr::GfMatrix4d& m, const std::string& subject, bool rigid) {
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c)
            require(std::isfinite(m[r][c]), "USD_BINDING_NONFINITE", subject);
    require(m[0][3] == 0 && m[1][3] == 0 && m[2][3] == 0 && m[3][3] == 1,
            "USD_BINDING_NONAFFINE", subject);
    pxr::GfVec3d rows[3];
    for (int r = 0; r < 3; ++r) {
        rows[r] = pxr::GfVec3d(m[r][0], m[r][1], m[r][2]);
        const double length = rows[r].GetLength();
        require(std::isfinite(length) && length > 0 && length <= std::numeric_limits<float>::max(),
                "USD_BINDING_SCALE", subject);
        require(!rigid || std::abs(length - 1) <= 1e-6, "USD_BINDING_PLACEMENT_SCALE", subject);
        rows[r] /= length;
    }
    for (int r = 0; r < 3; ++r)
        for (int c = r + 1; c < 3; ++c)
            require(std::abs(pxr::GfDot(rows[r], rows[c])) <= 1e-6, "USD_BINDING_SHEAR", subject);
    require(m.GetDeterminant3() > 0, "USD_BINDING_REFLECTION", subject);
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
        const pxr::UsdSkelSkeleton usdSkeleton(stage->GetPrimAtPath(config.skeleton));
        require(bool(usdSkeleton), "USD_BINDING_SKELETON", config.skeleton.GetString());
        require(pxr::UsdGeomGetStageUpAxis(stage) == pxr::UsdGeomTokens->y,
                "USD_BINDING_UP_AXIS", config.avatarRoot.GetString());
        const double units = pxr::UsdGeomGetStageMetersPerUnit(stage);
        require(std::isfinite(units) && units > 0, "USD_BINDING_UNITS", config.avatarRoot.GetString());
        pxr::VtTokenArray tokens;
        pxr::VtMatrix4dArray rest;
        require(usdSkeleton.GetJointsAttr().Get(&tokens) && !tokens.empty() &&
                tokens.size() <= size_t(std::numeric_limits<int32_t>::max()),
                "USD_BINDING_JOINTS", config.skeleton.GetString());
        require(usdSkeleton.GetRestTransformsAttr().Get(&rest) && rest.size() == tokens.size(),
                "USD_BINDING_REST_COUNT", config.skeleton.GetString());
        std::set<std::string> unique;
        std::vector<pxr::GfMatrix4d> matrices;
        for (size_t i = 0; i < tokens.size(); ++i) {
            const auto token = tokens[i].GetString();
            const pxr::SdfPath path(token);
            require(!token.empty() && path.IsPrimPath() && !path.IsAbsolutePath() &&
                    unique.insert(token).second, "USD_BINDING_JOINT_TOKEN", token);
            ids.push_back(token);
            validateMatrix(rest[i], token, false);
            auto matrix = rest[i];
            for (int k = 0; k < 3; ++k) {
                matrix[3][k] *= units;
                require(std::isfinite(matrix[3][k]) && std::abs(matrix[3][k]) <= std::numeric_limits<float>::max(),
                        "USD_BINDING_FLOAT_RANGE", token);
            }
            matrices.push_back(matrix);
        }
        auto built = motion::BuildSkeletonDescriptor(ids, matrices);
        require(bool(built.skeleton), "USD_BINDING_OWNER_SKELETON", config.skeleton.GetString());
        skeleton = std::move(*built.skeleton);
        require(skeleton.IsTopologicallyOrdered(), "USD_BINDING_PARENT_ORDER", config.skeleton.GetString());
        pxr::UsdSkelTopology topology(tokens);
        std::string reason;
        require(topology.Validate(&reason), "USD_BINDING_TOPOLOGY", config.skeleton.GetString() + ": " + reason);
        for (size_t i = 0; i < ids.size(); ++i)
            require(topology.GetParent(i) == skeleton.GetJoints()[i].parent,
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
        pxr::UsdGeomXformCache cache;
        auto world = cache.GetLocalToWorldTransform(usdSkeleton.GetPrim());
        validateMatrix(world, config.skeleton.GetString(), true);
        const auto rotation = world.ExtractRotationQuat().GetNormalized();
        for (int k = 0; k < 3; ++k) {
            placement.translation[k] = world[3][k] * units;
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
