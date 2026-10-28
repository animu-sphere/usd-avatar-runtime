#pragma once
#include "avatarUsd/SkeletonBinding.h"
#include "vrmRig/SpringBoneSolver.h"
#include <stdexcept>
#include <utility>

namespace avatarVrmUsd {
// A retained owner refusal; no numeric owner code is invented for this API.
class SpringBoneOwnerError : public std::invalid_argument {
public:
    SpringBoneOwnerError(std::string subject, std::string detail, std::string version)
        : std::invalid_argument("VRM_SPRING_BINDING_OWNER: " + subject + ": " + detail),
          subject_(std::move(subject)), detail_(std::move(detail)), version_(std::move(version)) {}
    const char *Owner() const noexcept { return "vrmRig"; }
    const std::string &OwnerVersion() const noexcept { return version_; }
    const std::string &Subject() const noexcept { return subject_; }
    const std::string &Detail() const noexcept { return detail_; }

private:
    std::string subject_, detail_, version_;
};

// Default-time, contract-v2 snapshot scoped to the explicit avatar/skeleton.
// Rigid metre stages only until the owner's scale/unit question is resolved.
// Discovers applied spring APIs and follows their collider-group relationships.
// No stage, solver state, evaluator or clock is retained; copies share owned values.
class SpringBoneBinding {
public:
    SpringBoneBinding(const pxr::UsdStagePtr &stage, avatarUsd::SkeletonBindingConfig config);
    const avatarUsd::SkeletonBinding &Skeleton() const;
    const vrmRig::SpringBoneRig &Rig() const;
    const vrmRig::SpringBonePose &RestPose() const;
    const std::vector<std::string> &SpringIds() const;
    const std::vector<std::string> &ColliderIds() const;
    const std::vector<std::string> &ColliderGroupIds() const;

private:
    struct Impl;
    std::shared_ptr<const Impl> impl_;
};
} // namespace avatarVrmUsd
