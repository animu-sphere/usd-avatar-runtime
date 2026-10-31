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
    // Native inputs as a VRMA import authors them, keyed between the body keys. Nothing
    // reads them unless selected.
    const auto expression = [&](const char* name, std::initializer_list<std::pair<int,float>> keys) {
        auto prim = s->DefinePrim(SdfPath(std::string("/Clip/Expressions/")+name));
        prim.CreateAttribute(TfToken("vrm:expressionName"),SdfValueTypeNames->Token,false,SdfVariabilityUniform).Set(TfToken(name));
        auto weight = prim.CreateAttribute(TfToken("vrm:expressionWeight"),SdfValueTypeNames->Float,false);
        for (const auto& [time,value] : keys) weight.Set(value,double(time));
    };
    expression("happy",{{0,0.f},{20,1.f},{60,.25f}}); expression("blink",{{30,.5f},{45,1.f}});
    auto gaze = s->DefinePrim(SdfPath("/Clip/LookAt")).CreateAttribute(TfToken("vrm:lookAtTarget"),SdfValueTypeNames->Point3f,false);
    gaze.Set(GfVec3f(-1,1.2f,2),0.); gaze.Set(GfVec3f(1,1.6f,2),40.); gaze.Set(GfVec3f(0,1.4f,2),60.);
    return s;
}
openstrata::motion::MotionStageReadOptions clipInputs() {
    openstrata::motion::MotionStageReadOptions o;
    for (const char* name : {"happy","blink"}) {
        const auto prim = std::string("/Clip/Expressions/")+name;
        o.channels.push_back({prim+".vrm:expressionName",prim+".vrm:expressionWeight","vrm:"});
    }
    o.lookAtTargetAttributePath = "/Clip/LookAt.vrm:lookAtTarget";
    return o;
}
const ArSourceSample* findSample(const ArStateView& v, const std::string& channel) {
    for (uint32_t i = 0; i < v.sample_count; ++i) if (channel == v.samples[i].channel_id) return &v.samples[i];
    return nullptr;
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
        // Clip-authored expressions and gaze, against the owner, past the end of the clip.
        const auto inputs = clipInputs();
        for (Gaze gaze : {Gaze::bone, Gaze::expression}) {
            const auto avatarPath = (std::filesystem::path(argv[1]) / (gaze == Gaze::bone ? "bone.usda" : "expression.usda")).string();
            for (bool springs : {true,false}) {
                std::vector<std::string> args{"check",avatarPath,motionPath,"0.016666666666666666","90"};
                for (const auto& channel : inputs.channels) {
                    args.push_back("--clip-expression"); args.push_back(channel.nameAttributePath); args.push_back(channel.valueAttributePath);
                }
                args.push_back("--clip-gaze"); args.push_back(inputs.lookAtTargetAttributePath);
                if (!springs) args.push_back("--no-springs");
                std::vector<char*> pointers; for (auto& arg : args) pointers.push_back(arg.data());
                require(avatarVrmSessionCheck(int(pointers.size()),pointers.data()) == 0, "generated clip face check");
            }
        }
        const auto selected = avatarMotionUsd::ReadStageClip(source,SdfPath("/Clip/Body"),inputs);
        ArDiagnosticSink quiet{}; quiet.emit = [](void*,const ArDiagnostic*) {};
        const auto clipConfig = [&](uint32_t budget) {
            avatarVrmSession::SessionConfig c; c.avatar = identity; c.clock = {0,1./60,budget};
            c.expressions = c.lookAt = c.clipGaze = true;
            c.clipExpressions = {{"vrm:happy","happy"},{"vrm:blink","blink"}};
            return c;
        };
        const auto state = [](const avatarVrmSession::Session& s) {
            ArStateView v{AR_HEADER(ArStateView)}; require(s.Api().get_snapshot(s.Snapshot(),&v) == AR_OK, "snapshot");
            return v;
        };
        {
            // Catch-up samples each tick: the request size cannot change the result.
            avatarVrmSession::Session single(avatar(true,Gaze::bone,true),selected,clipConfig(1));
            avatarVrmSession::Session chunked(avatar(true,Gaze::bone,true),selected,clipConfig(4));
            require(single.Advance(0,&quiet) == AR_OK && chunked.Advance(0,&quiet) == AR_OK, "clip seed");
            single.Play(); chunked.Play();
            for (int request = 1; request <= 24; ++request) {
                for (int tick = 0; tick < 4; ++tick) require(single.Advance(1./60,&quiet) == AR_OK, "single tick");
                require(chunked.Advance(4./60,&quiet) == AR_OK && chunked.Stats().tickCount == 4, "catch-up request");
                const auto a = state(single), b = state(chunked);
                require(a.evaluation_seconds == b.evaluation_seconds && a.blend_shape_count == 1 && b.blend_shape_count == 1 &&
                    a.blend_shapes[0].weight == b.blend_shapes[0].weight, "catch-up morph");
                for (uint32_t i = 0; i < a.joint_count; ++i) for (int k = 0; k < 4; ++k)
                    require(a.joints[i].local.rotation[k] == b.joints[i].local.rotation[k], "catch-up joint");
            }
            // The snapshot names the clip sample used, with the boundary's time once held.
            const auto last = state(chunked);
            require(last.evaluation_seconds > 1 && last.input_revision == last.frame_id, "held tick identity");
            const auto* happy = findSample(last,"clip:vrm:happy"); const auto* look = findSample(last,"motion:lookAtTarget");
            require(happy && happy->source_seconds == 1 && std::string(happy->source_id) == "vrm.session.motion", "held scalar sample");
            require(look && look->source_seconds == 1 && look->validity == AR_OBSERVATION_STALE, "held gaze sample");
            single.Close(); chunked.Close();
            // Inside the clip the gaze is valid and each sample carries its own tick time.
            avatarVrmSession::Session inside(avatar(true,Gaze::bone,true),selected,clipConfig(4));
            require(inside.Advance(0,&quiet) == AR_OK, "inside seed"); inside.Play();
            require(inside.Advance(.5,&quiet) == AR_OK && inside.Stats().tickCount == 4, "inside catch-up");
            const auto v = state(inside);
            look = findSample(v,"motion:lookAtTarget");
            require(look && look->source_seconds == v.evaluation_seconds && look->validity == AR_OBSERVATION_VALID, "valid gaze sample");
            require(findSample(v,"clip:vrm:happy") && findSample(v,"clip:vrm:happy")->source_seconds == v.evaluation_seconds, "scalar sample time");
            require(!findSample(v,"clip:vrm:blink"), "a channel before its first key is absent");
            // A paused request selects and evaluates nothing, and reset replays from the clip.
            const auto kept = inside.Snapshot(); inside.Pause();
            require(inside.Advance(10,&quiet) == AR_OK && inside.Snapshot() == kept, "paused clip input");
            require(inside.Reset(0) == AR_OK && inside.Advance(0,&quiet) == AR_OK && state(inside).frame_id == 1, "reset clip input");
        }
        {
            // The host still supplies what the clip does not: one source per input.
            auto c = clipConfig(2); c.clipExpressions = {{"vrm:happy","happy"}}; c.expressionInputs = {{{"host","actor","expr:blink"},"blink"}};
            avatarVrmSession::Session mixed(avatar(true,Gaze::bone,true),selected,c);
            ArScalarInput blink{"host","actor","expr:blink",1.}; blink.clock_scale = 1;
            ArInputFrame frame{AR_HEADER(ArInputFrame)}; frame.scalars = &blink; frame.scalar_count = 1; frame.input_revision = 7;
            require(mixed.Advance(0,frame,&quiet) == AR_OK, "mixed seed");
            const auto v = state(mixed);
            require(findSample(v,"clip:vrm:happy") && findSample(v,"expr:blink") && v.input_revision == v.frame_id, "mixed samples");
            require(v.blend_shapes[0].weight > 0, "host expression at a clip key with no clip weight");
        }
        {
            const auto bad = [&](avatarVrmSession::SessionConfig c, const openstrata::motion::MotionStageRead& r, const char* message) {
                try { avatarVrmSession::Session unused(avatar(true,Gaze::bone,true),r,c); }
                catch (const std::invalid_argument&) { return; }
                throw std::runtime_error(message);
            };
            auto c = clipConfig(2); c.expressions = false; c.lookAt = false; c.clipGaze = false;
            bad(c,selected,"clip expressions without expressions");
            c = clipConfig(2); c.lookAt = false; c.clipExpressions.clear();
            bad(c,selected,"clip gaze without LookAt");
            c = clipConfig(2); c.gaze = {"host","actor","gaze:point"};
            bad(c,selected,"gaze from both the clip and the host");
            c = clipConfig(2); c.expressionInputs = {{{"host","actor","expr:happy"},"happy"}};
            bad(c,selected,"expression from both the clip and the host");
            c = clipConfig(2); c.clipExpressions = {{"vrm:happy","undeclared"}};
            bad(c,selected,"clip expression the avatar does not declare");
            c = clipConfig(2); c.clipExpressions = {{"vrm:absent","happy"}};
            bad(c,selected,"clip channel the clip does not author");
            c = clipConfig(2); c.clipExpressions.clear();
            bad(c,read,"clip gaze the clip does not author");
        }
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
