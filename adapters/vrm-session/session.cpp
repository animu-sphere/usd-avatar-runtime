#include "avatarVrmSession/Session.h"
#include "avatarMotion/ClipPoseAdapter.h"
#include "avatarVrm/SpringBoneAdapter.h"
#include "avatarVrmUsd/ExpressionBinding.h"
#include "avatarVrmUsd/LookAtBinding.h"
#include "avatarVrmUsd/SpringBoneBinding.h"
#include <limits>
#include <optional>
#include <stdexcept>

namespace avatarVrmSession {
namespace {
void ok(ArStatus status) {
    if (status != AR_OK) throw std::runtime_error("VRM session runtime status: " + std::to_string(status));
}
}
struct Session::Impl {
    avatarVrmUsd::HumanoidBinding humanoid;
    std::unique_ptr<avatarMotion::ClipPoseAdapter> motion;
    std::unique_ptr<avatarVrm::ExpressionAdapter> face;
    std::unique_ptr<avatarVrm::SpringBoneAdapter> spring;
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
        if (config.lookAt || config.expressions) {
            avatarVrm::ExpressionAdapterConfig f;
            if (config.lookAt) {
                f = avatarVrmUsd::LookAtBinding(stage, avatarVrmUsd::LookAtBindingConfig{config.avatar, {}})
                    .AdapterConfig("vrm.session.face", config.gaze, {"vrm.session.motion"});
                if (f.lookAt->type == vrmRig::LookAtType::Expression && !expressions)
                    throw std::invalid_argument("VRM_SESSION_LOOKAT: expression-type LookAt requires selecting expressions");
                if (expressions) expressions->ApplyTo(f);
                f.inputs = config.expressionInputs;
            } else f = expressions->AdapterConfig("vrm.session.face", config.expressionInputs, {"vrm.session.motion"});
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
    ArInputFrame input{AR_HEADER(ArInputFrame)};
    input.input_revision = s.request + 1;
    return s.clock->Advance(++s.request, elapsed, s.paused, input, sink);
}
ArStatus Session::Advance(double elapsed, const ArInputFrame& observations, const ArDiagnosticSink* sink) {
    auto& s = *impl_;
    if (!s.clock || s.request == std::numeric_limits<uint64_t>::max()) return AR_INVALID_STATE;
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
