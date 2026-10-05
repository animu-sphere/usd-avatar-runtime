#include "avatarVrmUsd/LookAtBinding.h"
#include "vrmSchema/vrmHumanoidAPI.h"
#include "vrmSchema/vrmLookAtAPI.h"
#include "pxr/usd/usdGeom/metrics.h"
#include "pxr/usd/usdGeom/scope.h"
#include "pxr/usd/usdGeom/xform.h"
#include "pxr/usd/usdGeom/tokens.h"
#include "pxr/usd/usdSkel/skeleton.h"
#include "pxr/usd/sdf/types.h"
#include "pxr/base/gf/rotation.h"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

#define CHECK(x) do { if (!(x)) { std::cerr << "Line " << __LINE__ << ": " << #x << '\n'; std::exit(1); } } while (0)
namespace {
using avatarVrmUsd::LookAtBinding;
avatarVrmUsd::LookAtBindingConfig config() {
    return {{pxr::SdfPath("/Avatar"), {}, "lookat.layout", 1}, {}};
}
pxr::UsdVrmLookAtAPI lookAt(const pxr::UsdStagePtr& s) {
    return pxr::UsdVrmLookAtAPI(s->GetPrimAtPath(pxr::SdfPath("/Avatar/Gaze")));
}
pxr::UsdStageRefPtr stage() {
    auto s = pxr::UsdStage::CreateInMemory();
    CHECK(pxr::UsdGeomSetStageUpAxis(s,pxr::UsdGeomTokens->y));
    CHECK(pxr::UsdGeomSetStageMetersPerUnit(s,0.01));
    auto root = pxr::UsdGeomXform::Define(s,pxr::SdfPath("/Avatar"));
    CHECK(root.AddTranslateOp().Set(pxr::GfVec3d(200,0,0)));
    auto skel = pxr::UsdSkelSkeleton::Define(s,pxr::SdfPath("/Avatar/Body"));
    CHECK(skel.CreateJointsAttr().Set(pxr::VtTokenArray{pxr::TfToken("Root"),pxr::TfToken("Root/Head"),
        pxr::TfToken("Root/Head/L"),pxr::TfToken("Root/Head/R"),pxr::TfToken("Extra")}));
    pxr::GfMatrix4d head(1), left(1), right(1);
    head.SetRotate(pxr::GfRotation(pxr::GfVec3d(0,1,0),15)); head.SetTranslateOnly(pxr::GfVec3d(0,100,0));
    left.SetRotate(pxr::GfRotation(pxr::GfVec3d(0,0,1),10)); left.SetTranslateOnly(pxr::GfVec3d(4,0,0));
    right.SetTranslate(pxr::GfVec3d(-4,0,0));
    CHECK(skel.CreateRestTransformsAttr().Set(pxr::VtMatrix4dArray{pxr::GfMatrix4d(1),head,left,right,pxr::GfMatrix4d(1)}));
    auto human = pxr::UsdVrmHumanoidAPI::Apply(pxr::UsdGeomScope::Define(s,pxr::SdfPath("/Avatar/Mapping")).GetPrim());
    CHECK(human.CreateVrmSkeletonRel().SetTargets({skel.GetPath()}));
    CHECK(human.CreateVrmHumanBonesHeadAttr().Set(pxr::TfToken("Root/Head")));
    auto gaze = pxr::UsdVrmLookAtAPI::Apply(pxr::UsdGeomScope::Define(s,pxr::SdfPath("/Avatar/Gaze")).GetPrim());
    CHECK(gaze.CreateVrmTypeAttr().Set(pxr::TfToken("bone")));
    CHECK(gaze.CreateVrmSkeletonRel().SetTargets({skel.GetPath()}));
    CHECK(gaze.CreateVrmLeftEyeAttr().Set(pxr::TfToken("Root/Head/L")));
    CHECK(gaze.CreateVrmRightEyeAttr().Set(pxr::TfToken("Root/Head/R")));
    // Raw type deliberately disagrees: typed normalized data takes precedence.
    gaze.GetPrim().SetCustomDataByKey(pxr::TfToken("vrm:lookAt:raw"),pxr::VtValue(std::string(
        R"({"type":"expression","offsetFromHeadBone":[0,0.05,0],"rangeMapHorizontalInner":{"inputMaxValue":80,"outputScale":12},"rangeMapHorizontalOuter":{"inputMaxValue":70,"outputScale":30},"rangeMapVerticalDown":{"inputMaxValue":60,"outputScale":10},"rangeMapVerticalUp":{"inputMaxValue":50,"outputScale":20}})")));
    return s;
}
bool closeEnough(double a, double b) { return std::abs(a-b) <= 1e-6; }
pxr::GfQuatd quaternion(const ArTransform& t) {
    return pxr::GfQuatd(t.rotation[3],pxr::GfVec3d(t.rotation[0],t.rotation[1],t.rotation[2]));
}
vrmRig::LookAtHead head(const ArStateView& state, const std::string& joint) {
    std::vector<pxr::GfQuatd> rotations;
    std::vector<pxr::GfVec3d> positions;
    for (uint32_t i = 0; i < state.joint_count; ++i) {
        const auto& j = state.joints[i];
        auto rotation = quaternion(j.local);
        pxr::GfVec3d position(j.local.translation[0],j.local.translation[1],j.local.translation[2]);
        if (j.parent_index >= 0) {
            position = positions[size_t(j.parent_index)] + rotations[size_t(j.parent_index)].Transform(position);
            rotation = rotations[size_t(j.parent_index)] * rotation;
        }
        rotations.push_back(rotation); positions.push_back(position);
        if (joint == j.joint_id) return {pxr::GfQuatf(rotation),pxr::GfVec3f(position)};
    }
    CHECK(false); return {};
}
void evaluate(const LookAtBinding& binding) {
    auto c = binding.AdapterConfig("usd.gaze",{"test","actor","gaze:point"});
    CHECK(c.headSkeleton == binding.Humanoid().Skeleton().SkeletonId());
    const auto& baseline = binding.Humanoid().Skeleton().Baseline();
    auto initial = baseline;
    std::vector<ArBlendShape> morphs;
    if (c.lookAt->type == vrmRig::LookAtType::Expression) {
        // Test sinks only: this verifies real LookAt weights reaching the
        // adapter, not extraction of this avatar's actual expression binds.
        for (const char* name : {"lookLeft","lookRight","lookUp","lookDown"}) {
            vrmRig::ExpressionDefinition definition; definition.name = name;
            definition.morphTargets.push_back({name,1}); CHECK(c.expressions.Add(definition));
            c.morphs.push_back({name,"test.mesh",name}); morphs.push_back({"test.mesh",name,0});
        }
        initial.blend_shapes = morphs.data(); initial.blend_shape_count = uint32_t(morphs.size());
    }
    avatarVrm::ExpressionAdapter adapter(c);
    ArRuntimeApi api{}; CHECK(arGetApi(AR_ABI_VERSION,sizeof(api),&api) == AR_OK);
    ArRuntime runtime = 0; CHECK(api.create_runtime(&runtime) == AR_OK);
    auto descriptor = adapter.Descriptor(); CHECK(api.register_evaluator(runtime,&descriptor,nullptr) == AR_OK);
    ArInstanceDesc d{AR_HEADER(ArInstanceDesc)}; d.generation = 1;
    d.layout_id = baseline.layout_id; d.layout_version = baseline.layout_version; d.initial_state = initial;
    const char* selected = descriptor.id; d.evaluators = &selected; d.evaluator_count = 1;
    d.bound_capabilities = descriptor.supplies; d.bound_capability_count = descriptor.supply_count;
    ArInstance instance = 0; CHECK(api.create_instance(runtime,&d,nullptr,&instance) == AR_OK);
    const auto h = head(baseline,c.headJoint);
    ArGazeInput gaze{"test","actor","gaze:point",AR_GAZE_POINT,AR_GAZE_RUNTIME_WORLD,
        AR_OBSERVATION_VALID,nullptr,nullptr,{h.position[0]+1,h.position[1]+0.5,h.position[2]+2},0,1,0};
    vrmRig::LookAtInput ownerInput; ownerInput.head = h;
    ownerInput.target = pxr::GfVec3f(float(gaze.value[0]),float(gaze.value[1]),float(gaze.value[2]));
    const auto expected = vrmRig::LookAtEvaluator(*c.lookAt).Evaluate(ownerInput);
    CHECK(expected.hasGaze && expected.eyeRotations.size() == c.eyes.size());
    ArSnapshot retained = 0;
    for (uint64_t f = 1; f <= 3; ++f) {
        ArInputFrame input{AR_HEADER(ArInputFrame)}; input.frame_id = f; input.generation = 1;
        input.gazes = &gaze; input.gaze_count = 1; input.input_revision = 7;
        ArSnapshot snapshot = 0; CHECK(api.evaluate_frame(runtime,instance,&input,nullptr,&snapshot) == AR_OK);
        ArStateView state{AR_HEADER(ArStateView)}; CHECK(api.get_snapshot(snapshot,&state) == AR_OK);
        CHECK(state.input_revision == 7 && state.layout_version == baseline.layout_version);
        for (const auto& weight : expected.expressions.entries) {
            uint32_t i = 0;
            while (i < state.blend_shape_count && weight.name != state.blend_shapes[i].target_id) ++i;
            CHECK(i < state.blend_shape_count && closeEnough(state.blend_shapes[i].weight,weight.value));
        }
        for (const auto& eye : expected.eyeRotations) {
            uint32_t i = 0;
            while (i < state.joint_count && eye.joint != state.joints[i].joint_id) ++i;
            CHECK(i < state.joint_count);
            const auto q = (pxr::GfQuatd(eye.rotation)*quaternion(baseline.joints[i].local)).GetNormalized();
            for (size_t k = 0; k < 3; ++k) {
                CHECK(closeEnough(state.joints[i].local.rotation[k],q.GetImaginary()[k]));
                CHECK(state.joints[i].local.translation[k] == baseline.joints[i].local.translation[k]);
            }
            CHECK(closeEnough(state.joints[i].local.rotation[3],q.GetReal()));
        }
        if (f == 1) retained = snapshot; else CHECK(api.release_snapshot(snapshot) == AR_OK);
    }
    CHECK(api.reset_instance(runtime,instance,2) == AR_OK);
    ArInputFrame absent{AR_HEADER(ArInputFrame)}; absent.frame_id = 1; absent.generation = 2;
    ArSnapshot reset = 0; CHECK(api.evaluate_frame(runtime,instance,&absent,nullptr,&reset) == AR_OK);
    ArStateView resetState{AR_HEADER(ArStateView)}; CHECK(api.get_snapshot(reset,&resetState) == AR_OK);
    for (uint32_t i = 0; i < baseline.joint_count; ++i)
        for (size_t k = 0; k < 4; ++k) CHECK(resetState.joints[i].local.rotation[k] == baseline.joints[i].local.rotation[k]);
    for (uint32_t i = 0; i < resetState.blend_shape_count; ++i) CHECK(resetState.blend_shapes[i].weight == 0);
    CHECK(api.destroy_runtime(runtime) == AR_OK);
    ArStateView old{AR_HEADER(ArStateView)}; CHECK(api.get_snapshot(retained,&old) == AR_OK);
    CHECK(old.frame_id == 1 && old.generation == 1 && old.input_revision == 7);
    CHECK(api.release_snapshot(retained) == AR_OK); CHECK(api.release_snapshot(reset) == AR_OK);
}
void ownershipAndParsing() {
    auto s = stage(); LookAtBinding binding(s,config()); auto copy = binding;
    auto c = binding.AdapterConfig("gaze",{"s","a","gaze:p"},{"motion"});
    CHECK(binding.LookAtId() == "/Avatar/Gaze" && binding.Warnings().empty());
    CHECK(c.lookAt->type == vrmRig::LookAtType::Bone && c.eyes.size() == 2);
    CHECK(c.after == std::vector<std::string>{"motion"});
    CHECK(c.headJoint == "Root/Head");
    CHECK(closeEnough(c.lookAt->offsetFromHeadBone.value()[1],0.05));
    CHECK(closeEnough(c.lookAt->horizontalOuter.outputScale,30));
    CHECK(closeEnough(c.eyes[0].restRotation[2],std::sin(5*3.141592653589793/180)));
    CHECK(closeEnough(binding.Humanoid().Skeleton().Baseline().joints[1].local.translation[1],1));
    auto relay = lookAt(s).GetPrim().CreateRelationship(pxr::TfToken("relay"),false);
    CHECK(relay.SetTargets({pxr::SdfPath("/Avatar/Body")}));
    CHECK(lookAt(s).GetVrmSkeletonRel().SetTargets({relay.GetPath()}));
    LookAtBinding forwarded(s,config());
    auto referenced = pxr::UsdStage::CreateInMemory();
    CHECK(pxr::UsdGeomSetStageUpAxis(referenced,pxr::UsdGeomTokens->y));
    CHECK(pxr::UsdGeomSetStageMetersPerUnit(referenced,0.01));
    CHECK(referenced->DefinePrim(pxr::SdfPath("/Placed")).GetReferences()
        .AddReference(s->GetRootLayer()->GetIdentifier(),pxr::SdfPath("/Avatar")));
    auto relocatedConfig = config(); relocatedConfig.humanoid.avatarRoot = pxr::SdfPath("/Placed");
    LookAtBinding relocated(referenced,relocatedConfig);
    CHECK(relocated.LookAtId() == "/Placed/Gaze");
    CHECK(relocated.AdapterConfig("g",{"s","a","g:p"}).headSkeleton == "/Placed/Body");
    CHECK(lookAt(s).GetVrmTypeAttr().Set(pxr::TfToken("expression")));
    lookAt(s).GetPrim().SetCustomDataByKey(pxr::TfToken("vrm:lookAt:raw"),pxr::VtValue(std::string(
        R"({"lookAtTypeName":"Bone","lookAtHorizontalOuter":{"xRange":80,"yRange":0.8,"curve":[0,0,0,1,1,1,1,0]}})")));
    LookAtBinding expression(s,config());
    auto e = expression.AdapterConfig("expression",{"s","a","g:p"});
    CHECK(e.lookAt->type == vrmRig::LookAtType::Expression && e.eyes.empty());
    CHECK(e.lookAt->horizontalOuter.curve.size() == 2 && closeEnough(e.lookAt->horizontalOuter.outputScale,0.8));
    avatarVrm::ExpressionAdapter expressionAdapter(e);
    evaluate(expression);
    // Mutation and stage destruction cannot change already-extracted values.
    CHECK(lookAt(s).GetVrmLeftEyeAttr().Set(pxr::TfToken("Missing")));
    s.Reset(); referenced.Reset();
    CHECK(copy.AdapterConfig("copy",{"s","a","g:p"}).eyes[0].ownerJoint == "Root/Head/L");
    evaluate(binding); evaluate(copy); evaluate(relocated);
}
void invalid() {
    auto reject = [](const pxr::UsdStagePtr& s, avatarVrmUsd::LookAtBindingConfig c, const char* code) {
        bool threw = false;
        try { LookAtBinding binding(s,c); }
        catch (const std::invalid_argument& e) { threw = true; CHECK(std::string(e.what()).find(code) == 0); }
        CHECK(threw);
    };
    reject({},config(),"VRM_BINDING_STAGE");
    auto c = config(); c.lookAt = pxr::SdfPath("/Other"); reject(stage(),c,"VRM_LOOKAT_BINDING_PATH");
    c = config(); c.lookAt = pxr::SdfPath("/Avatar/Body"); reject(stage(),c,"VRM_LOOKAT_BINDING_SCHEMA");
    auto s = stage(); CHECK(lookAt(s).GetPrim().RemoveAPI<pxr::UsdVrmLookAtAPI>());
    reject(s,config(),"VRM_LOOKAT_BINDING_MISSING");
    s = stage(); CHECK(lookAt(s).GetPrim().SetActive(false)); reject(s,config(),"VRM_LOOKAT_BINDING_MISSING");
    s = stage(); CHECK(pxr::UsdVrmLookAtAPI::Apply(pxr::UsdGeomScope::Define(s,pxr::SdfPath("/Avatar/Second")).GetPrim()));
    reject(s,config(),"VRM_LOOKAT_BINDING_AMBIGUOUS");
    c = config(); c.lookAt = pxr::SdfPath("/Avatar/Gaze"); LookAtBinding explicitBinding(s,c);
    s = stage(); CHECK(lookAt(s).GetVrmTypeAttr().Set(pxr::TfToken("invalid"))); reject(s,config(),"VRM_LOOKAT_BINDING_TYPE");
    s = stage(); lookAt(s).GetVrmTypeAttr().Block(); reject(s,config(),"VRM_LOOKAT_BINDING_TOKEN");
    s = stage(); CHECK(lookAt(s).GetVrmSkeletonRel().SetTargets({pxr::SdfPath("/Other")})); reject(s,config(),"VRM_LOOKAT_BINDING_SKELETON");
    s = stage(); CHECK(lookAt(s).GetVrmSkeletonRel().SetTargets({})); reject(s,config(),"VRM_LOOKAT_BINDING_SKELETON");
    s = stage(); CHECK(lookAt(s).GetVrmLeftEyeAttr().Set(pxr::TfToken("Missing"))); reject(s,config(),"VRM_LOOKAT_BINDING_EYE");
    s = stage(); CHECK(lookAt(s).GetVrmLeftEyeAttr().Set(pxr::TfToken("Extra"))); reject(s,config(),"VRM_LOOKAT_BINDING_EYE_PARENT");
    s = stage(); CHECK(lookAt(s).GetVrmRightEyeAttr().Set(pxr::TfToken("Root/Head/L"))); reject(s,config(),"VRM_LOOKAT_BINDING_EYE_DUPLICATE");
    s = stage(); CHECK(lookAt(s).GetVrmLeftEyeAttr().Clear()); CHECK(lookAt(s).GetVrmRightEyeAttr().Clear());
    reject(s,config(),"VRM_LOOKAT_BINDING_EYES_MISSING");
    CHECK(lookAt(s).GetVrmTypeAttr().Set(pxr::TfToken("expression"))); LookAtBinding withoutEyes(s,config());
    s = stage(); lookAt(s).GetVrmLeftEyeAttr().Block(); reject(s,config(),"VRM_LOOKAT_BINDING_TOKEN");
    s = stage(); CHECK(pxr::UsdVrmHumanoidAPI(s->GetPrimAtPath(pxr::SdfPath("/Avatar/Mapping"))).GetVrmHumanBonesHeadAttr().Clear());
    reject(s,config(),"VRM_LOOKAT_BINDING_HEAD");
    s = stage(); lookAt(s).GetPrim().SetCustomDataByKey(pxr::TfToken("vrm:lookAt:raw"),pxr::VtValue(42));
    reject(s,config(),"VRM_LOOKAT_BINDING_RAW");
    for (const auto& raw : {"not JSON","[]"}) {
        s = stage(); lookAt(s).GetPrim().SetCustomDataByKey(pxr::TfToken("vrm:lookAt:raw"),pxr::VtValue(std::string(raw)));
        reject(s,config(),"VRM_LOOKAT_BINDING_RAW");
    }
    s = stage(); lookAt(s).GetPrim().ClearCustomDataByKey(pxr::TfToken("vrm:lookAt:raw"));
    LookAtBinding defaults(s,config()); CHECK(closeEnough(defaults.AdapterConfig("g",{"s","a","g:p"}).lookAt->horizontalOuter.inputMaxValue,90));
    CHECK(lookAt(s).GetVrmSkeletonRel().ClearTargets(true)); LookAtBinding noRelationship(s,config());
    CHECK(noRelationship.AdapterConfig("g",{"s","a","g:p"}).headSkeleton == "/Avatar/Body");
    lookAt(s).GetPrim().SetCustomDataByKey(pxr::TfToken("vrm:lookAt:raw"),pxr::VtValue(std::string(
        R"({"rangeMapHorizontalOuter":{"inputMaxValue":0,"outputScale":5}})")));
    LookAtBinding warning(s,config()); CHECK(warning.Warnings().size() == 1);
    s = stage(); CHECK(lookAt(s).GetVrmRightEyeAttr().Clear()); LookAtBinding oneEye(s,config()); evaluate(oneEye);
    s = stage(); auto skel = pxr::UsdSkelSkeleton(s->GetPrimAtPath(pxr::SdfPath("/Avatar/Body")));
    pxr::VtMatrix4dArray rest; CHECK(skel.GetRestTransformsAttr().Get(&rest)); rest[0].SetScale(pxr::GfVec3d(2));
    CHECK(skel.GetRestTransformsAttr().Set(rest)); reject(s,config(),"VRM_LOOKAT_BINDING_HEAD_SCALE");
}
} // namespace
int main(int argc, char** argv) {
    try {
        ownershipAndParsing(); invalid();
        if (argc == 2) {
            auto s = pxr::UsdStage::Open(argv[1]); CHECK(s && s->GetDefaultPrim());
            auto c = config(); c.humanoid.avatarRoot = s->GetDefaultPrim().GetPath();
            LookAtBinding binding(s,c); s.Reset(); evaluate(binding);
            std::cout << "real-avatar LookAt: " << binding.LookAtId() << " joints="
                << binding.Humanoid().Skeleton().Baseline().joint_count << " warnings=" << binding.Warnings().size() << '\n';
            for (const auto& w : binding.Warnings()) std::cout << "owner warning: " << w << '\n';
        }
        std::cout << "LookAt USD binding contracts passed\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
