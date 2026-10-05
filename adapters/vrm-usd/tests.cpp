#include "avatarVrmUsd/HumanoidBinding.h"
#include "avatarRuntime/api.h"
#include "vrmSchema/vrmHumanoidAPI.h"
#include "pxr/usd/usdGeom/metrics.h"
#include "pxr/usd/usdGeom/scope.h"
#include "pxr/usd/usdGeom/tokens.h"
#include "pxr/usd/usdSkel/skeleton.h"
#include "pxr/usd/sdf/types.h"
#include "pxr/base/gf/rotation.h"
#ifdef AR_TEST_MOTION
#include "avatarMotion/ClipPoseAdapter.h"
#endif
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

#define CHECK(x) do { if (!(x)) { std::cerr << "Line " << __LINE__ << ": " << #x << '\n'; std::exit(1); } } while (0)
namespace {
namespace motion = openstrata::motion;
avatarVrmUsd::HumanoidBindingConfig config() {
    return {pxr::SdfPath("/Avatar"), {}, "vrm.schema.layout", 1};
}
pxr::UsdStageRefPtr stage() {
    auto s = pxr::UsdStage::CreateInMemory();
    CHECK(pxr::UsdGeomSetStageUpAxis(s, pxr::UsdGeomTokens->y));
    CHECK(pxr::UsdGeomSetStageMetersPerUnit(s, 1));
    pxr::UsdGeomScope::Define(s, pxr::SdfPath("/Avatar"));
    auto skeleton = pxr::UsdSkelSkeleton::Define(s, pxr::SdfPath("/Avatar/Body"));
    CHECK(skeleton.CreateJointsAttr().Set(pxr::VtTokenArray{
        pxr::TfToken("Pelvis"), pxr::TfToken("Pelvis/Skull"), pxr::TfToken("Pelvis/Skull/Eye"), pxr::TfToken("Extra")}));
    pxr::GfMatrix4d head(1); head.SetTranslate(pxr::GfVec3d(0,1,0));
    CHECK(skeleton.CreateRestTransformsAttr().Set(pxr::VtMatrix4dArray{pxr::GfMatrix4d(1),head,
        pxr::GfMatrix4d(1),pxr::GfMatrix4d(1)}));
    auto api = pxr::UsdVrmHumanoidAPI::Apply(pxr::UsdGeomScope::Define(s, pxr::SdfPath("/Avatar/Mapping")).GetPrim());
    CHECK(api);
    CHECK(api.CreateVrmSkeletonRel().SetTargets({skeleton.GetPath()}));
    CHECK(api.CreateVrmHumanBonesHipsAttr().Set(pxr::TfToken("Pelvis")));
    CHECK(api.CreateVrmHumanBonesHeadAttr().Set(pxr::TfToken("Pelvis/Skull")));
    CHECK(api.CreateVrmHumanBonesLeftEyeAttr().Set(pxr::TfToken("Pelvis/Skull/Eye")));
    CHECK(api.GetPrim().CreateAttribute(pxr::TfToken("vrm:humanBones:tail"),pxr::SdfValueTypeNames->Token)
        .Set(pxr::TfToken("Extra")));
    return s;
}
pxr::UsdVrmHumanoidAPI schema(const pxr::UsdStagePtr& s) {
    return pxr::UsdVrmHumanoidAPI(s->GetPrimAtPath(pxr::SdfPath("/Avatar/Mapping")));
}
void ownedAndComposed() {
    auto s = stage();
    avatarVrmUsd::HumanoidBinding binding(s,config());
    auto copy = binding;
    CHECK(binding.HumanoidId() == "/Avatar/Mapping");
    CHECK(binding.Skeleton().SkeletonId() == "/Avatar/Body");
    CHECK(binding.Skeleton().HumanoidMap().GetJointIndex(motion::HumanJoint::Hips) == 0);
    CHECK(binding.Skeleton().HumanoidMap().GetJointIndex(motion::HumanJoint::Head) == 1);
    CHECK(binding.Skeleton().HumanoidMap().GetJointIndex(motion::HumanJoint::LeftEye) == 2);
    CHECK(binding.Skeleton().HumanoidMap().GetJointIndex(motion::HumanJoint::RightEye) == -1);
    CHECK(binding.UnsupportedBones().size() == 1 && binding.UnsupportedBones()[0].role == "tail" &&
          binding.UnsupportedBones()[0].joint == "Extra");
    // A composed reference remaps the relationship target, but joint tokens
    // are skeleton-local and must not be remapped or guessed from prim names.
    auto composed = pxr::UsdStage::CreateInMemory();
    CHECK(pxr::UsdGeomSetStageUpAxis(composed,pxr::UsdGeomTokens->y));
    CHECK(pxr::UsdGeomSetStageMetersPerUnit(composed,1));
    CHECK(composed->DefinePrim(pxr::SdfPath("/Placed")).GetReferences()
        .AddReference(s->GetRootLayer()->GetIdentifier(),pxr::SdfPath("/Avatar")));
    auto c = config(); c.avatarRoot = pxr::SdfPath("/Placed");
    avatarVrmUsd::HumanoidBinding relocated(composed,c);
    CHECK(relocated.HumanoidId() == "/Placed/Mapping");
    CHECK(relocated.Skeleton().SkeletonId() == "/Placed/Body");
    CHECK(relocated.Skeleton().JointIds() == binding.Skeleton().JointIds());
    // Forwarded relationship targets are part of composed USD binding.
    auto forwarded = schema(s).GetPrim().CreateRelationship(pxr::TfToken("relay"));
    CHECK(forwarded.SetTargets({pxr::SdfPath("/Avatar/Body")}));
    CHECK(schema(s).GetVrmSkeletonRel().SetTargets({forwarded.GetPath()}));
    avatarVrmUsd::HumanoidBinding throughRelay(s,config());
    CHECK(throughRelay.Skeleton().SkeletonId() == "/Avatar/Body");
    CHECK(schema(s).GetVrmHumanBonesHeadAttr().Set(pxr::TfToken("Missing")));
    s.Reset(); composed.Reset();
    CHECK(copy.Skeleton().HumanoidMap().GetJointIndex(motion::HumanJoint::Head) == 1);
    CHECK(copy.Skeleton().Baseline().joints == binding.Skeleton().Baseline().joints);
    CHECK(copy.UnsupportedBones()[0].joint == "Extra");
}
void invalid() {
    auto reject = [](const pxr::UsdStagePtr& s, avatarVrmUsd::HumanoidBindingConfig c, const char* code) {
        bool threw = false;
        try { avatarVrmUsd::HumanoidBinding binding(s,c); }
        catch (const std::invalid_argument& e) { threw = true; CHECK(std::string(e.what()).find(code) == 0); }
        CHECK(threw);
    };
    reject({},config(),"VRM_BINDING_STAGE");
    auto c = config(); c.avatarRoot = pxr::SdfPath("/Missing"); reject(stage(),c,"VRM_BINDING_AVATAR_ROOT");
    c = config(); c.humanoid = pxr::SdfPath("/Other"); reject(stage(),c,"VRM_BINDING_HUMANOID_PATH");
    c = config(); c.humanoid = pxr::SdfPath("/Avatar/Body"); reject(stage(),c,"VRM_BINDING_HUMANOID_SCHEMA");
    auto s = stage(); CHECK(schema(s).GetPrim().RemoveAPI<pxr::UsdVrmHumanoidAPI>());
    reject(s,config(),"VRM_BINDING_HUMANOID_MISSING");
    s = stage(); CHECK(schema(s).GetPrim().SetActive(false)); reject(s,config(),"VRM_BINDING_HUMANOID_MISSING");
    s = stage(); CHECK(pxr::UsdVrmHumanoidAPI::Apply(pxr::UsdGeomScope::Define(s,pxr::SdfPath("/Avatar/Second")).GetPrim()));
    reject(s,config(),"VRM_BINDING_HUMANOID_AMBIGUOUS");
    c = config(); c.humanoid = pxr::SdfPath("/Avatar/Mapping");
    avatarVrmUsd::HumanoidBinding explicitSelection(s,c);
    CHECK(explicitSelection.HumanoidId() == "/Avatar/Mapping");
    // Another avatar does not create ambiguity within this root.
    s = stage(); CHECK(pxr::UsdVrmHumanoidAPI::Apply(pxr::UsdGeomScope::Define(s,pxr::SdfPath("/Other")).GetPrim()));
    avatarVrmUsd::HumanoidBinding scoped(s,config());
    for (const auto& paths : {pxr::SdfPathVector{}, pxr::SdfPathVector{pxr::SdfPath("/Avatar/Body"),pxr::SdfPath("/Avatar")}}) {
        s = stage(); CHECK(schema(s).GetVrmSkeletonRel().SetTargets(paths));
        reject(s,config(),"VRM_BINDING_SKELETON_TARGET");
    }
    s = stage(); CHECK(schema(s).GetVrmSkeletonRel().SetTargets({pxr::SdfPath("/Other")}));
    reject(s,config(),"VRM_BINDING_SKELETON_PATH");
    s = stage(); CHECK(schema(s).GetVrmSkeletonRel().SetTargets({pxr::SdfPath("/Avatar/Missing")}));
    reject(s,config(),"USD_BINDING_SKELETON");
    s = stage(); CHECK(schema(s).GetVrmHumanBonesHeadAttr().Set(pxr::TfToken())); reject(s,config(),"VRM_BINDING_BONE_VALUE");
    s = stage(); schema(s).GetVrmHumanBonesHeadAttr().Block(); reject(s,config(),"VRM_BINDING_BONE_VALUE");
    s = stage(); CHECK(schema(s).GetVrmHumanBonesHeadAttr().Set(pxr::TfToken("Missing")));
    reject(s,config(),"USD_BINDING_HUMANOID_JOINT");
    s = stage(); CHECK(schema(s).GetVrmHumanBonesHeadAttr().Set(pxr::TfToken("Pelvis")));
    reject(s,config(),"USD_BINDING_HUMANOID_DUPLICATE");
    s = stage(); CHECK(schema(s).GetPrim().CreateAttribute(pxr::TfToken("vrm:humanBones:bad"),pxr::SdfValueTypeNames->String)
        .Set(std::string("Extra"))); reject(s,config(),"VRM_BINDING_BONE_VALUE");
    c = config(); c.layoutVersion = 0; reject(stage(),c,"USD_BINDING_LAYOUT");
}
void standardVocabulary() {
    auto s = stage(); auto api = schema(s);
    CHECK(api.GetPrim().RemoveProperty(pxr::TfToken("vrm:humanBones:tail")));
    pxr::VtTokenArray joints; pxr::VtMatrix4dArray rest;
    for (size_t i = 0; i < motion::HumanJointCount; ++i) {
        const auto bone = static_cast<motion::HumanJoint>(i);
        const pxr::TfToken joint("J" + std::to_string(i));
        const pxr::TfToken name("vrm:humanBones:" + std::string(motion::HumanJointName(bone)));
        auto attr = api.GetPrim().GetAttribute(name); CHECK(attr);
        CHECK(attr.Set(joint)); joints.push_back(joint); rest.push_back(pxr::GfMatrix4d(1));
    }
    auto sk = pxr::UsdSkelSkeleton(s->GetPrimAtPath(pxr::SdfPath("/Avatar/Body")));
    CHECK(sk.GetJointsAttr().Set(joints)); CHECK(sk.GetRestTransformsAttr().Set(rest));
    avatarVrmUsd::HumanoidBinding binding(s,config());
    CHECK(binding.UnsupportedBones().empty());
    for (size_t i = 0; i < motion::HumanJointCount; ++i)
        CHECK(binding.Skeleton().HumanoidMap().GetJointIndex(static_cast<motion::HumanJoint>(i)) == int(i));
    // An unauthored role is absent; partial and empty maps do not impose
    // an owner evaluator's required-bone policy during schema discovery.
    for (const auto& name : pxr::UsdVrmHumanoidAPI::GetSchemaAttributeNames(false))
        CHECK(api.GetPrim().GetAttribute(name).Clear());
    avatarVrmUsd::HumanoidBinding empty(s,config());
    CHECK(empty.Skeleton().HumanoidMap().GetJointIndex(motion::HumanJoint::Hips) == -1);
}
void evaluate(const avatarVrmUsd::HumanoidBinding& binding) {
    const auto& skeleton = binding.Skeleton();
    const auto& baseline = skeleton.Baseline();
    ArRuntimeApi api{}; CHECK(arGetApi(AR_ABI_VERSION,sizeof(api),&api) == AR_OK);
    ArRuntime runtime = 0; CHECK(api.create_runtime(&runtime) == AR_OK);
    ArInstanceDesc d{AR_HEADER(ArInstanceDesc)}; d.generation = 1;
    d.layout_id = baseline.layout_id; d.layout_version = baseline.layout_version; d.initial_state = baseline;
#ifdef AR_TEST_MOTION
    avatarMotion::ClipPoseAdapterConfig c;
    c.evaluatorId = "schema.motion"; c.layoutId = baseline.layout_id; c.layoutVersion = baseline.layout_version;
    c.skeletonId = skeleton.SkeletonId(); c.skeleton = skeleton.Skeleton(); c.map = skeleton.HumanoidMap();
    c.jointIds = skeleton.JointIds(); c.rootPlacement = skeleton.RootPlacement();
    motion::MotionPose pose; pose.timestamp = 0;
    pose.root.hasPosition = true; pose.root.worldPosition = pxr::GfVec3f(0.25f,0,0);
    const size_t head = size_t(motion::HumanJoint::Head);
    pose.validRotations.set(head);
    pose.localRotations[head] = pxr::GfQuatf(pxr::GfRotation(pxr::GfVec3d(0,1,0),15).GetQuat());
    c.clip.samples.push_back(pose);
    avatarMotion::ClipPoseAdapter adapter(c);
    auto descriptor = adapter.Descriptor(); CHECK(api.register_evaluator(runtime,&descriptor,nullptr) == AR_OK);
    const char* evaluator = descriptor.id; d.evaluators = &evaluator; d.evaluator_count = 1;
    d.bound_capabilities = descriptor.supplies; d.bound_capability_count = descriptor.supply_count;
    const auto expected = motion::PoseRetargeter(c.skeleton,c.map,c.sourceRest,c.options).Retarget(pose);
#endif
    ArInstance instance = 0; CHECK(api.create_instance(runtime,&d,nullptr,&instance) == AR_OK);
    ArInputFrame input{AR_HEADER(ArInputFrame)}; input.frame_id = 1; input.generation = 1;
    ArSnapshot snapshot = 0; CHECK(api.evaluate_frame(runtime,instance,&input,nullptr,&snapshot) == AR_OK);
    ArStateView v{AR_HEADER(ArStateView)}; CHECK(api.get_snapshot(snapshot,&v) == AR_OK);
    CHECK(v.joint_count == baseline.joint_count);
    for (uint32_t i = 0; i < v.joint_count; ++i) {
        CHECK(std::string(v.joints[i].joint_id) == skeleton.JointIds()[i]);
#ifdef AR_TEST_MOTION
        auto position = pxr::GfVec3d(expected.translations[i]);
        auto rotation = pxr::GfQuatd(expected.rotations[i]).GetNormalized();
        if (v.joints[i].parent_index < 0) {
            const auto& p = skeleton.RootPlacement();
            const pxr::GfQuatd q(p.rotation[3],pxr::GfVec3d(p.rotation[0],p.rotation[1],p.rotation[2]));
            position = pxr::GfRotation(q).TransformDir(position) + pxr::GfVec3d(p.translation[0],p.translation[1],p.translation[2]);
            rotation = (q * rotation).GetNormalized();
        }
        for (int k = 0; k < 3; ++k) {
            CHECK(std::abs(v.joints[i].local.translation[k] - position[k]) < 1e-6);
            CHECK(std::abs(v.joints[i].local.rotation[k] - rotation.GetImaginary()[k]) < 1e-6);
            CHECK(std::abs(v.joints[i].local.scale[k] - c.skeleton.GetJoints()[i].restScale[k]) < 1e-6);
        }
        CHECK(std::abs(v.joints[i].local.rotation[3] - rotation.GetReal()) < 1e-6);
#endif
    }
    CHECK(api.destroy_runtime(runtime) == AR_OK);
    CHECK(api.get_snapshot(snapshot,&v) == AR_OK && v.joint_count == baseline.joint_count);
    CHECK(api.release_snapshot(snapshot) == AR_OK);
}
} // namespace
int main(int argc, char** argv) {
    try {
        if (argc == 2) {
            // Opt-in local asset validation: never copy or author the source.
            auto s = pxr::UsdStage::Open(argv[1]); CHECK(s && s->GetDefaultPrim());
            auto c = config(); c.avatarRoot = s->GetDefaultPrim().GetPath();
            avatarVrmUsd::HumanoidBinding binding(s,c);
            s.Reset(); evaluate(binding);
            size_t mapped = 0;
            for (size_t i = 0; i < motion::HumanJointCount; ++i)
                if (binding.Skeleton().HumanoidMap().GetJointIndex(static_cast<motion::HumanJoint>(i)) >= 0) ++mapped;
            std::cout << "Local avatar binding passed: " << binding.HumanoidId() << ", "
                << binding.Skeleton().Baseline().joint_count << " joints, "
                << mapped << " mapped roles, "
                << binding.UnsupportedBones().size() << " unsupported roles\n";
        } else {
            CHECK(argc == 1); ownedAndComposed(); invalid(); standardVocabulary();
            evaluate(avatarVrmUsd::HumanoidBinding(stage(),config()));
            std::cout << "VRM USD Humanoid binding contracts passed\n";
        }
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
