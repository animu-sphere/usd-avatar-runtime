#include <avatarUsd/SkeletonPoseOutput.h>
#include <pxr/usd/usdGeom/metrics.h>
#include <pxr/usd/usd/editContext.h>
#include <pxr/usd/usdGeom/xform.h>
#include <pxr/usd/usdSkel/animation.h>
#include <pxr/usd/usdSkel/bindingAPI.h>
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
    std::string original; stage->GetRootLayer()->ExportToString(&original);
    auto foreign = SdfLayer::CreateAnonymous("other-session-data.usda");
    stage->GetSessionLayer()->SetSubLayerPaths({foreign->GetIdentifier()});
    stage->GetSessionLayer()->SetSubLayerOffset(SdfLayerOffset(12, 2), 0);
    avatarUsd::SkeletonBinding binding(stage, {SdfPath("/Avatar"), skeleton.GetPath(), "layout", 1, {}});
    ArRuntimeApi api{}; CHECK(arGetApi(AR_ABI_VERSION, sizeof(api), &api) == AR_OK);
    ArRuntime runtime = 0; CHECK(api.create_runtime(&runtime) == AR_OK);
    ArInstanceDesc desc{AR_HEADER(ArInstanceDesc)};
    desc.generation = 1; desc.layout_id = "layout"; desc.layout_version = 1; desc.initial_state = binding.Baseline();
    ArInstance instance = 0; CHECK(api.create_instance(runtime, &desc, nullptr, &instance) == AR_OK);
    ArInputFrame input{AR_HEADER(ArInputFrame)}; input.generation = 1; input.frame_id = 1;
    ArSnapshot snapshot = 0; CHECK(api.evaluate_frame(runtime, instance, &input, nullptr, &snapshot) == AR_OK);
    CHECK(api.destroy_runtime(runtime) == AR_OK); // Publication reads an independently retained CPU value.
    ArStateView view{AR_HEADER(ArStateView)}; CHECK(api.get_snapshot(snapshot, &view) == AR_OK);
    avatarUsd::SkeletonPoseOutput output(stage, binding);
    output.Apply(api, snapshot);
    CHECK(stage->GetEditTarget().GetLayer() == stage->GetRootLayer());
    auto query = UsdSkelCache().GetSkelQuery(skeleton);
    CHECK(query && query.GetAnimQuery());
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
    CHECK(stage->GetSessionLayer()->GetSubLayerPaths().size() == 1);
    CHECK(stage->GetSessionLayer()->GetSubLayerPaths()[0] == foreign->GetIdentifier());
    CHECK(stage->GetSessionLayer()->GetSubLayerOffset(0) == SdfLayerOffset(12, 2));
    SdfPathVector targets; CHECK(UsdSkelBindingAPI(skeleton).GetAnimationSourceRel().GetTargets(&targets));
    CHECK(targets == SdfPathVector{previous.GetPath()});
    std::string after; stage->GetRootLayer()->ExportToString(&after); CHECK(after == original);
    bool closed = false; try { output.Apply(view); } catch (const std::invalid_argument&) { closed = true; } CHECK(closed);
    CHECK(api.get_snapshot(snapshot, &bad) == AR_OK && api.release_snapshot(snapshot) == AR_OK);
}
