#include "avatarVrmUsd/HumanoidBinding.h"
#include "vrmSchema/vrmHumanoidAPI.h"
#include "pxr/usd/usd/primRange.h"
#include "pxr/usd/sdf/types.h"
#include <set>
#include <stdexcept>

namespace avatarVrmUsd {
namespace {
void require(bool condition, const char* code, const std::string& subject) {
    if (!condition) throw std::invalid_argument(std::string(code) + ": " + subject);
}
struct Discovery {
    avatarUsd::SkeletonBindingConfig skeleton;
    std::string humanoid;
    std::vector<UnsupportedBone> unsupported;
};
Discovery discover(const pxr::UsdStagePtr& stage, const HumanoidBindingConfig& config) {
    require(bool(stage), "VRM_BINDING_STAGE", "null stage");
    require(config.avatarRoot.IsAbsolutePath() && config.avatarRoot.IsPrimPath(),
            "VRM_BINDING_AVATAR_ROOT", config.avatarRoot.GetString());
    const auto root = stage->GetPrimAtPath(config.avatarRoot);
    require(bool(root) && root.IsActive() && root.IsLoaded(),
            "VRM_BINDING_AVATAR_ROOT", config.avatarRoot.GetString());
    pxr::UsdPrim humanoid;
    if (!config.humanoid.IsEmpty()) {
        require(config.humanoid.IsAbsolutePath() && config.humanoid.IsPrimPath() &&
                config.humanoid.HasPrefix(config.avatarRoot),
                "VRM_BINDING_HUMANOID_PATH", config.humanoid.GetString());
        humanoid = stage->GetPrimAtPath(config.humanoid);
        require(bool(humanoid) && humanoid.IsActive() && humanoid.IsLoaded() &&
                humanoid.HasAPI<pxr::UsdVrmHumanoidAPI>(),
                "VRM_BINDING_HUMANOID_SCHEMA", config.humanoid.GetString());
    } else {
        // UsdPrimRange's default predicate visits active, defined, loaded,
        // non-abstract prims. Instance proxies require an explicit host policy.
        for (const auto& prim : pxr::UsdPrimRange(root)) {
            if (!prim.HasAPI<pxr::UsdVrmHumanoidAPI>()) continue;
            require(!humanoid, "VRM_BINDING_HUMANOID_AMBIGUOUS", config.avatarRoot.GetString());
            humanoid = prim;
        }
        require(bool(humanoid), "VRM_BINDING_HUMANOID_MISSING", config.avatarRoot.GetString());
    }
    pxr::UsdVrmHumanoidAPI api(humanoid);
    pxr::SdfPathVector targets;
    require(api.GetVrmSkeletonRel().GetForwardedTargets(&targets) && targets.size() == 1,
            "VRM_BINDING_SKELETON_TARGET", humanoid.GetPath().GetString());
    require(targets[0].IsAbsolutePath() && targets[0].IsPrimPath() &&
            targets[0].HasPrefix(config.avatarRoot),
            "VRM_BINDING_SKELETON_PATH", targets[0].GetString());
    Discovery result{{config.avatarRoot, targets[0], config.layoutId, config.layoutVersion, {}},
                     humanoid.GetPath().GetString(), {}};
    const auto& names = pxr::UsdVrmHumanoidAPI::GetSchemaAttributeNames(false);
    const std::set<pxr::TfToken> standard(names.begin(), names.end());
    const std::string prefix = "vrm:humanBones:";
    for (const auto& attr : humanoid.GetAttributes()) {
        const auto name = attr.GetName().GetString();
        if (name.compare(0, prefix.size(), prefix) != 0 || !attr.HasAuthoredValueOpinion()) continue;
        pxr::TfToken joint;
        require(attr.GetTypeName() == pxr::SdfValueTypeNames->Token && attr.Get(&joint) && !joint.IsEmpty(),
                "VRM_BINDING_BONE_VALUE", attr.GetPath().GetString());
        const auto role = name.substr(prefix.size());
        const auto bone = openstrata::motion::FindHumanJoint(role);
        if (standard.count(attr.GetName()) && bone) {
            result.skeleton.humanoid.push_back({*bone, joint.GetString()});
        } else {
            result.unsupported.push_back({role, joint.GetString()});
        }
    }
    return result;
}
} // namespace
struct HumanoidBinding::Impl {
    avatarUsd::SkeletonBinding skeleton;
    std::string humanoid;
    std::vector<UnsupportedBone> unsupported;
    Impl(const pxr::UsdStagePtr& stage, Discovery result)
        : skeleton(stage, std::move(result.skeleton)), humanoid(std::move(result.humanoid)),
          unsupported(std::move(result.unsupported)) {}
};
HumanoidBinding::HumanoidBinding(const pxr::UsdStagePtr& stage, HumanoidBindingConfig config)
    : impl_(std::make_shared<Impl>(stage, discover(stage, config))) {}
const avatarUsd::SkeletonBinding& HumanoidBinding::Skeleton() const { return impl_->skeleton; }
const std::string& HumanoidBinding::HumanoidId() const { return impl_->humanoid; }
const std::vector<UnsupportedBone>& HumanoidBinding::UnsupportedBones() const { return impl_->unsupported; }
} // namespace avatarVrmUsd
