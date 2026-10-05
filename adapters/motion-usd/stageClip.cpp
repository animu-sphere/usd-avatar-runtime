#include "avatarMotionUsd/StageClip.h"
#include "avatarUsd/SkeletonBinding.h"
#include "pxr/usd/usdGeom/metrics.h"
#include <cmath>
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
    Impl(const pxr::UsdStagePtr& stage, const pxr::SdfPath& path) {
        require(bool(stage), "MOTION_USD_STAGE", "null stage");
        // The owner reader carries stage-space translations verbatim. Refuse
        // unsupported units/placement rather than silently treating them as world metres.
        require(pxr::UsdGeomGetStageMetersPerUnit(stage) == 1.0, "MOTION_USD_UNITS", path.GetString());
        require(std::isfinite(stage->GetTimeCodesPerSecond()) && stage->GetTimeCodesPerSecond() > 0,
                "MOTION_USD_RATE", path.GetString());
        avatarUsd::SkeletonBinding source(stage, {path, path, "motion.source", 1, {}});
        const auto& placement = source.RootPlacement();
        for (int k = 0; k < 3; ++k)
            require(std::abs(placement.translation[k]) <= 1e-12 &&
                    std::abs(placement.rotation[k]) <= 1e-12,
                    "MOTION_USD_PLACEMENT", path.GetString());
        require(std::abs(std::abs(placement.rotation[3]) - 1) <= 1e-12,
                "MOTION_USD_PLACEMENT", path.GetString());
        std::string error;
        const bool readOk = openstrata::motion::ReadMotionStage(stage, path.GetString(), &read, &error);
        require(readOk, "MOTION_USD_READ", path.GetString() + ": " + error);
        auto built = openstrata::motion::BuildSourceRestPose(source.Skeleton());
        require(bool(built.rest), "MOTION_USD_SOURCE_REST", path.GetString());
        rest = std::move(*built.rest);
    }
};
StageClip::StageClip(const pxr::UsdStagePtr& stage, const pxr::SdfPath& skeleton)
    : impl_(std::make_shared<Impl>(stage, skeleton)) {}
const openstrata::motion::MotionStageRead& StageClip::Read() const { return impl_->read; }
const openstrata::motion::SourceRestPose& StageClip::SourceRest() const { return impl_->rest; }
} // namespace avatarMotionUsd
