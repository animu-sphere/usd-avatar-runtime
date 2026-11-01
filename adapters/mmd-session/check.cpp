#include "../common/StateJson.h"
#include <avatarMmdSession/Session.h>
#include <avatarUsd/SkeletonBinding.h>
#include <mmdModel/Canonicalize.h>
#include <mmdPhysicsAdapter/Adapter.h>
#include <mmdPmx/Reader.h>
#include <motionVmd/Motion.h>
#include <motionVmd/Reader.h>
#include <pxr/base/gf/quatd.h>
#include <pxr/usd/sdf/layer.h>
#include <pxr/usd/usd/primRange.h>
#include <pxr/usd/usdPhysics/rigidBodyAPI.h>
#include <pxr/usd/usdSkel/skeleton.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <vector>

// Headless check of the session against the MMD owner. Jolt owns the simulation, so
// the session's own bodies are the owner's input: from them each tick is evaluated
// again through the owner, and any difference is one of composition. Whether a value
// is right for a given model is the expected state's question, not this host's.
namespace {
using Body = mmd::physics::BodyTransform;
constexpr double kFramesPerSecond = 30; // PhysicsAdapterConfig's default, which the session keeps
constexpr double kTick = .0083334; // just over the session's fixed 1/120 s, for one Step per call
void require(bool value, const std::string& message) { if (!value) throw std::runtime_error(message); }
template<class R> auto take(R result) {
    if (!result) throw std::invalid_argument(result.fatal()->code + ": " + result.fatal()->message);
    return std::move(result).value();
}
ArStateView view(const ArRuntimeApi& api, ArSnapshot s) {
    ArStateView v{AR_HEADER(ArStateView)};
    require(api.get_snapshot(s, &v) == AR_OK, "Snapshot");
    return v;
}
std::vector<double> values(const ArStateView& v) {
    std::vector<double> result;
    for (uint32_t i = 0; i < v.joint_count; ++i) {
        result.insert(result.end(), v.joints[i].local.translation, v.joints[i].local.translation + 3);
        result.insert(result.end(), v.joints[i].local.rotation, v.joints[i].local.rotation + 4);
    }
    for (uint32_t i = 0; i < v.blend_shape_count; ++i) result.push_back(v.blend_shapes[i].weight);
    for (uint32_t i = 0; i < v.vertex_uv_count; ++i)
        result.insert(result.end(), v.vertex_uvs[i].value, v.vertex_uvs[i].value + 2);
    for (uint32_t i = 0; i < v.material_count; ++i)
        result.insert(result.end(), v.materials[i].value, v.materials[i].value + 4);
    for (double value : result) require(std::isfinite(value), "Nonfinite output");
    return result;
}
double rotationError(const double* actual, pxr::GfQuatd q) {
    q.Normalize();
    double dot = actual[3] * q.GetReal();
    for (int k = 0; k < 3; ++k) dot += actual[k] * q.GetImaginary()[k];
    if (dot < 0) q = -q;
    double error = std::abs(actual[3] - q.GetReal());
    for (int k = 0; k < 3; ++k) error = std::max(error, std::abs(actual[k] - q.GetImaginary()[k]));
    return error;
}
bool sameTransform(const mmd::control::JointTransform& a, const mmd::control::JointTransform& b) {
    for (int k = 0; k < 3; ++k) if (std::abs(a.translation[k] - b.translation[k]) > 1e-6) return false;
    double dot = 0;
    for (int k = 0; k < 4; ++k) dot += a.rotation[k] * b.rotation[k];
    return std::abs(std::abs(dot) - 1) <= 1e-9;
}
}
static int run(int argc, char** argv) {
    try {
        require(argc >= 4, "Usage: avatarMmdSessionCheck avatar.pmx motion.vmd ticks [--state-output file]");
        std::string stateOutput;
        for (int i = 4; i < argc; ++i) {
            const std::string option = argv[i];
            if (option == "--state-output" && i + 1 < argc) stateOutput = argv[++i];
            else require(false, "Unknown or incomplete option " + option);
        }
        const auto ticks = std::stoull(argv[3]);
        require(ticks > 0 && ticks <= 1000000, "Invalid tick count");
        auto stage = pxr::UsdStage::Open(argv[1]);
        require(bool(stage), "Cannot open avatar");
        std::string rootBefore, sessionBefore;
        stage->GetRootLayer()->ExportToString(&rootBefore); stage->GetSessionLayer()->ExportToString(&sessionBefore);

        // The owner's view of the same files, bound as the session binds them.
        auto source = take(mmd::pmx::ReadFile(std::filesystem::u8path(argv[1])));
        auto model = take(mmd::Canonicalize(source));
        auto evaluator = take(mmd::control::Evaluator::Prepare(model));
        auto motion = take(mmd::binding::Bind(take(motionVmd::BuildMotion(
            take(motionVmd::ReadFile(std::filesystem::u8path(argv[2]))))), model));
        const avatarUsd::SkeletonBinding binding(stage, {pxr::SdfPath("/Asset"), pxr::SdfPath("/Asset/skel/Skeleton"), "live-mmd", 1, {}});
        std::vector<pxr::UsdPrim> bodies;
        for (auto prim : stage->Traverse()) if (prim.HasAPI<pxr::UsdPhysicsRigidBodyAPI>()) bodies.push_back(prim);
        auto coupling = take(mmd::physics::Adapter::Bind(
            pxr::UsdSkelSkeleton::Get(stage, pxr::SdfPath(binding.SkeletonId())), bodies));
        const auto& placement = binding.RootPlacement();
        const pxr::GfQuatd place(placement.rotation[3], {placement.rotation[0], placement.rotation[1], placement.rotation[2]});
        // Vertex morphs by runtime target, in the session's own mapping.
        std::map<std::string, int32_t> morphIndex;
        for (const auto& morph : model.morphs) if (morph.type == mmd::MorphType::Vertex) morphIndex[morph.name.stableId] = int32_t(morph.sourceIndex);

        avatarMmdSession::Session session(stage, argv[1], argv[2], false, false, false);
        const auto base = values(session.Baseline());
        double maxPoseError = 0, maxMorphError = 0, maxUvError = 0;
        std::set<uint32_t> poseChanged, feedbackChanged, morphChanged, uvChanged;
        std::vector<std::vector<double>> first;
        std::vector<std::string> states;
        ArSnapshot retained = 0;
        const auto& rest = session.Baseline();
        session.Play();
        for (uint64_t tick = 1; tick <= ticks; ++tick) {
            require(session.Advance(kTick).steps == tick, "One Step per fixed tick expected");
            const auto a = view(session.Api(), session.Snapshot());
            require(a.frame_id == tick && a.generation == 1, "Tick identity");
            first.push_back(values(a));
            if (!stateOutput.empty() && tick % 30 == 0) states.push_back(avatarCheck::StateJson(a));

            std::vector<Body> rows;
            for (const auto& body : session.BodyStates())
                rows.push_back({pxr::SdfPath(body.path), {{body.translation[0], body.translation[1], body.translation[2]},
                    {body.rotation[0], body.rotation[1], body.rotation[2], body.rotation[3]}}});
            const auto continuation = evaluator.BeginPhysics(motion, a.evaluation_seconds * kFramesPerSecond);
            const auto feedback = take(coupling.ApplyPhysicsToPose(rows, continuation.BeforePhysics()));
            const auto pose = take(evaluator.ResumePhysics(continuation, feedback));
            require(pose.joints.size() == a.joint_count, "Owner joint count");
            for (const auto& row : feedback)
                if (!sameTransform(row.local, continuation.BeforePhysics().joints[size_t(row.joint)])) feedbackChanged.insert(uint32_t(row.joint));
            for (uint32_t i = 0; i < a.joint_count; ++i) {
                const auto& joint = pose.joints[i];
                pxr::GfVec3d t(joint.translation[0], joint.translation[1], joint.translation[2]);
                pxr::GfQuatd q(joint.rotation[3], {joint.rotation[0], joint.rotation[1], joint.rotation[2]});
                if (a.joints[i].parent_index < 0) {
                    t = place.Transform(t) + pxr::GfVec3d(placement.translation); q = place * q;
                }
                maxPoseError = std::max(maxPoseError, rotationError(a.joints[i].local.rotation, q));
                for (int k = 0; k < 3; ++k) maxPoseError = std::max(maxPoseError, std::abs(a.joints[i].local.translation[k] - t[k]));
                bool moved = rotationError(a.joints[i].local.rotation, pxr::GfQuatd(rest.joints[i].local.rotation[3],
                    {rest.joints[i].local.rotation[0], rest.joints[i].local.rotation[1], rest.joints[i].local.rotation[2]})) > 1e-6;
                for (int k = 0; k < 3; ++k) moved = moved || std::abs(a.joints[i].local.translation[k] - rest.joints[i].local.translation[k]) > 1e-6;
                if (moved) poseChanged.insert(i);
            }
            std::map<int32_t, double> weights;
            for (const auto& channel : evaluator.ResolveMorphChannels(pose.channels)) weights[channel.morph] = channel.weight;
            for (uint32_t i = 0; i < a.blend_shape_count; ++i) {
                const auto index = morphIndex.find(a.blend_shapes[i].target_id);
                require(index != morphIndex.end(), std::string("Morph target unknown to the owner: ") + a.blend_shapes[i].target_id);
                const auto weight = weights.find(index->second);
                maxMorphError = std::max(maxMorphError, std::abs(a.blend_shapes[i].weight - (weight == weights.end() ? 0 : weight->second)));
                if (a.blend_shapes[i].weight != 0) morphChanged.insert(i);
            }
            if (a.vertex_uv_count) {
                std::vector<std::array<double, 2>> uvs(a.vertex_uv_count);
                for (uint32_t i = 0; i < rest.vertex_uv_count; ++i) {
                    require(rest.vertex_uvs[i].vertex_index < uvs.size(), "UV vertex layout");
                    uvs[rest.vertex_uvs[i].vertex_index] = {rest.vertex_uvs[i].value[0], rest.vertex_uvs[i].value[1]};
                }
                for (const auto& delta : take(evaluator.EvaluateUvMorphs(pose.channels))) {
                    require(!delta.channel && delta.vertex >= 0 && size_t(delta.vertex) < uvs.size(), "Owner UV delta");
                    for (int k = 0; k < 2; ++k) uvs[delta.vertex][k] += delta.delta[k];
                }
                for (uint32_t i = 0; i < a.vertex_uv_count; ++i) for (int k = 0; k < 2; ++k) {
                    maxUvError = std::max(maxUvError, std::abs(a.vertex_uvs[i].value[k] - uvs[a.vertex_uvs[i].vertex_index][k]));
                    if (a.vertex_uvs[i].value[k] != rest.vertex_uvs[i].value[k]) uvChanged.insert(i);
                }
            }
            if (tick == ticks / 2) {
                retained = session.Snapshot();
                require(session.Api().retain_snapshot(retained) == AR_OK, "Retain");
                session.Pause(); session.Advance(10);
                require(session.GetStats().steps == tick, "Pause holds state");
                session.Play();
            }
        }
        require(maxPoseError <= 1e-6 && maxMorphError <= 1e-6 && maxUvError <= 1e-6,
            "Owner/session value mismatch: pose " + std::to_string(maxPoseError) + " morph " +
            std::to_string(maxMorphError) + " uv " + std::to_string(maxUvError));
        require(first.back() != base, "Nothing changed committed state");
        if (!stateOutput.empty()) {
            std::ofstream file(std::filesystem::u8path(stateOutput), std::ios::binary);
            file << '[';
            for (size_t i = 0; i < states.size(); ++i) file << (i ? "," : "") << '\n' << states[i];
            file << "]\n";
            require(bool(file), "Cannot write " + stateOutput);
        }
        // The same inputs replay exactly per platform, generation after generation.
        for (uint64_t generation = 2; generation <= 3; ++generation) {
            session.Reset();
            require(session.GetStats().paused && !session.Snapshot(), "Reset pauses and clears display output");
            session.Play();
            for (uint64_t tick = 1; tick <= ticks; ++tick) {
                require(session.Advance(kTick).steps == tick, "One Step per fixed tick expected");
                const auto a = view(session.Api(), session.Snapshot());
                require(a.frame_id == tick && a.generation == generation, "Tick identity");
                require(values(a) == first[tick - 1], "Exact reset replay");
            }
        }
        session.Close(); session.Close();
        require(session.GetStats().bodies == 0 && !session.Snapshot(), "Whole-world teardown");
        const auto held = view(session.Api(), retained);
        require(held.frame_id == ticks / 2 && held.generation == 1 && values(held) == first[ticks / 2 - 1], "Retained unload output");
        const auto materials = held.material_count;
        require(session.Api().release_snapshot(retained) == AR_OK, "Release");
        std::string rootAfter, sessionAfter;
        stage->GetRootLayer()->ExportToString(&rootAfter); stage->GetSessionLayer()->ExportToString(&sessionAfter);
        require(rootBefore == rootAfter && sessionBefore == sessionAfter, "Direct snapshots never author USD");
        std::cout << "avatar=" << argv[1] << " ticks=" << ticks << " joints=" << rest.joint_count
            << " bodies=" << bodies.size() << " pose_changed=" << poseChanged.size()
            << " feedback_changed=" << feedbackChanged.size() << " morphs_changed=" << morphChanged.size()
            << " uvs_changed=" << uvChanged.size() << " max_pose_error=" << maxPoseError
            << " max_morph_error=" << maxMorphError << " max_uv_error=" << maxUvError
            << " materials_not_compared=" << materials
            << " reset_replay=exact retained_unload=passed\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
// Arguments are UTF-8, as the session and OpenUSD expect. On Windows char** holds the
// system code page, which cannot name every model file.
#ifdef _WIN32
int wmain(int argc, wchar_t** wide) {
    std::vector<std::string> storage;
    std::vector<char*> argv;
    for (int i = 0; i < argc; ++i) {
        const auto text = std::filesystem::path(wide[i]).u8string();
        storage.emplace_back(text.begin(), text.end());
    }
    for (auto& text : storage) argv.push_back(text.data());
    return run(argc, argv.data());
}
#else
int main(int argc, char** argv) { return run(argc, argv); }
#endif
