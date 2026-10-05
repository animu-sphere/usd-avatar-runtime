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
} ArInputFrame;
#endif
