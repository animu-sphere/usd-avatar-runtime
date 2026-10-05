#pragma once
#include "avatarUsd/SkeletonBinding.h"
#include <memory>

namespace avatarVrmUsd {
struct HumanoidBindingConfig {
    pxr::SdfPath avatarRoot;
    // Empty: discover exactly one applied VrmHumanoidAPI below avatarRoot.
    // Explicit: select this active, loaded descendant prim.
    pxr::SdfPath humanoid;
    std::string layoutId;
    uint64_t layoutVersion = 0;
};
struct UnsupportedBone {
    std::string role, joint;
};

// Default-time schema snapshot. Reads the installed owner's applied schema and
// skeleton relationship, maps exact role names with motion::FindHumanJoint,
// and delegates rest/topology/unit/placement validation to SkeletonBinding.
// Partial standard maps are allowed; custom roles are retained for reporting.
// Throws invalid_argument with VRM_BINDING_* or delegated USD_BINDING_* codes.
// No stage handle is retained; copies share immutable owned storage.
class HumanoidBinding {
public:
    HumanoidBinding(const pxr::UsdStagePtr& stage, HumanoidBindingConfig config);
    const avatarUsd::SkeletonBinding& Skeleton() const;
    const std::string& HumanoidId() const;
    const std::vector<UnsupportedBone>& UnsupportedBones() const;
private:
    struct Impl;
    std::shared_ptr<const Impl> impl_;
};
} // namespace avatarVrmUsd
