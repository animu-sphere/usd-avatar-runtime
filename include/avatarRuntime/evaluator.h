#ifndef AVATAR_RUNTIME_EVALUATOR_H
#define AVATAR_RUNTIME_EVALUATOR_H
#include "avatarRuntime/input.h"
#include "avatarRuntime/state.h"
#include "avatarRuntime/diagnostics.h"

typedef struct ArEvaluationContext {
    uint32_t struct_size;
    uint32_t abi_version;
    const ArInputFrame* input;
    const ArStateView* prior; /* null before first commit/reset */
    const ArStateView* working; /* only declared read/write domains exposed */
    ArDiagnosticSink diagnostics; /* provider codes retained, context stamped */
} ArEvaluationContext;

#define AR_EVALUATOR_STATEFUL 1u
typedef struct ArEvaluatorDesc {
    uint32_t struct_size;
    uint32_t abi_version;
    const char* id;
    const char* provider_id;
    const char* provider_version;
    ArPhase phase;
    ArDomain reads;
    ArDomain writes;
    uint32_t flags;
    const char* const* after; /* required predecessor evaluator IDs */
    uint32_t after_count;
    const ArCapability* supplies;
    uint32_t supply_count;
    const ArCapability* required;
    uint32_t require_count;
    void* user_data; /* borrowed until runtime destruction; code stays loaded */
    ArStatus (AR_CALL *create_state)(void*, ArInstance, uint64_t, void**);
    void (AR_CALL *destroy_state)(void*, void*);
    ArStatus (AR_CALL *begin_frame)(void*, void*, const ArEvaluationContext*);
    ArStatus (AR_CALL *evaluate)(void*, void*, const ArEvaluationContext*, const ArStateWriter*);
    void (AR_CALL *end_frame)(void*, void*, uint32_t commit);
} ArEvaluatorDesc;

/* STATEFUL requires all lifecycle callbacks. Stateless descriptors supply only
   evaluate. end_frame/destroy_state must not fail or throw. begin_frame failure
   still receives end_frame(commit=0); abort runs in reverse begin order.
   destroy_state must accept null/partial state after failed create_state. */
#endif
