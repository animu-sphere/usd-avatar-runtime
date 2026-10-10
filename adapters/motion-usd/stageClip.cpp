#include "avatarMotionUsd/StageClip.h"
#include "avatarMotionUsd/ReadStageClip.h"

namespace avatarMotionUsd {
struct StageClip::Impl {
    openstrata::motion::MotionStageRead read;
    Impl(const pxr::UsdStagePtr& stage, const pxr::SdfPath& path,
         const openstrata::motion::MotionStageReadOptions& inputs)
        : read(ReadStageClip(stage, path, inputs)) {}
};
StageClip::StageClip(const pxr::UsdStagePtr& stage, const pxr::SdfPath& skeleton)
    : StageClip(stage, skeleton, {}) {}
StageClip::StageClip(const pxr::UsdStagePtr& stage, const pxr::SdfPath& skeleton,
                     const openstrata::motion::MotionStageReadOptions& inputs)
    : impl_(std::make_shared<Impl>(stage, skeleton, inputs)) {}
const openstrata::motion::MotionStageRead& StageClip::Read() const { return impl_->read; }
const openstrata::motion::SourceRestPose& StageClip::SourceRest() const { return *impl_->read.sourceRest; }
} // namespace avatarMotionUsd
