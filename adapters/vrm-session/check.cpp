#include "avatarVrmSession/Session.h"
#include "avatarVrmUsd/SpringBoneBinding.h"
#include "avatarUsd/SkeletonPoseOutput.h"
#include "motionRetarget/PoseRetargeter.h"
#include "motionSampling/MotionSource.h"
#include "pxr/usd/usdSkel/animation.h"
#include "pxr/usd/usd/primRange.h"
#include "pxr/usd/usdSkel/skeleton.h"
#include <algorithm>
#include <cmath>
#include <iostream>
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
}
struct Retained {
    ArRuntimeApi api; ArSnapshot handle = 0;
    ~Retained() { if (handle) api.release_snapshot(handle); }
};
}
int avatarVrmSessionCheck(int argc, char** argv) {
    try {
        require(argc == 5 || (argc == 6 && std::string(argv[5]) == "--no-springs"),
            "Usage: avatarVrmSessionCheck avatar.vrm clip.vrma fixed_seconds ticks [--no-springs]");
        auto stage = pxr::UsdStage::Open(argv[1]); auto clipStage = pxr::UsdStage::Open(argv[2]);
        require(bool(stage) && bool(clipStage), "Cannot open avatar/clip");
        pxr::SdfPath clipSkeleton;
        for (const auto& prim : clipStage->Traverse()) if (prim.IsA<pxr::UsdSkelSkeleton>()) {
            require(clipSkeleton.IsEmpty(), "Clip skeleton is ambiguous"); clipSkeleton = prim.GetPath();
        }
        require(!clipSkeleton.IsEmpty() && bool(stage->GetDefaultPrim()), "Missing clip skeleton/avatar default prim");
        const auto clip = avatarMotionUsd::ReadStageClip(clipStage, clipSkeleton);
        require(!clip.clip.samples.empty(), "Empty clip"); clipStage.Reset();
        const double dt = std::stod(argv[3]);
        const auto ticks = std::stoull(argv[4]);
        require(std::isfinite(dt) && dt > 0 && ticks > 0 && ticks <= 1000000, "Invalid duration/tick count");
        const bool springs = argc == 5;
        avatarVrmSession::SessionConfig config;
        config.avatar = {stage->GetDefaultPrim().GetPath(), {}, "vrm.session.check", 1};
        config.clock = {clip.clip.samples.front().timestamp, dt, 2}; config.secondaryMotion = springs;
        avatarVrmSession::Session session(stage, clip, config);
        const auto binding = session.Binding();
        auto motionConfig = config; motionConfig.secondaryMotion = false;
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
        ok(session.Advance(0, &sink)); ok(animated.Advance(0, &sink));
        Retained seed{session.Api(), session.Snapshot()}; ok(seed.api.retain_snapshot(seed.handle));
        session.Play(); animated.Play();
        openstrata::motion::PoseRetargeter retarget(binding.Skeleton(), binding.HumanoidMap(), *clip.sourceRest);
        double maxOwnerError = 0, maxMotionError = 0, maxOutputError = 0, maxScaleError = 0;
        std::set<uint32_t> changedMotion, changedSpring;
        for (uint64_t tick = 0; tick <= ticks; ++tick) {
            if (tick) { ok(session.Advance(dt, &sink)); ok(animated.Advance(dt, &sink)); }
            require(session.Stats().tick == tick, "One fixed tick expected");
            const auto a = view(session.Api(), session.Snapshot());
            const auto m = view(animated.Api(), animated.Snapshot());
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
                    const auto& local = m.joints[i].local;
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
                    if (rotationError(a.joints[i].local.rotation, pxr::GfQuatd(m.joints[i].local.rotation[3],
                        {m.joints[i].local.rotation[0],m.joints[i].local.rotation[1],m.joints[i].local.rotation[2]})) > 1e-6)
                        changedSpring.insert(i);
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
        require(maxOwnerError <= 1e-6 && maxMotionError <= 1e-6 && maxOutputError <= 1e-6,
            "Owner/runtime/output value mismatch: " + std::to_string(maxOwnerError) + "/" +
            std::to_string(maxMotionError) + "/" + std::to_string(maxOutputError));
        require(!changedMotion.empty(), "Clip did not move avatar");
        if (springs) require(!changedSpring.empty(), "No secondary motion observed");
        Retained final{session.Api(), session.Snapshot()}; ok(final.api.retain_snapshot(final.handle));
        session.Pause(); const auto last = session.Snapshot();
        ok(session.Advance(100, &sink)); require(session.Snapshot() == last, "Pause changed solver");
        ok(session.Reset(config.clock.startSeconds)); ok(session.Advance(0, &sink));
        identical(view(seed.api, seed.handle), view(session.Api(), session.Snapshot()));
        session.Play();
        for (uint64_t i = 0; i < ticks; ++i) ok(session.Advance(dt, &sink));
        identical(view(final.api, final.handle), view(session.Api(), session.Snapshot()));
        output.Apply(session.Api(), session.Snapshot());
        output.Close(); session.Close(); session.Close(); animated.Close(); stage.Reset();
        require(view(final.api, final.handle).joint_count == binding.JointIds().size(), "Retained unload output");
        std::cout << "avatar=" << argv[1] << " ticks=" << ticks << " joints=" << binding.JointIds().size()
            << " motion_changed=" << changedMotion.size() << " spring_changed=" << changedSpring.size()
            << " max_motion_error=" << maxMotionError << " max_spring_error=" << maxOwnerError
            << " max_output_error=" << maxOutputError
            << " max_output_scale_error=" << maxScaleError
            << " reset_replay=exact retained_unload=passed secondary=" << springs << '\n';
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
#ifndef AR_VRM_SESSION_TEST_ENTRY
int main(int argc, char** argv) { return avatarVrmSessionCheck(argc, argv); }
#endif
