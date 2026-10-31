#include "avatarVrmSession/Session.h"
#include "vrmSchema/vrmExpressionAPI.h"
#include "vrmSchema/vrmHumanoidAPI.h"
#include "vrmSchema/vrmLookAtAPI.h"
#include "vrmSchema/vrmSpringBoneAPI.h"
#include "vrmRig/MaterialColorSlots.h"
#include "pxr/usd/usdGeom/mesh.h"
#include "pxr/usd/usdGeom/metrics.h"
#include "pxr/usd/usdGeom/scope.h"
#include "pxr/usd/usdGeom/xform.h"
#include "pxr/usd/usdSkel/skeleton.h"
#include "pxr/usd/usdSkel/animation.h"
#include "pxr/usd/usdSkel/bindingAPI.h"
#include "pxr/usd/usdSkel/blendShape.h"
#include "pxr/usd/usdShade/material.h"
#include "pxr/usd/sdf/types.h"
#include "pxr/base/gf/rotation.h"
#include <filesystem>
#include <iostream>
#include <stdexcept>

int avatarVrmSessionCheck(int argc, char** argv);
namespace {
using namespace pxr;
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
enum class Gaze { none, bone, expression };
// One blend shape, one material with every canonical slot, and the look expressions.
void authorFace(const UsdStageRefPtr& s, const SdfPath& skeleton, Gaze gaze, bool expressions) {
    if (expressions) {
        auto shape = UsdSkelBlendShape::Define(s,SdfPath("/Avatar/Shapes/Smile"));
        auto binding = UsdSkelBindingAPI::Apply(UsdGeomMesh::Define(s,SdfPath("/Avatar/Face")).GetPrim());
        binding.CreateBlendShapesAttr().Set(VtTokenArray{TfToken("smile")});
        binding.CreateBlendShapeTargetsRel().SetTargets({shape.GetPath()});
        auto material = UsdShadeMaterial::Define(s,SdfPath("/Avatar/Material"));
        material.GetPrim().ApplyAPI(TfToken("VrmMaterialAPI")); material.GetPrim().ApplyAPI(TfToken("VrmMToonAPI"));
        VtTokenArray types; VtVec4fArray values; VtIntArray indices;
        for (const auto& slot : vrmRig::GetMaterialColorSlots()) {
            material.GetPrim().CreateAttribute(TfToken(slot.colorInput),SdfValueTypeNames->Color3f,false).Set(GfVec3f(.2f,.3f,.4f));
            if (slot.alphaInput) material.GetPrim().CreateAttribute(TfToken(slot.alphaInput),SdfValueTypeNames->Float,false).Set(.8f);
            types.push_back(TfToken(slot.name)); values.push_back(GfVec4f(.9f,.7f,.5f,.6f)); indices.push_back(0);
        }
        for (const char* name : {"happy","blink","lookLeft","lookRight","lookUp","lookDown"}) {
            auto api = UsdVrmExpressionAPI::Apply(UsdGeomScope::Define(s,SdfPath(std::string("/Avatar/Expressions/")+name)).GetPrim());
            api.CreateVrmExpressionNameAttr().Set(TfToken(name));
            api.CreateVrmMorphTargetsRel().SetTargets({shape.GetPath()});
            api.CreateVrmMorphTargetWeightsAttr().Set(VtFloatArray{.7f});
            if (std::string(name) == "happy") {
                api.CreateVrmOverrideBlinkAttr().Set(TfToken("blend"));
                api.CreateVrmMaterialColorTargetsRel().SetTargets({material.GetPath()});
                api.CreateVrmMaterialColorTypesAttr().Set(types); api.CreateVrmMaterialColorValuesAttr().Set(values);
                api.CreateVrmMaterialColorTargetIndicesAttr().Set(indices);
            }
        }
    }
    if (gaze == Gaze::none) return;
    auto look = UsdVrmLookAtAPI::Apply(UsdGeomScope::Define(s,SdfPath("/Avatar/Gaze")).GetPrim());
    look.CreateVrmSkeletonRel().SetTargets({skeleton});
    look.CreateVrmTypeAttr().Set(TfToken(gaze == Gaze::bone ? "bone" : "expression"));
    if (gaze == Gaze::bone) {
        look.CreateVrmLeftEyeAttr().Set(TfToken("Root/Head/EyeL"));
        look.CreateVrmRightEyeAttr().Set(TfToken("Root/Head/EyeR"));
    }
}
UsdStageRefPtr avatar(bool roots, Gaze gaze = Gaze::none, bool expressions = false) {
    auto s = UsdStage::CreateInMemory();
    UsdGeomSetStageUpAxis(s, UsdGeomTokens->y); UsdGeomSetStageMetersPerUnit(s, 1);
    auto root = UsdGeomXform::Define(s, SdfPath("/Avatar")); s->SetDefaultPrim(root.GetPrim());
    root.GetPrim().SetCustomDataByKey(TfToken("vrm:schemaContractVersion"), VtValue(2));
    root.AddTranslateOp().Set(GfVec3d(3,4,5)); root.AddRotateYOp().Set(30.f);
    const VtTokenArray joints{TfToken("Root"), TfToken("Root/Head"), TfToken("Root/Head/Hair"), TfToken("Root/Head/Hair/Tip"),
        TfToken("Root/Head/EyeL"), TfToken("Root/Head/EyeR")};
    auto sk = UsdSkelSkeleton::Define(s, SdfPath("/Avatar/Body")); sk.CreateJointsAttr().Set(joints);
    GfMatrix4d hip(1), head(1), hair(1), tip(1), eyeL(1), eyeR(1);
    eyeL.SetTranslate(GfVec3d(.03,.05,.08)); eyeR.SetTranslate(GfVec3d(-.03,.05,.08));
    hip.SetTranslate(GfVec3d(0,.8,0)); head.SetTranslate(GfVec3d(0,.5,0));
    hair.SetTranslate(GfVec3d(0,.1,0)); tip.SetTranslate(GfVec3d(.1,.2,0));
    sk.CreateRestTransformsAttr().Set(VtMatrix4dArray{hip,head,hair,tip,eyeL,eyeR});
    authorFace(s, sk.GetPath(), gaze, expressions);
    auto human = UsdVrmHumanoidAPI::Apply(s->DefinePrim(SdfPath("/Avatar/Human")));
    human.CreateVrmSkeletonRel().SetTargets({sk.GetPath()});
    human.CreateVrmHumanBonesHipsAttr().Set(joints[0]); human.CreateVrmHumanBonesHeadAttr().Set(joints[1]);
    auto spring = UsdVrmSpringBoneAPI::Apply(s->DefinePrim(SdfPath("/Avatar/Hair")));
    const VtTokenArray springJoints = roots ? VtTokenArray{joints[2]} : VtTokenArray{joints[2],joints[3]};
    (roots ? spring.CreateVrmRootJointsAttr() : spring.CreateVrmJointsAttr()).Set(springJoints);
    const auto n = springJoints.size();
    spring.CreateVrmStiffnessAttr().Set(VtFloatArray(n,.6f));
    spring.CreateVrmDragForceAttr().Set(VtFloatArray(n,.3f));
    spring.CreateVrmGravityPowerAttr().Set(VtFloatArray(n,.4f));
    spring.CreateVrmGravityDirAttr().Set(VtVec3fArray(n,GfVec3f(0,-1,0)));
    spring.CreateVrmHitRadiusAttr().Set(VtFloatArray(n,.02f));
    return s;
}
UsdStageRefPtr clip() {
    auto s = UsdStage::CreateInMemory();
    UsdGeomSetStageUpAxis(s, UsdGeomTokens->y); UsdGeomSetStageMetersPerUnit(s, 1); s->SetTimeCodesPerSecond(60);
    const VtTokenArray joints{TfToken("hips"), TfToken("hips/head")};
    auto sk = UsdSkelSkeleton::Define(s,SdfPath("/Clip/Body")); sk.CreateJointsAttr().Set(joints);
    GfMatrix4d hip(1), head(1); hip.SetTranslate(GfVec3d(0,.8,0)); head.SetTranslate(GfVec3d(0,.5,0));
    sk.CreateRestTransformsAttr().Set(VtMatrix4dArray{hip,head});
    auto a = UsdSkelAnimation::Define(s,SdfPath("/Clip/Animation")); a.CreateJointsAttr().Set(joints);
    UsdSkelBindingAPI::Apply(sk.GetPrim()).CreateAnimationSourceRel().SetTargets({a.GetPath()});
    for (int time : {0,60}) {
        auto q = GfQuatf(GfRotation(GfVec3d(0,0,1), time ? 35 : -15).GetQuat());
        a.CreateRotationsAttr().Set(VtQuatfArray{GfQuatf(1),q}, time);
        a.CreateTranslationsAttr().Set(VtVec3fArray{GfVec3f(0,.8f,float(time)*.003f), GfVec3f(0,.5f,0)}, time);
    }
    return s;
}
}
int main(int argc, char** argv) {
    try {
        require(argc == 2, "fixture directory required");
        std::filesystem::create_directories(argv[1]);
        const auto motionPath = (std::filesystem::path(argv[1]) / "clip.usda").string();
        auto source = clip(); require(source->GetRootLayer()->Export(motionPath), "export clip");
        for (bool roots : {false,true}) {
            const auto avatarPath = (std::filesystem::path(argv[1]) / (roots ? "roots.usda" : "chain.usda")).string();
            auto s = avatar(roots); require(s->GetRootLayer()->Export(avatarPath), "export avatar");
            for (bool springs : {true,false}) {
                std::vector<std::string> args{"check",avatarPath,motionPath,"0.016666666666666666","60"};
                if (!springs) args.push_back("--no-springs");
                std::vector<char*> pointers; for (auto& arg : args) pointers.push_back(arg.data());
                require(avatarVrmSessionCheck(int(pointers.size()),pointers.data()) == 0, "generated session check");
            }
            // Host owns values independently of input stages and handles bounded catch-up.
            const auto read = avatarMotionUsd::ReadStageClip(source,SdfPath("/Clip/Body"));
            avatarVrmSession::Session session(s, read, {{SdfPath("/Avatar"),{},"owned",1}, {0,1./60,2},true});
            s.Reset(); source.Reset();
            require(session.Advance(0) == AR_OK, "seed after stage destruction"); session.Play();
            require(session.Advance(.1) == AR_OK && session.Stats().tick == 2 && session.Stats().pendingSeconds > .06, "bounded catch-up");
            session.Pause(); const auto held = session.Snapshot();
            require(session.Advance(10) == AR_OK && session.Snapshot() == held, "pause backlog");
            session.Play(); require(session.Advance(0) == AR_OK && session.Stats().tick == 4, "resume backlog");
            session.Close(); require(session.Advance(0) == AR_INVALID_STATE, "closed advance");
            source = clip();
        }
        // Owner parity for the selected face, with and without springs, for both gaze types.
        for (Gaze gaze : {Gaze::bone, Gaze::expression}) {
            const auto avatarPath = (std::filesystem::path(argv[1]) / (gaze == Gaze::bone ? "bone.usda" : "expression.usda")).string();
            require(avatar(true,gaze,true)->GetRootLayer()->Export(avatarPath), "export face avatar");
            for (bool springs : {true,false}) {
                std::vector<std::string> args{"check",avatarPath,motionPath,"0.016666666666666666","60","--face"};
                if (!springs) args.push_back("--no-springs");
                std::vector<char*> pointers; for (auto& arg : args) pointers.push_back(arg.data());
                require(avatarVrmSessionCheck(int(pointers.size()),pointers.data()) == 0, "generated face check");
            }
        }
        const auto read = avatarMotionUsd::ReadStageClip(source,SdfPath("/Clip/Body"));
        const avatarVrmUsd::HumanoidBindingConfig identity{SdfPath("/Avatar"),{},"face",1};
        const auto refused = [&](const UsdStageRefPtr& s, avatarVrmSession::SessionConfig c) {
            c.avatar = identity; c.clock = {0,1./60,2};
            try { avatarVrmSession::Session unused(s,read,c); return false; }
            catch (const std::invalid_argument&) { return true; }
        };
        {
            avatarVrmSession::SessionConfig c; c.expressions = true;
            require(refused(avatar(true), c), "selected expressions on an avatar with none");
            c = {}; c.lookAt = true; c.gaze = {"host","actor","gaze:point"};
            require(refused(avatar(true), c), "selected LookAt on an avatar with none");
            require(refused(avatar(true,Gaze::expression,true), c), "expression LookAt without expressions");
            c = {}; c.expressionInputs = {{{"host","actor","expr:happy"},"happy"}};
            require(refused(avatar(true,Gaze::none,true), c), "inputs without expressions");
        }
        {
            // Absent and stale observations contribute nothing; bone LookAt works alone.
            avatarVrmSession::SessionConfig c; c.avatar = identity; c.clock = {0,1./60,2}; c.secondaryMotion = false;
            c.lookAt = true; c.gaze = {"host","actor","gaze:point"};
            avatarVrmSession::Session session(avatar(true,Gaze::bone,false),read,c);
            ArDiagnosticSink quiet{}; quiet.emit = [](void*,const ArDiagnostic*) {};
            const auto eye = [&] {
                ArStateView v{AR_HEADER(ArStateView)}; require(session.Api().get_snapshot(session.Snapshot(),&v) == AR_OK, "snapshot");
                for (uint32_t i = 0; i < v.joint_count; ++i)
                    if (std::string(v.joints[i].joint_id) == "Root/Head/EyeL") return GfQuatd(v.joints[i].local.rotation[3],
                        GfVec3d(v.joints[i].local.rotation[0],v.joints[i].local.rotation[1],v.joints[i].local.rotation[2]));
                throw std::runtime_error("eye joint");
            };
            ArGazeInput g{"host","actor","gaze:point",AR_GAZE_POINT,AR_GAZE_RUNTIME_WORLD,AR_OBSERVATION_VALID};
            g.value[0] = 3; g.value[1] = 6; g.value[2] = 5; g.clock_scale = 1;
            ArInputFrame frame{AR_HEADER(ArInputFrame)}; frame.gazes = &g; frame.gaze_count = 1; frame.input_revision = 7;
            require(session.Advance(0,&quiet) == AR_OK, "absent seed");
            const auto absent = eye(); session.Play();
            require(session.Advance(1./60,frame,&quiet) == AR_OK, "valid gaze");
            require(eye() != absent, "valid gaze moves the eye");
            g.validity = AR_OBSERVATION_STALE;
            require(session.Advance(1./60,frame,&quiet) == AR_OK && eye() == absent, "stale gaze contributes nothing");
            session.Close();
        }
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
