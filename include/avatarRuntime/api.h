#ifndef AVATAR_RUNTIME_API_H
#define AVATAR_RUNTIME_API_H
#include "avatarRuntime/evaluator.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ArInstanceDesc {
    uint32_t struct_size;
    uint32_t abi_version;
    uint64_t generation; /* nonzero; reset increases it */
    const char* const* evaluators; /* selected IDs; discovery order is ignored */
    uint32_t evaluator_count;
    const ArCapability* bound_capabilities;
    uint32_t bound_capability_count;
    const ArCapability* required_capabilities;
    uint32_t required_capability_count;
    ArStateView initial_state; /* complete authored baseline; identity ignored */
} ArInstanceDesc;

typedef struct ArRuntimeApi {
    uint32_t struct_size;
    uint32_t abi_version;
    ArStatus (AR_CALL *create_runtime)(ArRuntime*);
    ArStatus (AR_CALL *destroy_runtime)(ArRuntime);
    ArStatus (AR_CALL *register_evaluator)(ArRuntime, const ArEvaluatorDesc*, const ArDiagnosticSink*);
    ArStatus (AR_CALL *create_instance)(ArRuntime, const ArInstanceDesc*, const ArDiagnosticSink*, ArInstance*);
    ArStatus (AR_CALL *destroy_instance)(ArRuntime, ArInstance);
    ArStatus (AR_CALL *reset_instance)(ArRuntime, ArInstance, uint64_t generation);
    ArStatus (AR_CALL *evaluate_frame)(ArRuntime, ArInstance, const ArInputFrame*, const ArDiagnosticSink*, ArSnapshot*);
    ArStatus (AR_CALL *get_snapshot)(ArSnapshot, ArStateView*);
    ArStatus (AR_CALL *retain_snapshot)(ArSnapshot);
    ArStatus (AR_CALL *release_snapshot)(ArSnapshot);
    ArStatus (AR_CALL *get_capabilities)(ArRuntime, ArInstance, const ArCapability**, uint32_t*);
} ArRuntimeApi;

/* Only this symbol crosses the shared-library boundary. Caller owns out_api;
   size must cover the complete revision-1 table. Larger tails are untouched. */
AR_EXPORT ArStatus AR_CALL arGetApi(uint32_t version, uint32_t size, ArRuntimeApi* out_api);

#ifdef __cplusplus
}
#endif
#endif
