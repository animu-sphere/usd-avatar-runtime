#include "avatarVrmUsd/SpringBoneBinding.h"
#include "pxr/usd/sdf/types.h"
#include "pxr/usd/usd/primRange.h"
#include "pxr/usd/usdGeom/metrics.h"
#include "vrmSchema/vrmColliderAPI.h"
#include "vrmSchema/vrmSpringBoneAPI.h"
#include <algorithm>
#include <cmath>
#include <map>

namespace avatarVrmUsd {
namespace {
void require(bool condition, const char *code, const std::string &subject) {
    if (!condition)
        throw std::invalid_argument(std::string(code) + ": " + subject);
}
template <class T> T read(const pxr::UsdAttribute &attr, const pxr::SdfValueTypeName &type) {
    T result{};
    require(attr && attr.GetTypeName() == type && attr.Get(&result), "VRM_SPRING_BINDING_VALUE",
            attr.GetPath().GetString());
    return result;
}
vrmRig::SpringBoneTransform rigid(const ArTransform &t, const std::string &subject) {
    vrmRig::SpringBoneTransform result;
    for (int k = 0; k < 3; ++k) {
        require(std::abs(t.scale[k] - 1) <= 1e-6, "VRM_SPRING_BINDING_SCALE", subject);
        result.translation[k] = float(t.translation[k]);
    }
    result.rotation =
        pxr::GfQuatf(float(t.rotation[3]), pxr::GfVec3f(float(t.rotation[0]), float(t.rotation[1]),
                                                        float(t.rotation[2])));
    return result;
}
} // namespace
struct SpringBoneBinding::Impl {
    avatarUsd::SkeletonBinding skeleton;
    vrmRig::SpringBoneRig rig;
    vrmRig::SpringBonePose pose;
    std::vector<std::string> springs, colliders, groups;
    std::map<pxr::SdfPath, int> colliderMap, groupMap;
    int joint(const pxr::TfToken &name, const std::string &subject) const {
        const auto &ids = skeleton.JointIds();
        const auto found = std::find(ids.begin(), ids.end(), name.GetString());
        require(!name.IsEmpty() && found != ids.end(), "VRM_SPRING_BINDING_JOINT",
                subject + ": " + name.GetString());
        return int(found - ids.begin());
    }
    int group(const pxr::UsdStagePtr &stage, const pxr::SdfPath &path, const pxr::SdfPath &root) {
        const auto found = groupMap.find(path);
        if (found != groupMap.end())
            return found->second;
        require(path.IsPrimPath() && path.IsAbsolutePath() && path.HasPrefix(root),
                "VRM_SPRING_BINDING_GROUP", path.GetString());
        const auto prim = stage->GetPrimAtPath(path);
        require(prim && prim.IsActive() && prim.IsLoaded() && prim.IsDefined() &&
                    !prim.IsAbstract() && !prim.HasAPI<pxr::UsdVrmColliderAPI>(),
                "VRM_SPRING_BINDING_GROUP", path.GetString());
        std::vector<int> indices;
        for (const auto &child : prim.GetChildren()) {
            require(child.HasAPI<pxr::UsdVrmColliderAPI>(), "VRM_SPRING_BINDING_COLLIDER_SCHEMA",
                    child.GetPath().GetString());
            const auto old = colliderMap.find(child.GetPath());
            if (old != colliderMap.end()) {
                indices.push_back(old->second);
                continue;
            }
            pxr::UsdVrmColliderAPI api(child);
            const auto subject = child.GetPath().GetString();
            const auto shape =
                read<pxr::TfToken>(api.GetVrmShapeAttr(), pxr::SdfValueTypeNames->Token);
            require(shape == pxr::TfToken("sphere") || shape == pxr::TfToken("capsule"),
                    "VRM_SPRING_BINDING_COLLIDER_SHAPE", subject);
            vrmRig::SpringBoneCollider c;
            c.joint = joint(read<pxr::TfToken>(api.GetVrmNodeAttr(), pxr::SdfValueTypeNames->Token),
                            subject);
            c.shape = shape == pxr::TfToken("sphere") ? vrmRig::SpringBoneColliderShape::Sphere
                                                      : vrmRig::SpringBoneColliderShape::Capsule;
            c.offset = read<pxr::GfVec3f>(api.GetVrmOffsetAttr(), pxr::SdfValueTypeNames->Float3);
            c.radius = read<float>(api.GetVrmRadiusAttr(), pxr::SdfValueTypeNames->Float);
            if (c.shape == vrmRig::SpringBoneColliderShape::Capsule)
                c.tail = read<pxr::GfVec3f>(api.GetVrmTailAttr(), pxr::SdfValueTypeNames->Float3);
            const int index = int(rig.colliders.size());
            colliderMap.emplace(child.GetPath(), index);
            colliders.push_back(subject);
            rig.colliders.push_back(c);
            indices.push_back(index);
        }
        const int index = int(rig.colliderGroups.size());
        groupMap.emplace(path, index);
        groups.push_back(path.GetString());
        rig.colliderGroups.push_back(std::move(indices));
        return index;
    }
    Impl(const pxr::UsdStagePtr &stage, const avatarUsd::SkeletonBindingConfig &config)
        : skeleton(stage, config) {
        require(pxr::UsdGeomGetStageMetersPerUnit(stage) == 1.0, "VRM_SPRING_BINDING_UNITS",
                config.avatarRoot.GetString());
        const auto root = stage->GetPrimAtPath(config.avatarRoot);
        const auto contract = root.GetCustomDataByKey(pxr::TfToken("vrm:schemaContractVersion"));
        require(contract.IsHolding<int>() && contract.UncheckedGet<int>() == 2,
                "VRM_SPRING_BINDING_CONTRACT", config.avatarRoot.GetString());
        // Descriptor rest is skeleton-local. Baseline() has placement already
        // applied to roots, so using it here would apply placement twice.
        for (const auto &j : skeleton.Skeleton().GetJoints()) {
            rig.parents.push_back(j.parent);
            ArTransform t{};
            for (int k = 0; k < 3; ++k) {
                t.translation[k] = j.restTranslation[k];
                t.scale[k] = j.restScale[k];
                t.rotation[k] = j.restRotation.GetImaginary()[k];
            }
            t.rotation[3] = j.restRotation.GetReal();
            rig.restTransforms.push_back(rigid(t, j.token));
        }
        pose.localTransforms = rig.restTransforms;
        pose.placement = rigid(skeleton.RootPlacement(), skeleton.SkeletonId());
        for (const auto &prim : pxr::UsdPrimRange(root)) {
            if (!prim.HasAPI<pxr::UsdVrmSpringBoneAPI>())
                continue;
            const auto subject = prim.GetPath().GetString();
            pxr::UsdVrmSpringBoneAPI api(prim);
            const bool chain = api.GetVrmJointsAttr().HasAuthoredValueOpinion();
            const bool roots = api.GetVrmRootJointsAttr().HasAuthoredValueOpinion();
            require(chain != roots, "VRM_SPRING_BINDING_DECLARATION", subject);
            const auto names =
                read<pxr::VtTokenArray>(roots ? api.GetVrmRootJointsAttr() : api.GetVrmJointsAttr(),
                                        pxr::SdfValueTypeNames->TokenArray);
            vrmRig::SpringBoneSpring spring;
            auto &joints = roots ? spring.rootJoints : spring.joints;
            for (const auto &name : names)
                joints.push_back(joint(name, subject));
            const auto stiffness = read<pxr::VtFloatArray>(api.GetVrmStiffnessAttr(),
                                                           pxr::SdfValueTypeNames->FloatArray);
            const auto drag = read<pxr::VtFloatArray>(api.GetVrmDragForceAttr(),
                                                      pxr::SdfValueTypeNames->FloatArray);
            const auto gravity = read<pxr::VtFloatArray>(api.GetVrmGravityPowerAttr(),
                                                         pxr::SdfValueTypeNames->FloatArray);
            const auto radius = read<pxr::VtFloatArray>(api.GetVrmHitRadiusAttr(),
                                                        pxr::SdfValueTypeNames->FloatArray);
            const auto direction = read<pxr::VtVec3fArray>(api.GetVrmGravityDirAttr(),
                                                           pxr::SdfValueTypeNames->Float3Array);
            require(stiffness.size() == names.size() && drag.size() == names.size() &&
                        gravity.size() == names.size() && radius.size() == names.size() &&
                        direction.size() == names.size(),
                    "VRM_SPRING_BINDING_PARAMETERS", subject);
            for (size_t i = 0; i < names.size(); ++i)
                spring.parameters.push_back(
                    {stiffness[i], drag[i], gravity[i], direction[i], radius[i]});
            if (api.GetVrmCenterAttr().HasAuthoredValueOpinion())
                spring.center =
                    joint(read<pxr::TfToken>(api.GetVrmCenterAttr(), pxr::SdfValueTypeNames->Token),
                          subject);
            pxr::SdfPathVector targets;
            const auto rel = api.GetVrmColliderGroupsRel();
            if (rel.HasAuthoredTargets())
                require(rel.GetForwardedTargets(&targets), "VRM_SPRING_BINDING_GROUP_TARGETS",
                        subject);
            for (const auto &target : targets)
                spring.colliderGroups.push_back(group(stage, target, config.avatarRoot));
            springs.push_back(subject);
            rig.springs.push_back(std::move(spring));
        }
        require(!springs.empty(), "VRM_SPRING_BINDING_MISSING", config.avatarRoot.GetString());
        // Expansion, parameter mathematics and overlapping solver ownership are
        // exclusively vrmRig's. Keep its refusal detail and installed version.
        vrmRig::SpringBoneSolver solver;
        vrmRig::SpringBoneResult result;
        std::string error;
        if (!solver.Configure(rig, &error) || !solver.Reset(pose, &result, &error))
            throw SpringBoneOwnerError(config.avatarRoot.GetString(), error,
                                       AR_VRM_SPRING_OWNER_VERSION);
    }
};
SpringBoneBinding::SpringBoneBinding(const pxr::UsdStagePtr &stage,
                                     avatarUsd::SkeletonBindingConfig config)
    : impl_(std::make_shared<Impl>(stage, config)) {}
const avatarUsd::SkeletonBinding &SpringBoneBinding::Skeleton() const { return impl_->skeleton; }
const vrmRig::SpringBoneRig &SpringBoneBinding::Rig() const { return impl_->rig; }
const vrmRig::SpringBonePose &SpringBoneBinding::RestPose() const { return impl_->pose; }
const std::vector<std::string> &SpringBoneBinding::SpringIds() const { return impl_->springs; }
const std::vector<std::string> &SpringBoneBinding::ColliderIds() const { return impl_->colliders; }
const std::vector<std::string> &SpringBoneBinding::ColliderGroupIds() const {
    return impl_->groups;
}
} // namespace avatarVrmUsd
