#include "avatarRuntime/FixedTickSession.h"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

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
    // Observations selected for each evaluation.
    {
        ArRuntime r = 0; CHECK(api.create_runtime(&r) == AR_OK);
        ArInstance i = 0; CHECK(api.create_instance(r, &d, nullptr, &i) == AR_OK);
        avatarRuntime::FixedTickSession ticks(r, i, {1,.25,2});
        std::vector<std::pair<uint64_t,double>> calls;
        ArScalarInput scalar{"host","actor","app:value"}; scalar.clock_scale = 1;
        bool refuse = false, thrown = false;
        ArStatus reentry = AR_OK;
        const avatarRuntime::FixedTickObservations source = [&](uint64_t frame, double seconds) {
            if (thrown) throw std::runtime_error("source");
            calls.push_back({frame,seconds});
            reentry = ticks.Close();
            scalar.value = seconds; scalar.source_seconds = seconds;
            scalar.clock_scale = refuse ? 0 : 1;
            ArInputFrame f{AR_HEADER(ArInputFrame)}; f.scalars = &scalar; f.scalar_count = 1;
            f.input_revision = frame + 100;
            // The runtime supplies identity and time, whatever the source says.
            f.frame_id = 999; f.evaluation_seconds = -7;
            return f;
        };
        CHECK(ticks.Advance(1,.1,true,source) == AR_OK);
        CHECK(reentry == AR_BUSY);
        CHECK(calls.size() == 1 && calls[0].first == 1 && calls[0].second == 1); // the seed, even paused
        CHECK(ticks.Advance(2,10,true,source) == AR_OK && calls.size() == 1); // evaluates nothing
        // Catch-up samples each tick at its own time, up to the budget.
        CHECK(ticks.Advance(3,.75,false,source) == AR_OK && ticks.Stats().tick == 2 && calls.size() == 3);
        CHECK(calls[1].first == 2 && calls[1].second == 1.25 && calls[2].first == 3 && calls[2].second == 1.5);
        CHECK(api.get_snapshot(ticks.Snapshot(), &v) == AR_OK && v.frame_id == 3 && v.evaluation_seconds == 1.5);
        CHECK(v.input_revision == 103 && v.sample_count == 1 && v.samples[0].source_seconds == 1.5);
        // A refused or throwing source leaves the tick due; backlog and output survive.
        const auto last = ticks.Snapshot();
        refuse = true;
        CHECK(ticks.Advance(4,0,false,source) == AR_INVALID_ARGUMENT);
        CHECK(ticks.Stats().tick == 2 && ticks.Stats().pendingSeconds == .25 && ticks.Snapshot() == last);
        refuse = false; thrown = true;
        CHECK(ticks.Advance(5,0,false,source) == AR_PROVIDER_ERROR);
        CHECK(ticks.Stats().tick == 2 && ticks.Stats().pendingSeconds == .25 && ticks.Snapshot() == last);
        thrown = false;
        CHECK(ticks.Advance(6,0,false,source) == AR_OK && ticks.Stats().tick == 3);
        CHECK(calls.back().first == 4 && calls.back().second == 1.75);
        // A source without a header is refused; the held overload still works.
        const avatarRuntime::FixedTickObservations headerless = [](uint64_t, double) { return ArInputFrame{}; };
        CHECK(ticks.Advance(7,.25,false,headerless) != AR_OK && ticks.Stats().tick == 3);
        CHECK(ticks.Advance(8,0,false,input) == AR_OK && ticks.Stats().tick == 4);
        CHECK(ticks.Close() == AR_OK); CHECK(api.destroy_runtime(r) == AR_OK);
    }
    std::cout << "Backend-free native session, clock validation and retained lifetime passed\n";
}
