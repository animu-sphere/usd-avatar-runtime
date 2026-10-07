#pragma once
#include "motionUsd/ClipReader.h"
#include "motionRetarget/RestPose.h"
#include <memory>

namespace avatarMotionUsd {
// Host-side, immutable configuration. The installed format plugin opens the
// stage; motionUsd returns coherent semantic samples, descriptor and source rest.
// Explicit skeleton selection is required. Scoped to Y-up, metre stages with
// identity skeleton placement and authored rest. No stage is retained or edited.
// Copy Read().clip and SourceRest() into ClipPoseAdapterConfig together.
// Throws avatarUsd::MotionUsdReadError for owner reading refusals, preserving
// code/subject/detail and version; it remains an invalid_argument subclass.
// Read() retains the owner's descriptor/sourceRest; SourceRest() aliases that rest.
class StageClip {
public:
    StageClip(const pxr::UsdStagePtr& stage, const pxr::SdfPath& skeleton);
    // Attribute selection is supplied by the format owner or host. The adapter
    // neither discovers native attributes nor interprets them itself. Gaze is
    // returned in canonical clip space; place it before world-gaze assembly.
    StageClip(const pxr::UsdStagePtr& stage, const pxr::SdfPath& skeleton,
              const openstrata::motion::MotionStageReadOptions& inputs);
    const openstrata::motion::MotionStageRead& Read() const;
    const openstrata::motion::SourceRestPose& SourceRest() const;
private:
    struct Impl;
    std::shared_ptr<const Impl> impl_;
};
} // namespace avatarMotionUsd
