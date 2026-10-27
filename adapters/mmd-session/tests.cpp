#include <avatarMmdSession/Session.h>
#include <pxr/usd/sdf/layer.h>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
std::vector<double> values(const ArStateView& view) {
    std::vector<double> result;
    for (uint32_t i = 0; i < view.joint_count; ++i) {
        const auto& joint = view.joints[i];
        result.insert(result.end(), joint.local.translation, joint.local.translation + 3);
        result.insert(result.end(), joint.local.rotation, joint.local.rotation + 4);
    }
    for (uint32_t i = 0; i < view.blend_shape_count; ++i) result.push_back(view.blend_shapes[i].weight);
    for (uint32_t i = 0; i < view.vertex_uv_count; ++i) {
        result.push_back(view.vertex_uvs[i].value[0]); result.push_back(view.vertex_uvs[i].value[1]);
    }
    for (double value : result) require(std::isfinite(value), "nonfinite output");
    return result;
}
}
int main(int argc, char** argv) {
    try {
        require(argc == 3, "expected generated PMX and mixed UV/vertex VMD");
        auto stage = pxr::UsdStage::Open(argv[1]); require(bool(stage), "open PMX");
        std::string original, originalSession;
        stage->GetRootLayer()->ExportToString(&original); stage->GetSessionLayer()->ExportToString(&originalSession);
        avatarMmdSession::Session session(stage, argv[1], argv[2], false, false, false);
        require(!session.Snapshot() && session.GetStats().paused, "initial paused output");
        require(session.Baseline().joint_count && session.Baseline().blend_shape_count &&
            session.Baseline().vertex_uv_count, "complete joint/morph/UV layout");
        const auto base = values(session.Baseline());
        session.Advance(10); require(session.GetStats().steps == 0, "paused clock");
        std::vector<std::vector<double>> first;
        ArSnapshot retained = 0;
        for (int generation = 1; generation <= 3; ++generation) {
            session.Play();
            for (int tick = 1; tick <= 240; ++tick) {
                require(session.Advance(.0083334).steps == uint64_t(tick), "one Step per fixed tick");
                ArStateView view{AR_HEADER(ArStateView)}; require(session.Api().get_snapshot(session.Snapshot(), &view) == AR_OK, "snapshot");
                require(view.frame_id == uint64_t(tick) && view.generation == uint64_t(generation), "tick identity");
                auto resolved = values(view);
                if (generation == 1) first.push_back(resolved);
                else require(resolved == first[tick - 1], "exact reset replay");
                if (generation == 1 && tick == 120) {
                    retained = session.Snapshot(); require(session.Api().retain_snapshot(retained) == AR_OK, "retain");
                    session.Pause(); session.Advance(10); require(session.GetStats().steps == 120, "pause holds state");
                    session.Play();
                }
            }
            require(first.front() != first[119] && first[119] != base, "motion/physics/morph/UV changed");
            if (generation < 3) session.Reset();
        }
        session.Close(); session.Close(); require(session.GetStats().bodies == 0 && !session.Snapshot(), "whole-world teardown");
        ArStateView held{AR_HEADER(ArStateView)}; require(session.Api().get_snapshot(retained, &held) == AR_OK, "retained across reset/close");
        require(held.frame_id == 120 && held.generation == 1 && values(held) == first[119], "immutable retained output");
        require(session.Api().release_snapshot(retained) == AR_OK, "release");
        bool refused = false; try { session.Play(); } catch (...) { refused = true; } require(refused, "closed play refuses");
        std::string after, afterSession;
        stage->GetRootLayer()->ExportToString(&after); stage->GetSessionLayer()->ExportToString(&afterSession);
        require(original == after && originalSession == afterSession, "direct snapshots never author USD");
        std::cout << "Native MMD snapshots: 720 ticks; pause/reset/close/retention; USD unchanged\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
