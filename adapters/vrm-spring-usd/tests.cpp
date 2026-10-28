#include "avatarVrmUsd/SpringBoneBinding.h"
#include "pxr/usd/sdf/types.h"
#include "pxr/usd/usdGeom/metrics.h"
#include "pxr/usd/usdGeom/scope.h"
#include "pxr/usd/usdGeom/tokens.h"
#include "pxr/usd/usdGeom/xform.h"
#include "pxr/usd/usdSkel/skeleton.h"
#include "vrmSchema/vrmColliderAPI.h"
#include "vrmSchema/vrmSpringBoneAPI.h"
#include <cmath>
#include <cstdlib>
#include <iostream>

#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            std::cerr << "Line " << __LINE__ << ": " << #x << '\n';                                \
            std::exit(1);                                                                          \
        }                                                                                          \
    } while (0)
namespace {
using namespace pxr;
avatarUsd::SkeletonBindingConfig config() {
    return {SdfPath("/Avatar"), SdfPath("/Avatar/Body"), "spring.layout", 1, {}};
}
UsdVrmSpringBoneAPI spring(const UsdStagePtr &s) {
    return UsdVrmSpringBoneAPI(s->GetPrimAtPath(SdfPath("/Avatar/Secondary/Hair")));
}
UsdVrmColliderAPI collider(const UsdStagePtr &s) {
    return UsdVrmColliderAPI(s->GetPrimAtPath(SdfPath("/Avatar/Collision/Head/Collider")));
}
UsdStageRefPtr stage(bool roots = false, bool capsule = false) {
    auto s = UsdStage::CreateInMemory();
    CHECK(UsdGeomSetStageUpAxis(s, UsdGeomTokens->y));
    CHECK(UsdGeomSetStageMetersPerUnit(s, 1));
    auto root = UsdGeomXform::Define(s, SdfPath("/Avatar"));
    root.GetPrim().SetCustomDataByKey(TfToken("vrm:schemaContractVersion"), VtValue(2));
    CHECK(root.AddTranslateOp().Set(GfVec3d(3, 4, 5)));
    CHECK(root.AddRotateYOp().Set(30.0f));
    auto skel = UsdSkelSkeleton::Define(s, SdfPath("/Avatar/Body"));
    CHECK(skel.CreateJointsAttr().Set(VtTokenArray{TfToken("Root"), TfToken("Root/Hair"),
                                                   TfToken("Root/Hair/Tip"),
                                                   TfToken("Root/Hair/Side")}));
    GfMatrix4d rootRest(1), hair(1), tip(1), side(1);
    rootRest.SetTranslate(GfVec3d(0, 0.8, 0));
    hair.SetTranslate(GfVec3d(0, 0.2, 0));
    tip.SetTranslate(GfVec3d(0.1, 0.2, 0));
    side.SetTranslate(GfVec3d(-0.1, 0.1, 0));
    CHECK(skel.CreateRestTransformsAttr().Set(VtMatrix4dArray{rootRest, hair, tip, side}));
    UsdGeomScope::Define(s, SdfPath("/Avatar/Secondary"));
    auto p = UsdVrmSpringBoneAPI::Apply(
        UsdGeomScope::Define(s, SdfPath("/Avatar/Secondary/Hair")).GetPrim());
    const VtTokenArray names = roots ? VtTokenArray{TfToken("Root/Hair")}
                                     : VtTokenArray{TfToken("Root/Hair"), TfToken("Root/Hair/Tip")};
    CHECK((roots ? p.CreateVrmRootJointsAttr() : p.CreateVrmJointsAttr()).Set(names));
    const size_t count = names.size();
    CHECK(p.CreateVrmStiffnessAttr().Set(VtFloatArray(count, 0.6f)));
    CHECK(p.CreateVrmDragForceAttr().Set(VtFloatArray(count, 0.3f)));
    CHECK(p.CreateVrmGravityPowerAttr().Set(VtFloatArray(count, 0.4f)));
    CHECK(p.CreateVrmGravityDirAttr().Set(VtVec3fArray(count, GfVec3f(0, -1, 0))));
    CHECK(p.CreateVrmHitRadiusAttr().Set(VtFloatArray(count, 0.02f)));
    CHECK(p.CreateVrmCenterAttr().Set(TfToken("Root")));
    UsdGeomScope::Define(s, SdfPath("/Avatar/Collision"));
    UsdGeomScope::Define(s, SdfPath("/Avatar/Collision/Head"));
    auto c = UsdVrmColliderAPI::Apply(
        UsdGeomScope::Define(s, SdfPath("/Avatar/Collision/Head/Collider")).GetPrim());
    CHECK(c.CreateVrmNodeAttr().Set(TfToken("Root")));
    CHECK(c.CreateVrmShapeAttr().Set(TfToken(capsule ? "capsule" : "sphere")));
    CHECK(c.CreateVrmOffsetAttr().Set(GfVec3f(0.1f, 0.1f, 0.2f)));
    CHECK(c.CreateVrmRadiusAttr().Set(0.03f));
    if (capsule)
        CHECK(c.CreateVrmTailAttr().Set(GfVec3f(0.1f, 0.3f, 0.2f)));
    CHECK(p.CreateVrmColliderGroupsRel().SetTargets({SdfPath("/Avatar/Collision/Head")}));
    return s;
}
void fails(const UsdStagePtr &s, const char *code) {
    try {
        avatarVrmUsd::SpringBoneBinding binding(s, config());
        CHECK(false);
    } catch (const std::invalid_argument &e) {
        CHECK(std::string(e.what()).find(code) != std::string::npos);
    }
}
void mapping(bool roots, bool capsule) {
    auto s = stage(roots, capsule);
    avatarVrmUsd::SpringBoneBinding binding(s, config());
    const auto &rig = binding.Rig();
    CHECK(rig.parents == std::vector<int>({-1, 0, 1, 1}));
    CHECK(rig.springs.size() == 1 && rig.colliders.size() == 1 && rig.colliderGroups.size() == 1);
    const auto &p = rig.springs[0];
    CHECK(p.center == 0 && p.colliderGroups == std::vector<int>({0}));
    CHECK(roots ? p.rootJoints == std::vector<int>({1}) && p.joints.empty()
                : p.joints == std::vector<int>({1, 2}) && p.rootJoints.empty());
    CHECK(p.parameters.size() == (roots ? 1 : 2));
    for (const auto &parameter : p.parameters) {
        CHECK(parameter.stiffness == 0.6f && parameter.dragForce == 0.3f &&
              parameter.gravityPower == 0.4f);
        CHECK(parameter.hitRadius == 0.02f && parameter.gravityDirection == GfVec3f(0, -1, 0));
    }
    const auto &c = rig.colliders[0];
    CHECK(c.joint == 0 && c.offset == GfVec3f(0.1f, 0.1f, 0.2f) && c.radius == 0.03f);
    CHECK(c.shape == (capsule ? vrmRig::SpringBoneColliderShape::Capsule
                              : vrmRig::SpringBoneColliderShape::Sphere));
    if (capsule)
        CHECK(c.tail == GfVec3f(0.1f, 0.3f, 0.2f));
    CHECK(binding.SpringIds() == std::vector<std::string>({"/Avatar/Secondary/Hair"}));
    CHECK(binding.ColliderIds() == std::vector<std::string>({"/Avatar/Collision/Head/Collider"}));
    CHECK(binding.ColliderGroupIds() == std::vector<std::string>({"/Avatar/Collision/Head"}));
    CHECK(binding.RestPose().localTransforms[0].translation == GfVec3f(0, 0.8f, 0));
    CHECK(binding.RestPose().placement.translation == GfVec3f(3, 4, 5));
    CHECK(std::abs(binding.RestPose().placement.rotation.GetReal() - std::cos(3.14159265f / 12)) <
          1e-6f);
    auto copy = binding;
    s->RemovePrim(SdfPath("/Avatar"));
    s.Reset();
    CHECK(copy.Skeleton().JointIds()[1] == "Root/Hair");
    // Construct a separate owner rig from known fixture values, including a
    // rotated placement. This detects double-applied placement and root expansion.
    vrmRig::SpringBoneRig control = rig;
    control.parents = {-1, 0, 1, 1};
    control.restTransforms = {{{0, 0.8f, 0}, GfQuatf(1)},
                              {{0, 0.2f, 0}, GfQuatf(1)},
                              {{0.1f, 0.2f, 0}, GfQuatf(1)},
                              {{-0.1f, 0.1f, 0}, GfQuatf(1)}};
    vrmRig::SpringBonePose controlPose{
        control.restTransforms,
        {{3, 4, 5},
         GfQuatf(std::cos(3.14159265f / 12), GfVec3f(0, std::sin(3.14159265f / 12), 0))}};
    vrmRig::SpringBoneSolver solver, direct;
    vrmRig::SpringBoneResult result, expected;
    CHECK(solver.Configure(copy.Rig()) && direct.Configure(control));
    CHECK(solver.Reset(copy.RestPose(), &result) && direct.Reset(controlPose, &expected));
    for (int i = 0; i < 12; ++i) {
        CHECK(solver.Step(1.0f / 60, copy.RestPose(), &result));
        CHECK(direct.Step(1.0f / 60, controlPose, &expected));
        CHECK(result.tails.size() == (roots ? 3 : 1));
        for (size_t j = 0; j < result.tails.size(); ++j) {
            CHECK(result.tails[j].joint == expected.tails[j].joint);
            CHECK((result.tails[j].position - expected.tails[j].position).GetLength() < 1e-5f);
        }
        for (size_t j = 0; j < result.localRotations.size(); ++j)
            CHECK(std::abs(GfDot(result.localRotations[j], expected.localRotations[j])) >
                  1 - 1e-5f);
    }
}
} // namespace
int runContracts() {
    for (bool roots : {false, true})
        for (bool capsule : {false, true})
            mapping(roots, capsule);
    {
        auto s = stage();
        spring(s).CreateVrmRootJointsAttr().Set(VtTokenArray{});
        fails(s, "DECLARATION");
    }
    {
        auto s = stage();
        spring(s).GetPrim().RemoveProperty(TfToken("vrm:joints"));
        fails(s, "DECLARATION");
    }
    {
        auto s = stage();
        spring(s).CreateVrmJointsAttr().Set(VtTokenArray{TfToken("missing")});
        fails(s, "JOINT");
    }
    {
        auto s = stage();
        spring(s).CreateVrmCenterAttr().Set(TfToken("missing"));
        fails(s, "JOINT");
    }
    {
        auto s = stage();
        collider(s).CreateVrmNodeAttr().Set(TfToken("missing"));
        fails(s, "JOINT");
    }
    {
        auto s = stage();
        spring(s).CreateVrmDragForceAttr().Set(VtFloatArray{0.3f});
        fails(s, "PARAMETERS");
    }
    {
        auto s = stage();
        spring(s).CreateVrmColliderGroupsRel().SetTargets({SdfPath("/Avatar/Missing")});
        fails(s, "GROUP");
    }
    {
        auto s = stage();
        spring(s).CreateVrmColliderGroupsRel().SetTargets({SdfPath("/Elsewhere")});
        fails(s, "GROUP");
    }
    {
        auto s = stage();
        collider(s).GetPrim().RemoveAPI<UsdVrmColliderAPI>();
        fails(s, "COLLIDER_SCHEMA");
    }
    {
        auto s = stage();
        collider(s).CreateVrmShapeAttr().Set(TfToken("plane"));
        fails(s, "COLLIDER_SHAPE");
    }
    {
        auto s = stage(false, true);
        collider(s).GetPrim().RemoveProperty(TfToken("vrm:tail"));
        fails(s, "VALUE");
    }
    {
        auto s = stage();
        UsdGeomSetStageMetersPerUnit(s, 0.01);
        fails(s, "UNITS");
    }
    {
        auto s = stage();
        UsdGeomXform(s->GetPrimAtPath(SdfPath("/Avatar"))).AddScaleOp().Set(GfVec3f(2));
        fails(s, "PLACEMENT");
    }
    {
        auto s = stage();
        s->GetPrimAtPath(SdfPath("/Avatar"))
            .SetCustomDataByKey(TfToken("vrm:schemaContractVersion"), VtValue(1));
        fails(s, "CONTRACT");
    }
    {
        auto s = stage();
        s->RemovePrim(SdfPath("/Avatar/Secondary"));
        fails(s, "MISSING");
    }
    {
        auto s = stage();
        UsdVrmSpringBoneAPI::Apply(s->DefinePrim(SdfPath("/Unrelated")));
        avatarVrmUsd::SpringBoneBinding b(s, config());
        CHECK(b.Rig().springs.size() == 1);
    }
    {
        auto s = stage();
        auto skel = UsdSkelSkeleton(s->GetPrimAtPath(SdfPath("/Avatar/Body")));
        VtMatrix4dArray rest;
        CHECK(skel.GetRestTransformsAttr().Get(&rest));
        GfMatrix4d scaled(1);
        scaled.SetScale(GfVec3d(2));
        rest[1] = scaled;
        CHECK(skel.GetRestTransformsAttr().Set(rest));
        fails(s, "SCALE");
    }
    {
        auto s = stage();
        spring(s).GetVrmDragForceAttr().Block();
        fails(s, "VALUE");
    }
    {
        auto s = stage();
        spring(s).GetVrmColliderGroupsRel().ClearTargets(true);
        spring(s).GetPrim().RemoveProperty(TfToken("vrm:center"));
        avatarVrmUsd::SpringBoneBinding b(s, config());
        CHECK(b.Rig().colliders.empty() && b.Rig().springs[0].center == -1);
    }
    {
        auto s = stage();
        auto forward =
            s->GetPrimAtPath(SdfPath("/Avatar")).CreateRelationship(TfToken("selectedGroups"));
        forward.SetTargets({SdfPath("/Avatar/Collision/Head")});
        spring(s).GetVrmColliderGroupsRel().SetTargets({forward.GetPath()});
        avatarVrmUsd::SpringBoneBinding b(s, config());
        CHECK(b.Rig().colliderGroups.size() == 1);
    }
    {
        auto s = stage();
        auto alias = s->DefinePrim(SdfPath("/Copy"));
        alias.GetReferences().AddInternalReference(SdfPath("/Avatar"));
        auto c = config();
        c.avatarRoot = SdfPath("/Copy");
        c.skeleton = SdfPath("/Copy/Body");
        avatarVrmUsd::SpringBoneBinding b(s, c);
        CHECK(b.ColliderGroupIds()[0] == "/Copy/Collision/Head");
    }
    {
        auto s = stage();
        spring(s).CreateVrmStiffnessAttr().Set(VtFloatArray(2, -1));
        std::string detail;
        try {
            avatarVrmUsd::SpringBoneBinding b(s, config());
            CHECK(false);
        } catch (const avatarVrmUsd::SpringBoneOwnerError &e) {
            CHECK(std::string(e.Owner()) == "vrmRig" && !e.OwnerVersion().empty());
            CHECK(e.Subject() == "/Avatar");
            detail = e.Detail();
            CHECK(detail.find("invalid joint or parameters") != std::string::npos);
        }
        s.Reset();
        CHECK(!detail.empty());
    }
    {
        auto s = stage();
        spring(s).CreateVrmJointsAttr().Set(
            VtTokenArray{TfToken("Root/Hair/Tip"), TfToken("Root/Hair")});
        fails(s, "OWNER");
    }
    {
        auto s = stage();
        auto alias = s->DefinePrim(SdfPath("/Avatar/Secondary/Duplicate"));
        alias.GetReferences().AddInternalReference(SdfPath("/Avatar/Secondary/Hair"));
        fails(s, "OWNER");
    }
    std::cout << "VRM spring stage binding contracts passed\n";
    return 0;
}
int main() {
    try {
        return runContracts();
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
