#include "avatarMotionUsd/ReadStageClip.h"
#include "avatarUsd/MotionUsdReadError.h"
#include <utility>

namespace avatarMotionUsd {
openstrata::motion::MotionStageRead ReadStageClip(
    const pxr::UsdStagePtr& stage, const pxr::SdfPath& skeleton,
    const openstrata::motion::MotionStageReadOptions& inputs) {
    openstrata::motion::MotionStageRead read;
    openstrata::motion::SkeletonReadDiagnostic diagnostic;
    if (!openstrata::motion::ReadCanonicalMotionStage(stage, skeleton, inputs, &read, &diagnostic))
        throw avatarUsd::MotionUsdReadError(std::move(diagnostic), AR_MOTION_USD_VERSION);
    return read;
}
} // namespace avatarMotionUsd
