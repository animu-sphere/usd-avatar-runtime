#ifndef AVATAR_RUNTIME_PHYSICS_H
#define AVATAR_RUNTIME_PHYSICS_H
#include "avatarRuntime/api.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Optional experimental table; the revision-4 core table/layout is unchanged. */
#define AR_PHYSICS_API_VERSION 1u
typedef uint64_t ArPhysicsWorld;

typedef struct ArPhysicsParticipant {
    ArInstance instance;
    /* First post-Step evaluator in the existing ordered plan. Required.
       Earlier evaluators run before Step; this evaluator and its suffix after. */
    const char* post_physics_evaluator;
} ArPhysicsParticipant;

typedef struct ArPhysicsTick {
    uint64_t tick_id;
    double evaluation_seconds;
    double delta_seconds;
} ArPhysicsTick;

typedef struct ArPhysicsWorldDesc {
    uint32_t struct_size;
    uint32_t abi_version;
    const char* id;
    const ArPhysicsParticipant* participants;
    uint32_t participant_count;
    double start_seconds;
    double fixed_step_seconds;
    uint32_t max_catch_up_ticks; /* excess time is retained, never dropped */
    void* user_data; /* solver/adapter state and code outlive the world binding */
    ArStatus (AR_CALL *step)(void*, const ArPhysicsTick*, const ArDiagnosticSink*);
    /* Reinitialize the entire shared world, including non-avatar objects.
       Must restore the same declared start state on every successful call. */
    ArStatus (AR_CALL *reset)(void*, const ArDiagnosticSink*);
} ArPhysicsWorldDesc;

typedef struct ArPhysicsInput {
    ArInstance instance;
    /* Observations/revision are borrowed for one advance. Identity and time
       are assigned by runtime per fixed tick, not copied from this template. */
    const ArInputFrame* input;
} ArPhysicsInput;

typedef struct ArPhysicsAdvanceDesc {
    uint32_t struct_size;
    uint32_t abi_version;
    uint64_t request_id; /* strictly increasing once elapsed time is accepted */
    double elapsed_seconds;
    uint32_t paused; /* paused requests discard elapsed time, including backlog processing */
    const ArPhysicsInput* inputs; /* exactly one per participant, in any order */
    uint32_t input_count;
} ArPhysicsAdvanceDesc;

typedef struct ArPhysicsAdvanceResult {
    uint32_t struct_size;
    uint32_t abi_version;
    uint32_t tick_count;
    uint64_t last_tick_id;
    double simulation_seconds;
    double pending_seconds;
} ArPhysicsAdvanceResult;

typedef struct ArPhysicsApi {
    uint32_t struct_size;
    uint32_t abi_version;
    ArStatus (AR_CALL *create_world)(ArRuntime, const ArPhysicsWorldDesc*, const ArDiagnosticSink*, ArPhysicsWorld*);
    ArStatus (AR_CALL *destroy_world)(ArRuntime, ArPhysicsWorld);
    ArStatus (AR_CALL *reset_world)(ArRuntime, ArPhysicsWorld, uint64_t generation, const ArDiagnosticSink*);
    /* snapshots has input_count writable slots, returned in input order.
       A zero-tick request retains the latest committed snapshots (or zero
       before the first tick). On failure every slot is zero. */
    ArStatus (AR_CALL *advance_world)(ArRuntime, ArPhysicsWorld, const ArPhysicsAdvanceDesc*,
        const ArDiagnosticSink*, ArPhysicsAdvanceResult*, ArSnapshot* snapshots);
} ArPhysicsApi;

/* Descriptor/view headers use AR_HEADER, just like core revision 4. The
   explicit version here selects the independent physics table revision. */
AR_EXPORT ArStatus AR_CALL arGetPhysicsApi(uint32_t version, uint32_t size, ArPhysicsApi* out_api);

#ifdef __cplusplus
}
#endif
#endif
