"""Drive the same native session as usdview, without a window or a bake."""
import sys
from pxr import Usd, UsdSkel, UsdGeom
from usdviewMmdPhysics import _native

stage = Usd.Stage.Open(sys.argv[1])
assert stage
original = stage.GetRootLayer().ExportToString()
session_layer = stage.GetSessionLayer().ExportToString()
skeleton = UsdSkel.Skeleton.Get(stage, "/Asset/skel/Skeleton")
session = _native.Session(stage, sys.argv[1], "")
assert session.stats["bodies"] == 2 and session.stats["steps"] == 0
assert session.unappliedMetadata
session.advance(10)
assert session.stats["steps"] == 0


def pose():
    targets = UsdSkel.BindingAPI(skeleton).GetAnimationSourceRel().GetTargets()
    assert len(targets) == 1
    animation = UsdSkel.Animation.Get(stage, targets[0])
    assert animation and not animation.GetRotationsAttr().GetTimeSamples()
    return list(animation.GetRotationsAttr().Get()), list(animation.GetTranslationsAttr().Get())


first = []
for generation in (1, 2, 3):
    session.play()
    for frame in range(40):
        session.advance(.025)
        assert session.stats["steps"] == (frame + 1) * 3
        value = pose()
        if generation == 1:
            first.append(value)
        else:
            assert value == first[frame], (generation, frame, value, first[frame])
    # This catches a disconnected solver/feedback/publication path: an empty
    # motion with gravity changes the mesh's driving bone rotation.
    assert max(abs(q.GetImaginary()[2]) for q in pose()[0]) > .1
    cache = UsdSkel.Cache()
    cache.Populate(UsdSkel.Root.Get(stage, "/Asset"), Usd.PrimDefaultPredicate)
    mesh = next(p for p in stage.Traverse() if p.IsA(UsdGeom.Mesh))
    original_points = UsdGeom.Mesh(mesh).GetPointsAttr().Get()
    points = UsdGeom.Mesh(mesh).GetPointsAttr().Get()
    transforms = cache.GetSkelQuery(skeleton).ComputeSkinningTransforms()
    assert cache.GetSkinningQuery(mesh).ComputeSkinnedPoints(transforms, points)
    assert (points[2] - original_points[2]).GetLength() > .1
    retained_pose = pose()
    session.pause()
    session.advance(10)
    assert session.stats["steps"] == 120 and pose() == retained_pose
    if generation < 3:
        session.reset()
        assert session.stats["generation"] == generation + 1
        assert session.stats["steps"] == 0 and session.stats["paused"]
        assert stage.GetSessionLayer().ExportToString() == session_layer

try:
    session.advance(-1)
    raise AssertionError("negative elapsed accepted")
except ValueError:
    pass
session.close()
session.close()
assert stage.GetSessionLayer().ExportToString() == session_layer
assert stage.GetRootLayer().ExportToString() == original
del session
print("live MMD: 3 x 120 ticks, feedback rotation, pause/reset/replay and clean close passed")
