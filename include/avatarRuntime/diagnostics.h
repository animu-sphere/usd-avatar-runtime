#ifndef AVATAR_RUNTIME_DIAGNOSTICS_H
#define AVATAR_RUNTIME_DIAGNOSTICS_H
#include "avatarRuntime/types.h"

#define AR_SEVERITY_INFO 0u
#define AR_SEVERITY_WARNING 1u
#define AR_SEVERITY_ERROR 2u
typedef struct ArDiagnostic {
    uint32_t struct_size;
    uint32_t abi_version;
    const char* code;
    const char* origin;
    const char* evaluator_id; /* empty for runtime validation */
    const char* subject;      /* source/channel/target when available */
    const char* message;
    uint32_t severity;
    ArStatus status; /* independent of severity */
    ArInstance instance;
    uint64_t frame_id;
    ArPhase phase; /* 0 for runtime boundaries */
} ArDiagnostic;

typedef struct ArDiagnosticSink {
    void* user_data;
    void (AR_CALL *emit)(void*, const ArDiagnostic*);
} ArDiagnosticSink;
#endif
