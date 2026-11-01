#pragma once
#include <avatarRuntime/api.h>
#include <sstream>
#include <string>

namespace avatarCheck {
// One committed state as JSON, for scripts/compare_avatar_state.py. Identities are
// paths and tokens, which need no escaping.
inline std::string StateJson(const ArStateView& v) {
    std::ostringstream o; o.precision(17);
    const auto numbers = [&](const double* values, int count) {
        o << '['; for (int k = 0; k < count; ++k) o << (k ? "," : "") << values[k]; o << ']';
    };
    o << "{\"layout\":\"" << v.layout_id << "\",\"layout_version\":" << v.layout_version << ",\"frame\":" << v.frame_id
      << ",\"generation\":" << v.generation << ",\"seconds\":" << v.evaluation_seconds << ",\"capabilities\":[";
    for (uint32_t i = 0; i < v.capability_count; ++i) o << (i ? "," : "") << '"' << v.capabilities[i].id << '"';
    o << "],\"joints\":[";
    for (uint32_t i = 0; i < v.joint_count; ++i) {
        const auto& j = v.joints[i];
        o << (i ? "," : "") << "{\"skeleton\":\"" << j.skeleton_id << "\",\"joint\":\"" << j.joint_id << "\",\"parent\":" << j.parent_index << ",\"translation\":";
        numbers(j.local.translation,3); o << ",\"rotation\":"; numbers(j.local.rotation,4); o << ",\"scale\":"; numbers(j.local.scale,3); o << '}';
    }
    o << "],\"morphs\":[";
    for (uint32_t i = 0; i < v.blend_shape_count; ++i) {
        const auto& m = v.blend_shapes[i];
        o << (i ? "," : "") << "{\"mesh\":\"" << m.mesh_id << "\",\"target\":\"" << m.target_id << "\",\"weight\":" << m.weight << '}';
    }
    o << "],\"uvs\":[";
    for (uint32_t i = 0; i < v.vertex_uv_count; ++i) {
        const auto& u = v.vertex_uvs[i];
        o << (i ? "," : "") << "{\"mesh\":\"" << u.mesh_id << "\",\"vertex\":" << u.vertex_index << ",\"value\":"; numbers(u.value,2); o << '}';
    }
    o << "],\"materials\":[";
    for (uint32_t i = 0; i < v.material_count; ++i) {
        const auto& m = v.materials[i];
        o << (i ? "," : "") << "{\"material\":\"" << m.material_id << "\",\"input\":\"" << m.input_id << "\",\"type\":" << m.value_type
          << ",\"overridden\":" << m.overridden << ",\"value\":"; numbers(m.value,4); o << '}';
    }
    o << "],\"samples\":[";
    for (uint32_t i = 0; i < v.sample_count; ++i) {
        const auto& r = v.samples[i];
        o << (i ? "," : "") << "{\"channel\":\"" << r.channel_id << "\",\"kind\":" << r.kind << ",\"validity\":" << r.validity
          << ",\"resolution\":" << r.resolution << ",\"source_seconds\":" << r.source_seconds << '}';
    }
    o << "]}";
    return o.str();
}
} // namespace avatarCheck
