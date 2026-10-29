#include "avatarVrmSession/Session.h"
#include "vrmSchema/vrmHumanoidAPI.h"
#include "vrmSchema/vrmSpringBoneAPI.h"
#include "pxr/usd/usdGeom/metrics.h"
#include "pxr/usd/usdGeom/xform.h"
#include "pxr/usd/usdSkel/skeleton.h"
#include "pxr/usd/usdSkel/animation.h"
#include "pxr/usd/usdSkel/bindingAPI.h"
#include "pxr/base/gf/rotation.h"
#include <filesystem>
#include <iostream>
#include <stdexcept>

int avatarVrmSessionCheck(int argc, char** argv);
namespace {
using namespace pxr;
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
UsdStageRefPtr avatar(bool roots) {
    auto s = UsdStage::CreateInMemory();
    UsdGeomSetStageUpAxis(s, UsdGeomTokens->y); UsdGeomSetStageMetersPerUnit(s, 1);
    auto root = UsdGeomXform::Define(s, SdfPath("/Avatar")); s->SetDefaultPrim(root.GetPrim());
    root.GetPrim().SetCustomDataByKey(TfToken("vrm:schemaContractVersion"), VtValue(2));
    root.AddTranslateOp().Set(GfVec3d(3,4,5)); root.AddRotateYOp().Set(30.f);
    const VtTokenArray joints{TfToken("Root"), TfToken("Root/Head"), TfToken("Root/Head/Hair"), TfToken("Root/Head/Hair/Tip")};
    auto sk = UsdSkelSkeleton::Define(s, SdfPath("/Avatar/Body")); sk.CreateJointsAttr().Set(joints);
    GfMatrix4d hip(1), head(1), hair(1), tip(1);
    hip.SetTranslate(GfVec3d(0,.8,0)); head.SetTranslate(GfVec3d(0,.5,0));
    hair.SetTranslate(GfVec3d(0,.1,0)); tip.SetTranslate(GfVec3d(.1,.2,0));
    sk.CreateRestTransformsAttr().Set(VtMatrix4dArray{hip,head,hair,tip});
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
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
