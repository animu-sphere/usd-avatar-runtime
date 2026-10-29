#include <avatarUsd/SkeletonPoseOutput.h>
#include <pxr/usd/usd/editContext.h>
#include <pxr/usd/usdGeom/metrics.h>
#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/usd/usdGeom/primvarsAPI.h>
#include <pxr/usd/usdGeom/xformCache.h>
#include <pxr/usd/usdSkel/animation.h>
#include <pxr/usd/usdSkel/bindingAPI.h>
#include <pxr/usd/usdSkel/blendShape.h>
#include <pxr/usd/usdSkel/skeleton.h>
#include <pxr/usd/usdSkel/utils.h>
#include <pxr/usd/sdf/changeBlock.h>
#include <pxr/usd/sdf/relationshipSpec.h>
#include <pxr/base/gf/rotation.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>

namespace avatarUsd {
using namespace pxr;
namespace {
void require(bool ok, const char* message) {
    if (!ok) throw std::invalid_argument(message);
}
GfMatrix4d matrix(const ArTransform& t) {
    GfMatrix4d s(1), r(1), p(1);
    double norm = 0;
    for (auto x : t.translation) require(std::isfinite(x), "USD_OUTPUT_TRANSFORM: nonfinite translation");
    for (auto x : t.scale) require(std::isfinite(x) && x > 0, "USD_OUTPUT_TRANSFORM: invalid scale");
    for (auto x : t.rotation) {
        require(std::isfinite(x), "USD_OUTPUT_TRANSFORM: nonfinite quaternion"); norm += x*x;
    }
    require(std::abs(norm - 1) <= 1e-6, "USD_OUTPUT_TRANSFORM: nonunit quaternion");
    s.SetScale(GfVec3d(t.scale[0], t.scale[1], t.scale[2]));
    r.SetRotate(GfQuatd(t.rotation[3], GfVec3d(t.rotation[0], t.rotation[1], t.rotation[2])));
    p.SetTranslate(GfVec3d(t.translation[0], t.translation[1], t.translation[2]));
    return s*r*p;
}
}
SkeletonPoseOutput::SkeletonPoseOutput(UsdStageRefPtr stage, SkeletonBinding binding,
                                     SdfPath blendShapeMesh, SdfPath uvMesh, std::vector<SdfPath> materialInputs)
    : stage_(std::move(stage)), binding_(std::move(binding)),
      layer_(SdfLayer::CreateAnonymous("avatar-runtime-pose.usda")), mesh_(std::move(blendShapeMesh)),
      uvMesh_(std::move(uvMesh)), materialInputs_(std::move(materialInputs)) {
    require(bool(stage_) && bool(stage_->GetSessionLayer()), "USD_OUTPUT_STAGE: missing stage/session layer");
    std::set<SdfPath> uniqueInputs;
    for (const auto& path : materialInputs_) {
        const auto attr = stage_->GetAttributeAtPath(path);
        require(path.IsPropertyPath() && uniqueInputs.insert(path).second && attr &&
            stage_->GetPrimAtPath(path.GetPrimPath()).GetTypeName() == TfToken("Material") &&
            path.GetName().rfind("inputs:", 0) == 0 && !attr.HasAuthoredConnections(),
            "USD_OUTPUT_MATERIAL: invalid/duplicate material input");
        const auto type = attr.GetTypeName();
        require(type == SdfValueTypeNames->Float || type == SdfValueTypeNames->Color3f ||
            type == SdfValueTypeNames->Color4f || type == SdfValueTypeNames->Float3 ||
            type == SdfValueTypeNames->Float4, "USD_OUTPUT_MATERIAL: unsupported input type");
        materialTypes_.push_back(type.GetAsToken());
    }
    if (!mesh_.IsEmpty()) {
        const auto mesh = UsdGeomMesh::Get(stage_, mesh_);
        const UsdSkelBindingAPI api(mesh);
        require(mesh && api.GetInheritedSkeleton().GetPath() == SdfPath(binding_.SkeletonId()) &&
            api.GetBlendShapesAttr().Get(&shapes_) && !shapes_.empty() &&
            api.GetBlendShapeTargetsRel().GetTargets(&targets_) && shapes_.size() == targets_.size(),
            "USD_OUTPUT_MORPHS: invalid mesh/skeleton/blend-shape binding");
        std::set<TfToken> names;
        std::set<SdfPath> targets;
        for (size_t i = 0; i < shapes_.size(); ++i)
            require(!shapes_[i].IsEmpty() && names.insert(shapes_[i]).second &&
                targets.insert(targets_[i]).second && bool(UsdSkelBlendShape::Get(stage_, targets_[i])),
                "USD_OUTPUT_MORPHS: invalid/duplicate blend-shape identity");
    }
    if (!uvMesh_.IsEmpty()) {
        const auto mesh = UsdGeomMesh::Get(stage_, uvMesh_);
        const auto st = UsdGeomPrimvarsAPI(mesh).GetPrimvar(TfToken("st"));
        VtVec3fArray points; VtVec2fArray uvs;
        require(mesh && st && st.GetTypeName() == SdfValueTypeNames->TexCoord2fArray &&
            st.GetInterpolation() == UsdGeomTokens->vertex && !st.IsIndexed() &&
            st.GetElementSize() == 1 && mesh.GetPointsAttr().Get(&points) &&
            st.Get(&uvs) && !uvs.empty() && uvs.size() == points.size() &&
            mesh.GetFaceVertexIndicesAttr().Get(&uvFaces_) &&
            mesh.GetFaceVertexCountsAttr().Get(&uvFaceCounts_),
            "USD_OUTPUT_UV: requires unindexed vertex st matching mesh points");
        for (const auto& uv : uvs) for (int a = 0; a < 2; ++a)
            require(std::isfinite(uv[a]), "USD_OUTPUT_UV: nonfinite base st");
        uvCount_ = uvs.size();
    }
    units_ = UsdGeomGetStageMetersPerUnit(stage_);
    static std::atomic<uint64_t> next{0};
    do {
        animation_ = SdfPath("/__AvatarRuntimePose" + std::to_string(++next));
    } while (stage_->GetPrimAtPath(animation_));
}
SkeletonPoseOutput::~SkeletonPoseOutput() { Close(); }

void SkeletonPoseOutput::Apply(const ArRuntimeApi& api, ArSnapshot snapshot) {
    require(api.get_snapshot != nullptr, "USD_OUTPUT_SNAPSHOT: missing accessor");
    ArStateView state{AR_HEADER(ArStateView)};
    require(api.get_snapshot(snapshot, &state) == AR_OK, "USD_OUTPUT_SNAPSHOT: invalid snapshot");
    Apply(state);
}

void SkeletonPoseOutput::Apply(const ArStateView& state) {
    require(bool(stage_), "USD_OUTPUT_CLOSED");
    require(stage_->GetSessionLayer()->PermissionToEdit() && layer_->PermissionToEdit(),
        "USD_OUTPUT_LAYER: layer is not editable");
    const auto& baseline = binding_.Baseline();
    require(state.struct_size >= sizeof(ArStateView) && state.abi_version == AR_ABI_VERSION &&
        state.layout_id && baseline.layout_id == std::string(state.layout_id) &&
        state.layout_version == baseline.layout_version, "USD_OUTPUT_LAYOUT: incompatible layout");
    require(state.instance && state.generation && state.frame_id &&
        (!instance_ || instance_ == state.instance), "USD_OUTPUT_IDENTITY: invalid instance/frame");
    require(state.generation > generation_ ||
        (state.generation == generation_ && state.frame_id >= frame_), "USD_OUTPUT_STALE: older output");
    require(state.joint_count && state.joints, "USD_OUTPUT_JOINTS: missing joints");
    std::vector<VtValue> materials;
    require(!state.material_count || state.materials, "USD_OUTPUT_MATERIAL: missing material array");
    for (size_t n = 0; n < materialInputs_.size(); ++n) {
        const auto& path = materialInputs_[n];
        const auto attr = stage_->GetAttributeAtPath(path);
        require(attr && attr.GetTypeName().GetAsToken() == materialTypes_[n] && !attr.HasAuthoredConnections(),
            "USD_OUTPUT_MATERIAL: changed material input; rebind required");
        require(!stage_->GetSessionLayer()->GetAttributeAtPath(path),
            "USD_OUTPUT_MATERIAL: session-root input would mask live output");
        const ArMaterialInput* found = nullptr;
        for (uint32_t i = 0; i < state.material_count; ++i) {
            const auto& m = state.materials[i];
            if (m.material_id && m.input_id && path.GetPrimPath().GetString() == m.material_id && path.GetName() == m.input_id) {
                require(!found, "USD_OUTPUT_MATERIAL: duplicate input identity"); found = &m;
            }
        }
        const auto type = attr.GetTypeName();
        const uint32_t count = type == SdfValueTypeNames->Float ? 1 :
            (type == SdfValueTypeNames->Color3f || type == SdfValueTypeNames->Float3) ? 3 : 4;
        require(found && found->value_type == count, "USD_OUTPUT_MATERIAL: missing/mistyped input");
        for (uint32_t a = 0; a < 4; ++a)
            require(std::isfinite(found->value[a]) && std::abs(found->value[a]) <= std::numeric_limits<float>::max() &&
                (a < count || found->value[a] == 0), "USD_OUTPUT_MATERIAL: invalid numeric value");
        const auto* v = found->value;
        if (count == 1) materials.emplace_back(float(v[0]));
        else if (count == 3) materials.emplace_back(GfVec3f(float(v[0]),float(v[1]),float(v[2])));
        else materials.emplace_back(GfVec4f(float(v[0]),float(v[1]),float(v[2]),float(v[3])));
    }
    const auto skeleton = UsdSkelSkeleton::Get(stage_, SdfPath(binding_.SkeletonId()));
    VtTokenArray tokens;
    require(skeleton && skeleton.GetJointsAttr().Get(&tokens) && tokens.size() == baseline.joint_count,
        "USD_OUTPUT_TOPOLOGY: changed skeleton");
    for (size_t i = 0; i < tokens.size(); ++i)
        require(tokens[i].GetString() == binding_.JointIds()[i], "USD_OUTPUT_TOPOLOGY: changed joints");
    const double units = UsdGeomGetStageMetersPerUnit(stage_);
    require(std::isfinite(units) && units > 0 && units == units_, "USD_OUTPUT_UNITS: invalid/changed units");
    const auto stronger = stage_->GetSessionLayer()->GetRelationshipAtPath(
        skeleton.GetPath().AppendProperty(TfToken("skel:animationSource")));
    require(!stronger || !stronger->HasInfo(TfToken("targetPaths")),
        "USD_OUTPUT_LAYER: session-root animation binding would mask live output");
    auto current = UsdGeomXformCache().GetLocalToWorldTransform(skeleton.GetPrim());
    for (int c = 0; c < 3; ++c) current[3][c] *= units;
    const auto placement = matrix(binding_.RootPlacement());
    for (int r = 0; r < 4; ++r) for (int c = 0; c < 4; ++c)
        require(std::abs(current[r][c] - placement[r][c]) <= 1e-6,
            "USD_OUTPUT_PLACEMENT: rebind after placement edits");

    std::map<std::pair<std::string, std::string>, uint32_t> indices;
    for (uint32_t i = 0; i < state.joint_count; ++i) {
        const auto& j = state.joints[i];
        require(j.skeleton_id && j.joint_id &&
            indices.emplace(std::make_pair(j.skeleton_id, j.joint_id), i).second,
            "USD_OUTPUT_JOINTS: duplicate/missing identity");
    }
    VtMatrix4dArray local(tokens.size());
    const auto inverse = placement.GetInverse();
    for (size_t i = 0; i < tokens.size(); ++i) {
        const auto found = indices.find({binding_.SkeletonId(), tokens[i].GetString()});
        require(found != indices.end(), "USD_OUTPUT_JOINTS: missing bound joint");
        const auto& joint = state.joints[found->second];
        const auto parent = baseline.joints[i].parent_index;
        const int64_t expectedParent = parent < 0 ? -1 : int64_t(indices.at(
            {binding_.SkeletonId(), tokens[parent].GetString()}));
        require(joint.parent_index == expectedParent, "USD_OUTPUT_TOPOLOGY: runtime parent mismatch");
        local[i] = matrix(joint.local);
        if (parent < 0) local[i] *= inverse;
        for (int c = 0; c < 3; ++c) local[i][3][c] /= units;
    }
    VtVec3fArray translations; VtQuatfArray rotations; VtVec3hArray scales;
    require(UsdSkelDecomposeTransforms(local, &translations, &rotations, &scales),
        "USD_OUTPUT_TRANSFORM: USD decomposition refused");
    for (size_t i = 0; i < tokens.size(); ++i) for (int c = 0; c < 3; ++c)
        require(std::isfinite(translations[i][c]) && std::isfinite(float(scales[i][c])) &&
            float(scales[i][c]) > 0, "USD_OUTPUT_TRANSFORM: USD narrowing overflow/underflow");

    VtFloatArray weights;
    if (!mesh_.IsEmpty()) {
        const UsdSkelBindingAPI api(UsdGeomMesh::Get(stage_, mesh_));
        VtTokenArray names; SdfPathVector targets;
        require(api && api.GetInheritedSkeleton().GetPath() == skeleton.GetPath() &&
            api.GetBlendShapesAttr().Get(&names) && names == shapes_ &&
            api.GetBlendShapeTargetsRel().GetTargets(&targets) && targets == targets_,
            "USD_OUTPUT_MORPHS: changed mesh binding");
        for (const auto& target : targets_)
            require(bool(UsdSkelBlendShape::Get(stage_, target)), "USD_OUTPUT_MORPHS: missing blend shape");
        require(!state.blend_shape_count || state.blend_shapes, "USD_OUTPUT_MORPHS: missing runtime array");
        std::map<std::pair<std::string, std::string>, double> values;
        for (uint32_t i = 0; i < state.blend_shape_count; ++i) {
            const auto& shape = state.blend_shapes[i];
            require(shape.mesh_id && shape.target_id &&
                values.emplace(std::make_pair(shape.mesh_id, shape.target_id), shape.weight).second,
                "USD_OUTPUT_MORPHS: duplicate/missing runtime identity");
        }
        for (const auto& name : shapes_) {
            const auto found = values.find({mesh_.GetString(), name.GetString()});
            require(found != values.end() && std::isfinite(found->second) &&
                std::abs(found->second) <= std::numeric_limits<float>::max(),
                "USD_OUTPUT_MORPHS: missing/nonfinite/overflow weight");
            weights.push_back(float(found->second));
        }
    }

    VtVec2fArray uvs;
    if (!uvMesh_.IsEmpty()) {
        const auto mesh = UsdGeomMesh::Get(stage_, uvMesh_);
        const auto st = UsdGeomPrimvarsAPI(mesh).GetPrimvar(TfToken("st"));
        VtVec3fArray points; VtIntArray faces, counts;
        require(mesh && st && st.GetTypeName() == SdfValueTypeNames->TexCoord2fArray &&
            st.GetInterpolation() == UsdGeomTokens->vertex && !st.IsIndexed() && st.GetElementSize() == 1 &&
            mesh.GetPointsAttr().Get(&points) && points.size() == uvCount_ &&
            mesh.GetFaceVertexIndicesAttr().Get(&faces) && faces == uvFaces_ &&
            mesh.GetFaceVertexCountsAttr().Get(&counts) && counts == uvFaceCounts_,
            "USD_OUTPUT_UV: changed mesh/primvar binding");
        require(!stage_->GetSessionLayer()->GetAttributeAtPath(st.GetAttr().GetPath()) &&
            !stage_->GetSessionLayer()->GetAttributeAtPath(uvMesh_.AppendProperty(TfToken("primvars:st:indices"))),
            "USD_OUTPUT_UV: session-root st would mask live output");
        require(!state.vertex_uv_count || state.vertex_uvs, "USD_OUTPUT_UV: missing runtime array");
        uvs.resize(uvCount_); std::vector<bool> found(uvCount_, false);
        for (uint32_t i = 0; i < state.vertex_uv_count; ++i) {
            const auto& uv = state.vertex_uvs[i];
            require(uv.mesh_id != nullptr, "USD_OUTPUT_UV: missing mesh identity");
            if (uvMesh_.GetString() != uv.mesh_id) continue;
            require(uv.vertex_index < uvCount_ && !found[uv.vertex_index],
                "USD_OUTPUT_UV: duplicate/invalid vertex");
            found[uv.vertex_index] = true;
            for (int a = 0; a < 2; ++a) {
                require(std::isfinite(uv.value[a]) && std::abs(uv.value[a]) <= std::numeric_limits<float>::max(),
                    "USD_OUTPUT_UV: nonfinite/overflow UV");
                uvs[uv.vertex_index][a] = float(uv.value[a]);
            }
        }
        require(std::all_of(found.begin(), found.end(), [](bool x) { return x; }),
            "USD_OUTPUT_UV: incomplete vertex stream");
    }

    // Prepare the entire publication away from the displayed stage. Invalid
    // input never changes the live layer or its previously displayed values.
    auto scratch = UsdStage::CreateInMemory();
    auto animation = UsdSkelAnimation::Define(scratch, animation_);
    require(animation.CreateJointsAttr().Set(tokens) &&
        animation.CreateTranslationsAttr().Set(translations) &&
        animation.CreateRotationsAttr().Set(rotations) && animation.CreateScalesAttr().Set(scales),
        "USD_OUTPUT_AUTHOR: animation authoring failed");
    if (!mesh_.IsEmpty())
        require(animation.CreateBlendShapesAttr().Set(shapes_) &&
            animation.CreateBlendShapeWeightsAttr().Set(weights), "USD_OUTPUT_AUTHOR: blend-shape authoring failed");
    auto prim = scratch->OverridePrim(skeleton.GetPath());
    require(UsdSkelBindingAPI::Apply(prim).CreateAnimationSourceRel().SetTargets({animation_}),
        "USD_OUTPUT_AUTHOR: animation binding failed");
    if (!uvMesh_.IsEmpty())
        require(UsdGeomPrimvarsAPI(scratch->OverridePrim(uvMesh_)).CreatePrimvar(TfToken("st"),
            SdfValueTypeNames->TexCoord2fArray, UsdGeomTokens->vertex).Set(uvs),
            "USD_OUTPUT_AUTHOR: UV authoring failed");
    for (size_t n = 0; n < materialInputs_.size(); ++n) {
        const auto& path = materialInputs_[n];
        require(scratch->OverridePrim(path.GetPrimPath()).CreateAttribute(path.GetNameToken(),
            stage_->GetAttributeAtPath(path).GetTypeName(), false).Set(materials[n]),
            "USD_OUTPUT_AUTHOR: material authoring failed");
    }
    auto paths = stage_->GetSessionLayer()->GetSubLayerPaths();
    std::vector<std::string> ordered(paths.begin(), paths.end());
    const auto id = layer_->GetIdentifier();
    require(ordered.empty() || std::count(ordered.begin(), ordered.end(), id) <= 1,
        "USD_OUTPUT_LAYER: duplicate layer attachment");
    const bool attached = std::find(ordered.begin(), ordered.end(), id) != ordered.end();
    {
        SdfChangeBlock block;
        layer_->TransferContent(scratch->GetRootLayer());
        if (!attached) stage_->GetSessionLayer()->InsertSubLayerPath(id, 0);
    }
    instance_ = state.instance; generation_ = state.generation; frame_ = state.frame_id;
}

void SkeletonPoseOutput::Close() {
    if (!stage_) return;
    auto paths = stage_->GetSessionLayer()->GetSubLayerPaths();
    std::vector<std::string> ordered(paths.begin(), paths.end());
    for (size_t i = ordered.size(); i; --i)
        if (ordered[i - 1] == layer_->GetIdentifier())
            stage_->GetSessionLayer()->RemoveSubLayerPath(int(i - 1));
    stage_.Reset();
}
} // namespace avatarUsd
