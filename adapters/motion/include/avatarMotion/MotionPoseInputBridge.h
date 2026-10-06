#pragma once
#include "avatarRuntime/input.h"
#include "avatarMotion/ValidationError.h"
#include "motionCore/MotionPose.h"
#include <memory>
#include <string>
#include <vector>

namespace avatarMotion {
struct ChannelInputBinding {
    std::string ownerChannel; // verbatim owner name
    std::string channel;      // explicit namespaced runtime identity
};
struct MotionPoseInputBridgeConfig {
    std::string source, actor;
    std::vector<ChannelInputBinding> channels;
    // Empty disables gaze mapping. Owner lookAtTarget is a runtime-world point.
    std::string gazeChannel;
    // runtime_sample_seconds = MotionPose.timestamp * clockScale + clockOffset.
    double clockScale = 1.0, clockOffset = 0.0;
};

// Immutable owned arrays and strings. Copies share ownership; views remain
// valid while any copy lives, independently of the bridge/source lifetime.
class MotionInputFrame {
public:
    const ArInputFrame& View() const;
    const std::vector<std::string>& UnmappedChannels() const;
    bool HasUnmappedGaze() const;
private:
    struct Impl;
    std::shared_ptr<const Impl> impl_;
    explicit MotionInputFrame(std::shared_ptr<const Impl> impl);
    friend class MotionPoseInputBridge;
};

// Maps already selected owner values; never samples, polls or arbitrates.
// Context must have empty input arrays. Absent pose/fields stay absent; zero
// remains present. Held/extrapolated source policy belongs to the host, which
// may explicitly mark a present gaze VALID or STALE. No scalar validity is
// invented. Invalid configuration/input throws invalid_argument. Owner
// observation rejection throws MotionValidationError; Assemble's optional
// sink receives the original report synchronously and is never retained.
class MotionPoseInputBridge {
public:
    explicit MotionPoseInputBridge(MotionPoseInputBridgeConfig config);
    ~MotionPoseInputBridge();
    MotionPoseInputBridge(const MotionPoseInputBridge&) = delete;
    MotionPoseInputBridge& operator=(const MotionPoseInputBridge&) = delete;
    MotionInputFrame Assemble(const openstrata::motion::MotionPose* pose,
                             const ArInputFrame& context,
                             uint32_t gazeValidity = AR_OBSERVATION_VALID,
                             ArDiagnosticSink diagnostics = {}) const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace avatarMotion
