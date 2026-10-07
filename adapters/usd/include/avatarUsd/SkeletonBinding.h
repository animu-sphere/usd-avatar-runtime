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
// asserts the canonical +Z-forward basis; owner ReadMotionSkeleton (Generic)
// supplies the validated descriptor and separate rigid placement in metres.
// Explicit humanoid mappings use the owner's RetargetMap.
// Binding errors throw invalid_argument; owner reading refusals use its
// MotionUsdReadError subclass with owned code/subject/detail and version.
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
