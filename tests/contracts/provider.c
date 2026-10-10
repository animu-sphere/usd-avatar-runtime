#include "avatarRuntime/api.h"
#include <stdlib.h>

#if defined(_WIN32)
#define PROVIDER_EXPORT __declspec(dllexport)
#else
#define PROVIDER_EXPORT __attribute__((visibility("default")))
#endif

typedef struct Counter { double committed, pending; } Counter;
static ArStatus AR_CALL create(void* user, ArInstance instance, uint64_t generation, void** out) {
    (void)user; (void)instance; (void)generation;
    *out = calloc(1, sizeof(Counter));
    return *out ? AR_OK : AR_OUT_OF_MEMORY;
}
static void AR_CALL destroy(void* user, void* state) { (void)user; free(state); }
static ArStatus AR_CALL begin(void* user, void* state, const ArEvaluationContext* ctx) {
    Counter* c = (Counter*)state; (void)user; (void)ctx;
    c->pending = c->committed;
    return AR_OK;
}
static ArStatus AR_CALL evaluate(void* user, void* state, const ArEvaluationContext* ctx, const ArStateWriter* writer) {
    Counter* c = (Counter*)state;
    ArTransform t = ctx->working->joints[0].local;
    (void)user;
    c->pending += 1.0;
    t.translation[0] = c->pending + ctx->input->evaluation_seconds;
    /* Synthetic transport check only; this is not a LookAt algorithm. */
    if (ctx->input->gaze_count && ctx->input->gazes[0].validity == AR_OBSERVATION_VALID)
        t.translation[1] = ctx->input->gazes[0].value[0];
    {
        /* Held boundary sample: the content keeps its original source time. */
        ArSourceSample sample = {"clip", "actor", "test:pose", "ignored", AR_SOURCE_POSE,
            AR_OBSERVATION_VALID, AR_SAMPLE_HELD, 1, 2, 0.5, -1};
        ArStatus status = writer->report_sample(writer->context, &sample);
        if (status != AR_OK) return status;
    }
    return writer->set_joint(writer->context, 0, &t);
}
static void AR_CALL finish(void* user, void* state, uint32_t commit) {
    Counter* c = (Counter*)state; (void)user;
    if (commit) c->committed = c->pending;
    else c->pending = c->committed;
}
PROVIDER_EXPORT ArStatus AR_CALL registerContractProvider(const ArRuntimeApi* api, ArRuntime runtime) {
    ArCapability capability = {"test.pose", 1};
    ArEvaluatorDesc desc = {AR_HEADER(ArEvaluatorDesc)};
    desc.id = "test.counter"; desc.provider_id = "test.c-provider"; desc.provider_version = "1";
    desc.phase = AR_PHASE_BASE_POSE; desc.writes = AR_DOMAIN_POSE;
    desc.flags = AR_EVALUATOR_STATEFUL; desc.supplies = &capability; desc.supply_count = 1;
    desc.create_state = create; desc.destroy_state = destroy; desc.begin_frame = begin;
    desc.evaluate = evaluate; desc.end_frame = finish;
    return api->register_evaluator(runtime, &desc, NULL);
}
