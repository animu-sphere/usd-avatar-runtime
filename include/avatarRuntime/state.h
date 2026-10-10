#ifndef AVATAR_RUNTIME_STATE_H
#define AVATAR_RUNTIME_STATE_H
#include "avatarRuntime/types.h"

typedef struct ArJoint {
    const char* skeleton_id;
    const char* joint_id; /* opaque rig identity, not a humanoid vocabulary */
    int32_t parent_index; /* -1, or earlier joint in the same skeleton */
    ArTransform local;
} ArJoint;

typedef struct ArBlendShape {
    const char* mesh_id;
    const char* target_id;
    double weight;
} ArBlendShape;

#define AR_VALUE_SCALAR 1u
#define AR_VALUE_VEC2 2u
#define AR_VALUE_VEC3 3u
#define AR_VALUE_VEC4 4u
typedef struct ArMaterialInput {
    const char* material_id;
    const char* input_id; /* canonical typed input supplied by binding adapter */
    uint32_t value_type;
    uint32_t overridden; /* 0 = use authored value, 1 = resolved override */
    double value[4];     /* unused components must be zero */
} ArMaterialInput;

typedef struct ArVisibility {
    const char* target_id;
    uint32_t visible; /* 0 or 1 */
} ArVisibility;

#define AR_SOURCE_SCALAR 1u
#define AR_SOURCE_GAZE 2u
#define AR_SOURCE_POSE 3u
#define AR_SAMPLE_SELECTED 1u     /* input observation exactly as the host selected it */
#define AR_SAMPLE_INTERPOLATED 2u /* provider sampled within, or on, observed samples */
#define AR_SAMPLE_HELD 3u         /* provider held the nearest observed boundary sample */
#define AR_SAMPLE_EXTRAPOLATED 4u /* provider extrapolated past the newest observed sample */

/* Source-time provenance retained with a snapshot. Selected inputs are copied
   by the runtime; providers report the samples they resolved themselves.
   source_seconds is the time of the sample content, so a held value keeps its
   original time. The runtime reads no clock; it only applies the mapping. */
typedef struct ArSourceSample {
    const char* source_id;
    const char* actor_id;
    const char* channel_id;   /* namespaced; unique with source/actor per snapshot */
    const char* evaluator_id; /* reporting evaluator; empty for selected inputs */
    uint32_t kind;            /* AR_SOURCE_* */
    uint32_t validity;        /* AR_OBSERVATION_*; scalar and provider samples are VALID */
    uint32_t resolution;      /* SELECTED for inputs; INTERPOLATED/HELD/EXTRAPOLATED otherwise */
    double source_seconds;
    double clock_scale;       /* positive: runtime sample = source * scale + offset */
    double clock_offset;
    double runtime_seconds;   /* stamped by the runtime from the mapping above */
} ArSourceSample;

typedef struct ArStateView {
    uint32_t struct_size;
    uint32_t abi_version;
    ArInstance instance;
    uint64_t frame_id;
    uint64_t generation;
    double evaluation_seconds;
    const ArJoint* joints;
    uint32_t joint_count;
    const ArBlendShape* blend_shapes;
    uint32_t blend_shape_count;
    const ArMaterialInput* materials;
    uint32_t material_count;
    const ArVisibility* visibility;
    uint32_t visibility_count;
    const char* layout_id; /* copied opaque binding identity; not reset generation */
    uint64_t layout_version; /* nonzero; identifies channel order/types/parents */
    uint64_t input_revision; /* echoes ArInputFrame; 0 = unspecified */
    const ArCapability* capabilities; /* active set; retained with snapshot */
    uint32_t capability_count;
    const ArSourceSample* samples; /* selected inputs, then provider reports in plan order */
    uint32_t sample_count;         /* published/prior views only; working views are empty */
} ArStateView;

/* All writer operations are scoped to one callback. Errors also latch a frame
   failure, even if the provider ignores the returned status. Layout is fixed.
   report_sample copies an AR_SOURCE_POSE record and needs the pose write
   domain; the runtime stamps evaluator_id and runtime_seconds. */
typedef struct ArStateWriter {
    void* context;
    ArStatus (AR_CALL *set_joint)(void*, uint32_t, const ArTransform*);
    ArStatus (AR_CALL *set_blend_shape)(void*, uint32_t, double);
    ArStatus (AR_CALL *set_material)(void*, uint32_t, uint32_t, const double*);
    ArStatus (AR_CALL *set_visibility)(void*, uint32_t, uint32_t);
    ArStatus (AR_CALL *report_sample)(void*, const ArSourceSample*);
} ArStateWriter;
#endif
