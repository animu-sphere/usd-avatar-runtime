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
} ArStateView;

/* All writer operations are scoped to one callback. Errors also latch a frame
   failure, even if the provider ignores the returned status. Layout is fixed. */
typedef struct ArStateWriter {
    void* context;
    ArStatus (AR_CALL *set_joint)(void*, uint32_t, const ArTransform*);
    ArStatus (AR_CALL *set_blend_shape)(void*, uint32_t, double);
    ArStatus (AR_CALL *set_material)(void*, uint32_t, uint32_t, const double*);
    ArStatus (AR_CALL *set_visibility)(void*, uint32_t, uint32_t);
} ArStateWriter;
#endif
