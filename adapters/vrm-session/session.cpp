#include "avatarVrmSession/Session.h"
#include "avatarMotion/ClipPoseAdapter.h"
#include "avatarVrm/SpringBoneAdapter.h"
#include "avatarVrmUsd/ExpressionBinding.h"
#include "avatarVrmUsd/LookAtBinding.h"
#include "avatarVrmUsd/SpringBoneBinding.h"
#include "avatarMotion/MotionPoseInputBridge.h"
#include "motionSampling/MotionSource.h"
#include "pxr/base/gf/quatd.h"
#include <algorithm>
#include <limits>
#include <optional>
#include <stdexcept>

namespace avatarVrmSession {
namespace {
void ok(ArStatus status) {
    if (status != AR_OK) throw std::runtime_error("VRM session runtime status: " + std::to_string(status));
}
// Clip observations for each tick: owner sampling and the owner-value bridge, with
// the hold policy and the placement of clip space in the runtime's world.
class ClipObservations {
public:
    ClipObservations(openstrata::motion::MotionClip clip, avatarMotion::MotionPoseInputBridgeConfig config, const ArTransform& placement)
        : clip_(std::move(clip)), bridge_(std::move(config)), placement_(placement) {}
    ArInputFrame Tick(uint64_t frameId, uint64_t generation, double seconds, const ArInputFrame* host, const ArDiagnosticSink* sink) {
        namespace motion = openstrata::motion;
        const auto sample = motion::SampleClip(clip_, seconds);
        if (!sample) throw std::logic_error("VRM_SESSION_CLIP: nonempty clip required");
        motion::MotionPose observed;
        observed.timestamp = sample.pose->timestamp; observed.channels = sample.pose->channels;
        observed.lookAtTarget = sample.pose->lookAtTarget;
        // SampleClip restamps a held pose; report the time of the boundary sample used.
        const bool held = sample.status == motion::PoseSampleStatus::Held;
        if (held) observed.timestamp = seconds < clip_.samples.front().timestamp
            ? clip_.samples.front().timestamp : clip_.samples.back().timestamp;
        if (observed.lookAtTarget) {
            const auto& p = placement_;
            const pxr::GfQuatd rotation(p.rotation[3], pxr::GfVec3d(p.rotation[0], p.rotation[1], p.rotation[2]));
            observed.lookAtTarget = pxr::GfVec3f(rotation.Transform(pxr::GfVec3d(*observed.lookAtTarget)) +
                pxr::GfVec3d(p.translation[0], p.translation[1], p.translation[2]));
        }
        ArInputFrame context{AR_HEADER(ArInputFrame)};
        context.frame_id = frameId; context.generation = generation; context.evaluation_seconds = seconds;
        frame_ = bridge_.Assemble(&observed, context, held ? AR_OBSERVATION_STALE : AR_OBSERVATION_VALID,
            sink ? *sink : ArDiagnosticSink{});
        const auto& clipFrame = frame_->View();
        scalars_.assign(clipFrame.scalars, clipFrame.scalars + clipFrame.scalar_count);
        gazes_.assign(clipFrame.gazes, clipFrame.gazes + clipFrame.gaze_count);
        if (host) {
            scalars_.insert(scalars_.end(), host->scalars, host->scalars + host->scalar_count);
            gazes_.insert(gazes_.end(), host->gazes, host->gazes + host->gaze_count);
        }
        ArInputFrame input{AR_HEADER(ArInputFrame)};
        input.scalars = scalars_.data(); input.scalar_count = uint32_t(scalars_.size());
        input.gazes = gazes_.data(); input.gaze_count = uint32_t(gazes_.size());
        input.input_revision = frameId; // sampled content changes every tick and replays by frame
        return input;
    }
private:
    openstrata::motion::MotionClip clip_;
    avatarMotion::MotionPoseInputBridge bridge_;
    ArTransform placement_;
    std::optional<avatarMotion::MotionInputFrame> frame_;
    std::vector<ArScalarInput> scalars_;
    std::vector<ArGazeInput> gazes_;
};
}
struct Session::Impl {
    avatarVrmUsd::HumanoidBinding humanoid;
    std::unique_ptr<avatarMotion::ClipPoseAdapter> motion;
    std::unique_ptr<avatarVrm::ExpressionAdapter> face;
    std::unique_ptr<avatarVrm::SpringBoneAdapter> spring;
    std::unique_ptr<ClipObservations> clipObservations;
    ArRuntimeApi api{};
    ArRuntime runtime = 0;
    std::unique_ptr<avatarRuntime::FixedTickSession> clock;
    uint64_t request = 0, generation = 1;
    bool paused = true;
    Impl(const pxr::UsdStagePtr& stage, const openstrata::motion::MotionStageRead& clip,
        const SessionConfig& config) : humanoid(stage, config.avatar) {
        if (!clip.sourceRest || clip.clip.samples.empty())
            throw std::invalid_argument("VRM_SESSION_CLIP: nonempty clip and coherent sourceRest required");
        const auto& binding = humanoid.Skeleton();
        const auto& baseline = binding.Baseline();
        avatarMotion::ClipPoseAdapterConfig m;
        m.evaluatorId = "vrm.session.motion"; m.layoutId = baseline.layout_id; m.layoutVersion = baseline.layout_version;
        m.skeletonId = binding.SkeletonId(); m.skeleton = binding.Skeleton(); m.map = binding.HumanoidMap();
        m.jointIds = binding.JointIds(); m.rootPlacement = binding.RootPlacement();
        m.clip = clip.clip; m.sourceRest = *clip.sourceRest;
        motion = std::make_unique<avatarMotion::ClipPoseAdapter>(std::move(m));
        if (!config.expressions && !config.expressionInputs.empty())
            throw std::invalid_argument("VRM_SESSION_EXPRESSION: expression inputs require selecting expressions");
        std::optional<avatarVrmUsd::ExpressionBinding> expressions;
        if (config.expressions) expressions.emplace(stage, avatarVrmUsd::ExpressionBindingConfig{config.avatar, {}});
        if (!config.expressions && !config.clipExpressions.empty())
            throw std::invalid_argument("VRM_SESSION_EXPRESSION: clip expressions require selecting expressions");
        if (config.clipGaze && !config.lookAt)
            throw std::invalid_argument("VRM_SESSION_LOOKAT: clip gaze requires selecting LookAt");
        if (config.clipGaze && !(config.gaze.source.empty() && config.gaze.actor.empty() && config.gaze.channel.empty()))
            throw std::invalid_argument("VRM_SESSION_LOOKAT: gaze comes from either the clip or the host");
        auto inputs = config.expressionInputs;
        auto gaze = config.gaze;
        if (!config.clipExpressions.empty() || config.clipGaze) {
            avatarMotion::MotionPoseInputBridgeConfig b;
            b.source = "vrm.session.motion"; b.actor = binding.SkeletonId();
            const auto authored = [&](const auto& has) { return std::any_of(clip.clip.samples.begin(), clip.clip.samples.end(), has); };
            for (const auto& e : config.clipExpressions) {
                if (!expressions->Rig().Find(e.expression))
                    throw std::invalid_argument("VRM_SESSION_EXPRESSION: the avatar does not declare clip expression " + e.expression);
                if (!authored([&](const auto& s) { return s.channels.Find(e.channel) != nullptr; }))
                    throw std::invalid_argument("VRM_SESSION_CLIP: the clip does not author channel " + e.channel);
                b.channels.push_back({e.channel, "clip:" + e.channel});
                inputs.push_back({{b.source, b.actor, "clip:" + e.channel}, e.expression});
            }
            if (config.clipGaze) {
                if (!authored([](const auto& s) { return s.lookAtTarget.has_value(); }))
                    throw std::invalid_argument("VRM_SESSION_CLIP: the clip does not author a gaze");
                b.gazeChannel = "motion:lookAtTarget";
                gaze = {b.source, b.actor, b.gazeChannel};
            }
            clipObservations = std::make_unique<ClipObservations>(clip.clip, std::move(b), binding.RootPlacement());
        }
        if (config.lookAt || config.expressions) {
            avatarVrm::ExpressionAdapterConfig f;
            if (config.lookAt) {
                f = avatarVrmUsd::LookAtBinding(stage, avatarVrmUsd::LookAtBindingConfig{config.avatar, {}})
                    .AdapterConfig("vrm.session.face", gaze, {"vrm.session.motion"});
                if (f.lookAt->type == vrmRig::LookAtType::Expression && !expressions)
                    throw std::invalid_argument("VRM_SESSION_LOOKAT: expression-type LookAt requires selecting expressions");
                if (expressions) expressions->ApplyTo(f);
                f.inputs = inputs;
            } else f = expressions->AdapterConfig("vrm.session.face", inputs, {"vrm.session.motion"});
            face = std::make_unique<avatarVrm::ExpressionAdapter>(std::move(f));
        }
        if (config.secondaryMotion) {
            avatarVrmUsd::SpringBoneBinding springBinding(stage,
                {config.avatar.avatarRoot, pxr::SdfPath(binding.SkeletonId()), baseline.layout_id, baseline.layout_version, {}});
            avatarVrm::SpringBoneAdapterConfig s;
            s.evaluatorId = "vrm.session.spring"; s.layoutId = baseline.layout_id; s.layoutVersion = baseline.layout_version;
            s.skeletonId = binding.SkeletonId(); s.jointIds = binding.JointIds();
            s.rig = springBinding.Rig(); s.placement = springBinding.RestPose().placement;
            s.fixedStepSeconds = config.clock.fixedStepSeconds; s.after = {"vrm.session.motion"};
            if (face) s.after.push_back("vrm.session.face");
            spring = std::make_unique<avatarVrm::SpringBoneAdapter>(std::move(s));
        }
        ok(arGetApi(AR_ABI_VERSION, sizeof(api), &api));
        ok(api.create_runtime(&runtime));
        try {
            const auto md = motion->Descriptor(); ok(api.register_evaluator(runtime, &md, nullptr));
            std::vector<const char*> ids{md.id};
            std::vector<ArCapability> caps(md.supplies, md.supplies + md.supply_count);
            if (face) {
                const auto fd = face->Descriptor(); ok(api.register_evaluator(runtime, &fd, nullptr));
                ids.push_back(fd.id); caps.insert(caps.end(), fd.supplies, fd.supplies + fd.supply_count);
            }
            if (spring) {
                const auto sd = spring->Descriptor(); ok(api.register_evaluator(runtime, &sd, nullptr));
                ids.push_back(sd.id); caps.insert(caps.end(), sd.supplies, sd.supplies + sd.supply_count);
            }
            ArInstanceDesc d{AR_HEADER(ArInstanceDesc)};
            d.generation = generation; d.layout_id = baseline.layout_id; d.layout_version = baseline.layout_version;
            d.initial_state = expressions ? expressions->Baseline() : baseline; d.evaluators = ids.data(); d.evaluator_count = uint32_t(ids.size());
            d.bound_capabilities = caps.data(); d.bound_capability_count = uint32_t(caps.size());
            ArInstance instance = 0; ok(api.create_instance(runtime, &d, nullptr, &instance));
            clock = std::make_unique<avatarRuntime::FixedTickSession>(runtime, instance, config.clock);
        } catch (...) { api.destroy_runtime(runtime); runtime = 0; throw; }
    }
    void close() {
        if (clock) { ok(clock->Close()); clock.reset(); }
        if (runtime) { ok(api.destroy_runtime(runtime)); runtime = 0; }
        paused = true;
    }
    ~Impl() { close(); }
    ArStatus advanceClip(double elapsed, const ArInputFrame* host, const ArDiagnosticSink* sink) {
        return clock->Advance(++request, elapsed, paused, [&](uint64_t frame, double seconds) {
            return clipObservations->Tick(frame, generation, seconds, host, sink);
        }, sink);
    }
};
Session::Session(const pxr::UsdStagePtr& avatar, const openstrata::motion::MotionStageRead& clip,
    SessionConfig config) : impl_(std::make_unique<Impl>(avatar, clip, config)) {}
Session::~Session() = default;
void Session::Play() { if (impl_->clock) impl_->paused = false; }
void Session::Pause() { impl_->paused = true; }
bool Session::IsPaused() const { return impl_->paused; }
ArStatus Session::Advance(double elapsed, const ArDiagnosticSink* sink) {
    auto& s = *impl_;
    if (!s.clock || s.request == std::numeric_limits<uint64_t>::max()) return AR_INVALID_STATE;
    if (s.clipObservations) return s.advanceClip(elapsed, nullptr, sink);
    ArInputFrame input{AR_HEADER(ArInputFrame)};
    input.input_revision = s.request + 1;
    return s.clock->Advance(++s.request, elapsed, s.paused, input, sink);
}
ArStatus Session::Advance(double elapsed, const ArInputFrame& observations, const ArDiagnosticSink* sink) {
    auto& s = *impl_;
    if (!s.clock || s.request == std::numeric_limits<uint64_t>::max()) return AR_INVALID_STATE;
    if (s.clipObservations) return s.advanceClip(elapsed, &observations, sink);
    ArInputFrame input{AR_HEADER(ArInputFrame)};
    input.scalars = observations.scalars; input.scalar_count = observations.scalar_count;
    input.gazes = observations.gazes; input.gaze_count = observations.gaze_count;
    input.input_revision = observations.input_revision;
    return s.clock->Advance(++s.request, elapsed, s.paused, input, sink);
}
ArStatus Session::Reset(double start) {
    auto& s = *impl_; s.paused = true;
    if (!s.clock || s.generation == std::numeric_limits<uint64_t>::max()) return AR_INVALID_STATE;
    const auto status = s.clock->Reset(s.generation + 1, start);
    if (status == AR_OK) { ++s.generation; s.request = 0; }
    return status;
}
void Session::Close() { impl_->close(); }
const avatarRuntime::FixedTickStats& Session::Stats() const {
    if (!impl_->clock) throw std::logic_error("VRM_SESSION_CLOSED");
    return impl_->clock->Stats();
}
const avatarUsd::SkeletonBinding& Session::Binding() const { return impl_->humanoid.Skeleton(); }
const ArRuntimeApi& Session::Api() const { return impl_->api; }
ArSnapshot Session::Snapshot() const { return impl_->clock ? impl_->clock->Snapshot() : 0; }
} // namespace avatarVrmSession
