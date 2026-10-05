#include "avatarVrmUsd/ExpressionBinding.h"
#include "vrmRig/MaterialColorSlots.h"
#include "vrmSchema/vrmExpressionAPI.h"
#include "pxr/usd/usd/primRange.h"
#include "pxr/usd/usdGeom/mesh.h"
#include "pxr/usd/usdShade/material.h"
#include "pxr/usd/usdSkel/bindingAPI.h"
#include "pxr/usd/usdSkel/blendShape.h"
#include "pxr/usd/sdf/types.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <stdexcept>

namespace avatarVrmUsd {
namespace {
void require(bool ok, const char* code, const std::string& subject) {
    if (!ok) throw std::invalid_argument(std::string(code) + ": " + subject);
}
template<class T> T read(const pxr::UsdAttribute& attr, const pxr::SdfValueTypeName& type,
                         T fallback = T{}, bool required = false) {
    if (!required && !attr.HasAuthoredValueOpinion()) return fallback;
    T value{};
    require(attr.GetTypeName() == type && attr.Get(&value),
            "VRM_EXPRESSION_BINDING_VALUE", attr.GetPath().GetString());
    return value;
}
pxr::SdfPathVector targets(const pxr::UsdRelationship& rel) {
    pxr::SdfPathVector result;
    if (rel.HasAuthoredTargets())
        require(rel.GetForwardedTargets(&result), "VRM_EXPRESSION_BINDING_RELATIONSHIP", rel.GetPath().GetString());
    return result;
}
vrmRig::ExpressionOverride overrideValue(const pxr::UsdAttribute& attr) {
    const auto value = read(attr, pxr::SdfValueTypeNames->Token, pxr::TfToken("none"));
    bool recognized = false;
    auto result = vrmRig::ParseExpressionOverride(value.GetString(), &recognized);
    require(recognized, "VRM_EXPRESSION_BINDING_OVERRIDE", attr.GetPath().GetString());
    return result;
}
} // namespace
struct ExpressionBinding::Impl {
    HumanoidBinding humanoid;
    vrmRig::ExpressionRig rig;
    std::vector<avatarVrm::MorphBinding> morphBindings;
    std::vector<ArBlendShape> morphs;
    // Map nodes keep owned identity strings stable for the borrowed C view.
    std::map<std::pair<std::string,std::string>, ArMaterialInput> materialsById;
    std::vector<ArMaterialInput> materials;
    ArStateView baseline{};

    Impl(const pxr::UsdStagePtr& stage, const ExpressionBindingConfig& config)
        : humanoid(stage, config.humanoid) {
        const auto rootPath = config.humanoid.avatarRoot;
        const auto selected = config.expressionsRoot.IsEmpty() ? rootPath : config.expressionsRoot;
        require(selected.IsAbsolutePath() && selected.IsPrimPath() && selected.HasPrefix(rootPath),
                "VRM_EXPRESSION_BINDING_PATH", selected.GetString());
        const auto root = stage->GetPrimAtPath(selected);
        require(root && root.IsActive() && root.IsLoaded(), "VRM_EXPRESSION_BINDING_ROOT", selected.GetString());
        std::map<std::string,avatarVrm::MorphBinding> morphById;
        auto scopedPrim = [&](const pxr::SdfPath& path) {
            require(path.IsPrimPath() && path.HasPrefix(rootPath), "VRM_EXPRESSION_BINDING_TARGET", path.GetString());
            auto prim = stage->GetPrimAtPath(path);
            require(prim && prim.IsActive() && prim.IsLoaded(), "VRM_EXPRESSION_BINDING_TARGET", path.GetString());
            return prim;
        };
        auto materialInput = [&](const pxr::UsdPrim& prim, const char* name, uint32_t type) {
            const auto key = std::make_pair(prim.GetPath().GetString(),std::string(name));
            if (materialsById.count(key)) return;
            const auto attr = prim.GetAttribute(pxr::TfToken(name));
            require(!attr.HasAuthoredConnections(), "VRM_EXPRESSION_BINDING_CONNECTED", attr.GetPath().GetString());
            ArMaterialInput value{}; value.value_type = type;
            if (type == AR_VALUE_VEC3) {
                const auto rgb = read<pxr::GfVec3f>(attr, pxr::SdfValueTypeNames->Color3f, {}, true);
                for (int i = 0; i < 3; ++i) value.value[i] = rgb[i];
            } else value.value[0] = read<float>(attr, pxr::SdfValueTypeNames->Float, 0, true);
            for (double v : value.value)
                require(std::isfinite(v), "VRM_EXPRESSION_BINDING_FINITE", prim.GetPath().GetString());
            auto node = materialsById.emplace(key,value).first;
            node->second.material_id = node->first.first.c_str(); node->second.input_id = node->first.second.c_str();
        };
        for (const auto& prim : pxr::UsdPrimRange(root)) {
            if (!prim.HasAPI<pxr::UsdVrmExpressionAPI>()) continue;
            pxr::UsdVrmExpressionAPI api(prim);
            vrmRig::ExpressionDefinition d;
            d.name = read<pxr::TfToken>(api.GetVrmExpressionNameAttr(), pxr::SdfValueTypeNames->Token, {}, true).GetString();
            d.isBinary = read<bool>(api.GetVrmIsBinaryAttr(), pxr::SdfValueTypeNames->Bool);
            d.overrideBlink = overrideValue(api.GetVrmOverrideBlinkAttr());
            d.overrideLookAt = overrideValue(api.GetVrmOverrideLookAtAttr());
            d.overrideMouth = overrideValue(api.GetVrmOverrideMouthAttr());
            const auto paths = targets(api.GetVrmMorphTargetsRel());
            const auto weights = read<pxr::VtFloatArray>(api.GetVrmMorphTargetWeightsAttr(), pxr::SdfValueTypeNames->FloatArray);
            require(paths.size() == weights.size(), "VRM_EXPRESSION_BINDING_MORPH_ARRAYS", prim.GetPath().GetString());
            for (size_t i = 0; i < paths.size(); ++i) {
                require(scopedPrim(paths[i]).IsA<pxr::UsdSkelBlendShape>(), "VRM_EXPRESSION_BINDING_MORPH", paths[i].GetString());
                require(std::isfinite(weights[i]), "VRM_EXPRESSION_BINDING_FINITE", prim.GetPath().GetString());
                const auto id = paths[i].GetString();
                if (!morphById.count(id)) {
                    std::vector<avatarVrm::MorphBinding> found;
                    for (const auto& mesh : pxr::UsdPrimRange(stage->GetPrimAtPath(rootPath))) {
                        if (!mesh.IsA<pxr::UsdGeomMesh>() || !mesh.HasAPI<pxr::UsdSkelBindingAPI>()) continue;
                        pxr::UsdSkelBindingAPI binding(mesh);
                        const auto shapes = targets(binding.GetBlendShapeTargetsRel());
                        if (std::find(shapes.begin(),shapes.end(),paths[i]) == shapes.end()) continue;
                        const auto names = read<pxr::VtTokenArray>(binding.GetBlendShapesAttr(), pxr::SdfValueTypeNames->TokenArray, {}, true);
                        require(names.size() == shapes.size(), "VRM_EXPRESSION_BINDING_MESH_ARRAYS", mesh.GetPath().GetString());
                        std::vector<std::string> seen;
                        for (size_t j = 0; j < names.size(); ++j) {
                            require(!names[j].IsEmpty() && std::find(seen.begin(),seen.end(),names[j].GetString()) == seen.end(),
                                    "VRM_EXPRESSION_BINDING_MESH_NAMES", mesh.GetPath().GetString());
                            seen.push_back(names[j].GetString());
                            if (shapes[j] == paths[i]) found.push_back({id,mesh.GetPath().GetString(),names[j].GetString()});
                        }
                    }
                    require(found.size() == 1, "VRM_EXPRESSION_BINDING_MORPH_MAPPING", id);
                    morphById.emplace(id,found.front());
                }
                d.morphTargets.push_back({id,weights[i]});
            }
            const auto colors = targets(api.GetVrmMaterialColorTargetsRel());
            const auto types = read<pxr::VtTokenArray>(api.GetVrmMaterialColorTypesAttr(), pxr::SdfValueTypeNames->TokenArray);
            const auto values = read<pxr::VtVec4fArray>(api.GetVrmMaterialColorValuesAttr(), pxr::SdfValueTypeNames->Float4Array);
            auto indices = read<pxr::VtIntArray>(api.GetVrmMaterialColorTargetIndicesAttr(), pxr::SdfValueTypeNames->IntArray);
            if (!api.GetVrmMaterialColorTargetIndicesAttr().HasAuthoredValueOpinion())
                for (size_t i = 0; i < colors.size(); ++i) indices.push_back(int(i));
            require(indices.size() == types.size() && indices.size() == values.size() &&
                    (colors.empty() == indices.empty()), "VRM_EXPRESSION_BINDING_MATERIAL_ARRAYS", prim.GetPath().GetString());
            for (size_t i = 0; i < indices.size(); ++i) {
                require(indices[i] >= 0 && size_t(indices[i]) < colors.size(), "VRM_EXPRESSION_BINDING_MATERIAL_INDEX", prim.GetPath().GetString());
                const auto material = scopedPrim(colors[size_t(indices[i])]);
                const auto* slot = vrmRig::FindMaterialColorSlot(types[i].GetString());
                require(slot, "VRM_EXPRESSION_BINDING_SLOT", types[i].GetString());
                const auto schemas = material.GetAppliedSchemas();
                require(material.IsA<pxr::UsdShadeMaterial>() && std::find(schemas.begin(),schemas.end(),pxr::TfToken(slot->schema)) != schemas.end(),
                        "VRM_EXPRESSION_BINDING_MATERIAL_SCHEMA", material.GetPath().GetString());
                for (int k = 0; k < 4; ++k) require(std::isfinite(values[i][k]), "VRM_EXPRESSION_BINDING_FINITE", prim.GetPath().GetString());
                materialInput(material,slot->colorInput,AR_VALUE_VEC3);
                if (slot->alphaInput) materialInput(material,slot->alphaInput,AR_VALUE_SCALAR);
                d.materialColors.push_back({material.GetPath().GetString(),types[i].GetString(),values[i]});
            }
            require(rig.Add(std::move(d)), "VRM_EXPRESSION_BINDING_NAME", prim.GetPath().GetString());
        }
        require(!rig.IsEmpty(), "VRM_EXPRESSION_BINDING_MISSING", selected.GetString());
        for (const auto& entry : morphById) morphBindings.push_back(entry.second);
        for (const auto& entry : morphBindings) morphs.push_back({entry.mesh.c_str(),entry.target.c_str(),0});
        for (const auto& entry : materialsById) materials.push_back(entry.second);
        baseline = humanoid.Skeleton().Baseline();
        baseline.blend_shapes = morphs.data(); baseline.blend_shape_count = uint32_t(morphs.size());
        baseline.materials = materials.data(); baseline.material_count = uint32_t(materials.size());
    }
};
ExpressionBinding::ExpressionBinding(const pxr::UsdStagePtr& stage, ExpressionBindingConfig config)
    : impl_(std::make_shared<Impl>(stage,config)) {}
const HumanoidBinding& ExpressionBinding::Humanoid() const { return impl_->humanoid; }
const ArStateView& ExpressionBinding::Baseline() const { return impl_->baseline; }
const vrmRig::ExpressionRig& ExpressionBinding::Rig() const { return impl_->rig; }
void ExpressionBinding::ApplyTo(avatarVrm::ExpressionAdapterConfig& config) const {
    const auto& state = Baseline();
    require(config.layoutId == state.layout_id && config.layoutVersion == state.layout_version &&
            (!config.lookAt || config.headSkeleton == Humanoid().Skeleton().SkeletonId()),
            "VRM_EXPRESSION_BINDING_LAYOUT", config.layoutId);
    require(config.expressions.IsEmpty() && config.morphs.empty(), "VRM_EXPRESSION_BINDING_CONFIG", config.evaluatorId);
    config.expressions = impl_->rig; config.morphs = impl_->morphBindings;
}
avatarVrm::ExpressionAdapterConfig ExpressionBinding::AdapterConfig(
    std::string evaluatorId, std::vector<avatarVrm::ExpressionInputBinding> inputs, std::vector<std::string> after) const {
    avatarVrm::ExpressionAdapterConfig result;
    result.evaluatorId = std::move(evaluatorId); result.layoutId = Baseline().layout_id;
    result.layoutVersion = Baseline().layout_version; result.inputs = std::move(inputs); result.after = std::move(after);
    ApplyTo(result); return result;
}
} // namespace avatarVrmUsd
