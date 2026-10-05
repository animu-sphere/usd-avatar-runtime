#ifndef AVATAR_RUNTIME_INPUT_H
#define AVATAR_RUNTIME_INPUT_H
#include "avatarRuntime/types.h"

/* Missing array entry is absent; an entry with value 0 is explicitly present.
   Identity is (source_id, actor_id, channel_id). No implicit arbitration. */
typedef struct ArScalarInput {
    const char* source_id;
    const char* actor_id;
    const char* channel_id;
    double value;
    double source_seconds;
    double clock_scale;  /* positive: runtime sample = source * scale + offset */
    double clock_offset;
} ArScalarInput;

#define AR_GAZE_POINT 1u
#define AR_GAZE_DIRECTION 2u
#define AR_GAZE_RUNTIME_WORLD 1u
#define AR_GAZE_JOINT_LOCAL 2u
#define AR_OBSERVATION_VALID 1u
#define AR_OBSERVATION_UNAVAILABLE 2u
#define AR_OBSERVATION_STALE 3u

/* Already selected observations; the runtime never arbitrates or converts.
   All vectors use the canonical motion basis (+Y up, +Z forward, right-handed).
   Head/eye-relative data uses JOINT_LOCAL with an explicit bound rig joint. */
typedef struct ArGazeInput {
    const char* source_id;
    const char* actor_id;
    const char* channel_id; /* namespaced; shares identity with scalar inputs */
    uint32_t kind;         /* POINT (metres) or DIRECTION (unit vector) */
    uint32_t space;        /* RUNTIME_WORLD or JOINT_LOCAL */
    uint32_t validity;     /* VALID, UNAVAILABLE or STALE; never inferred */
    const char* skeleton_id; /* JOINT_LOCAL: bound skeleton identity */
    const char* joint_id;    /* JOINT_LOCAL: bound joint identity */
                            /* RUNTIME_WORLD: both reference pointers are null */
    double value[3]; /* UNAVAILABLE: all zero; STALE retains the old observation */
    double source_seconds;
    double clock_scale; /* positive: runtime sample = source * scale + offset */
    double clock_offset;
} ArGazeInput;

typedef struct ArInputFrame {
    uint32_t struct_size;
    uint32_t abi_version;
    uint64_t frame_id; /* strictly increasing after each successful commit */
    uint64_t generation;
    double evaluation_seconds;
    uint32_t has_usd_mapping; /* 0 or 1; no stage dependency */
    double usd_time_codes_per_second;
    double usd_time_code_offset;
    const ArScalarInput* scalars;
    uint32_t scalar_count;
    uint64_t input_revision; /* host-selected source/mapping revision; 0 = unspecified */
    const ArGazeInput* gazes; /* borrowed synchronously; no entry means absent */
    uint32_t gaze_count;
} ArInputFrame;
#endif
