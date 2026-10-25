"""Generated VMD vertex weights + Jolt feedback -> final UsdSkel deformation."""
import math
import sys

from pxr import Gf, Usd, UsdGeom, UsdSkel
from usdviewMmdPhysics import _native


stage = Usd.Stage.Open(sys.argv[1])
original = stage.GetRootLayer().ExportToString()
session_layer = stage.GetSessionLayer().ExportToString()
skeleton = UsdSkel.Skeleton.Get(stage, "/Asset/skel/Skeleton")
mesh = stage.GetPrimAtPath("/Asset/geo/Mesh")
baseline = list(UsdGeom.Mesh(mesh).GetPointsAttr().Get())
max_error = 0.
for motion, constant in ((sys.argv[2], False), (sys.argv[3], True), (sys.argv[4], False)):
    session = _native.Session(stage, sys.argv[1], motion)
    replay = []
    try:
        for generation in (1, 2, 3):
            session.play()
            for tick in range(240):
                session.advance(.0083334)
                assert session.stats["steps"] == tick + 1
                cache = UsdSkel.Cache()
                cache.Populate(UsdSkel.Root.Get(stage, "/Asset"), Usd.PrimDefaultPredicate)
                query = cache.GetSkelQuery(skeleton)
                anim = query.GetAnimQuery()
                weights = anim.ComputeBlendShapeWeights()
                assert list(anim.GetBlendShapeOrder()) == ["wide"]
                seconds = (tick + 1) / 120
                # Direct or resolved mixed weight peaks at .5; the group-only track holds .5.
                expected_weight = .5 if constant else min(seconds, max(0., 1 - seconds))
                assert len(weights) == 1 and abs(weights[0] - expected_weight) < 1e-6
                skin = cache.GetSkinningQuery(mesh)
                mapped = skin.GetBlendShapeMapper().Remap(weights)
                shapes = UsdSkel.BlendShapeQuery(UsdSkel.BindingAPI(mesh))
                sub_weights, shape_indices, sub_indices = shapes.ComputeSubShapeWeights(mapped)
                points = UsdGeom.Mesh(mesh).GetPointsAttr().Get()
                assert shapes.ComputeDeformedPoints(sub_weights, shape_indices, sub_indices,
                                                    shapes.ComputeBlendShapePointIndices(),
                                                    shapes.ComputeSubShapePointOffsets(), points)
                assert skin.ComputeSkinnedPoints(query.ComputeSkinningTransforms(), points)
                bodies = session.bodyStates
                for row in bodies.values():
                    assert all(math.isfinite(v) for field in row.values() for v in field)
                anchor = Gf.Vec3d(*bodies["/Asset/physics/rigidBodies/anchor"]["translation"])
                swing = bodies["/Asset/physics/rigidBodies/swing"]["rotation"]
                rotation = Gf.Rotation(Gf.Quatd(swing[3], Gf.Vec3d(*swing[:3])).GetNormalized())
                for i, (source, actual) in enumerate(zip(baseline, points)):
                    delta = Gf.Vec3d(.16 * expected_weight if i in (1, 2) else 0, 0, 0)
                    expected = anchor + rotation.TransformDir(Gf.Vec3d(source) + delta - Gf.Vec3d(0, 1.6, 0))
                    error = (Gf.Vec3d(actual) - expected).GetLength()
                    max_error = max(max_error, error)
                    assert error < 2e-5, (tick, i, error)
                value = (list(weights), list(points), bodies)
                if generation == 1:
                    replay.append(value)
                else:
                    assert value == replay[tick], (generation, tick)
                assert list(UsdGeom.Mesh(mesh).GetPointsAttr().Get()) == baseline
                animation_path = UsdSkel.BindingAPI(skeleton).GetAnimationSourceRel().GetTargets()[0]
                animation = UsdSkel.Animation.Get(stage, animation_path)
                assert not animation.GetBlendShapeWeightsAttr().GetTimeSamples()
                if tick == 59:
                    session.pause()
                    session.advance(1)
                    assert session.stats["steps"] == 60 and session.bodyStates == bodies
                    assert list(animation.GetBlendShapeWeightsAttr().Get()) == list(weights)
                    session.play()
            assert abs(replay[59][0][0] - .5) < 1e-6
            assert replay[119][0] == replay[239][0] == ([.5] if constant else [0.])
            session.pause()
            session.advance(10)
            assert session.stats["steps"] == 240
            if generation < 3:
                session.reset()
                assert session.stats["generation"] == generation + 1
                assert session.stats["steps"] == 0 and session.stats["paused"]
                assert stage.GetSessionLayer().ExportToString() == session_layer
    finally:
        session.close()
    assert stage.GetRootLayer().ExportToString() == original
    assert stage.GetSessionLayer().ExportToString() == session_layer

# A declared unsupported channel still refuses rather than being silently lost.
for motion in sys.argv[5:]:
    unsupported = _native.Session(stage, sys.argv[1], motion)
    try:
        unsupported.play()
        try:
            unsupported.advance(.0083334)
            raise AssertionError("Unsupported morph must refuse")
        except RuntimeError as error:
            assert "MMD_ADAPTER_MORPH" in str(error), error
        assert stage.GetSessionLayer().ExportToString() == session_layer
    finally:
        unsupported.close()
assert stage.GetRootLayer().ExportToString() == original
assert stage.GetSessionLayer().ExportToString() == session_layer
print(f"live morph: direct/group/mixed each 3 x 240 ticks, resolved/zero weights, final deformation error {max_error:.6g} m, "
      "exact reset replay, cleanup and unsupported material refusal passed")
