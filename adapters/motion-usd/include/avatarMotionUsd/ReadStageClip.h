#pragma once
#include "motionUsd/ClipReader.h"

namespace avatarMotionUsd {
// Thin throwing entry point to the installed strict owner reader. Returns the
// owner's value without a runtime clip wrapper, retaining all metadata/warnings
// and its coherent descriptor/sourceRest. No stage or options are retained.
// Explicit skeleton and optional format-owner attribute selection are required;
// gaze remains in canonical clip space. Supply clip and *sourceRest together to
// ClipPoseAdapterConfig, with explicit host target bindings and clock mapping.
// Throws avatarUsd::MotionUsdReadError with owned owner diagnostic/version data.
openstrata::motion::MotionStageRead ReadStageClip(
    const pxr::UsdStagePtr& stage, const pxr::SdfPath& skeleton,
    const openstrata::motion::MotionStageReadOptions& inputs = {});
} // namespace avatarMotionUsd
