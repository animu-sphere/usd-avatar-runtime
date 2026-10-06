#pragma once
#include "motionUsd/ClipReader.h"
#include "motionRetarget/RestPose.h"
#include <memory>

namespace avatarMotionUsd {
// Host-side, immutable configuration. The installed format plugin opens the
// stage; motionUsd reads semantic samples and motionRetarget builds source rest.
// Explicit skeleton selection is required. Scoped to Y-up, metre stages with
// identity skeleton placement and authored rest. No stage is retained or edited.
// Copy Read().clip and SourceRest() into ClipPoseAdapterConfig together.
// Throws avatarUsd::MotionUsdReadError for owner reading refusals, preserving
// code/subject/detail and version; it remains an invalid_argument subclass.
// Owner source-rest builder failures retain MOTION_USD_SOURCE_REST.
class StageClip {
public:
    StageClip(const pxr::UsdStagePtr& stage, const pxr::SdfPath& skeleton);
    const openstrata::motion::MotionStageRead& Read() const;
    const openstrata::motion::SourceRestPose& SourceRest() const;
private:
    struct Impl;
    std::shared_ptr<const Impl> impl_;
};
} // namespace avatarMotionUsd
