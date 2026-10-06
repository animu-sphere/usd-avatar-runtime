#include "VrmCheck.h"
#include "vrmSchema/vrmExpressionAPI.h"
#include "vrmSchema/vrmHumanoidAPI.h"
#include "vrmSchema/vrmLookAtAPI.h"
#include "pxr/usd/usdGeom/metrics.h"
#include "pxr/usd/usdGeom/scope.h"
#include "pxr/usd/usdGeom/xform.h"
#include "pxr/usd/usdGeom/mesh.h"
#include "pxr/usd/usdSkel/skeleton.h"
#include "pxr/usd/usdSkel/animation.h"
#include "pxr/usd/usdSkel/blendShape.h"
#include "pxr/usd/usdSkel/bindingAPI.h"
#include "pxr/usd/usdShade/material.h"
#include <filesystem>
#include <iostream>
#include <sstream>

int avatarMotionCheckMain(int argc, char** argv);
namespace {
using avatarMotionCheck::verify;
pxr::UsdStageRefPtr avatar(bool bone) {
    auto s = pxr::UsdStage::CreateInMemory();
    pxr::UsdGeomSetStageUpAxis(s,pxr::UsdGeomTokens->y); pxr::UsdGeomSetStageMetersPerUnit(s,1);
    s->SetDefaultPrim(s->DefinePrim(pxr::SdfPath("/Avatar")));
    const pxr::VtTokenArray joints{pxr::TfToken("Pelvis"),pxr::TfToken("Pelvis/Head"),
        pxr::TfToken("Pelvis/Head/LeftEye"),pxr::TfToken("Pelvis/Head/RightEye")};
    auto sk = pxr::UsdSkelSkeleton::Define(s,pxr::SdfPath("/Avatar/Body"));
    verify(sk.CreateJointsAttr().Set(joints),"target joints");
    pxr::GfMatrix4d hip(1), head(1), eye(1);
    hip.SetTranslate(pxr::GfVec3d(0,1,0)); head.SetTranslate(pxr::GfVec3d(0,.5,0));
    eye.SetRotate(pxr::GfRotation(pxr::GfVec3d(0,0,1),12));
    verify(sk.CreateRestTransformsAttr().Set(pxr::VtMatrix4dArray{hip,head,eye,eye}),"target rest");
    auto human = pxr::UsdVrmHumanoidAPI::Apply(pxr::UsdGeomScope::Define(s,pxr::SdfPath("/Avatar/Human")).GetPrim());
    verify(human.CreateVrmSkeletonRel().SetTargets({sk.GetPath()}),"target skeleton");
    human.CreateVrmHumanBonesHipsAttr().Set(joints[0]); human.CreateVrmHumanBonesHeadAttr().Set(joints[1]);
    human.CreateVrmHumanBonesLeftEyeAttr().Set(joints[2]); human.CreateVrmHumanBonesRightEyeAttr().Set(joints[3]);
    auto shape = pxr::UsdSkelBlendShape::Define(s,pxr::SdfPath("/Avatar/Smile"));
    auto mesh = pxr::UsdGeomMesh::Define(s,pxr::SdfPath("/Avatar/Face"));
    auto binding = pxr::UsdSkelBindingAPI::Apply(mesh.GetPrim());
    binding.CreateBlendShapesAttr().Set(pxr::VtTokenArray{pxr::TfToken("opaque_smile")});
    binding.CreateBlendShapeTargetsRel().SetTargets({shape.GetPath()});
    auto material = pxr::UsdShadeMaterial::Define(s,pxr::SdfPath("/Avatar/Material"));
    material.GetPrim().ApplyAPI(pxr::TfToken("VrmMaterialAPI")); material.GetPrim().ApplyAPI(pxr::TfToken("VrmMToonAPI"));
    pxr::VtTokenArray types; pxr::VtVec4fArray values; pxr::VtIntArray indices;
    for (const auto& slot : vrmRig::GetMaterialColorSlots()) {
        material.GetPrim().CreateAttribute(pxr::TfToken(slot.colorInput),pxr::SdfValueTypeNames->Color3f,false).Set(pxr::GfVec3f(.2f,.3f,.4f));
        if (slot.alphaInput) material.GetPrim().CreateAttribute(pxr::TfToken(slot.alphaInput),pxr::SdfValueTypeNames->Float,false).Set(.8f);
        types.push_back(pxr::TfToken(slot.name)); values.push_back(pxr::GfVec4f(.9f,.7f,.5f,.6f)); indices.push_back(0);
    }
    for (const char* name : {"happy","blink","lookLeft","lookRight","lookUp","lookDown"}) {
        auto e = pxr::UsdVrmExpressionAPI::Apply(pxr::UsdGeomScope::Define(s,pxr::SdfPath(std::string("/Avatar/Expressions/")+name)).GetPrim());
        e.CreateVrmExpressionNameAttr().Set(pxr::TfToken(name));
        e.CreateVrmMorphTargetsRel().SetTargets({shape.GetPath()}); e.CreateVrmMorphTargetWeightsAttr().Set(pxr::VtFloatArray{.7f});
        if (std::string(name) == "happy") {
            e.CreateVrmOverrideBlinkAttr().Set(pxr::TfToken("blend"));
            e.CreateVrmMaterialColorTargetsRel().SetTargets({material.GetPath()});
            e.CreateVrmMaterialColorTypesAttr().Set(types); e.CreateVrmMaterialColorValuesAttr().Set(values);
            e.CreateVrmMaterialColorTargetIndicesAttr().Set(indices);
        }
    }
    auto gaze = pxr::UsdVrmLookAtAPI::Apply(pxr::UsdGeomScope::Define(s,pxr::SdfPath("/Avatar/Gaze")).GetPrim());
    gaze.CreateVrmTypeAttr().Set(pxr::TfToken(bone ? "bone" : "expression"));
    gaze.CreateVrmSkeletonRel().SetTargets({sk.GetPath()});
    gaze.CreateVrmLeftEyeAttr().Set(joints[2]); gaze.CreateVrmRightEyeAttr().Set(joints[3]);
    return s;
}
pxr::UsdStageRefPtr clip() {
    auto s = pxr::UsdStage::CreateInMemory();
    pxr::UsdGeomSetStageUpAxis(s,pxr::UsdGeomTokens->y); pxr::UsdGeomSetStageMetersPerUnit(s,1); s->SetTimeCodesPerSecond(60);
    const pxr::VtTokenArray joints{pxr::TfToken("hips"),pxr::TfToken("hips/head")};
    auto sk = pxr::UsdSkelSkeleton::Define(s,pxr::SdfPath("/Clip/Body")); sk.CreateJointsAttr().Set(joints);
    pxr::GfMatrix4d hip(1), head(1); hip.SetTranslate(pxr::GfVec3d(0,.8,0)); head.SetTranslate(pxr::GfVec3d(0,.5,0));
    sk.CreateRestTransformsAttr().Set(pxr::VtMatrix4dArray{hip,head});
    auto a = pxr::UsdSkelAnimation::Define(s,pxr::SdfPath("/Clip/Animation")); a.CreateJointsAttr().Set(joints);
    pxr::UsdSkelBindingAPI::Apply(sk.GetPrim()).CreateAnimationSourceRel().SetTargets({a.GetPath()});
    for (int time : {0,60}) {
        const auto rotation = pxr::GfQuatf(pxr::GfRotation(pxr::GfVec3d(0,1,0),time ? 55 : -25).GetQuat());
        a.CreateRotationsAttr().Set(pxr::VtQuatfArray{pxr::GfQuatf(1),rotation},time);
        a.CreateTranslationsAttr().Set(pxr::VtVec3fArray{pxr::GfVec3f(float(time)/60,.8f,0),pxr::GfVec3f(0,.5f,0)},time);
    }
    // A semantic expression changes between body keys, and explicit zero is authored.
    auto channel = s->DefinePrim(pxr::SdfPath("/Clip/Happy"));
    channel.CreateAttribute(pxr::TfToken("motion:channelName"),pxr::SdfValueTypeNames->String).Set(std::string("vrm:happy"));
    auto value = channel.CreateAttribute(pxr::TfToken("motion:channelValue"),pxr::SdfValueTypeNames->Float);
    value.Set(.4f,0); value.Set(.9f,30); value.Set(0.f,60);
    auto unknown = s->DefinePrim(pxr::SdfPath("/Clip/Other"));
    unknown.CreateAttribute(pxr::TfToken("motion:channelName"),pxr::SdfValueTypeNames->String).Set(std::string("custom:unmapped"));
    unknown.CreateAttribute(pxr::TfToken("motion:channelValue"),pxr::SdfValueTypeNames->Float).Set(.3f);
    return s;
}
std::pair<int,std::string> run(std::vector<std::string> args) {
    args.insert(args.begin(),"avatarMotionCheck");
    std::vector<char*> argv; for (auto& arg : args) argv.push_back(arg.data());
    std::ostringstream output; auto* old = std::cout.rdbuf(output.rdbuf());
    const int status = avatarMotionCheckMain(int(argv.size()),argv.data());
    std::cout.rdbuf(old); return {status,output.str()};
}
}
int main(int argc, char** argv) {
    try {
        verify(argc == 2,"fixture output directory required");
        const std::filesystem::path root(argv[1]); std::filesystem::create_directories(root);
        const auto source = (root/"motion.usda").string();
        verify(clip()->GetRootLayer()->Export(source),"export constructed motion");
        for (bool bone : {false,true}) {
            const auto asset = (root/(bone ? "bone.usda" : "expression.usda")).string();
            auto stage = avatar(bone); verify(stage->GetRootLayer()->Export(asset),"export constructed avatar");
            auto native = run({"--vrm",asset,source}); verify(native.first == 0,"native composition failed");
            verify(native.second.find("unmapped_channel=custom:unmapped") != std::string::npos,"unmapped channel not reported");
            verify(native.second.find("changed_morph_frames=0") == std::string::npos,"native morph never changed");
            verify(native.second.find("probe_weights=0 probe_gaze=0") != std::string::npos,"native run injected probes");
            auto projected = clip();
            verify(projected->RemovePrim(pxr::SdfPath("/Clip/Happy")),"remove common scalar");
            auto ownerInput = projected->DefinePrim(pxr::SdfPath("/Clip/Opaque"));
            ownerInput.CreateAttribute(pxr::TfToken("vrm:expressionName"),pxr::SdfValueTypeNames->Token).Set(pxr::TfToken("happy"));
            auto weight = ownerInput.CreateAttribute(pxr::TfToken("vrm:expressionWeight"),pxr::SdfValueTypeNames->Float);
            weight.Set(.4f,0); weight.Set(.9f,30); weight.Set(0.f,60);
            auto target = ownerInput.CreateAttribute(pxr::TfToken("vrm:lookAtTarget"),pxr::SdfValueTypeNames->Point3f);
            target.Set(pxr::GfVec3f(0),15); target.Set(pxr::GfVec3f(1,1.5f,3),45);
            const auto projectedPath = (root/"owner-input.usda").string();
            verify(projected->GetRootLayer()->Export(projectedPath),"export owner inputs");
            std::vector<std::string> options{"--vrm","--channel-input","/Clip/Opaque.vrm:expressionName",
                "/Clip/Opaque.vrm:expressionWeight","vrm:","--gaze-input","/Clip/Opaque.vrm:lookAtTarget"};
            auto arguments = options; arguments.insert(arguments.end(),{asset,projectedPath});
            auto selected = run(arguments); verify(selected.first == 0,"owner-selected composition failed");
            verify(selected.second.find("native_gazes=0") == std::string::npos,"owner gaze never reached input");
            verify(selected.second.find("changed_morph_frames=0") == std::string::npos,"owner scalar never changed morph");
            verify(selected.second.find("probe_weights=0 probe_gaze=0") != std::string::npos,"owner run injected probes");
            verify(run({"--vrm",asset,projectedPath}).second.find("native_gazes=0") != std::string::npos,
                   "native attribute was discovered implicitly");
            // Native points follow avatar placement; explicit probe points are world-space.
            pxr::GfMatrix4d placement(1);
            placement.SetRotate(pxr::GfRotation(pxr::GfVec3d(0,1,0),90));
            placement.SetTranslateOnly(pxr::GfVec3d(3,0,5));
            verify(pxr::UsdGeomXform::Define(stage,pxr::SdfPath("/Avatar")).AddTransformOp().Set(placement),"avatar placement");
            const auto placed = (root/(bone ? "placed-bone.usda" : "placed-expression.usda")).string();
            verify(stage->GetRootLayer()->Export(placed),"export placed avatar");
            arguments = options; arguments.insert(arguments.end(),{placed,projectedPath});
            verify(run(arguments).first == 0,"placed native composition failed");
            auto probe = run({"--vrm","--gaze-point","1","1.5","3","--weight","blink=.8",asset,source});
            verify(probe.first == 0,"gaze composition failed");
            verify(run({asset,source}).first == 0,"pose mode regression");
            verify(run({"--vrm","--weight","missing=.5",asset,source}).first != 0,"unknown expression accepted");
            auto still = clip();
            auto animation = pxr::UsdSkelAnimation(still->GetPrimAtPath(pxr::SdfPath("/Clip/Animation")));
            for (int time : {0,60}) {
                animation.GetRotationsAttr().Set(pxr::VtQuatfArray{pxr::GfQuatf(1),pxr::GfQuatf(1)},time);
                animation.GetTranslationsAttr().Set(pxr::VtVec3fArray{pxr::GfVec3f(0,.8f,0),pxr::GfVec3f(0,.5f,0)},time);
            }
            const auto stillPath = (root/"still.usda").string();
            verify(still->GetRootLayer()->Export(stillPath),"export static motion");
            verify(run({"--vrm","--gaze-point","1","1.5","3",asset,stillPath}).first != 0,
                   "test gaze masked unchanged motion pose");
            // The oracle must reject perturbed effects instead of only checking successful calls.
            avatarVrmUsd::HumanoidBindingConfig hc{pxr::SdfPath("/Avatar"),{},"test",1};
            avatarVrmUsd::ExpressionBinding expressions(stage,{hc,{}});
            avatarVrmUsd::LookAtBinding look(stage,{hc,{}});
            avatarMotionCheck::VrmCheck oracle(expressions,look,{},{});
            openstrata::motion::MotionPose inputPoint; inputPoint.lookAtTarget = pxr::GfVec3f(0);
            const auto origin = oracle.Select(inputPoint,false,expressions.Humanoid().Skeleton());
            verify((*origin.lookAtTarget-pxr::GfVec3f(3,0,5)).GetLength() < 1e-6,"origin gaze placement wrong");
            inputPoint.lookAtTarget = pxr::GfVec3f(1,1.5f,0);
            const auto rotated = oracle.Select(inputPoint,false,expressions.Humanoid().Skeleton());
            verify((*rotated.lookAtTarget-pxr::GfVec3f(3,1.5f,4)).GetLength() < 1e-6,"gaze rotation/translation wrong");
            oracle.probeGaze = pxr::GfVec3f(1,2,3);
            verify(oracle.Select(inputPoint,true,expressions.Humanoid().Skeleton()).lookAtTarget == oracle.probeGaze,
                   "world probe was transformed twice");
            auto state = expressions.Baseline();
            std::vector<ArBlendShape> morphs(state.blend_shapes,state.blend_shapes+state.blend_shape_count);
            state.blend_shapes = morphs.data(); morphs[0].weight = .1;
            bool rejected = false;
            try { oracle.Compare(state,{}); } catch (const std::runtime_error&) { rejected = true; }
            verify(rejected,"oracle accepted incorrect morph");
            morphs[0].weight = 0;
            std::vector<ArMaterialInput> materials(state.materials,state.materials+state.material_count);
            state.materials = materials.data(); materials[0].value[0] += .1;
            rejected = false;
            try { oracle.Compare(state,{}); } catch (const std::runtime_error&) { rejected = true; }
            verify(rejected,"oracle accepted incorrect material");
        }
        verify(run({"--vrm","--gaze-point","nan","0","1"}).first != 0,"nonfinite probe accepted");
        verify(run({"--weight","happy=.5"}).first != 0,"probe without VRM accepted");
        verify(run({"--vrm","--weight","happy=.5","--weight","happy=.7"}).first != 0,"duplicate probe accepted");
        std::cout << "Motion/VRM check composition passed\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
