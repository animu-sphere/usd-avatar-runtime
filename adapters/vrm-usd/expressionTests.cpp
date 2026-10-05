#include "avatarVrmUsd/ExpressionBinding.h"
#include "avatarVrmUsd/LookAtBinding.h"
#include "vrmSchema/vrmExpressionAPI.h"
#include "vrmSchema/vrmHumanoidAPI.h"
#include "vrmSchema/vrmLookAtAPI.h"
#include "vrmRig/MaterialColorSlots.h"
#include "pxr/usd/usdGeom/metrics.h"
#include "pxr/usd/usdGeom/scope.h"
#include "pxr/usd/usdGeom/mesh.h"
#include "pxr/usd/usdGeom/tokens.h"
#include "pxr/usd/usdSkel/skeleton.h"
#include "pxr/usd/usdSkel/blendShape.h"
#include "pxr/usd/usdSkel/bindingAPI.h"
#include "pxr/usd/usdShade/material.h"
#include "pxr/usd/sdf/types.h"
#include "pxr/base/gf/quatd.h"
#include <cmath>
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>

#define CHECK(x) do { if (!(x)) { std::cerr << "Line " << __LINE__ << ": " << #x << '\n'; std::exit(1); } } while (0)
using avatarVrmUsd::ExpressionBinding;
namespace {
avatarVrmUsd::ExpressionBindingConfig config() { return {{pxr::SdfPath("/Avatar"),{},"expression.layout",1},{}}; }
pxr::UsdVrmExpressionAPI expression(const pxr::UsdStagePtr& s, const char* name = "happy") {
    return pxr::UsdVrmExpressionAPI(s->GetPrimAtPath(pxr::SdfPath(std::string("/Avatar/Expressions/")+name)));
}
pxr::UsdStageRefPtr stage() {
    auto s = pxr::UsdStage::CreateInMemory();
    CHECK(pxr::UsdGeomSetStageUpAxis(s,pxr::UsdGeomTokens->y));
    CHECK(pxr::UsdGeomSetStageMetersPerUnit(s,1));
    s->DefinePrim(pxr::SdfPath("/Avatar"));
    auto skeleton = pxr::UsdSkelSkeleton::Define(s,pxr::SdfPath("/Avatar/Body"));
    CHECK(skeleton.CreateJointsAttr().Set(pxr::VtTokenArray{pxr::TfToken("Head")}));
    CHECK(skeleton.CreateRestTransformsAttr().Set(pxr::VtMatrix4dArray{pxr::GfMatrix4d(1)}));
    auto human = pxr::UsdVrmHumanoidAPI::Apply(pxr::UsdGeomScope::Define(s,pxr::SdfPath("/Avatar/Human")).GetPrim());
    CHECK(human.CreateVrmSkeletonRel().SetTargets({skeleton.GetPath()}));
    CHECK(human.CreateVrmHumanBonesHeadAttr().Set(pxr::TfToken("Head")));
    auto shape = pxr::UsdSkelBlendShape::Define(s,pxr::SdfPath("/Avatar/Shapes/Smile"));
    auto mesh = pxr::UsdGeomMesh::Define(s,pxr::SdfPath("/Avatar/Face"));
    auto binding = pxr::UsdSkelBindingAPI::Apply(mesh.GetPrim());
    CHECK(binding.CreateBlendShapesAttr().Set(pxr::VtTokenArray{pxr::TfToken("smile_alias")}));
    CHECK(binding.CreateBlendShapeTargetsRel().SetTargets({shape.GetPath()}));
    auto material = pxr::UsdShadeMaterial::Define(s,pxr::SdfPath("/Avatar/Material"));
    CHECK(material.GetPrim().ApplyAPI(pxr::TfToken("VrmMaterialAPI")));
    CHECK(material.GetPrim().ApplyAPI(pxr::TfToken("VrmMToonAPI")));
    for (const auto& slot : vrmRig::GetMaterialColorSlots()) {
        CHECK(material.GetPrim().CreateAttribute(pxr::TfToken(slot.colorInput),pxr::SdfValueTypeNames->Color3f,false).Set(pxr::GfVec3f(.2f,.3f,.4f)));
        if (slot.alphaInput) CHECK(material.GetPrim().CreateAttribute(pxr::TfToken(slot.alphaInput),pxr::SdfValueTypeNames->Float,false).Set(.8f));
    }
    for (const char* name : {"happy","blink","lookLeft","lookRight","lookUp","lookDown"}) {
        auto api = pxr::UsdVrmExpressionAPI::Apply(pxr::UsdGeomScope::Define(s,pxr::SdfPath(std::string("/Avatar/Expressions/")+name)).GetPrim());
        CHECK(api.CreateVrmExpressionNameAttr().Set(pxr::TfToken(name)));
        CHECK(api.CreateVrmMorphTargetsRel().SetTargets({shape.GetPath()}));
        CHECK(api.CreateVrmMorphTargetWeightsAttr().Set(pxr::VtFloatArray{.7f}));
    }
    auto api = expression(s);
    CHECK(api.CreateVrmOverrideBlinkAttr().Set(pxr::TfToken("blend")));
    CHECK(api.CreateVrmMaterialColorTargetsRel().SetTargets({material.GetPath()}));
    pxr::VtTokenArray types; pxr::VtVec4fArray values; pxr::VtIntArray indices;
    for (const auto& slot : vrmRig::GetMaterialColorSlots()) {
        types.push_back(pxr::TfToken(slot.name)); values.push_back(pxr::GfVec4f(.9f,.7f,.5f,.6f)); indices.push_back(0);
    }
    CHECK(api.CreateVrmMaterialColorTypesAttr().Set(types));
    CHECK(api.CreateVrmMaterialColorValuesAttr().Set(values));
    CHECK(api.CreateVrmMaterialColorTargetIndicesAttr().Set(indices));
    auto gaze = pxr::UsdVrmLookAtAPI::Apply(pxr::UsdGeomScope::Define(s,pxr::SdfPath("/Avatar/Gaze")).GetPrim());
    CHECK(gaze.CreateVrmTypeAttr().Set(pxr::TfToken("expression")));
    return s;
}
bool close(double a, double b) { return std::abs(a-b) <= 1e-6; }
vrmRig::LookAtHead head(const ArStateView& state, const std::string& joint) {
    std::vector<pxr::GfQuatd> rotations; std::vector<pxr::GfVec3d> positions;
    for (uint32_t i = 0; i < state.joint_count; ++i) {
        const auto& j = state.joints[i]; const auto& t = j.local;
        pxr::GfQuatd rotation(t.rotation[3],pxr::GfVec3d(t.rotation[0],t.rotation[1],t.rotation[2]));
        pxr::GfVec3d position(t.translation[0],t.translation[1],t.translation[2]);
        if (j.parent_index >= 0) {
            position = positions[size_t(j.parent_index)] + rotations[size_t(j.parent_index)].Transform(position);
            rotation = rotations[size_t(j.parent_index)] * rotation;
        }
        rotations.push_back(rotation); positions.push_back(position);
        if (joint == j.joint_id) return {pxr::GfQuatf(rotation),pxr::GfVec3f(position)};
    }
    CHECK(false); return {};
}
void evaluate(const ExpressionBinding& binding, avatarVrm::ExpressionAdapterConfig c) {
    c.after.clear(); // This harness supplies an authored pose, without a motion evaluator.
    c.inputs = {{{"test","actor","expr:happy"},"happy"},{{"test","actor","expr:blink"},"blink"}};
    avatarVrm::ExpressionAdapter adapter(c);
    ArRuntimeApi api{}; CHECK(arGetApi(AR_ABI_VERSION,sizeof(api),&api) == AR_OK);
    ArRuntime runtime = 0; CHECK(api.create_runtime(&runtime) == AR_OK);
    auto descriptor = adapter.Descriptor(); CHECK(api.register_evaluator(runtime,&descriptor,nullptr) == AR_OK);
    ArInstanceDesc d{AR_HEADER(ArInstanceDesc)}; d.generation = 1; d.initial_state = binding.Baseline();
    d.layout_id = d.initial_state.layout_id; d.layout_version = d.initial_state.layout_version;
    const char* ids[]{descriptor.id}; d.evaluators = ids; d.evaluator_count = 1;
    d.bound_capabilities = descriptor.supplies; d.bound_capability_count = descriptor.supply_count;
    ArDiagnosticSink sink{};
    sink.emit = [](void*,const ArDiagnostic* diagnostic) { std::cerr << diagnostic->code << ": " << diagnostic->message << '\n'; };
    ArInstance instance = 0; CHECK(api.create_instance(runtime,&d,&sink,&instance) == AR_OK);
    ArSnapshot retained = 0;
    for (uint64_t f = 1; f <= 3; ++f) {
        ArScalarInput scalars[2]{};
        for (auto& scalar : scalars) { scalar.source_id = "test"; scalar.actor_id = "actor"; scalar.clock_scale = 1; }
        scalars[0].channel_id = "expr:happy"; scalars[0].value = f == 1 ? .4 : 0;
        scalars[1].channel_id = "expr:blink"; scalars[1].value = f == 1 ? .8 : 0;
        ArGazeInput gaze{}; gaze.source_id = "test"; gaze.actor_id = "actor"; gaze.channel_id = "gaze:point";
        gaze.validity = AR_OBSERVATION_VALID; gaze.kind = AR_GAZE_POINT; gaze.space = AR_GAZE_RUNTIME_WORLD;
        gaze.value[0] = 1; gaze.value[1] = .5; gaze.value[2] = 3;
        gaze.clock_scale = 1;
        ArInputFrame frame{AR_HEADER(ArInputFrame)}; frame.frame_id = f; frame.generation = 1;
        frame.scalars = scalars; frame.scalar_count = f == 3 ? 0 : 2;
        if (c.lookAt && f == 1) { frame.gazes = &gaze; frame.gaze_count = 1; }
        ArSnapshot snapshot = 0; CHECK(api.evaluate_frame(runtime,instance,&frame,&sink,&snapshot) == AR_OK);
        ArStateView state{AR_HEADER(ArStateView)}; CHECK(api.get_snapshot(snapshot,&state) == AR_OK);
        openstrata::motion::MotionChannelSet weights;
        if (f != 3) { weights.Set("happy",float(scalars[0].value)); weights.Set("blink",float(scalars[1].value)); }
        if (c.lookAt && f == 1) {
            vrmRig::LookAtInput input; input.target = pxr::GfVec3f(1,.5f,3);
            input.head = head(binding.Baseline(),c.headJoint);
            const auto look = vrmRig::LookAtEvaluator(*c.lookAt).Evaluate(input);
            for (const auto& e : look.expressions.entries) weights.Set(e.name,e.value);
        }
        auto expected = vrmRig::ExpressionResolver(binding.Rig()).Resolve(weights);
        CHECK(state.blend_shape_count == binding.Baseline().blend_shape_count);
        for (uint32_t i = 0; i < state.blend_shape_count; ++i) {
            double weight = 0;
            for (const auto& e : expected.morphTargets)
                for (const auto& b : c.morphs)
                    if (b.ownerTarget == e.target && b.mesh == state.blend_shapes[i].mesh_id && b.target == state.blend_shapes[i].target_id) weight = e.weight;
            if (!close(state.blend_shapes[i].weight,weight)) std::cerr << "frame=" << f << " gaze=" << bool(c.lookAt) << " actual=" << state.blend_shapes[i].weight << " expected=" << weight << '\n';
            CHECK(close(state.blend_shapes[i].weight,weight));
        }
        for (uint32_t i = 0; i < state.material_count; ++i) {
            const auto& actual = state.materials[i]; const auto& base = binding.Baseline().materials[i];
            double value[4]; std::copy_n(base.value,4,value); bool overridden = false;
            for (const auto& e : expected.materialColors) {
                const auto* slot = vrmRig::FindMaterialColorSlot(e.colorType);
                if (e.material != actual.material_id) continue;
                if (std::string(actual.input_id) == slot->colorInput || (slot->alphaInput && std::string(actual.input_id) == slot->alphaInput)) {
                    pxr::GfVec4f original(0,0,0,1);
                    for (uint32_t j = 0; j < binding.Baseline().material_count; ++j) {
                        const auto& authored = binding.Baseline().materials[j];
                        if (e.material != authored.material_id) continue;
                        if (std::string(authored.input_id) == slot->colorInput)
                            for (int k = 0; k < 3; ++k) original[k] = float(authored.value[k]);
                        if (slot->alphaInput && std::string(authored.input_id) == slot->alphaInput) original[3] = float(authored.value[0]);
                    }
                    auto resolved = e.Apply(original); overridden = true;
                    if (actual.value_type == AR_VALUE_VEC3) for (int k = 0; k < 3; ++k) value[k] = resolved[k];
                    else value[0] = resolved[3];
                }
            }
            CHECK(actual.overridden == uint32_t(overridden));
            for (int k = 0; k < 4; ++k) CHECK(close(actual.value[k],value[k]));
        }
        if (f == 1) retained = snapshot; else CHECK(api.release_snapshot(snapshot) == AR_OK);
    }
    CHECK(api.reset_instance(runtime,instance,2) == AR_OK);
    ArInputFrame absent{AR_HEADER(ArInputFrame)}; absent.frame_id = 1; absent.generation = 2;
    ArSnapshot reset = 0; CHECK(api.evaluate_frame(runtime,instance,&absent,nullptr,&reset) == AR_OK);
    ArStateView resetState{AR_HEADER(ArStateView)}; CHECK(api.get_snapshot(reset,&resetState) == AR_OK);
    for (uint32_t i = 0; i < resetState.blend_shape_count; ++i) CHECK(resetState.blend_shapes[i].weight == 0);
    for (uint32_t i = 0; i < resetState.material_count; ++i) {
        CHECK(resetState.materials[i].overridden == 0);
        for (int k = 0; k < 4; ++k) CHECK(resetState.materials[i].value[k] == binding.Baseline().materials[i].value[k]);
    }
    CHECK(api.destroy_runtime(runtime) == AR_OK);
    ArStateView old{AR_HEADER(ArStateView)}; CHECK(api.get_snapshot(retained,&old) == AR_OK && old.frame_id == 1);
    CHECK(api.release_snapshot(retained) == AR_OK); CHECK(api.release_snapshot(reset) == AR_OK);
}
void valid() {
    auto s = stage(); ExpressionBinding b(s,config()); auto copy = b;
    CHECK(b.Rig().GetSize() == 6 && b.Baseline().material_count == 7 && b.Baseline().blend_shape_count == 1);
    CHECK(std::string(b.Baseline().blend_shapes[0].target_id) == "smile_alias");
    auto c = b.AdapterConfig("expr",{}, {"motion"}); CHECK(c.after == std::vector<std::string>{"motion"});
    avatarVrmUsd::LookAtBinding look(s,{config().humanoid,{}});
    auto composed = look.AdapterConfig("gaze",{"test","actor","gaze:point"}); b.ApplyTo(composed);
    evaluate(b,c); evaluate(b,composed);
    auto scoped = config(); scoped.expressionsRoot = pxr::SdfPath("/Avatar/Expressions/happy");
    ExpressionBinding one(s,scoped); CHECK(one.Rig().GetSize() == 1);
    // Pre-index schema: one bind per relationship target.
    auto api = expression(s); CHECK(api.GetVrmMaterialColorTargetIndicesAttr().Clear());
    CHECK(api.GetVrmMaterialColorTypesAttr().Set(pxr::VtTokenArray{pxr::TfToken("color")}));
    CHECK(api.GetVrmMaterialColorValuesAttr().Set(pxr::VtVec4fArray{pxr::GfVec4f(1)}));
    CHECK(api.CreateVrmIsBinaryAttr().Set(true));
    ExpressionBinding legacy(s,config()); CHECK(legacy.Rig().Find("happy")->isBinary);
    evaluate(legacy,legacy.AdapterConfig("legacy",{}));
    // Forwarding preserves the parallel arrays and relocates with references.
    auto relay = api.GetPrim().CreateRelationship(pxr::TfToken("relay"),false);
    CHECK(relay.SetTargets({pxr::SdfPath("/Avatar/Shapes/Smile")})); CHECK(api.GetVrmMorphTargetsRel().SetTargets({relay.GetPath()}));
    ExpressionBinding forwarded(s,config());
    auto referenced = pxr::UsdStage::CreateInMemory();
    CHECK(pxr::UsdGeomSetStageUpAxis(referenced,pxr::UsdGeomTokens->y)); CHECK(pxr::UsdGeomSetStageMetersPerUnit(referenced,1));
    CHECK(referenced->DefinePrim(pxr::SdfPath("/Placed")).GetReferences().AddReference(s->GetRootLayer()->GetIdentifier(),pxr::SdfPath("/Avatar")));
    auto rc = config(); rc.humanoid.avatarRoot = pxr::SdfPath("/Placed"); ExpressionBinding relocated(referenced,rc);
    CHECK(std::string(relocated.Baseline().blend_shapes[0].mesh_id) == "/Placed/Face");
    s.Reset(); referenced.Reset(); evaluate(copy,c); evaluate(b,composed);
}
template<class F> void reject(F mutate, const char* code) {
    auto s = stage(); auto c = config(); mutate(s,c); bool threw = false;
    try { ExpressionBinding b(s,c); } catch (const std::invalid_argument& e) { threw = true; CHECK(std::string(e.what()).find(code) == 0); }
    CHECK(threw);
}
void invalid() {
    reject([](auto s,auto&) { expression(s).GetVrmExpressionNameAttr().Set(pxr::TfToken("blink")); },"VRM_EXPRESSION_BINDING_NAME");
    reject([](auto s,auto&) { expression(s).GetVrmExpressionNameAttr().Set(pxr::TfToken()); },"VRM_EXPRESSION_BINDING_NAME");
    reject([](auto s,auto&) { expression(s).CreateVrmOverrideBlinkAttr().Set(pxr::TfToken("unknown")); },"VRM_EXPRESSION_BINDING_OVERRIDE");
    reject([](auto s,auto&) { expression(s).GetVrmMorphTargetWeightsAttr().Set(pxr::VtFloatArray{}); },"VRM_EXPRESSION_BINDING_MORPH_ARRAYS");
    reject([](auto s,auto&) { expression(s).GetVrmMorphTargetWeightsAttr().Set(pxr::VtFloatArray{std::numeric_limits<float>::infinity()}); },"VRM_EXPRESSION_BINDING_FINITE");
    reject([](auto s,auto&) { s->RemovePrim(pxr::SdfPath("/Avatar/Face")); },"VRM_EXPRESSION_BINDING_MORPH_MAPPING");
    reject([](auto s,auto&) { auto mesh = pxr::UsdGeomMesh::Define(s,pxr::SdfPath("/Avatar/Other")); auto api = pxr::UsdSkelBindingAPI::Apply(mesh.GetPrim()); api.CreateBlendShapesAttr().Set(pxr::VtTokenArray{pxr::TfToken("same")}); api.CreateBlendShapeTargetsRel().SetTargets({pxr::SdfPath("/Avatar/Shapes/Smile")}); },"VRM_EXPRESSION_BINDING_MORPH_MAPPING");
    reject([](auto s,auto&) { expression(s).GetVrmMaterialColorTargetIndicesAttr().Set(pxr::VtIntArray{5,0,0,0,0,0}); },"VRM_EXPRESSION_BINDING_MATERIAL_INDEX");
    reject([](auto s,auto&) { expression(s).GetVrmMaterialColorTypesAttr().Set(pxr::VtTokenArray{}); },"VRM_EXPRESSION_BINDING_MATERIAL_ARRAYS");
    reject([](auto s,auto&) { expression(s).GetVrmMaterialColorValuesAttr().Set(pxr::VtVec4fArray(6,pxr::GfVec4f(std::numeric_limits<float>::quiet_NaN()))); },"VRM_EXPRESSION_BINDING_FINITE");
    reject([](auto s,auto&) { expression(s).GetVrmMaterialColorTypesAttr().Set(pxr::VtTokenArray(6,pxr::TfToken("unknown"))); },"VRM_EXPRESSION_BINDING_SLOT");
    reject([](auto s,auto&) { s->GetPrimAtPath(pxr::SdfPath("/Avatar/Material")).RemoveAPI(pxr::TfToken("VrmMToonAPI")); },"VRM_EXPRESSION_BINDING_MATERIAL_SCHEMA");
    reject([](auto s,auto&) { s->GetPrimAtPath(pxr::SdfPath("/Avatar/Material")).GetAttribute(pxr::TfToken("inputs:vrm:material:baseColorFactor")).SetConnections({pxr::SdfPath("/Avatar/Material.source")}); },"VRM_EXPRESSION_BINDING_CONNECTED");
    reject([](auto s,auto&) { s->GetPrimAtPath(pxr::SdfPath("/Avatar/Material")).GetAttribute(pxr::TfToken("inputs:vrm:material:baseColorFactor")).Block(); },"VRM_EXPRESSION_BINDING_VALUE");
    reject([](auto s,auto&) { pxr::UsdSkelBindingAPI(s->GetPrimAtPath(pxr::SdfPath("/Avatar/Face"))).GetBlendShapesAttr().Set(pxr::VtTokenArray{}); },"VRM_EXPRESSION_BINDING_MESH_ARRAYS");
    reject([](auto s,auto&) { expression(s).GetVrmMorphTargetsRel().SetTargets({pxr::SdfPath("/Outside")}); },"VRM_EXPRESSION_BINDING_TARGET");
    reject([](auto,auto& c) { c.expressionsRoot = pxr::SdfPath("/Other"); },"VRM_EXPRESSION_BINDING_PATH");
    reject([](auto,auto& c) { c.expressionsRoot = pxr::SdfPath("/Avatar/Human"); },"VRM_EXPRESSION_BINDING_MISSING");
    auto s = stage(); ExpressionBinding b(s,config()); auto c = b.AdapterConfig("x",{}); c.layoutVersion++;
    bool threw = false; try { b.ApplyTo(c); } catch (const std::invalid_argument&) { threw = true; } CHECK(threw);
}
} // namespace
int main(int argc, char** argv) {
    valid(); invalid();
    if (argc == 2) {
        auto s = pxr::UsdStage::Open(argv[1]); CHECK(s);
        auto c = config(); c.humanoid.avatarRoot = s->GetDefaultPrim().GetPath();
        ExpressionBinding b(s,c);
        std::cout << "real-avatar expressions=" << b.Rig().GetSize() << " morphs=" << b.Baseline().blend_shape_count << " materials=" << b.Baseline().material_count << '\n';
        auto look = avatarVrmUsd::LookAtBinding(s,{c.humanoid,{}}).AdapterConfig("real.gaze",{"test","actor","gaze:point"});
        b.ApplyTo(look); s.Reset(); evaluate(b,look);
    }
    std::cout << "Expression USD binding contracts passed\n";
}
