#include "avatarUsd/SkeletonBinding.h"
#include "avatarRuntime/api.h"
#include "pxr/usd/usdGeom/xform.h"
#include "pxr/usd/usdGeom/metrics.h"
#include "pxr/usd/usdGeom/tokens.h"
#include "pxr/usd/usdSkel/skeleton.h"
#include "pxr/base/gf/rotation.h"
#ifdef AR_TEST_MOTION
#include "avatarMotion/ClipPoseAdapter.h"
#endif
#ifdef AR_TEST_VRM
#include "avatarVrm/ExpressionAdapter.h"
#endif
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>

#define CHECK(x) do { if (!(x)) { std::cerr << "Line " << __LINE__ << ": " << #x << '\n'; std::exit(1); } } while (0)
namespace {
namespace motion = openstrata::motion;
bool closeEnough(double a, double b) { return std::abs(a-b) < 1e-6; }
pxr::GfMatrix4d trs(pxr::GfVec3d t, double yaw = 0, pxr::GfVec3d scale = pxr::GfVec3d(1)) {
    pxr::GfMatrix4d s(1), r(1); s.SetScale(scale);
    r.SetRotate(pxr::GfRotation(pxr::GfVec3d(0,1,0),yaw));
    auto m = s * r; m.SetTranslateOnly(t); return m;
}
avatarUsd::SkeletonBindingConfig config() {
    return {pxr::SdfPath("/Avatar"), pxr::SdfPath("/Avatar/Rig"), "usd.layout", 2,
        {{motion::HumanJoint::Hips,"Pelvis"}, {motion::HumanJoint::Head,"Pelvis/Skull"}}};
}
pxr::UsdStageRefPtr stage() {
    auto s = pxr::UsdStage::CreateInMemory();
    CHECK(pxr::UsdGeomSetStageUpAxis(s,pxr::UsdGeomTokens->y));
    CHECK(pxr::UsdGeomSetStageMetersPerUnit(s,0.01));
    auto avatar = pxr::UsdGeomXform::Define(s,pxr::SdfPath("/Avatar"));
    CHECK(avatar.AddTransformOp().Set(trs(pxr::GfVec3d(300,0,500),90)));
    auto skeleton = pxr::UsdSkelSkeleton::Define(s,pxr::SdfPath("/Avatar/Rig"));
    CHECK(skeleton.AddTranslateOp().Set(pxr::GfVec3d(100,0,0)));
    CHECK(skeleton.CreateJointsAttr().Set(pxr::VtTokenArray{
        pxr::TfToken("Pelvis"),pxr::TfToken("Pelvis/Skull"),pxr::TfToken("Pelvis/Skull/Extra"),pxr::TfToken("Other")}));
    CHECK(skeleton.CreateRestTransformsAttr().Set(pxr::VtMatrix4dArray{
        trs(pxr::GfVec3d(0,100,0)), trs(pxr::GfVec3d(0,50,0),20),
        trs(pxr::GfVec3d(0,20,0),15,pxr::GfVec3d(0.5,1,2)), trs(pxr::GfVec3d(20,0,0))}));
    return s;
}
void baseline() {
    auto s = stage();
    avatarUsd::SkeletonBinding binding(s,config());
    auto copy = binding;
    CHECK(binding.SkeletonId() == "/Avatar/Rig");
    CHECK(binding.Skeleton().GetSize() == 4 && binding.JointIds()[1] == "Pelvis/Skull");
    CHECK(binding.HumanoidMap().GetJointIndex(motion::HumanJoint::Head) == 1);
    CHECK(binding.HumanoidMap().GetJointIndex(motion::HumanJoint::LeftEye) == -1);
    CHECK(closeEnough(binding.Skeleton().GetJoints()[0].restTranslation[1],1));
    const auto& v = binding.Baseline();
    CHECK(v.layout_version == 2 && std::string(v.layout_id) == "usd.layout");
    CHECK(v.joint_count == 4 && v.joints[0].parent_index == -1 && v.joints[1].parent_index == 0 &&
          v.joints[2].parent_index == 1 && v.joints[3].parent_index == -1);
    // USD row-vector composition: nested local +X becomes world -Z.
    CHECK(closeEnough(v.joints[0].local.translation[0],3) && closeEnough(v.joints[0].local.translation[1],1) &&
          closeEnough(v.joints[0].local.translation[2],4));
    CHECK(closeEnough(v.joints[3].local.translation[2],3.8));
    CHECK(closeEnough(v.joints[1].local.translation[1],0.5));
    CHECK(closeEnough(v.joints[2].local.scale[0],0.5) && closeEnough(v.joints[2].local.scale[2],2));
    // Mutation/destruction must not change owned strings/arrays.
    CHECK(pxr::UsdSkelSkeleton(s->GetPrimAtPath(config().skeleton)).GetJointsAttr().Set(pxr::VtTokenArray{}));
    s.Reset();
    CHECK(copy.Baseline().joints == v.joints);
    CHECK(std::string(copy.Baseline().joints[2].joint_id) == "Pelvis/Skull/Extra");
    ArRuntimeApi api{}; CHECK(arGetApi(AR_ABI_VERSION,sizeof(api),&api) == AR_OK);
    ArRuntime runtime = 0; CHECK(api.create_runtime(&runtime) == AR_OK);
    ArInstanceDesc d{AR_HEADER(ArInstanceDesc)}; d.generation = 1;
    d.layout_id = v.layout_id; d.layout_version = v.layout_version; d.initial_state = v;
    ArInstance instance = 0; CHECK(api.create_instance(runtime,&d,nullptr,&instance) == AR_OK);
    ArInputFrame input{AR_HEADER(ArInputFrame)}; input.frame_id = 1; input.generation = 1;
    ArSnapshot snapshot = 0; CHECK(api.evaluate_frame(runtime,instance,&input,nullptr,&snapshot) == AR_OK);
    CHECK(api.destroy_runtime(runtime) == AR_OK);
    ArStateView retained{AR_HEADER(ArStateView)}; CHECK(api.get_snapshot(snapshot,&retained) == AR_OK);
    CHECK(closeEnough(retained.joints[0].local.translation[2],4));
    CHECK(api.release_snapshot(snapshot) == AR_OK);
}
void invalid() {
    auto reject = [](const pxr::UsdStagePtr& s, avatarUsd::SkeletonBindingConfig c, const char* code) {
        bool threw = false;
        try { avatarUsd::SkeletonBinding binding(s,std::move(c)); }
        catch (const std::invalid_argument& e) { threw = true; CHECK(std::string(e.what()).find(code) == 0); }
        CHECK(threw);
    };
    reject({},config(),"USD_BINDING_STAGE");
    auto c = config(); c.layoutVersion = 0; reject(stage(),c,"USD_BINDING_LAYOUT");
    c = config(); c.avatarRoot = pxr::SdfPath("/Missing"); reject(stage(),c,"USD_BINDING_AVATAR_ROOT");
    c = config(); c.skeleton = pxr::SdfPath("/Other"); reject(stage(),c,"USD_BINDING_SKELETON_PATH");
    c = config(); c.skeleton = c.avatarRoot; reject(stage(),c,"USD_BINDING_SKELETON");
    c = config(); c.humanoid[0].joint = "Missing"; reject(stage(),c,"USD_BINDING_HUMANOID_JOINT");
    c = config(); c.humanoid[1].joint = c.humanoid[0].joint; reject(stage(),c,"USD_BINDING_HUMANOID_DUPLICATE");
    c = config(); c.humanoid[1].bone = c.humanoid[0].bone; reject(stage(),c,"USD_BINDING_HUMANOID_DUPLICATE");
    auto s = stage(); CHECK(pxr::UsdGeomSetStageUpAxis(s,pxr::UsdGeomTokens->z)); reject(s,config(),"USD_BINDING_UP_AXIS");
    s = stage(); CHECK(pxr::UsdGeomSetStageMetersPerUnit(s,0)); reject(s,config(),"USD_BINDING_UNITS");
    s = stage(); auto sk = pxr::UsdSkelSkeleton(s->GetPrimAtPath(config().skeleton));
    CHECK(sk.GetRestTransformsAttr().Set(pxr::VtMatrix4dArray{})); reject(s,config(),"USD_BINDING_REST_COUNT");
    auto badTokens = [&](pxr::VtTokenArray tokens, const char* code) {
        auto st = stage(); auto skeleton = pxr::UsdSkelSkeleton(st->GetPrimAtPath(config().skeleton));
        CHECK(skeleton.GetJointsAttr().Set(tokens)); reject(st,config(),code);
    };
    badTokens({pxr::TfToken("A"),pxr::TfToken("A"),pxr::TfToken("B"),pxr::TfToken("C")},"USD_BINDING_JOINT_TOKEN");
    badTokens({pxr::TfToken("A/B"),pxr::TfToken("A"),pxr::TfToken("B"),pxr::TfToken("C")},"USD_BINDING_PARENT_ORDER");
    auto badMatrix = [&](pxr::GfMatrix4d matrix, const char* code) {
        auto st = stage(); auto skeleton = pxr::UsdSkelSkeleton(st->GetPrimAtPath(config().skeleton));
        pxr::VtMatrix4dArray matrices; CHECK(skeleton.GetRestTransformsAttr().Get(&matrices));
        matrices[0] = matrix; CHECK(skeleton.GetRestTransformsAttr().Set(matrices)); reject(st,config(),code);
    };
    auto m = trs(pxr::GfVec3d(0)); m[0][1] = 0.2; badMatrix(m,"USD_BINDING_SHEAR");
    badMatrix(trs(pxr::GfVec3d(0),0,pxr::GfVec3d(-1,1,1)),"USD_BINDING_REFLECTION");
    badMatrix(trs(pxr::GfVec3d(0),0,pxr::GfVec3d(0,1,1)),"USD_BINDING_SCALE");
    m = trs(pxr::GfVec3d(0)); m[0][3] = 1; badMatrix(m,"USD_BINDING_NONAFFINE");
    m = trs(pxr::GfVec3d(0)); m[0][0] = std::numeric_limits<double>::infinity(); badMatrix(m,"USD_BINDING_NONFINITE");
    badMatrix(trs(pxr::GfVec3d(1e100,0,0)),"USD_BINDING_FLOAT_RANGE");
    s = stage(); CHECK(pxr::UsdGeomXform(s->GetPrimAtPath(config().avatarRoot)).AddScaleOp().Set(pxr::GfVec3f(2)));
    reject(s,config(),"USD_BINDING_PLACEMENT_SCALE");
}
#ifdef AR_TEST_MOTION
void composition() {
    avatarUsd::SkeletonBinding binding(stage(),config());
    avatarMotion::ClipPoseAdapterConfig c;
    c.evaluatorId = "usd.motion"; c.layoutId = binding.Baseline().layout_id; c.layoutVersion = binding.Baseline().layout_version;
    c.skeletonId = binding.SkeletonId(); c.skeleton = binding.Skeleton(); c.map = binding.HumanoidMap();
    c.jointIds = binding.JointIds(); c.rootPlacement = binding.RootPlacement();
    motion::MotionPose pose; pose.timestamp = 0; pose.root.hasPosition = true; pose.root.worldPosition = pxr::GfVec3f(2,0,0);
    c.clip.samples.push_back(pose);
    avatarMotion::ClipPoseAdapter adapter(c);
    ArRuntimeApi api{}; CHECK(arGetApi(AR_ABI_VERSION,sizeof(api),&api) == AR_OK);
    ArRuntime runtime = 0; CHECK(api.create_runtime(&runtime) == AR_OK);
    auto descriptor = adapter.Descriptor(); CHECK(api.register_evaluator(runtime,&descriptor,nullptr) == AR_OK);
    std::vector<const char*> evaluators{descriptor.id};
    std::vector<ArCapability> capabilities(descriptor.supplies,descriptor.supplies + descriptor.supply_count);
#ifdef AR_TEST_VRM
    avatarVrm::ExpressionAdapterConfig vrm;
    vrm.evaluatorId = "usd.vrm"; vrm.layoutId = c.layoutId; vrm.layoutVersion = c.layoutVersion;
    vrmRig::LookAtRig look; look.type = vrmRig::LookAtType::Bone; look.leftEyeJoint = "extra";
    look.horizontalInner = {90,30}; look.horizontalOuter = {90,30}; look.verticalUp = {90,20}; look.verticalDown = {90,20};
    vrm.lookAt = look; vrm.headSkeleton = c.skeletonId; vrm.headJoint = c.jointIds[1];
    vrm.gaze = {"test","actor","gaze:world"}; vrm.after = {c.evaluatorId};
    const auto& rest = binding.Baseline().joints[2].local;
    vrm.eyes = {{"extra",c.skeletonId,c.jointIds[2],{rest.rotation[0],rest.rotation[1],rest.rotation[2],rest.rotation[3]}}};
    avatarVrm::ExpressionAdapter vrmAdapter(vrm);
    auto vd = vrmAdapter.Descriptor(); CHECK(api.register_evaluator(runtime,&vd,nullptr) == AR_OK);
    evaluators.push_back(vd.id); capabilities.insert(capabilities.end(),vd.supplies,vd.supplies+vd.supply_count);
#endif
    ArInstanceDesc d{AR_HEADER(ArInstanceDesc)}; d.generation = 1; d.layout_id = c.layoutId.c_str(); d.layout_version = c.layoutVersion;
    d.initial_state = binding.Baseline(); d.evaluators = evaluators.data(); d.evaluator_count = uint32_t(evaluators.size());
    d.bound_capabilities = capabilities.data(); d.bound_capability_count = uint32_t(capabilities.size());
    ArInstance instance = 0; CHECK(api.create_instance(runtime,&d,nullptr,&instance) == AR_OK);
    ArInputFrame input{AR_HEADER(ArInputFrame)}; input.frame_id = 1; input.generation = 1;
    ArSnapshot snapshot = 0; CHECK(api.evaluate_frame(runtime,instance,&input,nullptr,&snapshot) == AR_OK);
    ArStateView v{AR_HEADER(ArStateView)}; CHECK(api.get_snapshot(snapshot,&v) == AR_OK);
    // Owner default hips delta (2,1,0) gets placement once: (3,1,2).
    CHECK(closeEnough(v.joints[0].local.translation[0],3) && closeEnough(v.joints[0].local.translation[1],1) &&
          closeEnough(v.joints[0].local.translation[2],2));
    CHECK(closeEnough(v.joints[3].local.translation[2],3.8));
    CHECK(closeEnough(v.joints[2].local.scale[0],0.5));
    CHECK(api.release_snapshot(snapshot) == AR_OK);
#ifdef AR_TEST_VRM
    ArGazeInput gaze{"test","actor","gaze:world",AR_GAZE_DIRECTION,AR_GAZE_RUNTIME_WORLD,
        AR_OBSERVATION_VALID,nullptr,nullptr,{0,0,1},0,1,0};
    input.frame_id = 2; input.gazes = &gaze; input.gaze_count = 1;
    CHECK(api.evaluate_frame(runtime,instance,&input,nullptr,&snapshot) == AR_OK);
    CHECK(api.get_snapshot(snapshot,&v) == AR_OK);
    auto ownerPose = motion::PoseRetargeter(c.skeleton,c.map,c.sourceRest,c.options).Retarget(pose);
    const auto& placement = c.rootPlacement;
    const pxr::GfQuatd world(placement.rotation[3],pxr::GfVec3d(
        placement.rotation[0],placement.rotation[1],placement.rotation[2]));
    const auto headRotation = (world * pxr::GfQuatd(ownerPose.rotations[0]) *
        pxr::GfQuatd(ownerPose.rotations[1])).GetNormalized();
    vrmRig::LookAtHead head;
    head.orientation = pxr::GfQuatf(headRotation);
    auto expected = vrmRig::LookAtEvaluator(look).EvaluateDirection(pxr::GfVec3f(0,0,1),head,0);
    CHECK(expected.hasGaze && expected.eyeRotations.size() == 1);
    const auto expectedEye = (pxr::GfQuatd(expected.eyeRotations[0].rotation) *
        pxr::GfQuatd(rest.rotation[3],pxr::GfVec3d(rest.rotation[0],rest.rotation[1],rest.rotation[2]))).GetNormalized();
    for (int k = 0; k < 3; ++k) CHECK(closeEnough(v.joints[2].local.rotation[k],expectedEye.GetImaginary()[k]));
    CHECK(closeEnough(v.joints[2].local.rotation[3],expectedEye.GetReal()));
    CHECK(api.release_snapshot(snapshot) == AR_OK);
    // Absence restores this frame's retargeted rest; repeated frames never
    // accumulate placement or gaze, and reset preserves the layout.
    input.frame_id = 3; input.gazes = nullptr; input.gaze_count = 0;
    CHECK(api.evaluate_frame(runtime,instance,&input,nullptr,&snapshot) == AR_OK);
    CHECK(api.get_snapshot(snapshot,&v) == AR_OK);
    CHECK(closeEnough(v.joints[0].local.translation[2],2));
    for (int k = 0; k < 3; ++k) CHECK(closeEnough(v.joints[2].local.rotation[k],ownerPose.rotations[2].GetImaginary()[k]));
    CHECK(api.release_snapshot(snapshot) == AR_OK);
    CHECK(api.reset_instance(runtime,instance,2) == AR_OK);
    input.generation = 2; input.frame_id = 1;
    CHECK(api.evaluate_frame(runtime,instance,&input,nullptr,&snapshot) == AR_OK);
    CHECK(api.get_snapshot(snapshot,&v) == AR_OK);
    CHECK(closeEnough(v.joints[0].local.translation[2],2) && v.layout_version == 2);
    CHECK(api.release_snapshot(snapshot) == AR_OK);
#endif
    CHECK(api.destroy_runtime(runtime) == AR_OK);
    // An empty clip must leave the already placed USD baseline untouched.
    c.clip.samples.clear();
    avatarMotion::ClipPoseAdapter empty(c);
    CHECK(api.create_runtime(&runtime) == AR_OK);
    auto ed = empty.Descriptor(); CHECK(api.register_evaluator(runtime,&ed,nullptr) == AR_OK);
    d.evaluator_count = 1;
    CHECK(api.create_instance(runtime,&d,nullptr,&instance) == AR_OK);
    input.generation = 1; input.frame_id = 1;
    CHECK(api.evaluate_frame(runtime,instance,&input,nullptr,&snapshot) == AR_OK);
    CHECK(api.get_snapshot(snapshot,&v) == AR_OK);
    CHECK(closeEnough(v.joints[0].local.translation[2],4));
    CHECK(api.destroy_runtime(runtime) == AR_OK);
    CHECK(closeEnough(v.joints[0].local.translation[2],4));
    CHECK(api.release_snapshot(snapshot) == AR_OK);
    for (auto invalidPlacement : {0,1,2}) {
        auto bad = c;
        if (invalidPlacement == 0) bad.rootPlacement.scale[0] = 2;
        if (invalidPlacement == 1) bad.rootPlacement.rotation[3] = 2;
        if (invalidPlacement == 2) bad.rootPlacement.translation[0] = std::numeric_limits<double>::infinity();
        bool threw = false; try { avatarMotion::ClipPoseAdapter rejected(bad); }
        catch (const std::invalid_argument&) { threw = true; } CHECK(threw);
    }
}
#endif
} // namespace
int main() {
    try {
        baseline(); invalid();
#ifdef AR_TEST_MOTION
        composition();
#endif
        std::cout << "USD skeleton binding contracts passed\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n'; return 1;
    }
}
