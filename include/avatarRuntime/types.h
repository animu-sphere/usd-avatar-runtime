#ifndef AVATAR_RUNTIME_TYPES_H
#define AVATAR_RUNTIME_TYPES_H

#include <stdint.h>

#if defined(_WIN32)
#define AR_CALL __cdecl
#if defined(AR_BUILDING_LIBRARY)
#define AR_EXPORT __declspec(dllexport)
#else
#define AR_EXPORT __declspec(dllimport)
#endif
#else
#define AR_CALL
#define AR_EXPORT __attribute__((visibility("default")))
#endif

/* Experimental revision; not a frozen ecosystem ABI. Default platform packing. */
#define AR_ABI_VERSION 2u
#define AR_MAX_DIAGNOSTICS 256u
#define AR_HEADER(type) (uint32_t)sizeof(type), AR_ABI_VERSION

typedef uint64_t ArRuntime;
typedef uint64_t ArInstance;
typedef uint64_t ArSnapshot;
typedef int32_t ArStatus;
#define AR_OK 0
#define AR_INVALID_ARGUMENT 1
#define AR_INCOMPATIBLE_ABI 2
#define AR_INVALID_HANDLE 3
#define AR_DUPLICATE_ID 4
#define AR_MISSING_DEPENDENCY 5
#define AR_DEPENDENCY_CYCLE 6
#define AR_PHASE_ORDER 7
#define AR_WRITE_CONFLICT 8
#define AR_MISSING_CAPABILITY 9
#define AR_PROVIDER_ERROR 10
#define AR_INVALID_STATE 11
#define AR_OUT_OF_MEMORY 12
#define AR_BUSY 13

typedef uint32_t ArDomain;
#define AR_DOMAIN_POSE 1u
#define AR_DOMAIN_DEFORMATION 2u
#define AR_DOMAIN_MATERIAL 4u
#define AR_DOMAIN_VISIBILITY 8u
#define AR_DOMAIN_ALL 15u

typedef uint32_t ArPhase;
#define AR_PHASE_SAMPLE 1u
#define AR_PHASE_BLEND 2u
#define AR_PHASE_RETARGET 3u
#define AR_PHASE_BASE_POSE 4u
#define AR_PHASE_CONSTRAINTS 5u
#define AR_PHASE_GAZE 6u
#define AR_PHASE_EXPRESSIONS 7u
#define AR_PHASE_SECONDARY 8u
#define AR_PHASE_FINAL_POSE 9u
#define AR_PHASE_APPEARANCE 10u

/* Fixed value layouts are versioned by their enclosing descriptor/view. */
typedef struct ArTransform {
    double translation[3]; /* metres, parent-local; roots are runtime-world */
    double rotation[4];    /* unit quaternion x,y,z,w */
    double scale[3];
} ArTransform;

typedef struct ArCapability {
    const char* id; /* UTF-8, stable namespaced identity */
    uint32_t version; /* exact version matching in this revision */
} ArCapability;

#endif
