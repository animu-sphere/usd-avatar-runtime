#include "avatarRuntime/physics.h"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#define CHECK(x) do { if (!(x)) { std::cerr << "Line " << __LINE__ << ": " << #x << '\n'; std::exit(1); } } while (0)

struct Fixture {
    ArRuntimeApi api{};
    ArPhysicsApi physics{};
    ArRuntime runtime = 0;
    ArPhysicsWorld world = 0;
    ArInstance avatars[2]{};
    ArSnapshot snapshots[2]{};
    ArInputFrame input{AR_HEADER(ArInputFrame)};
    ArPhysicsInput inputs[2]{};
    ArPhysicsAdvanceResult result{AR_HEADER(ArPhysicsAdvanceResult)};
    uint64_t request = 0;
    int steps = 0, resets = 0;
    bool failPre = false, failPost = false, failStep = false, failReset = false;
    std::vector<std::string> calls;
    std::vector<double> times;

    static ArStatus AR_CALL pre(void* p, void*, const ArEvaluationContext* c, const ArStateWriter* w) {
        auto& f = *static_cast<Fixture*>(p);
        f.calls.push_back("pre." + std::to_string(c->working->instance));
        f.times.push_back(c->input->evaluation_seconds);
        if (f.failPre) return AR_PROVIDER_ERROR;
        auto pose = c->working->joints[0].local;
        pose.translation[0] = c->input->evaluation_seconds;
        return w->set_joint(w->context, 0, &pose);
    }
    static ArStatus AR_CALL post(void* p, void*, const ArEvaluationContext* c, const ArStateWriter* w) {
        auto& f = *static_cast<Fixture*>(p);
        f.calls.push_back("post." + std::to_string(c->working->instance));
        if (f.failPost && c->working->instance == f.avatars[1]) return AR_PROVIDER_ERROR;
        auto pose = c->working->joints[0].local;
        pose.translation[1] = double(f.steps);
        return w->set_joint(w->context, 0, &pose);
    }
    static ArStatus AR_CALL step(void* p, const ArPhysicsTick* tick, const ArDiagnosticSink*) {
        auto& f = *static_cast<Fixture*>(p); f.calls.emplace_back("step"); ++f.steps;
        CHECK(tick->delta_seconds == .125 && tick->tick_id == uint64_t(f.steps));
        CHECK(tick->evaluation_seconds == f.steps * .125);
        ArSnapshot snapshot = 99;
        CHECK(f.api.evaluate_frame(f.runtime, f.avatars[0], &f.input, nullptr, &snapshot) == AR_BUSY && snapshot == 0);
        return f.failStep ? AR_PROVIDER_ERROR : AR_OK;
    }
    static ArStatus AR_CALL reset(void* p, const ArDiagnosticSink*) {
        auto& f = *static_cast<Fixture*>(p); ++f.resets; f.steps = 0;
        return f.failReset ? AR_PROVIDER_ERROR : AR_OK;
    }
    Fixture() {
        CHECK(arGetApi(AR_ABI_VERSION, sizeof(api), &api) == AR_OK);
        CHECK(arGetPhysicsApi(AR_PHYSICS_API_VERSION, sizeof(physics), &physics) == AR_OK);
        CHECK(arGetPhysicsApi(99, sizeof(physics), &physics) == AR_INCOMPATIBLE_ABI);
        CHECK(arGetPhysicsApi(1, sizeof(physics) - 1, &physics) == AR_INCOMPATIBLE_ABI);
        CHECK(api.create_runtime(&runtime) == AR_OK);
        const char* after[] = {"owner.pre"};
        for (bool isPost : {false, true}) {
            ArEvaluatorDesc desc{AR_HEADER(ArEvaluatorDesc)};
            desc.id = isPost ? "owner.post" : "owner.pre";
            desc.provider_id = "test.owner"; desc.provider_version = "1";
            // Both sides use the same existing phase; dependency fixes the cut.
            desc.phase = AR_PHASE_CONSTRAINTS; desc.reads = desc.writes = AR_DOMAIN_POSE;
            desc.user_data = this; desc.evaluate = isPost ? post : pre;
            if (isPost) { desc.after = after; desc.after_count = 1; }
            CHECK(api.register_evaluator(runtime, &desc, nullptr) == AR_OK);
        }
        const char* selected[] = {"owner.post", "owner.pre"};
        ArJoint joint{"rig", "root", -1, {{0,0,0}, {0,0,0,1}, {1,1,1}}};
        ArInstanceDesc desc{AR_HEADER(ArInstanceDesc)};
        desc.generation = 1; desc.evaluators = selected; desc.evaluator_count = 2;
        desc.layout_id = "rig.layout"; desc.layout_version = 1;
        desc.initial_state = {AR_HEADER(ArStateView)};
        desc.initial_state.joints = &joint; desc.initial_state.joint_count = 1;
        for (auto& avatar : avatars) CHECK(api.create_instance(runtime, &desc, nullptr, &avatar) == AR_OK);
        ArPhysicsParticipant members[] = {{avatars[1], "owner.post"}, {avatars[0], "owner.post"}};
        ArPhysicsWorldDesc worldDesc{AR_HEADER(ArPhysicsWorldDesc)};
        worldDesc.id = "shared.world"; worldDesc.participants = members; worldDesc.participant_count = 2;
        worldDesc.fixed_step_seconds = .125; worldDesc.max_catch_up_ticks = 2;
        worldDesc.user_data = this; worldDesc.step = step; worldDesc.reset = reset;
        CHECK(physics.create_world(runtime, &worldDesc, nullptr, &world) == AR_OK);
        // Input/output order differs from deterministic instance order.
        inputs[0] = {avatars[1], &input}; inputs[1] = {avatars[0], &input};
    }
    ~Fixture() { release(); CHECK(api.destroy_runtime(runtime) == AR_OK); }
    void release() {
        for (auto& s : snapshots) { if (s) CHECK(api.release_snapshot(s) == AR_OK); s = 0; }
    }
    ArStatus advance(double elapsed, bool paused = false) {
        release();
        ArPhysicsAdvanceDesc desc{AR_HEADER(ArPhysicsAdvanceDesc)};
        desc.request_id = ++request; desc.elapsed_seconds = elapsed; desc.paused = paused;
        desc.inputs = inputs; desc.input_count = 2;
        return physics.advance_world(runtime, world, &desc, nullptr, &result, snapshots);
    }
    ArStateView view(ArSnapshot s) {
        ArStateView v{AR_HEADER(ArStateView)}; CHECK(api.get_snapshot(s, &v) == AR_OK); return v;
    }
};

void clockAndBarrier() {
    Fixture f;
    CHECK(f.advance(.0625) == AR_OK && f.result.tick_count == 0 && f.steps == 0 && !f.snapshots[0]);
    CHECK(f.advance(.0625) == AR_OK && f.result.tick_count == 1 && f.steps == 1);
    CHECK((f.calls == std::vector<std::string>{"pre." + std::to_string(f.avatars[0]),
        "pre." + std::to_string(f.avatars[1]), "step", "post." + std::to_string(f.avatars[0]),
        "post." + std::to_string(f.avatars[1])}));
    auto v = f.view(f.snapshots[0]); CHECK(v.instance == f.avatars[1] && v.frame_id == 1);
    CHECK(v.joints[0].local.translation[0] == .125 && v.joints[0].local.translation[1] == 1);
    const auto retained = f.snapshots[0]; CHECK(f.api.retain_snapshot(retained) == AR_OK);
    CHECK(f.advance(100, true) == AR_OK && f.steps == 1 && f.result.pending_seconds == 0);
    CHECK(f.advance(.5) == AR_OK && f.result.tick_count == 2 && f.steps == 3 && f.result.pending_seconds == .25);
    CHECK(f.advance(0, true) == AR_OK && f.steps == 3 && f.result.pending_seconds == .25);
    CHECK(f.advance(0) == AR_OK && f.result.tick_count == 2 && f.steps == 5 && f.result.pending_seconds == 0);
    CHECK((f.times == std::vector<double>{.125,.125,.25,.25,.375,.375,.5,.5,.625,.625}));
    CHECK(f.physics.reset_world(f.runtime, f.world, 2, nullptr) == AR_OK && f.resets == 1);
    CHECK(f.view(retained).frame_id == 1 && f.view(retained).generation == 1);
    CHECK(f.advance(.125) == AR_OK && f.result.last_tick_id == 1 && f.view(f.snapshots[0]).generation == 2);
    CHECK(f.api.release_snapshot(retained) == AR_OK);
    CHECK(f.api.reset_instance(f.runtime, f.avatars[0], 3) == AR_INVALID_STATE);
    CHECK(f.api.destroy_instance(f.runtime, f.avatars[0]) == AR_INVALID_STATE);
    ArSnapshot direct = 9;
    CHECK(f.api.evaluate_frame(f.runtime, f.avatars[0], &f.input, nullptr, &direct) == AR_INVALID_STATE && direct == 0);
    CHECK(f.physics.destroy_world(f.runtime, f.world) == AR_OK);
    CHECK(f.api.reset_instance(f.runtime, f.avatars[0], 3) == AR_OK);
    auto input = f.input; input.frame_id = 1; input.generation = 3;
    CHECK(f.api.evaluate_frame(f.runtime, f.avatars[0], &input, nullptr, &direct) == AR_OK);
    CHECK(f.api.release_snapshot(direct) == AR_OK);
}

void failures() {
    Fixture f;
    // Invalid input is rejected before consuming elapsed time.
    f.inputs[1].instance = f.avatars[1];
    CHECK(f.advance(.125) == AR_INVALID_ARGUMENT && f.steps == 0);
    f.inputs[1].instance = f.avatars[0];
    f.failPre = true;
    CHECK(f.advance(.125) == AR_PROVIDER_ERROR && f.steps == 0 && !f.snapshots[0]);
    f.failPre = false;
    CHECK(f.advance(0) == AR_OK && f.steps == 1);
    const auto retained = f.snapshots[0]; CHECK(f.api.retain_snapshot(retained) == AR_OK);
    f.failPost = true;
    CHECK(f.advance(.125) == AR_PROVIDER_ERROR && f.steps == 2 && !f.snapshots[0] && !f.snapshots[1]);
    f.failPost = false;
    CHECK(f.advance(0) == AR_INVALID_STATE && f.steps == 2);
    CHECK(f.view(retained).frame_id == 1 && f.view(retained).joints[0].local.translation[1] == 1);
    f.failReset = true;
    CHECK(f.physics.reset_world(f.runtime, f.world, 2, nullptr) == AR_PROVIDER_ERROR);
    CHECK(f.advance(0) == AR_INVALID_STATE);
    f.failReset = false;
    CHECK(f.physics.reset_world(f.runtime, f.world, 2, nullptr) == AR_OK);
    f.failStep = true;
    CHECK(f.advance(.125) == AR_PROVIDER_ERROR && f.steps == 1);
    CHECK(f.advance(0) == AR_INVALID_STATE && f.steps == 1);
    f.failStep = false;
    CHECK(f.physics.reset_world(f.runtime, f.world, 3, nullptr) == AR_OK);
    CHECK(f.advance(.125) == AR_OK && f.steps == 1);
    CHECK(f.api.release_snapshot(retained) == AR_OK);
}

void fractionalClock() {
    ArRuntimeApi api{}; ArPhysicsApi physics{}; ArRuntime runtime = 0; ArPhysicsWorld world = 0;
    CHECK(arGetApi(AR_ABI_VERSION, sizeof(api), &api) == AR_OK);
    CHECK(arGetPhysicsApi(AR_PHYSICS_API_VERSION, sizeof(physics), &physics) == AR_OK);
    CHECK(api.create_runtime(&runtime) == AR_OK);
    int steps = 0;
    ArPhysicsWorldDesc binding{AR_HEADER(ArPhysicsWorldDesc)};
    binding.id = "non-avatar.world"; binding.fixed_step_seconds = .1; binding.max_catch_up_ticks = 3;
    binding.user_data = &steps;
    binding.step = [](void* p, const ArPhysicsTick*, const ArDiagnosticSink*) -> ArStatus {
        ++*static_cast<int*>(p); return AR_OK;
    };
    binding.reset = [](void*, const ArDiagnosticSink*) -> ArStatus { return AR_OK; };
    CHECK(physics.create_world(runtime, &binding, nullptr, &world) == AR_OK);
    ArPhysicsAdvanceDesc request{AR_HEADER(ArPhysicsAdvanceDesc)};
    request.request_id = 1; request.elapsed_seconds = .3;
    ArPhysicsAdvanceResult result{AR_HEADER(ArPhysicsAdvanceResult)};
    CHECK(physics.advance_world(runtime, world, &request, nullptr, &result, nullptr) == AR_OK);
    CHECK(result.tick_count == 3 && steps == 3 && result.pending_seconds == 0);
    CHECK(physics.advance_world(runtime, world, &request, nullptr, &result, nullptr) == AR_INVALID_ARGUMENT && steps == 3);
    CHECK(api.destroy_runtime(runtime) == AR_OK);
}

int main() {
    clockAndBarrier(); failures(); fractionalClock();
    std::cout << "Shared-world order, fixed ticks, retained state and reset recovery passed\n";
}
