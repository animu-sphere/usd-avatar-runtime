#include <avatarUsd/SkeletonPoseOutput.h>
#include <pxr/usd/usdGeom/metrics.h>
#include <pxr/usd/usd/editContext.h>
#include <pxr/usd/usdGeom/xform.h>
#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/usd/usdGeom/primvarsAPI.h>
#include <pxr/usd/usdSkel/animation.h>
#include <pxr/usd/usdSkel/bindingAPI.h>
#include <pxr/usd/usdSkel/blendShape.h>
#include <pxr/usd/usdSkel/cache.h>
#include <pxr/usd/usdSkel/skeletonQuery.h>
#include <pxr/base/gf/rotation.h>
#include <cstdlib>
#include <iostream>
#include <limits>

#define CHECK(x) do { if (!(x)) { std::cerr << __LINE__ << ": " << #x << '\n'; std::exit(1); } } while(0)
using namespace pxr;
int main() {
    auto stage = UsdStage::CreateInMemory();
    UsdGeomSetStageUpAxis(stage, UsdGeomTokens->y);
    UsdGeomSetStageMetersPerUnit(stage, .01);
    auto root = UsdGeomXform::Define(stage, SdfPath("/Avatar"));
    root.AddTranslateOp().Set(GfVec3d(300, 0, 500));
    root.AddRotateYOp().Set(90.f);
    auto skeleton = UsdSkelSkeleton::Define(stage, SdfPath("/Avatar/Rig"));
    const VtTokenArray names{TfToken("root"), TfToken("root/child"), TfToken("other")};
    skeleton.CreateJointsAttr().Set(names);
    VtMatrix4dArray rest(3, GfMatrix4d(1));
    rest[0].SetTranslate(GfVec3d(0, 100, 0));
    rest[1].SetScale(GfVec3d(.5, 1, 2)); rest[1].SetTranslateOnly(GfVec3d(0, 50, 0));
    rest[2].SetTranslate(GfVec3d(20, 0, 0));
    skeleton.CreateRestTransformsAttr().Set(rest);
    auto previous = UsdSkelAnimation::Define(stage, SdfPath("/Previous"));
    previous.CreateJointsAttr().Set(names);
    UsdSkelBindingAPI::Apply(skeleton.GetPrim()).CreateAnimationSourceRel().SetTargets({previous.GetPath()});
    auto mesh = UsdGeomMesh::Define(stage, SdfPath("/Avatar/Mesh"));
    mesh.CreatePointsAttr().Set(VtVec3fArray{{0,0,0},{1,0,0},{0,1,0}});
    mesh.CreateFaceVertexCountsAttr().Set(VtIntArray{3});
    mesh.CreateFaceVertexIndicesAttr().Set(VtIntArray{0,1,2});
    auto st = UsdGeomPrimvarsAPI(mesh).CreatePrimvar(TfToken("st"), SdfValueTypeNames->TexCoord2fArray,
        UsdGeomTokens->vertex);
    const VtVec2fArray baseUvs{{0,0},{1,0},{0,1}}; st.Set(baseUvs);
    auto meshBinding = UsdSkelBindingAPI::Apply(mesh.GetPrim());
    meshBinding.CreateSkeletonRel().SetTargets({skeleton.GetPath()});
    const VtTokenArray shapes{TfToken("wide"), TfToken("tall")};
    const SdfPathVector targets{SdfPath("/Avatar/Wide"), SdfPath("/Avatar/Tall")};
    for (const auto& target : targets) UsdSkelBlendShape::Define(stage, target);
    meshBinding.CreateBlendShapesAttr().Set(shapes);
    meshBinding.CreateBlendShapeTargetsRel().SetTargets(targets);
    auto material = stage->DefinePrim(SdfPath("/Avatar/Surface"), TfToken("Material"));
    const auto color = material.CreateAttribute(TfToken("inputs:color"), SdfValueTypeNames->Color4f);
    const auto specular = material.CreateAttribute(TfToken("inputs:specular"), SdfValueTypeNames->Color3f);
    const auto power = material.CreateAttribute(TfToken("inputs:power"), SdfValueTypeNames->Float);
    color.Set(GfVec4f(1)); specular.Set(GfVec3f(.5)); power.Set(8.f);
    std::string original; stage->GetRootLayer()->ExportToString(&original);
    auto foreign = SdfLayer::CreateAnonymous("other-session-data.usda");
    stage->GetSessionLayer()->SetSubLayerPaths({foreign->GetIdentifier()});
    stage->GetSessionLayer()->SetSubLayerOffset(SdfLayerOffset(12, 2), 0);
    avatarUsd::SkeletonBinding binding(stage, {SdfPath("/Avatar"), skeleton.GetPath(), "layout", 1, {}});
    ArRuntimeApi api{}; CHECK(arGetApi(AR_ABI_VERSION, sizeof(api), &api) == AR_OK);
    ArRuntime runtime = 0; CHECK(api.create_runtime(&runtime) == AR_OK);
    ArInstanceDesc desc{AR_HEADER(ArInstanceDesc)};
    desc.generation = 1; desc.layout_id = "layout"; desc.layout_version = 1; desc.initial_state = binding.Baseline();
    // Runtime order differs from the mesh; unrelated channels are permitted.
    const ArBlendShape morphs[]{{"/Other", "wide", 99}, {"/Avatar/Mesh", "tall", .75},
                               {"/Avatar/Mesh", "wide", .25}};
    desc.initial_state.blend_shapes = morphs; desc.initial_state.blend_shape_count = 3;
    ArVertexUv uvs[]{{"/Avatar/Mesh",2,{-.25,1.5}}, {"/Avatar/Mesh",0,{.5,.25}}, {"/Avatar/Mesh",1,{1,0}}};
    desc.initial_state.vertex_uvs = uvs; desc.initial_state.vertex_uv_count = 3;
    ArMaterialInput materials[]{{"/Avatar/Surface","inputs:power",1,1,{4,0,0,0}},
        {"/Avatar/Surface","inputs:color",4,1,{.25,.5,.75,.125}},
        {"/Avatar/Surface","inputs:specular",3,1,{.125,.25,.5,0}}};
    desc.initial_state.materials = materials; desc.initial_state.material_count = 3;
    ArInstance instance = 0; CHECK(api.create_instance(runtime, &desc, nullptr, &instance) == AR_OK);
    ArInputFrame input{AR_HEADER(ArInputFrame)}; input.generation = 1; input.frame_id = 1;
    ArSnapshot snapshot = 0; CHECK(api.evaluate_frame(runtime, instance, &input, nullptr, &snapshot) == AR_OK);
    CHECK(api.destroy_runtime(runtime) == AR_OK); // Publication reads an independently retained CPU value.
    ArStateView view{AR_HEADER(ArStateView)}; CHECK(api.get_snapshot(snapshot, &view) == AR_OK);
    {
        avatarUsd::SkeletonPoseOutput poseOnly(stage, binding);
        poseOnly.Apply(api, snapshot);
        CHECK(!UsdSkelAnimation::Get(stage, poseOnly.AnimationPath()).GetBlendShapesAttr().HasAuthoredValue());
    }
    avatarUsd::SkeletonPoseOutput output(stage, binding, mesh.GetPath(), mesh.GetPath(),
        {color.GetPath(),specular.GetPath(),power.GetPath()});
    output.Apply(api, snapshot);
    GfVec4f rgba; GfVec3f rgb; float scalar;
    CHECK(color.Get(&rgba) && rgba == GfVec4f(.25,.5,.75,.125));
    CHECK(specular.Get(&rgb) && rgb == GfVec3f(.125,.25,.5));
    CHECK(power.Get(&scalar) && scalar == 4);
    VtVec2fArray actualUvs; CHECK(st.Get(&actualUvs) && actualUvs == VtVec2fArray({{.5,.25},{1,0},{-.25,1.5}}));
    CHECK(st.GetAttr().GetNumTimeSamples() == 0);
    CHECK(stage->GetEditTarget().GetLayer() == stage->GetRootLayer());
    auto query = UsdSkelCache().GetSkelQuery(skeleton);
    CHECK(query && query.GetAnimQuery());
    auto animation = UsdSkelAnimation::Get(stage, output.AnimationPath());
    VtTokenArray actualNames; VtFloatArray weights;
    CHECK(animation.GetBlendShapesAttr().Get(&actualNames) && actualNames == shapes);
    CHECK(query.GetAnimQuery().ComputeBlendShapeWeights(&weights, UsdTimeCode(123)) &&
        weights == VtFloatArray({.25f, .75f}));
    CHECK(animation.GetBlendShapeWeightsAttr().GetNumTimeSamples() == 0);
    auto unclamped = view;
    auto extrapolated = std::vector<ArBlendShape>(morphs, morphs + 3);
    extrapolated[1].weight = 1.25; extrapolated[2].weight = -.25;
    unclamped.blend_shapes = extrapolated.data(); output.Apply(unclamped);
    CHECK(animation.GetBlendShapeWeightsAttr().Get(&weights) && weights == VtFloatArray({-.25f, 1.25f}));
    output.Apply(view);
    VtMatrix4dArray actual;
    CHECK(query.ComputeJointLocalTransforms(&actual, UsdTimeCode(123)) && actual.size() == rest.size());
    for (size_t i = 0; i < rest.size(); ++i) for (int r = 0; r < 4; ++r) for (int c = 0; c < 4; ++c)
        CHECK(std::abs(actual[i][r][c] - rest[i][r][c]) < 2e-5);
    std::string good; output.Layer()->ExportToString(&good);
    auto refuse = [&](ArStateView bad) {
        bool refused = false;
        try { output.Apply(bad); } catch (const std::exception&) { refused = true; }
        CHECK(refused);
        std::string after; output.Layer()->ExportToString(&after); CHECK(after == good);
    };
    auto bad = view; bad.layout_version = 2; refuse(bad);
    bad = view; bad.materials = nullptr; refuse(bad);
    bad = view; bad.material_count = 2; refuse(bad);
    auto changedMaterials = std::vector<ArMaterialInput>(materials,materials+3);
    bad = view; bad.materials = changedMaterials.data();
    changedMaterials[0].value[0] = 1e100; refuse(bad);
    changedMaterials[0] = materials[1]; refuse(bad);
    changedMaterials[0] = materials[0]; changedMaterials[1].value_type = AR_VALUE_VEC3; refuse(bad);
    {
        UsdEditContext edit(stage,stage->GetSessionLayer());
        color.Set(GfVec4f(0)); refuse(view);
        material.RemoveProperty(TfToken("inputs:color"));
    }
    bad = view; bad.vertex_uvs = nullptr; refuse(bad);
    bad = view; bad.vertex_uv_count = 2; refuse(bad);
    auto changedUvs = std::vector<ArVertexUv>(uvs, uvs + 3);
    bad = view; bad.vertex_uvs = changedUvs.data();
    changedUvs[0].value[0] = std::numeric_limits<double>::infinity(); refuse(bad);
    changedUvs[0].value[0] = 1e100; refuse(bad);
    changedUvs[0] = uvs[0]; changedUvs[0].vertex_index = 0; refuse(bad);
    changedUvs[0].vertex_index = 3; refuse(bad);
    changedUvs[0] = uvs[0]; changedUvs[0].mesh_id = "/Other"; refuse(bad);
    mesh.GetFaceVertexIndicesAttr().Set(VtIntArray{1,0,2}); refuse(view);
    mesh.GetFaceVertexIndicesAttr().Set(VtIntArray{0,1,2});
    // Bind-time layout is rechecked even while live values override the base.
    {
        UsdEditContext edit(stage, stage->GetSessionLayer());
        st.SetInterpolation(UsdGeomTokens->faceVarying); refuse(view);
        st.GetAttr().ClearMetadata(TfToken("interpolation"));
        st.Set(baseUvs); refuse(view);
        mesh.GetPrim().RemoveProperty(TfToken("primvars:st"));
    }
    bad = view; bad.blend_shapes = nullptr; refuse(bad);
    bad = view; bad.blend_shape_count = 1; refuse(bad);
    std::vector<ArBlendShape> changedMorphs(morphs, morphs + 3);
    bad = view; bad.blend_shapes = changedMorphs.data();
    changedMorphs[2].weight = std::numeric_limits<double>::infinity(); refuse(bad);
    changedMorphs[2].weight = 1e100; refuse(bad);
    changedMorphs[2] = changedMorphs[1]; refuse(bad);
    changedMorphs[2] = morphs[2]; changedMorphs[2].target_id = nullptr; refuse(bad);
    auto changedShapes = shapes; changedShapes[0] = TfToken("edited");
    meshBinding.GetBlendShapesAttr().Set(changedShapes); refuse(view);
    meshBinding.GetBlendShapesAttr().Set(shapes);
    meshBinding.GetBlendShapeTargetsRel().SetTargets({targets[1], targets[0]}); refuse(view);
    meshBinding.GetBlendShapeTargetsRel().SetTargets(targets);
    meshBinding.GetSkeletonRel().SetTargets({SdfPath("/Missing")}); refuse(view);
    meshBinding.GetSkeletonRel().SetTargets({skeleton.GetPath()});
    bad = view; ++bad.instance; refuse(bad);
    bad = view; bad.frame_id = 0; refuse(bad);
    std::vector<ArJoint> joints(view.joints, view.joints + view.joint_count);
    bad = view; bad.joints = joints.data(); joints[1].parent_index = -1; refuse(bad);
    joints[1] = view.joints[1]; joints[0].local.rotation[3] = 0; refuse(bad);
    joints[0] = view.joints[0]; joints[2].local.translation[0] = std::numeric_limits<double>::infinity(); refuse(bad);
    joints[2] = view.joints[2]; joints[2].local.scale[0] = 1e100; refuse(bad);
    bad = view; bad.generation = 2; output.Apply(bad);
    output.Layer()->ExportToString(&good); refuse(view);
    auto changed = names; changed[1] = TfToken("root/edited"); skeleton.GetJointsAttr().Set(changed); refuse(bad);
    skeleton.GetJointsAttr().Set(names);
    root.GetOrderedXformOps(nullptr).front().Set(GfVec3d(301, 0, 500)); refuse(bad);
    root.GetOrderedXformOps(nullptr).front().Set(GfVec3d(300, 0, 500));
    UsdGeomSetStageMetersPerUnit(stage, 1); refuse(bad);
    UsdGeomSetStageMetersPerUnit(stage, .01);
    {
        UsdEditContext edit(stage, stage->GetSessionLayer());
        UsdSkelBindingAPI(skeleton).GetAnimationSourceRel().SetTargets({previous.GetPath()});
        refuse(bad);
        skeleton.GetPrim().RemoveProperty(TfToken("skel:animationSource"));
    }
    output.Close(); output.Close();
    CHECK(color.Get(&rgba) && rgba == GfVec4f(1));
    CHECK(specular.Get(&rgb) && rgb == GfVec3f(.5));
    CHECK(power.Get(&scalar) && scalar == 8);
    CHECK(st.Get(&actualUvs) && actualUvs == baseUvs);
    CHECK(stage->GetSessionLayer()->GetSubLayerPaths().size() == 1);
    CHECK(stage->GetSessionLayer()->GetSubLayerPaths()[0] == foreign->GetIdentifier());
    CHECK(stage->GetSessionLayer()->GetSubLayerOffset(0) == SdfLayerOffset(12, 2));
    SdfPathVector restoredTargets; CHECK(UsdSkelBindingAPI(skeleton).GetAnimationSourceRel().GetTargets(&restoredTargets));
    CHECK(restoredTargets == SdfPathVector{previous.GetPath()});
    std::string after; stage->GetRootLayer()->ExportToString(&after); CHECK(after == original);
    bool closed = false; try { output.Apply(view); } catch (const std::invalid_argument&) { closed = true; } CHECK(closed);
    CHECK(api.get_snapshot(snapshot, &bad) == AR_OK && api.release_snapshot(snapshot) == AR_OK);
}
