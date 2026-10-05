#pragma once
#include "avatarRuntime/state.h"
#include "motionRetarget/RetargetMap.h"
#include "pxr/usd/usd/stage.h"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace avatarUsd {
struct HumanoidBinding {
    openstrata::motion::HumanJoint bone;
    std::string joint; // exact USD joint token, supplied by the format owner/host
};
struct SkeletonBindingConfig {
    pxr::SdfPath avatarRoot, skeleton;
    std::string layoutId;
    uint64_t layoutVersion = 0;
    std::vector<HumanoidBinding> humanoid;
};

// Read authored default-time skeleton/rest and placement once. The caller
// asserts the canonical +Z-forward basis; Y-up and units are checked here.
// Uses owner BuildSkeletonDescriptor/RetargetMap, never name heuristics.
// Throws invalid_argument with a diagnostic code and subject on bad bindings.
// Immutable owned values outlive the stage and can be shared across copies.
class SkeletonBinding {
public:
    SkeletonBinding(const pxr::UsdStagePtr& stage, SkeletonBindingConfig config);
    const ArStateView& Baseline() const;
    const openstrata::motion::SkeletonDescriptor& Skeleton() const;
    const openstrata::motion::RetargetMap& HumanoidMap() const;
    const std::vector<std::string>& JointIds() const;
    const ArTransform& RootPlacement() const;
    const std::string& SkeletonId() const;
private:
    struct Impl;
    std::shared_ptr<const Impl> impl_;
};
} // namespace avatarUsd
