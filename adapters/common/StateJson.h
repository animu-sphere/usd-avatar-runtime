#pragma once
#include <avatarRuntime/api.h>
#include <cstdio>
#include <sstream>
#include <string>

namespace avatarCheck {
// One committed state as JSON, for scripts/compare_avatar_state.py. A consumer writes
// what it received in the same format; the fields are listed in
// docs/architecture/OUTPUT_PATHS.md#4-parity-evidence.
inline std::string StateJson(const ArStateView& v) {
    std::ostringstream o; o.precision(17);
    const auto numbers = [&](const double* values, int count) {
        o << '['; for (int k = 0; k < count; ++k) o << (k ? "," : "") << values[k]; o << ']';
    };
    // Identities are opaque UTF-8, copied byte for byte; only JSON's own characters are escaped.
    const auto text = [&](const char* value) {
        o << '"';
        for (const char* c = value ? value : ""; *c; ++c) {
            const auto byte = static_cast<unsigned char>(*c);
            if (byte == '"' || byte == '\\') o << '\\' << *c;
            else if (byte < 0x20) { char escaped[8]; std::snprintf(escaped, sizeof escaped, "\\u%04x", byte); o << escaped; }
            else o << *c;
        }
        o << '"';
    };
    o << "{\"instance\":" << v.instance << ",\"layout\":"; text(v.layout_id);
    o << ",\"layout_version\":" << v.layout_version << ",\"frame\":" << v.frame_id
      << ",\"generation\":" << v.generation << ",\"input_revision\":" << v.input_revision
      << ",\"seconds\":" << v.evaluation_seconds << ",\"capabilities\":[";
    for (uint32_t i = 0; i < v.capability_count; ++i) { o << (i ? "," : ""); text(v.capabilities[i].id); }
    o << "],\"joints\":[";
    for (uint32_t i = 0; i < v.joint_count; ++i) {
        const auto& j = v.joints[i];
        o << (i ? "," : "") << "{\"skeleton\":"; text(j.skeleton_id); o << ",\"joint\":"; text(j.joint_id);
        o << ",\"parent\":" << j.parent_index << ",\"translation\":";
        numbers(j.local.translation,3); o << ",\"rotation\":"; numbers(j.local.rotation,4); o << ",\"scale\":"; numbers(j.local.scale,3); o << '}';
    }
    o << "],\"morphs\":[";
    for (uint32_t i = 0; i < v.blend_shape_count; ++i) {
        const auto& m = v.blend_shapes[i];
        o << (i ? "," : "") << "{\"mesh\":"; text(m.mesh_id); o << ",\"target\":"; text(m.target_id);
        o << ",\"weight\":" << m.weight << '}';
    }
    o << "],\"uvs\":[";
    for (uint32_t i = 0; i < v.vertex_uv_count; ++i) {
        const auto& u = v.vertex_uvs[i];
        o << (i ? "," : "") << "{\"mesh\":"; text(u.mesh_id); o << ",\"vertex\":" << u.vertex_index << ",\"value\":"; numbers(u.value,2); o << '}';
    }
    o << "],\"materials\":[";
    for (uint32_t i = 0; i < v.material_count; ++i) {
        const auto& m = v.materials[i];
        o << (i ? "," : "") << "{\"material\":"; text(m.material_id); o << ",\"input\":"; text(m.input_id);
        o << ",\"type\":" << m.value_type << ",\"overridden\":" << m.overridden << ",\"value\":"; numbers(m.value,4); o << '}';
    }
    o << "],\"visibility\":[";
    for (uint32_t i = 0; i < v.visibility_count; ++i) {
        o << (i ? "," : "") << "{\"target\":"; text(v.visibility[i].target_id); o << ",\"visible\":" << v.visibility[i].visible << '}';
    }
    o << "],\"samples\":[";
    for (uint32_t i = 0; i < v.sample_count; ++i) {
        const auto& r = v.samples[i];
        o << (i ? "," : "") << "{\"channel\":"; text(r.channel_id);
        o << ",\"kind\":" << r.kind << ",\"validity\":" << r.validity
          << ",\"resolution\":" << r.resolution << ",\"source_seconds\":" << r.source_seconds << '}';
    }
    o << "]}";
    return o.str();
}
} // namespace avatarCheck
