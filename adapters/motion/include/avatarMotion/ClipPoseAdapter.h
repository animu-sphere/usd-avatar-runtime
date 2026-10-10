#pragma once
#include "avatarRuntime/api.h"
#include "avatarMotion/ValidationError.h"
#include "motionSampling/MotionSource.h"
#include "motionRetarget/PoseRetargeter.h"
#include <memory>
#include <string>
#include <vector>

namespace avatarMotion {
// Owner values stay in this optional C++ layer, outside the runtime C ABI.
struct ClipPoseAdapterConfig {
    std::string evaluatorId, layoutId, skeletonId;
    uint64_t layoutVersion = 0;
    openstrata::motion::MotionClip clip;
    openstrata::motion::SkeletonDescriptor skeleton;
    openstrata::motion::RetargetMap map;
    openstrata::motion::SourceRestPose sourceRest;
    openstrata::motion::RetargetOptions options;
    // One explicit runtime joint identity per owner skeleton slot. Owner
    // parents must match the runtime layout; roots remain runtime-world.
    std::vector<std::string> jointIds;
    // runtime_seconds = clip_seconds * clockScale + clockOffset.
    double clockScale = 1.0, clockOffset = 0.0;
    // Identity of the reported pose sample; empty source/actor default to the
    // evaluator/skeleton identity. The channel must be namespaced.
    std::string sourceId, actorId, channelId = "motion:pose";
    // Rigid skeleton-to-runtime-world placement, applied once to each root
    // after owner retargeting. Source/target rest remain skeleton-local.
    ArTransform rootPlacement{{0, 0, 0}, {0, 0, 0, 1}, {1, 1, 1}};
    std::vector<std::string> after;
};

// Immutable, stateless clip evaluation: SampleClip -> PoseRetargeter -> pose
// writer, atomic in RETARGET. Empty clips leave the working pose untouched;
// out-of-range requests use the owner's boundary hold and emit a diagnostic.
// Each sampled pose reports its clip sample time and owner status with the
// snapshot; a held pose keeps the boundary sample's time.
// Channels, gaze, confidence and contacts are not published by this adapter.
// Keep alive until runtime destruction. Invalid configuration throws
// invalid_argument; malformed owner values throw MotionValidationError with
// the original report/version. The optional construction sink is invoked only
// synchronously on owner rejection and is never retained.
// No stage, renderer, OpenExec or input acquisition.
class ClipPoseAdapter {
public:
    explicit ClipPoseAdapter(ClipPoseAdapterConfig config, ArDiagnosticSink diagnostics = {});
    ~ClipPoseAdapter();
    ClipPoseAdapter(const ClipPoseAdapter&) = delete;
    ClipPoseAdapter& operator=(const ClipPoseAdapter&) = delete;
    ArEvaluatorDesc Descriptor() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace avatarMotion
