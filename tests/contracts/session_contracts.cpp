#include "avatarRuntime/FixedTickSession.h"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>

#define CHECK(x) do { if (!(x)) { std::cerr << __LINE__ << ": " << #x << '\n'; std::exit(1); } } while (0)
int main() {
    ArRuntimeApi api{}; CHECK(arGetApi(AR_ABI_VERSION, sizeof(api), &api) == AR_OK);
    ArRuntime runtime = 0; CHECK(api.create_runtime(&runtime) == AR_OK);
    ArJoint joint{"rig","root",-1,{{0,0,0},{0,0,0,1},{1,1,1}}};
    ArInstanceDesc d{AR_HEADER(ArInstanceDesc)};
    d.generation = 1; d.layout_id = "native.clock"; d.layout_version = 1;
    d.initial_state = {AR_HEADER(ArStateView)}; d.initial_state.joints = &joint; d.initial_state.joint_count = 1;
    ArInstance instance = 0; CHECK(api.create_instance(runtime, &d, nullptr, &instance) == AR_OK);
    const double nan = std::numeric_limits<double>::quiet_NaN();
    for (auto config : {avatarRuntime::FixedTickConfig{0,0,1}, {0,-1,1}, {nan,.25,1},
                        {0,nan,1}, {0,.25,0}, {1e30,.25,1}}) {
        try { avatarRuntime::FixedTickSession invalid(runtime, instance, config); CHECK(false); }
        catch (const std::invalid_argument&) {}
    }
    avatarRuntime::FixedTickSession session(runtime, instance, {-1,.25,2});
    try { avatarRuntime::FixedTickSession duplicate(runtime, instance, {0,.25,2}); CHECK(false); }
    catch (const std::invalid_argument&) {}
    ArInputFrame input{AR_HEADER(ArInputFrame)};
    CHECK(session.Advance(1,nan,false,input) == AR_INVALID_ARGUMENT);
    CHECK(session.Advance(1,-1,false,input) == AR_INVALID_ARGUMENT);
    CHECK(session.Advance(1,.125,false,input) == AR_OK);
    CHECK(session.Stats().tick == 0 && session.Stats().pendingSeconds == .125);
    ArStateView v{AR_HEADER(ArStateView)};
    CHECK(api.get_snapshot(session.Snapshot(), &v) == AR_OK && v.evaluation_seconds == -1);
    const auto retained = session.Snapshot(); CHECK(api.retain_snapshot(retained) == AR_OK);
    CHECK(session.Advance(2,.125,false,input) == AR_OK && session.Stats().tick == 1);
    CHECK(session.Reset(1,0) == AR_INVALID_ARGUMENT);
    CHECK(session.Reset(2,nan) == AR_INVALID_ARGUMENT);
    CHECK(session.Reset(2,5) == AR_OK && session.Snapshot() == 0);
    CHECK(session.Advance(1,10,true,input) == AR_OK && session.Stats().tick == 0);
    CHECK(api.get_snapshot(session.Snapshot(), &v) == AR_OK && v.evaluation_seconds == 5);
    CHECK(api.destroy_runtime(runtime) == AR_OK);
    CHECK(session.Close() == AR_OK && session.Close() == AR_OK);
    CHECK(api.get_snapshot(retained, &v) == AR_OK && v.evaluation_seconds == -1 && v.generation == 1);
    CHECK(api.release_snapshot(retained) == AR_OK);
    std::cout << "Backend-free native session, clock validation and retained lifetime passed\n";
}
