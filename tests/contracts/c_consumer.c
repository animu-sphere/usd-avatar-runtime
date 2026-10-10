#include "avatarRuntime/api.h"
#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
__declspec(dllimport)
#endif
ArStatus AR_CALL registerContractProvider(const ArRuntimeApi*, ArRuntime);

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "Check failed at line %d: %s\n", __LINE__, #x); return 1; } } while (0)

int main(void) {
    ArRuntimeApi api = {0};
    ArRuntime runtime = 0;
    ArInstance instance = 0;
    ArSnapshot snapshot = 0;
    ArStateView state = {AR_HEADER(ArStateView)};
    ArJoint joint = {"rig", "root", -1, {{0,0,0}, {0,0,0,1}, {1,1,1}}};
    const char* selected[] = {"test.counter"};
    ArCapability capability = {"test.pose", 1};
    ArInstanceDesc desc = {AR_HEADER(ArInstanceDesc)};
    ArInputFrame input = {AR_HEADER(ArInputFrame)};
    ArGazeInput gaze = {"source", "actor", "test:gaze", AR_GAZE_POINT, AR_GAZE_RUNTIME_WORLD,
        AR_OBSERVATION_VALID, NULL, NULL, {3,0,1}, 5.25, 2, -4};
    ArSnapshot next = 0;
    char layout_id[] = "test.layout";
    const ArCapability* active = NULL;
    uint32_t count = 0;
    CHECK(arGetApi(AR_ABI_VERSION, sizeof(api), &api) == AR_OK);
    CHECK(arGetApi(2, sizeof(api), &api) == AR_INCOMPATIBLE_ABI);
    CHECK(arGetApi(3, sizeof(api), &api) == AR_INCOMPATIBLE_ABI);
    CHECK(api.create_runtime(&runtime) == AR_OK);
    CHECK(registerContractProvider(&api, runtime) == AR_OK);
    desc.generation = 1; desc.evaluators = selected; desc.evaluator_count = 1;
    desc.layout_id = layout_id; desc.layout_version = 5;
    desc.bound_capabilities = &capability; desc.bound_capability_count = 1;
    desc.required_capabilities = &capability; desc.required_capability_count = 1;
    desc.initial_state.struct_size = sizeof(ArStateView); desc.initial_state.abi_version = AR_ABI_VERSION;
    desc.initial_state.joints = &joint; desc.initial_state.joint_count = 1;
    CHECK(api.create_instance(runtime, &desc, NULL, &instance) == AR_OK);
    /* The implementation copied registration and layout strings/values. */
    joint.local.translation[0] = 999;
    layout_id[0] = 'X';
    CHECK(api.get_capabilities(runtime, instance, &active, &count) == AR_OK);
    CHECK(count == 1 && strcmp(active[0].id, "test.pose") == 0);
    input.frame_id = 1; input.generation = 1; input.evaluation_seconds = 0.25;
    input.input_revision = 42;
    input.gazes = &gaze; input.gaze_count = 1;
    CHECK(api.evaluate_frame(runtime, instance, &input, NULL, &snapshot) == AR_OK);
    CHECK(api.get_snapshot(snapshot, &state) == AR_OK);
    CHECK(state.instance == instance && state.frame_id == 1 && state.joint_count == 1);
    CHECK(strcmp(state.layout_id, "test.layout") == 0 && state.layout_version == 5 && state.input_revision == 42);
    CHECK(state.capability_count == 1 && strcmp(state.capabilities[0].id, "test.pose") == 0 && state.capabilities[0].version == 1);
    CHECK(state.joints[0].local.translation[0] == 1.25);
    CHECK(state.joints[0].local.translation[1] == 3);
    CHECK(state.sample_count == 2);
    CHECK(state.samples[0].kind == AR_SOURCE_GAZE && strcmp(state.samples[0].channel_id, "test:gaze") == 0);
    CHECK(state.samples[0].validity == AR_OBSERVATION_VALID && state.samples[0].resolution == AR_SAMPLE_SELECTED);
    CHECK(state.samples[0].runtime_seconds == 6.5 && strcmp(state.samples[0].evaluator_id, "") == 0);
    CHECK(state.samples[1].kind == AR_SOURCE_POSE && state.samples[1].resolution == AR_SAMPLE_HELD);
    CHECK(state.samples[1].runtime_seconds == 2.5 && strcmp(state.samples[1].evaluator_id, "test.counter") == 0);
    input.frame_id = 2; gaze.kind = AR_GAZE_DIRECTION; gaze.value[0] = 2;
    CHECK(api.evaluate_frame(runtime, instance, &input, NULL, &next) == AR_INVALID_ARGUMENT && next == 0);
    gaze.kind = AR_GAZE_POINT; gaze.validity = AR_OBSERVATION_STALE;
    CHECK(api.evaluate_frame(runtime, instance, &input, NULL, &next) == AR_OK);
    CHECK(api.get_snapshot(next, &state) == AR_OK);
    CHECK(state.joints[0].local.translation[0] == 2.25 && state.joints[0].local.translation[1] == 0);
    CHECK(state.sample_count == 2 && state.samples[0].validity == AR_OBSERVATION_STALE);
    CHECK(api.release_snapshot(next) == AR_OK);
    input.frame_id = 3; gaze.validity = AR_OBSERVATION_UNAVAILABLE;
    gaze.value[0] = gaze.value[2] = 0;
    CHECK(api.evaluate_frame(runtime, instance, &input, NULL, &next) == AR_OK);
    CHECK(api.get_snapshot(next, &state) == AR_OK);
    CHECK(state.joints[0].local.translation[0] == 3.25 && state.joints[0].local.translation[1] == 0);
    CHECK(api.release_snapshot(next) == AR_OK);
    CHECK(api.retain_snapshot(snapshot) == AR_OK);
    CHECK(api.destroy_runtime(runtime) == AR_OK);
    CHECK(api.get_snapshot(snapshot, &state) == AR_OK);
    CHECK(state.joints[0].local.translation[0] == 1.25);
    CHECK(state.joints[0].local.translation[1] == 3);
    CHECK(strcmp(state.layout_id, "test.layout") == 0 && state.layout_version == 5 && state.input_revision == 42);
    CHECK(state.capability_count == 1 && strcmp(state.capabilities[0].id, "test.pose") == 0);
    CHECK(state.sample_count == 2 && strcmp(state.samples[1].source_id, "clip") == 0 &&
          state.samples[1].source_seconds == 1 && state.samples[1].runtime_seconds == 2.5);
    CHECK(api.release_snapshot(snapshot) == AR_OK);
    CHECK(api.release_snapshot(snapshot) == AR_OK);
    CHECK(api.get_snapshot(snapshot, &state) == AR_INVALID_HANDLE);
    puts("Separate C provider/runtime/consumer boundary passed");
    return 0;
}
