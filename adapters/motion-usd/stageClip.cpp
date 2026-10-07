#include "avatarMotionUsd/StageClip.h"
#include "avatarUsd/MotionUsdReadError.h"

namespace avatarMotionUsd {
struct StageClip::Impl {
    openstrata::motion::MotionStageRead read;
    Impl(const pxr::UsdStagePtr& stage, const pxr::SdfPath& path,
         const openstrata::motion::MotionStageReadOptions& inputs) {
        openstrata::motion::SkeletonReadDiagnostic diagnostic;
        if (!openstrata::motion::ReadCanonicalMotionStage(stage, path, inputs, &read, &diagnostic))
            throw avatarUsd::MotionUsdReadError(std::move(diagnostic), AR_MOTION_USD_VERSION);
    }
};
StageClip::StageClip(const pxr::UsdStagePtr& stage, const pxr::SdfPath& skeleton)
    : StageClip(stage, skeleton, {}) {}
StageClip::StageClip(const pxr::UsdStagePtr& stage, const pxr::SdfPath& skeleton,
                     const openstrata::motion::MotionStageReadOptions& inputs)
    : impl_(std::make_shared<Impl>(stage, skeleton, inputs)) {}
const openstrata::motion::MotionStageRead& StageClip::Read() const { return impl_->read; }
const openstrata::motion::SourceRestPose& StageClip::SourceRest() const { return *impl_->read.sourceRest; }
} // namespace avatarMotionUsd
