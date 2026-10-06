#include "avatarMotionUsd/StageClip.h"
#include "avatarUsd/MotionUsdReadError.h"
#include "motionRetarget/PoseRetargeter.h"
#include "pxr/usd/usdGeom/metrics.h"
#include "pxr/usd/usdGeom/tokens.h"
#include "pxr/usd/usdGeom/xform.h"
#include "pxr/usd/usdSkel/skeleton.h"
#include "pxr/usd/usdSkel/animation.h"
#include "pxr/usd/usdSkel/bindingAPI.h"
#include "pxr/base/gf/rotation.h"
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <limits>

#define CHECK(x) do { if (!(x)) { std::cerr << "Line " << __LINE__ << ": " << #x << '\n'; std::exit(1); } } while (0)
namespace {
namespace motion = openstrata::motion;
const pxr::SdfPath path("/Clip/Body");
pxr::UsdStageRefPtr stage() {
    auto s = pxr::UsdStage::CreateInMemory();
    CHECK(pxr::UsdGeomSetStageUpAxis(s, pxr::UsdGeomTokens->y));
    CHECK(pxr::UsdGeomSetStageMetersPerUnit(s, 1));
    s->SetTimeCodesPerSecond(60);
    auto sk = pxr::UsdSkelSkeleton::Define(s,path);
    pxr::VtTokenArray joints{pxr::TfToken("hips"),pxr::TfToken("hips/head")};
    CHECK(sk.CreateJointsAttr().Set(joints));
    pxr::GfMatrix4d hip(1), head(1);
    hip.SetTranslate(pxr::GfVec3d(0,0.8,0));
    head.SetRotate(pxr::GfRotation(pxr::GfVec3d(0,1,0),20));
    head.SetTranslateOnly(pxr::GfVec3d(0,0.5,0));
    CHECK(sk.CreateRestTransformsAttr().Set(pxr::VtMatrix4dArray{hip,head}));
    auto a = pxr::UsdSkelAnimation::Define(s,pxr::SdfPath("/Clip/Animation"));
    CHECK(pxr::UsdSkelBindingAPI::Apply(sk.GetPrim()).CreateAnimationSourceRel().SetTargets({a.GetPath()}));
    CHECK(a.CreateJointsAttr().Set(joints));
    auto q = pxr::GfQuatf(head.ExtractRotationQuat());
    CHECK(a.CreateRotationsAttr().Set(pxr::VtQuatfArray{pxr::GfQuatf(1),q},0));
    CHECK(a.CreateRotationsAttr().Set(pxr::VtQuatfArray{pxr::GfQuatf(1),pxr::GfQuatf(1)},60));
    CHECK(a.CreateTranslationsAttr().Set(pxr::VtVec3fArray{pxr::GfVec3f(0,0.8f,0),pxr::GfVec3f(0,0.5f,0)},0));
    CHECK(a.CreateTranslationsAttr().Set(pxr::VtVec3fArray{pxr::GfVec3f(0,0.8f,1),pxr::GfVec3f(0,0.5f,0)},60));
    return s;
}
void reject(const pxr::UsdStagePtr& s, const char* code, pxr::SdfPath p = path) {
    bool threw = false;
    try { avatarMotionUsd::StageClip clip(s,p); }
    catch (const std::invalid_argument& e) { threw = true; CHECK(std::string(e.what()).find(code) == 0); }
    CHECK(threw);
}
}
int main() {
    auto s = stage();
    avatarMotionUsd::StageClip clip(s,path); auto copy = clip;
    CHECK(clip.Read().clip.samples.size() == 2);
    CHECK(clip.Read().clip.samples[1].timestamp == 1);
    CHECK(clip.Read().timeCodesPerSecond == 60);
    CHECK(clip.Read().animationPath == "/Clip/Animation");
    CHECK(clip.Read().skeleton.restTransformsAuthored);
    CHECK(!clip.Read().metadata.contractVersion);
    CHECK(std::abs(clip.SourceRest().localTranslations[size_t(motion::HumanJoint::Hips)][1] - 0.8f) < 1e-6);
    CHECK(clip.SourceRest().parents[size_t(motion::HumanJoint::Head)] == size_t(motion::HumanJoint::Hips));
    CHECK(std::abs(clip.SourceRest().localRotations[size_t(motion::HumanJoint::Head)].GetImaginary()[1]) > 0.1);
    // Carry source height into the retarget configuration: the clip's rest
    // root position must yield target height, not target + source height.
    motion::SkeletonJoint targetHip; targetHip.token = "Pelvis";
    targetHip.restTranslation = pxr::GfVec3f(0,1.6f,0);
    motion::SkeletonDescriptor target({targetHip}); motion::RetargetMap map;
    CHECK(map.SetJointIndex(motion::HumanJoint::Hips,0,1));
    const auto resolved = motion::PoseRetargeter(target,map,clip.SourceRest()).Retarget(clip.Read().clip.samples[0]);
    CHECK(std::abs(resolved.translations[0][1] - 1.6f) < 1e-6);
    CHECK(s->RemovePrim(pxr::SdfPath("/Clip"))); s.Reset();
    CHECK(copy.Read().clip.samples[1].root.worldPosition[2] == 1);
    CHECK(copy.Read().skeleton.path == path.GetString());
    // Explicit format-owner selection travels through the strict owner reader.
    s = stage();
    auto native = s->DefinePrim(pxr::SdfPath("/Native"));
    CHECK(native.CreateAttribute(pxr::TfToken("name"),pxr::SdfValueTypeNames->Token).Set(pxr::TfToken("happy")));
    auto weight = native.CreateAttribute(pxr::TfToken("weight"),pxr::SdfValueTypeNames->Float);
    CHECK(weight.Set(1.5f,30));
    auto gaze = native.CreateAttribute(pxr::TfToken("target"),pxr::SdfValueTypeNames->Point3f);
    CHECK(gaze.Set(pxr::GfVec3f(0),15));
    motion::MotionStageReadOptions inputs;
    inputs.channels.push_back({"/Native.name","/Native.weight","vrm:"});
    inputs.lookAtTargetAttributePath = "/Native.target";
    motion::MotionStageRead expected; motion::SkeletonReadDiagnostic selectedDiagnostic;
    CHECK(motion::ReadCanonicalMotionStage(s,path,inputs,&expected,&selectedDiagnostic));
    avatarMotionUsd::StageClip selected(s,path,inputs); auto selectedCopy = selected;
    CHECK(selected.Read().clip == expected.clip);
    CHECK(selected.Read().clip.samples.size() == 4);
    CHECK(selected.Read().clip.samples[1].lookAtTarget == pxr::GfVec3f(0));
    CHECK(*selected.Read().clip.samples[2].channels.Find("vrm:happy") == 1.5f);
    CHECK(!selected.Read().clip.samples[2].lookAtTarget);
    CHECK(avatarMotionUsd::StageClip(s,path).Read().clip.samples.size() == 2);
    CHECK(weight.Set(std::numeric_limits<float>::infinity(),30));
    CHECK(!motion::ReadCanonicalMotionStage(s,path,inputs,&expected,&selectedDiagnostic));
    bool selectedRefused = false;
    try { avatarMotionUsd::StageClip bad(s,path,inputs); }
    catch (const avatarUsd::MotionUsdReadError& e) {
        selectedRefused = true;
        CHECK(e.Diagnostic().code == selectedDiagnostic.code && e.Diagnostic().detail == selectedDiagnostic.detail);
    }
    CHECK(selectedRefused);
    CHECK(s->RemovePrim(pxr::SdfPath("/Native"))); s.Reset(); inputs = {};
    CHECK(*selectedCopy.Read().clip.samples[2].channels.Find("vrm:happy") == 1.5f);
    s = stage(); CHECK(pxr::UsdGeomSetStageMetersPerUnit(s,0.01));
    motion::MotionStageRead ownerRead; motion::SkeletonReadDiagnostic diagnostic;
    CHECK(!motion::ReadCanonicalMotionStage(s,path,&ownerRead,&diagnostic));
    bool ownerRefused = false;
    try { avatarMotionUsd::StageClip rejected(s,path); }
    catch (const avatarUsd::MotionUsdReadError& e) {
        const auto retained = e; s.Reset(); ownerRefused = true;
        CHECK(retained.Diagnostic().code == diagnostic.code && retained.Diagnostic().subject == diagnostic.subject &&
              retained.Diagnostic().detail == diagnostic.detail);
        CHECK(std::string(retained.Owner()) == "motionUsd" && !retained.OwnerVersion().empty());
    }
    CHECK(ownerRefused);
    reject({},"MOTION_USD_STAGE");
    reject(stage(),"MOTION_USD_SKELETON_PATH",pxr::SdfPath());
    s = stage(); CHECK(pxr::UsdGeomSetStageMetersPerUnit(s,0.01)); reject(s,"MOTION_USD_UNITS");
    s = stage(); CHECK(pxr::UsdGeomSetStageUpAxis(s,pxr::UsdGeomTokens->z)); reject(s,"MOTION_USD_UP_AXIS");
    s = stage(); s->SetTimeCodesPerSecond(0); reject(s,"MOTION_USD_RATE");
    s = stage(); pxr::UsdSkelSkeleton(s->GetPrimAtPath(path)).GetRestTransformsAttr().Block();
    reject(s,"MOTION_USD_REST_COUNT");
    s = stage(); auto x = pxr::UsdGeomXform::Define(s,pxr::SdfPath("/Clip"));
    CHECK(x.AddTranslateOp().Set(pxr::GfVec3d(1,0,0))); reject(s,"MOTION_USD_PLACEMENT");
    s = stage(); CHECK(s->RemovePrim(pxr::SdfPath("/Clip/Animation"))); reject(s,"MOTION_USD_READ");
    s = stage(); auto sk = pxr::UsdSkelSkeleton(s->GetPrimAtPath(path));
    CHECK(sk.GetJointsAttr().Set(pxr::VtTokenArray{pxr::TfToken("hips"),pxr::TfToken("hips/hips")}));
    reject(s,"MOTION_USD_SOURCE_REST");
    std::cout << "Motion USD clip binding contracts passed\n";
}
