#pragma once
#include <toon/fast/avatar_state.hpp>
#include "pxr/base/gf/matrix4d.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <stdexcept>

namespace avatarMotionCheck {
// An explicit host probe scene, not discovery of an avatar's render resources.
// Renderer conversion stays in the installed Toon adapter. USD matrices form
// an independent transport oracle after the existing owner/state comparison.
class ToonCheck {
    static void Check(bool value, const std::string& message) {
        if (!value) throw std::runtime_error("TOON_CHECK: " + message);
    }
    static Toon::AvatarMaterialField Field(const char* input) {
        using F = Toon::AvatarMaterialField;
        // Host policy consumes the format owner's canonical slot table.
        const std::map<std::string,F> fields{{"color",F::BaseColorRgb},{"emissionColor",F::Emissive},
            {"shadeColor",F::ShadeColor},{"matcapColor",F::Matcap},{"rimColor",F::RimColor},
            {"outlineColor",F::OutlineColor}};
        for (const auto& slot : vrmRig::GetMaterialColorSlots()) {
            if (std::string(input) == slot.colorInput) return fields.at(slot.name);
            if (slot.alphaInput && std::string(input) == slot.alphaInput) return F::Alpha;
        }
        throw std::runtime_error(std::string("TOON_CHECK: unsupported canonical material input: ") + input);
    }
    static void SetMaterial(Toon::ToonMaterial& material, Toon::AvatarMaterialField field, const double* value) {
        using F = Toon::AvatarMaterialField;
        const Toon::Float3 rgb{float(value[0]),float(value[1]),float(value[2])};
        switch (field) {
        case F::BaseColorRgb: material.base_color = rgb; break;
        case F::Alpha: material.alpha = float(value[0]); break;
        case F::Emissive: material.emissive = rgb; break;
        case F::ShadeColor: material.mtoon.shade_color = rgb; break;
        case F::Matcap: material.mtoon.matcap = rgb; break;
        case F::RimColor: material.mtoon.rim_color = rgb; break;
        case F::OutlineColor: material.outline_color = rgb; break;
        default: throw std::runtime_error("TOON_CHECK: unsupported probe field");
        }
    }
    static const Toon::MeshSnapshot& Mesh(const Toon::FrameSnapshot& state, Toon::MeshId id) {
        const auto found = std::find_if(state.meshes.begin(),state.meshes.end(),[&](const auto& m) { return m.id == id; });
        Check(found != state.meshes.end(),"missing probe mesh"); return *found;
    }
public:
    explicit ToonCheck(const ArStateView& baseline) {
        Check(baseline.joint_count != 0,"probe scene requires a rig");
        Check(baseline.visibility_count == 0,"probe scene has no visibility mapping");
        Toon::RenderWorld world;
        const auto mesh = world.CreateMesh();
        world.SetMeshPoints(mesh,{{-.4f,-.4f,0},{.4f,-.4f,0},{0,.4f,0}});
        world.SetMeshTopology(mesh,{0,1,2});
        if (baseline.joint_count) {
            Toon::ToonSkin skin; skin.constant = true; skin.influences_per_point = 1; skin.influences = {{0,1}};
            world.SetMeshSkin(mesh,skin);
            world.SetMeshSkinPose(mesh,{std::vector<Toon::Matrix4>(baseline.joint_count),{}});
            Toon::AvatarSkinBinding skinBinding; skinBinding.mesh = mesh;
            // Reversed palette order makes identity/slot errors observable.
            for (uint32_t i = baseline.joint_count; i; --i) skinBinding.joints.push_back(i-1);
            skinBinding.inverse_bind.resize(baseline.joint_count);
            bindings_.skins.push_back(std::move(skinBinding));
        }
        if (baseline.blend_shape_count) {
            Toon::ToonMorph morph;
            for (uint32_t i = 0; i < baseline.blend_shape_count; ++i) morph.offsets.push_back({{.01f,0,0},i,{},0});
            morph.ranges = {{0,baseline.blend_shape_count},{0,baseline.blend_shape_count},{0,baseline.blend_shape_count}};
            world.SetMeshMorph(mesh,morph);
            std::vector<float> weights(baseline.blend_shape_count);
            for (uint32_t i = 0; i < baseline.blend_shape_count; ++i) {
                const auto destination = baseline.blend_shape_count-1-i;
                weights[destination] = float(baseline.blend_shapes[i].weight);
                bindings_.morphs.push_back({i,mesh,destination});
            }
            world.SetMeshMorphWeights(mesh,weights);
        }
        std::map<std::string,Toon::MaterialId> ids;
        std::map<Toon::MaterialId,Toon::ToonMaterial> materials;
        for (uint32_t i = 0; i < baseline.material_count; ++i) {
            const auto& input = baseline.materials[i];
            auto inserted = ids.emplace(input.material_id,0);
            if (inserted.second) inserted.first->second = world.CreateMaterial();
            const auto id = inserted.first->second;
            materials[id].model = Toon::ToonShadingModel::MToon;
            const auto field = Field(input.input_id);
            SetMaterial(materials[id],field,input.value);
            bindings_.materials.push_back({i,id,field});
        }
        for (const auto& entry : materials) world.SetMaterial(entry.first,entry.second);
        if (!materials.empty()) world.SetMeshMaterial(mesh,materials.begin()->first);
        scene_ = world.Commit();
        // Exercise metre-to-scene-unit conversion independently of asset units.
        scene_.meters_per_unit = .01f;
        draws_ = Toon::ExtractDrawList(scene_);
    }
    double Compare(const ArStateView& state, const Toon::FrameSnapshot& output) const {
        Check(output.time_seconds == state.evaluation_seconds,"evaluation time mismatch");
        Check(output.meshes.size() == scene_.meshes.size(),"probe membership changed");
        double maxError = 0;
        auto delta = [&](double actual, double expected) {
            const auto error = std::abs(actual-expected)/std::max(1.,std::abs(expected));
            Check(std::isfinite(actual) && std::isfinite(expected) && error <= 2e-6,
                "resolved value mismatch: actual=" + std::to_string(actual) + " expected=" + std::to_string(expected));
            maxError = std::max(maxError,error);
        };
        std::vector<pxr::GfMatrix4d> world(state.joint_count);
        for (uint32_t i = 0; i < state.joint_count; ++i) {
            const auto& joint = state.joints[i]; const auto& t = joint.local;
            pxr::GfMatrix4d scale(1),rotation(1),translation(1);
            scale.SetScale(pxr::GfVec3d(t.scale[0],t.scale[1],t.scale[2]));
            rotation.SetRotate(pxr::GfQuatd(t.rotation[3],pxr::GfVec3d(t.rotation[0],t.rotation[1],t.rotation[2])));
            translation.SetTranslate(pxr::GfVec3d(t.translation[0],t.translation[1],t.translation[2])/double(scene_.meters_per_unit));
            world[i] = scale*rotation*translation;
            if (joint.parent_index >= 0) world[i] *= world[joint.parent_index];
        }
        for (const auto& skin : bindings_.skins) {
            const auto& mesh = Mesh(output,skin.mesh);
            Check(mesh.joints && mesh.joints->size() == skin.joints.size(),"palette count mismatch");
            for (size_t i = 0; i < skin.joints.size(); ++i)
                for (int row = 0; row < 4; ++row) for (int column = 0; column < 4; ++column) {
                    // Translation tolerance is in canonical metres, independent
                    // of the probe's scene unit; other entries are dimensionless.
                    const auto unit = row == 3 && column < 3 ? double(scene_.meters_per_unit) : 1.;
                    delta(double((*mesh.joints)[i].m[row*4+column])*unit,world[skin.joints[i]][row][column]*unit);
                }
        }
        for (const auto& binding : bindings_.morphs) {
            const auto& mesh = Mesh(output,binding.mesh);
            Check(mesh.morph_weights && binding.weight < mesh.morph_weights->size(),"weight slot missing");
            delta((*mesh.morph_weights)[binding.weight],state.blend_shapes[binding.source].weight);
        }
        for (const auto& original : scene_.materials) {
            auto expected = original.material;
            for (const auto& binding : bindings_.materials) if (binding.material == original.id) {
                const auto& value = state.materials[binding.source];
                if (value.overridden) SetMaterial(expected,binding.field,value.value);
            }
            const auto found = std::find_if(output.materials.begin(),output.materials.end(),[&](const auto& m) { return m.id == original.id; });
            Check(found != output.materials.end() && found->material == expected,"material RGB/alpha mismatch");
            Check(found->structure_revision == original.structure_revision,"material structure changed");
        }
        for (const auto& mesh : output.meshes) {
            const auto& original = Mesh(scene_,mesh.id);
            Check(mesh.points == original.points && mesh.indices == original.indices && mesh.influences == original.influences &&
                mesh.morph_offsets == original.morph_offsets,"static resources changed");
        }
        return maxError;
    }
    void Apply(const ArRuntimeApi& api, ArSnapshot handle) {
        std::string error;
        Check(current_.Reset(api,handle,error),error);
        const auto& state = *current_.View();
        if (!bound_) { Check(adapter_.Bind(state,scene_,1,bindings_,error),error); bound_ = true; }
        Check(adapter_.Apply(current_,scene_,1,output_,error),error);
        maxError_ = std::max(maxError_,Compare(state,output_));
        const auto& identity = adapter_.IdentityInfo();
        Check(identity.instance == state.instance && identity.frame == state.frame_id && identity.generation == state.generation &&
            identity.input_revision == state.input_revision && identity.layout == state.layout_id &&
            identity.layout_version == state.layout_version && identity.binding_epoch == 1,"frame identity lost");
        Check(Toon::ApplyFastSnapshot(output_,draws_,error),error);
        Check(draws_.draws.size() == output_.meshes.size() && draws_.draws.front().joints == output_.meshes.front().joints &&
            draws_.draws.front().morph_weights == output_.meshes.front().morph_weights,"late draw values differ");
        // A held duplicate must reuse dynamic arrays and revisions.
        const auto previous = output_;
        Check(adapter_.Apply(current_,scene_,1,output_,error),error);
        Check(output_.meshes.front().joints == previous.meshes.front().joints &&
            output_.meshes.front().pose_revision == previous.meshes.front().pose_revision &&
            output_.meshes.front().morph_weights == previous.meshes.front().morph_weights &&
            output_.meshes.front().morph_weights_revision == previous.meshes.front().morph_weights_revision,"duplicate update rewrote arrays");
        for (size_t i = 0; i < output_.materials.size(); ++i)
            Check(output_.materials[i].parameters_revision == previous.materials[i].parameters_revision,"duplicate material write");
        if (!first_.View()) Check(first_.Reset(api,handle,error),error);
        ++frames_;
    }
    void AfterProducerDestruction() {
        Check(current_.View() && first_.View(),"retained transport missing");
        std::string error;
        const auto prior = output_;
        Check(!adapter_.Apply(first_,scene_,1,output_,error) && !error.empty(),"pre-reset frame accepted");
        Check(output_.revision == prior.revision && output_.meshes.front().joints == prior.meshes.front().joints,"failed update changed output");
        Check(adapter_.Apply(current_,scene_,1,output_,error),error);
        Compare(*current_.View(),output_);
        Toon::AvatarStateAdapter rebound;
        Check(rebound.Bind(*first_.View(),scene_,2,bindings_,error),error);
        Toon::FrameSnapshot old;
        Check(rebound.Apply(first_,scene_,2,old,error),error); Compare(*first_.View(),old);
        auto replacement = *first_.View(); replacement.layout_version++;
        // Bind a different structural layout and reject the older snapshot.
        Check(rebound.Bind(replacement,scene_,3,bindings_,error),error);
        Check(!rebound.Apply(first_,scene_,3,old,error),"changed layout accepted without matching snapshot");
        Check(adapter_.Release(scene_,1,output_,error),error);
        Check(!adapter_.IdentityInfo().active,"release left identity active");
        Check(*output_.meshes.front().joints == *scene_.meshes.front().joints &&
            ((!scene_.meshes.front().morph_weights && !output_.meshes.front().morph_weights) ||
             (scene_.meshes.front().morph_weights && output_.meshes.front().morph_weights &&
              *scene_.meshes.front().morph_weights == *output_.meshes.front().morph_weights)),"release did not restore probe baseline");
        for (size_t i = 0; i < scene_.materials.size(); ++i)
            Check(output_.materials[i].material == scene_.materials[i].material,"release did not restore materials");
    }
    uint64_t Frames() const { return frames_; }
    double MaxError() const { return maxError_; }
    const Toon::FrameSnapshot& ProbeScene() const { return scene_; }
private:
    Toon::AvatarBindings bindings_;
    Toon::FrameSnapshot scene_,output_;
    Toon::DrawList draws_;
    Toon::AvatarStateAdapter adapter_;
    Toon::RetainedAvatarSnapshot first_,current_;
    bool bound_ = false;
    uint64_t frames_ = 0;
    double maxError_ = 0;
};
} // namespace avatarMotionCheck
