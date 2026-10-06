#include "avatarMotionUsd/StageClip.h"
#include "avatarUsd/MotionUsdReadError.h"
#include <stdexcept>

namespace avatarMotionUsd {
namespace {
void require(bool condition, const char* code, const std::string& subject) {
    if (!condition) throw std::invalid_argument(std::string(code) + ": " + subject);
}
}
struct StageClip::Impl {
    openstrata::motion::MotionStageRead read;
    openstrata::motion::SourceRestPose rest;
    Impl(const pxr::UsdStagePtr& stage, const pxr::SdfPath& path,
         const openstrata::motion::MotionStageReadOptions& inputs) {
        openstrata::motion::SkeletonReadDiagnostic diagnostic;
        if (!openstrata::motion::ReadCanonicalMotionStage(stage, path, inputs, &read, &diagnostic))
            throw avatarUsd::MotionUsdReadError(std::move(diagnostic), AR_MOTION_USD_VERSION);
        const auto skeleton = openstrata::motion::BuildSkeletonDescriptor(
            read.skeleton.jointTokens, read.skeleton.restTransforms);
        require(bool(skeleton.skeleton), "MOTION_USD_OWNER_SKELETON", path.GetString());
        auto built = openstrata::motion::BuildSourceRestPose(*skeleton.skeleton);
        require(bool(built.rest), "MOTION_USD_SOURCE_REST", path.GetString());
        rest = std::move(*built.rest);
    }
};
StageClip::StageClip(const pxr::UsdStagePtr& stage, const pxr::SdfPath& skeleton)
    : StageClip(stage, skeleton, {}) {}
StageClip::StageClip(const pxr::UsdStagePtr& stage, const pxr::SdfPath& skeleton,
                     const openstrata::motion::MotionStageReadOptions& inputs)
    : impl_(std::make_shared<Impl>(stage, skeleton, inputs)) {}
const openstrata::motion::MotionStageRead& StageClip::Read() const { return impl_->read; }
const openstrata::motion::SourceRestPose& StageClip::SourceRest() const { return impl_->rest; }
} // namespace avatarMotionUsd
