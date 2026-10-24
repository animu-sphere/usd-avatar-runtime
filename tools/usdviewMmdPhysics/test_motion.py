"""Generated keyed pivot -> live physics -> composed UsdSkel, without a bake."""
import math
import sys

from pxr import Gf, Usd, UsdGeom, UsdSkel
from usdviewMmdPhysics import _native


stage = Usd.Stage.Open(sys.argv[1])
assert stage
original = stage.GetRootLayer().ExportToString()
session_layer = stage.GetSessionLayer().ExportToString()
skeleton = UsdSkel.Skeleton.Get(stage, "/Asset/skel/Skeleton")
mesh = next(p for p in stage.Traverse() if p.IsA(UsdGeom.Mesh))
baseline_points = list(UsdGeom.Mesh(mesh).GetPointsAttr().Get())


def sample(session):
    targets = UsdSkel.BindingAPI(skeleton).GetAnimationSourceRel().GetTargets()
    assert len(targets) == 1
    animation = UsdSkel.Animation.Get(stage, targets[0])
    assert animation
    translations = list(animation.GetTranslationsAttr().Get())
    rotations = list(animation.GetRotationsAttr().Get())
    scales = list(animation.GetScalesAttr().Get())
    assert len(translations) == len(rotations) == len(scales) == 2
    assert all(math.isfinite(v) and v > 0 for scale in scales for v in scale)
    for attr in (animation.GetTranslationsAttr(), animation.GetRotationsAttr(),
                 animation.GetScalesAttr()):
        assert not attr.GetTimeSamples()
    bodies = session.bodyStates
    assert len(bodies) == 2
    for row in bodies.values():
        assert all(math.isfinite(v) for field in row.values() for v in field)
        assert abs(sum(v * v for v in row["rotation"]) - 1) < 1e-4
    for t, q in zip(translations, rotations):
        assert all(math.isfinite(v) for v in (*t, q.GetReal(), *q.GetImaginary()))
    return translations, rotations, bodies


# A motion-free replay supplies an independent control: ignoring the VMD
# must fail the keyed position and downstream response assertions below.
control = _native.Session(stage, sys.argv[1], "")
control.play()
rest = []
for _ in range(240):
    # Round up to the Windows steady-clock resolution (100 ns). Rounding
    # down 1/120 would delay the first fixed tick until the second display.
    control.advance(.0083334)
    rest.append(sample(control))
control.close()

session = _native.Session(stage, sys.argv[1], sys.argv[2])
replay = []
max_skinning_error = 0.0
max_joint_error = 0.0
try:
    for generation in (1, 2, 3):
        session.play()
        for tick in range(240):
            session.advance(.0083334)
            assert session.stats["steps"] == tick + 1
            value = sample(session)
            if generation == 1:
                replay.append(value)
            else:
                assert value == replay[tick], (generation, tick)

            translations, _, bodies = value
            # Published anchor translation and concrete kinematic body agree;
            # source units use the owner's 0.08 m scale.
            anchor = bodies["/Asset/physics/rigidBodies/anchor"]
            swing = bodies["/Asset/physics/rigidBodies/swing"]
            assert max(abs(a - b) for a, b in zip(translations[0], anchor["translation"])) < 2e-5
            assert max(abs(v) for row in bodies.values() for v in row["translation"]) < 3.0
            seconds = (tick + 1) / 120
            expected_x = .2 * (seconds if seconds <= .5 else max(0., 1 - seconds))
            assert abs(translations[0][0] - expected_x) < 1e-5
            body_q = Gf.Quatd(swing["rotation"][3], Gf.Vec3d(*swing["rotation"][:3]))
            rotation = Gf.Rotation(body_q.GetNormalized())
            pivot = Gf.Vec3d(*anchor["translation"])
            expected_center = pivot + rotation.TransformDir(Gf.Vec3d(.6, 0, 0))
            joint_error = (Gf.Vec3d(*swing["translation"]) - expected_center).GetLength()
            max_joint_error = max(max_joint_error, joint_error)
            assert joint_error < .02

            cache = UsdSkel.Cache()
            cache.Populate(UsdSkel.Root.Get(stage, "/Asset"), Usd.PrimDefaultPredicate)
            transforms = cache.GetSkelQuery(skeleton).ComputeSkinningTransforms()
            points = UsdGeom.Mesh(mesh).GetPointsAttr().Get()
            assert cache.GetSkinningQuery(mesh).ComputeSkinnedPoints(transforms, points)
            # All vertices are BDEF1 on swing. Compare composed skinning to
            # the concrete solver rotation around the animated pivot.
            for source, actual in zip(baseline_points, points):
                expected = pivot + rotation.TransformDir(Gf.Vec3d(source) - Gf.Vec3d(0, 1.6, 0))
                error = (Gf.Vec3d(actual) - expected).GetLength()
                max_skinning_error = max(max_skinning_error, error)
                assert error < 2e-5, (tick, source, actual, expected, error)

        # The anchor reaches 10 cm at 0.5 s, returns at 1 s, and holds.
        assert abs(replay[59][0][0][0] - .1) < 1e-5
        assert abs(replay[119][0][0][0]) < 1e-5
        assert abs(replay[239][0][0][0]) < 1e-5
        assert max(abs(value[0][0][0]) for value in rest) < 1e-6
        # Dynamic feedback responds to the keyed pivot, not merely gravity.
        def angle(a, b):
            qa, qb = a[1][1], b[1][1]
            dot = qa.GetReal() * qb.GetReal() + Gf.Dot(qa.GetImaginary(), qb.GetImaginary())
            return 2 * math.acos(min(1., abs(dot)))
        response = max(angle(a, b) for a, b in zip(replay, rest))
        assert response > .01, response
        retained = sample(session)
        session.pause()
        session.advance(10)
        assert session.stats["steps"] == 240 and sample(session) == retained
        if generation < 3:
            session.reset()
            assert session.stats["generation"] == generation + 1
            assert session.stats["steps"] == 0 and session.stats["paused"]
            assert stage.GetSessionLayer().ExportToString() == session_layer
finally:
    session.close()
assert stage.GetRootLayer().ExportToString() == original
assert stage.GetSessionLayer().ExportToString() == session_layer
print(f"live VMD: 3 x 240 ticks, 10 cm keyed pivot/return, feedback difference {response:.6g} rad, "
      f"joint error {max_joint_error:.6g} m, skinning error {max_skinning_error:.6g} m, "
      "pause/reset/exact replay/cleanup passed")
