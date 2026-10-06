#include "avatarMotion/MotionPoseInputBridge.h"
#include <cmath>
#include <limits>
#include <set>
#include <stdexcept>
#include <utility>

namespace avatarMotion {
namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::invalid_argument(message);
}
bool identity(const std::string& s) {
    return !s.empty() && s.find('\0') == std::string::npos;
}
bool channel(const std::string& s) {
    const auto colon = s.find(':');
    return identity(s) && colon != std::string::npos && colon != 0 && colon + 1 < s.size();
}
void validate(const MotionPoseInputBridgeConfig& c) {
    require(identity(c.source) && identity(c.actor), "Motion input requires source and actor identity");
    require(std::isfinite(c.clockScale) && c.clockScale > 0 && std::isfinite(c.clockOffset),
            "Motion input clock mapping must be finite with positive scale");
    require(c.channels.size() <= std::numeric_limits<uint32_t>::max(), "Too many motion channel mappings");
    std::set<std::string> names, ids;
    for (const auto& binding : c.channels) {
        require(identity(binding.ownerChannel) && names.insert(binding.ownerChannel).second,
                "Motion input owner channel mappings must be distinct and nonempty");
        require(channel(binding.channel) && ids.insert(binding.channel).second,
                "Motion input channels must be distinct and namespaced");
    }
    require(c.gazeChannel.empty() || (channel(c.gazeChannel) && ids.insert(c.gazeChannel).second),
            "Motion gaze channel must be namespaced and distinct from scalar mappings");
}
void validateContext(const ArInputFrame& c) {
    require(c.abi_version == AR_ABI_VERSION && c.struct_size >= sizeof(ArInputFrame),
            "Motion input context has an incompatible ABI header");
    require(c.frame_id && c.generation && std::isfinite(c.evaluation_seconds),
            "Motion input context requires valid frame identity, generation and time");
    require(!c.scalars && !c.scalar_count && !c.gazes && !c.gaze_count,
            "Motion input context must have empty arrays; composition requires explicit host selection");
    require(c.has_usd_mapping <= 1 && (!c.has_usd_mapping ||
            (std::isfinite(c.usd_time_codes_per_second) && c.usd_time_codes_per_second > 0 &&
             std::isfinite(c.usd_time_code_offset) && std::isfinite(c.evaluation_seconds *
                c.usd_time_codes_per_second + c.usd_time_code_offset))), "Invalid motion input USD clock mapping");
}
} // namespace

struct MotionInputFrame::Impl {
    MotionPoseInputBridgeConfig config;
    ArInputFrame frame;
    std::vector<ArScalarInput> scalars;
    std::vector<ArGazeInput> gazes;
    std::vector<std::string> unmapped;
    bool unmappedGaze = false;
    Impl(const MotionPoseInputBridgeConfig& c, const openstrata::motion::MotionPose* pose,
         const ArInputFrame& context, uint32_t gazeValidity, const ArDiagnosticSink& sink) : config(c), frame(context) {
        // Normalize the copied header to the representation we actually own.
        frame.struct_size = sizeof(ArInputFrame);
        if (!pose) return;
        // Validate only observations this bridge consumes, through the owner.
        // Pose-only rotations/confidence/root fields are not input channels.
        openstrata::motion::MotionPose observations;
        observations.timestamp = pose->timestamp;
        observations.channels = pose->channels;
        observations.lookAtTarget = pose->lookAtTarget;
        const auto report = openstrata::motion::ValidateMotionPose(observations);
        if (!report.IsValid()) {
            MotionValidationError error("usd-motion-plugins.motionCore", AR_MOTION_CORE_VERSION, report);
            error.Emit(sink); throw error;
        }
        require(std::isfinite(pose->timestamp * c.clockScale + c.clockOffset),
                "Motion input sample clock mapping is not finite");
        // Owner validation establishes Find preconditions; runtime checks C-string transport.
        for (const auto& entry : pose->channels.entries) {
            require(identity(entry.name), "Motion input channel names must be representable as C strings");
            bool mapped = false;
            for (const auto& binding : config.channels)
                if (binding.ownerChannel == entry.name) { mapped = true; break; }
            if (!mapped) unmapped.push_back(entry.name);
        }
        for (const auto& binding : config.channels) {
            const auto* value = pose->channels.Find(binding.ownerChannel);
            if (!value) continue;
            scalars.push_back({config.source.c_str(), config.actor.c_str(), binding.channel.c_str(),
                               double(*value), pose->timestamp, c.clockScale, c.clockOffset});
        }
        if (pose->lookAtTarget) {
            if (config.gazeChannel.empty()) unmappedGaze = true;
            else {
                ArGazeInput g{config.source.c_str(), config.actor.c_str(), config.gazeChannel.c_str(),
                              AR_GAZE_POINT, AR_GAZE_RUNTIME_WORLD, gazeValidity, nullptr, nullptr,
                              {}, pose->timestamp, c.clockScale, c.clockOffset};
                for (int k = 0; k < 3; ++k) g.value[k] = (*pose->lookAtTarget)[k];
                gazes.push_back(g);
            }
        }
        frame.scalars = scalars.empty() ? nullptr : scalars.data();
        frame.scalar_count = uint32_t(scalars.size());
        frame.gazes = gazes.empty() ? nullptr : gazes.data();
        frame.gaze_count = uint32_t(gazes.size());
    }
};
struct MotionPoseInputBridge::Impl {
    MotionPoseInputBridgeConfig config;
    explicit Impl(MotionPoseInputBridgeConfig c) : config(std::move(c)) { validate(config); }
};
MotionInputFrame::MotionInputFrame(std::shared_ptr<const Impl> impl) : impl_(std::move(impl)) {}
const ArInputFrame& MotionInputFrame::View() const { return impl_->frame; }
const std::vector<std::string>& MotionInputFrame::UnmappedChannels() const { return impl_->unmapped; }
bool MotionInputFrame::HasUnmappedGaze() const { return impl_->unmappedGaze; }
MotionPoseInputBridge::MotionPoseInputBridge(MotionPoseInputBridgeConfig config) : impl_(std::make_unique<Impl>(std::move(config))) {}
MotionPoseInputBridge::~MotionPoseInputBridge() = default;
MotionInputFrame MotionPoseInputBridge::Assemble(const openstrata::motion::MotionPose* pose,
                                               const ArInputFrame& context, uint32_t gazeValidity,
                                               ArDiagnosticSink diagnostics) const {
    validateContext(context);
    require(gazeValidity == AR_OBSERVATION_VALID || gazeValidity == AR_OBSERVATION_STALE,
            "A present owner gaze point must be explicitly valid or stale");
    return MotionInputFrame(std::make_shared<MotionInputFrame::Impl>(impl_->config, pose, context, gazeValidity, diagnostics));
}
} // namespace avatarMotion
