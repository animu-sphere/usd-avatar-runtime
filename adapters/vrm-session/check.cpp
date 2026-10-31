#include "avatarVrmSession/Session.h"
#include "avatarVrmUsd/ExpressionBinding.h"
#include "avatarVrmUsd/LookAtBinding.h"
#include "avatarVrmUsd/SpringBoneBinding.h"
#include "vrmRig/MaterialColorSlots.h"
#include "avatarUsd/SkeletonPoseOutput.h"
#include "motionRetarget/PoseRetargeter.h"
#include "motionSampling/MotionSource.h"
#include "pxr/usd/usdSkel/animation.h"
#include "pxr/usd/usd/primRange.h"
#include "pxr/usd/usdSkel/skeleton.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>

namespace {
void require(bool value, const std::string& message) { if (!value) throw std::runtime_error(message); }
void ok(ArStatus status) { require(status == AR_OK, "Runtime status " + std::to_string(status)); }
void AR_CALL diagnostic(void*, const ArDiagnostic* d) {
    if (d->severity == AR_SEVERITY_ERROR) std::cerr << d->origin << ':' << d->code << ": " << d->message << '\n';
}
ArStateView view(const ArRuntimeApi& api, ArSnapshot s) {
    ArStateView v{AR_HEADER(ArStateView)}; ok(api.get_snapshot(s, &v)); return v;
}
double rotationError(const double* actual, pxr::GfQuatd q) {
    q.Normalize();
    double dot = actual[3]*q.GetReal();
    for (int k = 0; k < 3; ++k) dot += actual[k]*q.GetImaginary()[k];
    if (dot < 0) q = -q;
    double error = std::abs(actual[3] - q.GetReal());
    for (int k = 0; k < 3; ++k) error = std::max(error, std::abs(actual[k] - q.GetImaginary()[k]));
    return error;
}
void identical(const ArStateView& a, const ArStateView& b) {
    require(a.joint_count == b.joint_count && std::string(a.layout_id) == b.layout_id &&
        a.layout_version == b.layout_version && a.evaluation_seconds == b.evaluation_seconds, "Replay layout/time");
    for (uint32_t i = 0; i < a.joint_count; ++i) {
        require(std::string(a.joints[i].joint_id) == b.joints[i].joint_id &&
            std::string(a.joints[i].skeleton_id) == b.joints[i].skeleton_id &&
            a.joints[i].parent_index == b.joints[i].parent_index, "Replay joint identity");
        for (int k = 0; k < 3; ++k) require(a.joints[i].local.translation[k] == b.joints[i].local.translation[k] &&
            a.joints[i].local.scale[k] == b.joints[i].local.scale[k], "Replay translation/scale");
        for (int k = 0; k < 4; ++k) require(a.joints[i].local.rotation[k] == b.joints[i].local.rotation[k], "Replay rotation");
    }
    require(a.blend_shape_count == b.blend_shape_count && a.material_count == b.material_count, "Replay output layout");
    for (uint32_t i = 0; i < a.blend_shape_count; ++i)
        require(a.blend_shapes[i].weight == b.blend_shapes[i].weight, "Replay blend shape");
    for (uint32_t i = 0; i < a.material_count; ++i) {
        require(a.materials[i].overridden == b.materials[i].overridden, "Replay material override");
        for (int k = 0; k < 4; ++k) require(a.materials[i].value[k] == b.materials[i].value[k], "Replay material");
    }
}
struct Retained {
    ArRuntimeApi api; ArSnapshot handle = 0;
    ~Retained() { if (handle) api.release_snapshot(handle); }
};
// Host-side worked example: selected observations for each tick, and what the owner
// resolves from them against the session's animated pose. Mirrors the adapter's
// marshalling so that a difference is one of composition, not of rounding.
class FaceCheck {
public:
    FaceCheck(const pxr::UsdStagePtr& stage, const avatarVrmUsd::HumanoidBindingConfig& avatar)
        : binding_(stage, avatarVrmUsd::ExpressionBindingConfig{avatar, {}}),
          config_(avatarVrmUsd::LookAtBinding(stage, avatarVrmUsd::LookAtBindingConfig{avatar, {}})
              .AdapterConfig("vrm.check.face", {"check", "actor", "gaze:point"})),
          resolver_((binding_.ApplyTo(config_), config_.expressions)), lookAt_(*config_.lookAt) {
        for (const auto& expression : config_.expressions.GetExpressions()) {
            channels_.push_back("expr:" + expression.name);
            names_.push_back(expression.name);
        }
        const auto rest = HeadWorld(binding_.Baseline());
        origin_ = pxr::GfVec3d(rest.position);
        for (uint32_t i = 0; i < binding_.Baseline().material_count; ++i)
            materialBase_.push_back(binding_.Baseline().materials[i]);
    }
    std::vector<avatarVrm::ExpressionInputBinding> Inputs() const {
        std::vector<avatarVrm::ExpressionInputBinding> inputs;
        for (size_t i = 0; i < names_.size(); ++i) inputs.push_back({{"check", "actor", channels_[i]}, names_[i]});
        return inputs;
    }
    avatarVrm::InputIdentity Gaze() const { return config_.gaze; }
    // Each expression sweeps and is regularly absent, as is the gaze.
    ArInputFrame Observe(uint64_t tick, double seconds) {
        scalars_.clear();
        for (size_t i = 0; i < names_.size(); ++i) {
            if ((tick + i) % 7 == 0) continue;
            ArScalarInput s{"check", "actor", channels_[i].c_str()};
            s.value = .5 + .5*std::sin(double(tick)*.1 + double(i));
            s.source_seconds = seconds; s.clock_scale = 1; scalars_.push_back(s);
        }
        hasGaze_ = tick % 30 != 29;
        gaze_ = ArGazeInput{"check", "actor", "gaze:point", AR_GAZE_POINT, AR_GAZE_RUNTIME_WORLD, AR_OBSERVATION_VALID};
        const double angle = double(tick)*.05;
        gaze_.value[0] = origin_[0] + .3*std::sin(angle); gaze_.value[1] = origin_[1] + .2*std::cos(angle);
        gaze_.value[2] = origin_[2] + 1;
        gaze_.source_seconds = seconds; gaze_.clock_scale = 1;
        ArInputFrame frame{AR_HEADER(ArInputFrame)};
        frame.scalars = scalars_.data(); frame.scalar_count = uint32_t(scalars_.size());
        frame.gazes = hasGaze_ ? &gaze_ : nullptr; frame.gaze_count = hasGaze_ ? 1 : 0;
        frame.input_revision = tick + 1;
        return frame;
    }
    // What the clip states at this tick, sampled separately from the session: channels
    // named vrm:<expression> and the placed gaze, which is stale outside the clip.
    void ObserveClip(const openstrata::motion::MotionClip& clip, const ArTransform& p, bool gaze, double seconds) {
        const auto sample = openstrata::motion::SampleClip(clip, seconds);
        require(bool(sample), "Owner clip sample");
        scalars_.clear();
        for (size_t i = 0; i < names_.size(); ++i)
            if (const auto* value = sample.pose->channels.Find("vrm:" + names_[i])) {
                ArScalarInput s{"check", "actor", channels_[i].c_str()};
                s.value = *value; s.source_seconds = seconds; s.clock_scale = 1; scalars_.push_back(s);
            }
        hasGaze_ = gaze && sample.pose->lookAtTarget && sample.status != openstrata::motion::PoseSampleStatus::Held;
        if (!hasGaze_) return;
        const pxr::GfQuatd rotation(p.rotation[3], {p.rotation[0],p.rotation[1],p.rotation[2]});
        const pxr::GfVec3f target(rotation.Transform(pxr::GfVec3d(*sample.pose->lookAtTarget)) + pxr::GfVec3d(p.translation));
        for (int k = 0; k < 3; ++k) gaze_.value[k] = target[k];
    }
    // Resolves the last observation over the animated pose; returns replaced eyes.
    std::map<uint32_t, pxr::GfQuatd> Resolve(const ArStateView& animated) {
        openstrata::motion::MotionChannelSet weights;
        for (const auto& s : scalars_) for (size_t i = 0; i < names_.size(); ++i)
            if (channels_[i] == s.channel_id) weights.Set(names_[i], float(s.value));
        std::map<uint32_t, pxr::GfQuatd> eyes;
        if (hasGaze_) {
            vrmRig::LookAtInput input;
            input.timestamp = animated.evaluation_seconds; input.head = HeadWorld(animated);
            input.target = pxr::GfVec3f(pxr::GfVec3d(gaze_.value[0], gaze_.value[1], gaze_.value[2]));
            const auto result = lookAt_.Evaluate(input);
            for (const auto& eye : result.eyeRotations) for (const auto& bound : config_.eyes) if (bound.ownerJoint == eye.joint)
                for (uint32_t i = 0; i < animated.joint_count; ++i) if (bound.joint == animated.joints[i].joint_id) {
                    const auto& r = bound.restRotation;
                    eyes[i] = (pxr::GfQuatd(eye.rotation)*pxr::GfQuatd(r[3], {r[0],r[1],r[2]})).GetNormalized();
                }
            for (const auto& value : result.expressions.entries) weights.Set(value.name, value.value);
        }
        resolved_ = resolver_.Resolve(weights);
        return eyes;
    }
    double OutputError(const ArStateView& state) {
        double error = 0;
        std::map<std::pair<std::string, std::string>, double> morphs;
        for (const auto& target : resolved_.morphTargets) for (const auto& b : config_.morphs)
            if (b.ownerTarget == target.target) morphs[{b.mesh, b.target}] = target.weight;
        for (uint32_t i = 0; i < state.blend_shape_count; ++i) {
            const auto& shape = state.blend_shapes[i];
            const auto found = morphs.find({shape.mesh_id, shape.target_id});
            const double expected = found == morphs.end() ? 0 : found->second;
            error = std::max(error, std::abs(shape.weight - expected));
            if (shape.weight != 0) morphChanged_.insert(i);
        }
        std::map<std::pair<std::string, std::string>, pxr::GfVec4d> materials;
        for (const auto& color : resolved_.materialColors) {
            const auto* slot = vrmRig::FindMaterialColorSlot(color.colorType);
            require(slot != nullptr, "Owner material slot");
            pxr::GfVec4f base(0, 0, 0, 1);
            for (const auto& input : materialBase_) if (color.material == input.material_id) {
                if (std::string(slot->colorInput) == input.input_id) for (int k = 0; k < 3; ++k) base[k] = float(input.value[k]);
                if (slot->alphaInput && std::string(slot->alphaInput) == input.input_id) base[3] = float(input.value[0]);
            }
            const auto value = color.Apply(base);
            materials[{color.material, slot->colorInput}] = pxr::GfVec4d(value[0], value[1], value[2], 0);
            if (slot->alphaInput) materials[{color.material, slot->alphaInput}] = pxr::GfVec4d(value[3], 0, 0, 0);
        }
        for (uint32_t i = 0; i < state.material_count; ++i) {
            const auto& input = state.materials[i];
            const auto found = materials.find({input.material_id, input.input_id});
            require(input.overridden == (found == materials.end() ? 0u : 1u), "Material override state");
            for (int k = 0; k < 4; ++k) error = std::max(error, std::abs(input.value[k] -
                (found == materials.end() ? materialBase_[i].value[k] : found->second[k])));
            if (input.overridden) materialChanged_.insert(i);
        }
        return error;
    }
    bool BoneType() const { return !config_.eyes.empty(); }
    const std::set<uint32_t>& MorphChanged() const { return morphChanged_; }
    const std::set<uint32_t>& MaterialChanged() const { return materialChanged_; }
    bool ExpectsMorphs() const { return !config_.morphs.empty(); }
    bool ExpectsMaterials() const { return !materialBase_.empty(); }
private:
    vrmRig::LookAtHead HeadWorld(const ArStateView& state) const {
        std::vector<pxr::GfVec3d> positions; std::vector<pxr::GfQuatd> rotations;
        for (uint32_t i = 0; i < state.joint_count; ++i) {
            const auto& j = state.joints[i]; const auto& t = j.local;
            pxr::GfVec3d p(0); pxr::GfQuatd q(1);
            if (j.parent_index >= 0) { p = positions[size_t(j.parent_index)]; q = rotations[size_t(j.parent_index)]; }
            p += q.Transform(pxr::GfVec3d(t.translation[0], t.translation[1], t.translation[2]));
            q = (q*pxr::GfQuatd(t.rotation[3], {t.rotation[0],t.rotation[1],t.rotation[2]})).GetNormalized();
            positions.push_back(p); rotations.push_back(q);
            if (config_.headJoint == j.joint_id) return {pxr::GfQuatf(q), pxr::GfVec3f(p)};
        }
        throw std::runtime_error("Head joint is missing from state");
    }
    avatarVrmUsd::ExpressionBinding binding_;
    avatarVrm::ExpressionAdapterConfig config_;
    vrmRig::ExpressionResolver resolver_;
    vrmRig::LookAtEvaluator lookAt_;
    std::vector<std::string> names_, channels_;
    std::vector<ArScalarInput> scalars_;
    std::vector<ArMaterialInput> materialBase_;
    ArGazeInput gaze_{};
    bool hasGaze_ = false;
    pxr::GfVec3d origin_{0};
    vrmRig::ResolvedExpressions resolved_;
    std::set<uint32_t> morphChanged_, materialChanged_;
};
}
int avatarVrmSessionCheck(int argc, char** argv) {
    try {
        bool springs = true, faces = false;
        require(argc >= 5, "Usage: avatarVrmSessionCheck avatar.vrm clip.vrma fixed_seconds ticks [--no-springs] [--face] "
            "[--clip-expression name-attribute value-attribute]... [--clip-gaze attribute]");
        openstrata::motion::MotionStageReadOptions clipInputs;
        for (int i = 5; i < argc; ++i) {
            const std::string option = argv[i];
            if (option == "--no-springs") springs = false;
            else if (option == "--face") faces = true;
            else if (option == "--clip-expression" && i + 2 < argc) { clipInputs.channels.push_back({argv[i+1], argv[i+2], "vrm:"}); i += 2; }
            else if (option == "--clip-gaze" && i + 1 < argc) clipInputs.lookAtTargetAttributePath = argv[++i];
            else require(false, "Unknown or incomplete option " + option);
        }
        const bool clipGaze = !clipInputs.lookAtTargetAttributePath.empty();
        const bool clipFace = clipGaze || !clipInputs.channels.empty();
        require(!(faces && clipFace), "--face generates the observations that --clip-* reads");
        auto stage = pxr::UsdStage::Open(argv[1]); auto clipStage = pxr::UsdStage::Open(argv[2]);
        require(bool(stage) && bool(clipStage), "Cannot open avatar/clip");
        pxr::SdfPath clipSkeleton;
        for (const auto& prim : clipStage->Traverse()) if (prim.IsA<pxr::UsdSkelSkeleton>()) {
            require(clipSkeleton.IsEmpty(), "Clip skeleton is ambiguous"); clipSkeleton = prim.GetPath();
        }
        require(!clipSkeleton.IsEmpty() && bool(stage->GetDefaultPrim()), "Missing clip skeleton/avatar default prim");
        const auto clip = avatarMotionUsd::ReadStageClip(clipStage, clipSkeleton, clipInputs);
        require(!clip.clip.samples.empty(), "Empty clip"); clipStage.Reset();
        const double dt = std::stod(argv[3]);
        const auto ticks = std::stoull(argv[4]);
        require(std::isfinite(dt) && dt > 0 && ticks > 0 && ticks <= 1000000, "Invalid duration/tick count");
        avatarVrmSession::SessionConfig config;
        config.avatar = {stage->GetDefaultPrim().GetPath(), {}, "vrm.session.check", 1};
        config.clock = {clip.clip.samples.front().timestamp, dt, 2}; config.secondaryMotion = springs;
        std::optional<FaceCheck> face;
        if (faces) {
            face.emplace(stage, config.avatar);
            config.expressions = config.lookAt = true; config.expressionInputs = face->Inputs(); config.gaze = face->Gaze();
        } else if (clipFace) {
            face.emplace(stage, config.avatar);
            config.expressions = config.lookAt = true; config.clipGaze = clipGaze;
            if (!clipGaze) config.gaze = face->Gaze();
            std::set<std::string> channels;
            for (const auto& sample : clip.clip.samples) for (const auto& entry : sample.channels.entries)
                if (entry.name.rfind("vrm:", 0) == 0) channels.insert(entry.name);
            for (const auto& channel : channels) config.clipExpressions.push_back({channel, channel.substr(4)});
        }
        avatarVrmSession::Session session(stage, clip, config);
        const auto binding = session.Binding();
        auto motionConfig = config; motionConfig.secondaryMotion = false;
        motionConfig.expressions = motionConfig.lookAt = motionConfig.clipGaze = false;
        motionConfig.expressionInputs.clear(); motionConfig.clipExpressions.clear();
        avatarVrmSession::Session animated(stage, clip, motionConfig);
        std::unique_ptr<avatarVrmUsd::SpringBoneBinding> springBinding;
        vrmRig::SpringBoneSolver direct;
        if (springs) {
            springBinding = std::make_unique<avatarVrmUsd::SpringBoneBinding>(stage,
                avatarUsd::SkeletonBindingConfig{config.avatar.avatarRoot, pxr::SdfPath(binding.SkeletonId()),
                    config.avatar.layoutId, 1, {}});
            std::string error; require(direct.Configure(springBinding->Rig(), &error), error);
        }
        // Optional USD display transport is tested at display boundaries only.
        avatarUsd::SkeletonPoseOutput output(stage, binding);
        ArDiagnosticSink sink{nullptr, diagnostic};
        const auto advance = [&](double elapsed, uint64_t tick) {
            if (clipFace) face->ObserveClip(clip.clip, binding.RootPlacement(), clipGaze, config.clock.startSeconds + double(tick)*dt);
            if (!face || clipFace) return session.Advance(elapsed, &sink);
            return session.Advance(elapsed, face->Observe(tick, config.clock.startSeconds + double(tick)*dt), &sink);
        };
        ok(advance(0, 0)); ok(animated.Advance(0, &sink));
        Retained seed{session.Api(), session.Snapshot()}; ok(seed.api.retain_snapshot(seed.handle));
        session.Play(); animated.Play();
        openstrata::motion::PoseRetargeter retarget(binding.Skeleton(), binding.HumanoidMap(), *clip.sourceRest);
        double maxOwnerError = 0, maxMotionError = 0, maxOutputError = 0, maxScaleError = 0, maxFaceError = 0;
        std::set<uint32_t> changedMotion, changedSpring, changedEyes;
        for (uint64_t tick = 0; tick <= ticks; ++tick) {
            if (tick) { ok(advance(dt, tick)); ok(animated.Advance(dt, &sink)); }
            require(session.Stats().tick == tick, "One fixed tick expected");
            const auto a = view(session.Api(), session.Snapshot());
            const auto m = view(animated.Api(), animated.Snapshot());
            // The pose the spring phase receives: motion, then any resolved eyes.
            std::map<uint32_t, pxr::GfQuatd> eyes;
            if (face) {
                eyes = face->Resolve(m);
                maxFaceError = std::max(maxFaceError, face->OutputError(a));
                for (const auto& [i, q] : eyes) changedEyes.insert(i);
            }
            const auto beforeSpring = [&](uint32_t i) {
                auto q = m.joints[i].local;
                const auto eye = eyes.find(i);
                if (eye != eyes.end()) {
                    for (int k = 0; k < 3; ++k) q.rotation[k] = eye->second.GetImaginary()[k];
                    q.rotation[3] = eye->second.GetReal();
                }
                return q;
            };
            const auto sampled = openstrata::motion::SampleClip(clip.clip, a.evaluation_seconds);
            require(bool(sampled), "Owner clip sample");
            const auto expectedMotion = retarget.Retarget(*sampled.pose);
            const auto& placement = binding.RootPlacement();
            const pxr::GfQuatd place(placement.rotation[3], {placement.rotation[0],placement.rotation[1],placement.rotation[2]});
            for (uint32_t i = 0; i < m.joint_count; ++i) {
                auto q = pxr::GfQuatd(expectedMotion.rotations[i]); auto t = pxr::GfVec3d(expectedMotion.translations[i]);
                if (m.joints[i].parent_index < 0) {
                    q = place*q; t = place.Transform(t) + pxr::GfVec3d(placement.translation);
                }
                maxMotionError = std::max(maxMotionError, rotationError(m.joints[i].local.rotation, q));
                for (int k = 0; k < 3; ++k) maxMotionError = std::max(maxMotionError, std::abs(m.joints[i].local.translation[k]-t[k]));
                const auto& rest = binding.Baseline().joints[i].local;
                bool moved = rotationError(m.joints[i].local.rotation,
                    pxr::GfQuatd(rest.rotation[3], {rest.rotation[0],rest.rotation[1],rest.rotation[2]})) > 1e-6;
                for (int k = 0; k < 3; ++k) moved = moved || std::abs(m.joints[i].local.translation[k] - rest.translation[k]) > 1e-6;
                if (moved) changedMotion.insert(i);
            }
            if (springs) {
                const auto& rest = springBinding->RestPose();
                vrmRig::SpringBonePose pose = rest;
                const auto placeQ = pxr::GfQuatd(rest.placement.rotation);
                // Root motion is a common rigid placement shift, as admitted by the binding.
                for (uint32_t i = 0; i < m.joint_count; ++i) if (m.joints[i].parent_index < 0) {
                    pose.placement.translation = pxr::GfVec3f(pxr::GfVec3d(m.joints[i].local.translation) -
                        placeQ.Transform(pxr::GfVec3d(rest.localTransforms[i].translation))); break;
                }
                for (uint32_t i = 0; i < m.joint_count; ++i) {
                    const auto local = beforeSpring(i);
                    auto q = pxr::GfQuatd(local.rotation[3], {local.rotation[0],local.rotation[1],local.rotation[2]});
                    auto t = pxr::GfVec3d(local.translation);
                    if (m.joints[i].parent_index < 0) {
                        q = placeQ.GetInverse()*q;
                        t = placeQ.GetInverse().Transform(t-pxr::GfVec3d(pose.placement.translation));
                    }
                    pose.localTransforms[i] = {pxr::GfVec3f(t), pxr::GfQuatf(q.GetNormalized())};
                }
                vrmRig::SpringBoneResult expected; std::string error;
                require(tick ? direct.Step(float(dt), pose, &expected, &error) : direct.Reset(pose, &expected, &error), error);
                for (uint32_t i = 0; i < a.joint_count; ++i) {
                    auto q = pxr::GfQuatd(expected.localRotations[i]);
                    if (a.joints[i].parent_index < 0) q = placeQ*q;
                    maxOwnerError = std::max(maxOwnerError, rotationError(a.joints[i].local.rotation, q));
                    const auto before = beforeSpring(i);
                    if (rotationError(a.joints[i].local.rotation, pxr::GfQuatd(before.rotation[3],
                        {before.rotation[0],before.rotation[1],before.rotation[2]})) > 1e-6)
                        changedSpring.insert(i);
                }
            } else if (face) {
                for (uint32_t i = 0; i < a.joint_count; ++i) {
                    const auto before = beforeSpring(i);
                    maxOwnerError = std::max(maxOwnerError, rotationError(a.joints[i].local.rotation,
                        pxr::GfQuatd(before.rotation[3], {before.rotation[0],before.rotation[1],before.rotation[2]})));
                }
            } else identical(a, m);
            // Deliver the same committed state twice; this is a consumer retry.
            output.Apply(session.Api(), session.Snapshot()); output.Apply(session.Api(), session.Snapshot());
            const pxr::UsdSkelAnimation displayed(stage->GetPrimAtPath(output.AnimationPath()));
            pxr::VtVec3fArray translations; pxr::VtQuatfArray rotations; pxr::VtVec3hArray scales;
            pxr::VtTokenArray joints;
            require(displayed.GetJointsAttr().Get(&joints) && displayed.GetTranslationsAttr().Get(&translations) &&
                displayed.GetRotationsAttr().Get(&rotations) && displayed.GetScalesAttr().Get(&scales) &&
                joints.size() == a.joint_count && translations.size() == a.joint_count &&
                rotations.size() == a.joint_count && scales.size() == a.joint_count, "Incomplete displayed pose");
            for (uint32_t i = 0; i < a.joint_count; ++i) {
                require(joints[i].GetString() == a.joints[i].joint_id, "Displayed joint identity");
                auto t = pxr::GfVec3d(a.joints[i].local.translation);
                auto q = pxr::GfQuatd(a.joints[i].local.rotation[3], {a.joints[i].local.rotation[0],
                    a.joints[i].local.rotation[1],a.joints[i].local.rotation[2]});
                if (a.joints[i].parent_index < 0) {
                    t = place.GetInverse().Transform(t-pxr::GfVec3d(placement.translation)); q = place.GetInverse()*q;
                }
                const double values[]{rotations[i].GetImaginary()[0],rotations[i].GetImaginary()[1],
                    rotations[i].GetImaginary()[2],rotations[i].GetReal()};
                maxOutputError = std::max(maxOutputError, rotationError(values, q));
                for (int k = 0; k < 3; ++k) {
                    maxOutputError = std::max(maxOutputError, std::abs(double(translations[i][k])-t[k]));
                    const double scale = a.joints[i].local.scale[k];
                    const double scaleError = std::abs(double(scales[i][k])-scale);
                    require(scaleError <= std::max(1.0, std::abs(scale))*5e-4, "USD half scale quantization");
                    maxScaleError = std::max(maxScaleError, scaleError);
                }
            }
        }
        require(maxOwnerError <= 1e-6 && maxMotionError <= 1e-6 && maxOutputError <= 1e-6 && maxFaceError <= 1e-6,
            "Owner/runtime/output/face value mismatch: " + std::to_string(maxOwnerError) + "/" +
            std::to_string(maxMotionError) + "/" + std::to_string(maxOutputError) + "/" + std::to_string(maxFaceError));
        require(!changedMotion.empty(), "Clip did not move avatar");
        if (springs) require(!changedSpring.empty(), "No secondary motion observed");
        if (face) {
            // A clip drives what it authors, so only generated observations must reach everything.
            require((clipFace && config.clipExpressions.empty()) || !face->ExpectsMorphs() || !face->MorphChanged().empty(), "No morph weight observed");
            require(clipFace || !face->ExpectsMaterials() || !face->MaterialChanged().empty(), "No material override observed");
            require((clipFace && !clipGaze) || !face->BoneType() || !changedEyes.empty(), "No eye rotation observed");
        }
        Retained final{session.Api(), session.Snapshot()}; ok(final.api.retain_snapshot(final.handle));
        session.Pause(); const auto last = session.Snapshot();
        ok(session.Advance(100, &sink)); require(session.Snapshot() == last, "Pause changed solver");
        ok(session.Reset(config.clock.startSeconds)); ok(advance(0, 0));
        identical(view(seed.api, seed.handle), view(session.Api(), session.Snapshot()));
        session.Play();
        for (uint64_t i = 1; i <= ticks; ++i) ok(advance(dt, i));
        identical(view(final.api, final.handle), view(session.Api(), session.Snapshot()));
        output.Apply(session.Api(), session.Snapshot());
        output.Close(); session.Close(); session.Close(); animated.Close(); stage.Reset();
        require(view(final.api, final.handle).joint_count == binding.JointIds().size(), "Retained unload output");
        std::cout << "avatar=" << argv[1] << " ticks=" << ticks << " joints=" << binding.JointIds().size()
            << " motion_changed=" << changedMotion.size() << " spring_changed=" << changedSpring.size()
            << " max_motion_error=" << maxMotionError << " max_spring_error=" << maxOwnerError
            << " max_output_error=" << maxOutputError
            << " max_output_scale_error=" << maxScaleError
            << " reset_replay=exact retained_unload=passed secondary=" << springs;
        if (face) std::cout << " max_face_error=" << maxFaceError << " morphs_changed=" << face->MorphChanged().size()
            << " materials_changed=" << face->MaterialChanged().size() << " eyes_changed=" << changedEyes.size();
        std::cout << '\n';
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
#ifndef AR_VRM_SESSION_TEST_ENTRY
int main(int argc, char** argv) { return avatarVrmSessionCheck(argc, argv); }
#endif
