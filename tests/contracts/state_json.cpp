// The consumer export format that scripts/compare_avatar_state.py reads: every field
// of the consumer contract is written, and identities survive as opaque UTF-8.
#include "../../adapters/common/StateJson.h"
#include <cstdio>
#include <string>

int main() {
    const ArCapability capabilities[] = {{"avatar.a", 1}, {"avatar.b", 1}};
    ArJoint joints[] = {
        {"/Skel", "root", -1, {{0, 1, 0}, {0, 0, 0, 1}, {1, 1, 1}}},
        {"/Skel", "root/\xe9\xab\xaa \"q\"\\", 0, {{0, 0.5, 0}, {0, 0, 0, 1}, {1, 1, 1}}},
    };
    const ArBlendShape morphs[] = {{"/Mesh", "smile", 0}};
    const ArVertexUv uvs[] = {{"/Mesh", 3, {0.25, 0.5}}};
    const ArMaterialInput materials[] = {{"/Mat", "inputs:alpha", AR_VALUE_SCALAR, 1, {0, 0, 0, 0}}};
    const ArVisibility visibility[] = {{"/Mesh\t1", 0}};
    ArSourceSample sample{"host", "actor", "motion:pose", "motion.body", AR_SOURCE_POSE, AR_OBSERVATION_VALID,
        AR_SAMPLE_HELD, 1.5, 1, 0, 1.5};

    ArStateView v{AR_HEADER(ArStateView)};
    v.instance = 42; v.frame_id = 3; v.generation = 2; v.evaluation_seconds = 0.25;
    v.joints = joints; v.joint_count = 2;
    v.blend_shapes = morphs; v.blend_shape_count = 1;
    v.materials = materials; v.material_count = 1;
    v.visibility = visibility; v.visibility_count = 1;
    v.layout_id = "layout"; v.layout_version = 7; v.input_revision = 9;
    v.capabilities = capabilities; v.capability_count = 2;
    v.samples = &sample; v.sample_count = 1;
    v.vertex_uvs = uvs; v.vertex_uv_count = 1;

    const std::string expected =
        "{\"instance\":42,\"layout\":\"layout\",\"layout_version\":7,\"frame\":3,\"generation\":2,\"input_revision\":9,"
        "\"seconds\":0.25,\"capabilities\":[\"avatar.a\",\"avatar.b\"],\"joints\":["
        "{\"skeleton\":\"/Skel\",\"joint\":\"root\",\"parent\":-1,\"translation\":[0,1,0],\"rotation\":[0,0,0,1],\"scale\":[1,1,1]},"
        "{\"skeleton\":\"/Skel\",\"joint\":\"root/\xe9\xab\xaa \\\"q\\\"\\\\\",\"parent\":0,\"translation\":[0,0.5,0],"
        "\"rotation\":[0,0,0,1],\"scale\":[1,1,1]}],"
        "\"morphs\":[{\"mesh\":\"/Mesh\",\"target\":\"smile\",\"weight\":0}],"
        "\"uvs\":[{\"mesh\":\"/Mesh\",\"vertex\":3,\"value\":[0.25,0.5]}],"
        "\"materials\":[{\"material\":\"/Mat\",\"input\":\"inputs:alpha\",\"type\":1,\"overridden\":1,\"value\":[0,0,0,0]}],"
        "\"visibility\":[{\"target\":\"/Mesh\\u00091\",\"visible\":0}],"
        "\"samples\":[{\"channel\":\"motion:pose\",\"kind\":3,\"validity\":1,\"resolution\":3,\"source_seconds\":1.5}]}";
    const auto actual = avatarCheck::StateJson(v);
    if (actual != expected) {
        std::fprintf(stderr, "state JSON differs\n actual:   %s\n expected: %s\n", actual.c_str(), expected.c_str());
        return 1;
    }

    // Empty channels stay present and empty: absence of a group is not a missing field.
    ArStateView empty{AR_HEADER(ArStateView)};
    empty.instance = 1; empty.frame_id = 1; empty.generation = 1; empty.layout_id = "l"; empty.layout_version = 1;
    const std::string bare = "{\"instance\":1,\"layout\":\"l\",\"layout_version\":1,\"frame\":1,\"generation\":1,"
        "\"input_revision\":0,\"seconds\":0,\"capabilities\":[],\"joints\":[],\"morphs\":[],\"uvs\":[],\"materials\":[],"
        "\"visibility\":[],\"samples\":[]}";
    if (avatarCheck::StateJson(empty) != bare) {
        std::fprintf(stderr, "empty state JSON differs\n actual:   %s\n", avatarCheck::StateJson(empty).c_str());
        return 1;
    }
    std::puts("state JSON: ok");
    return 0;
}
