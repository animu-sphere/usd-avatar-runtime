"""Composed mode-1 bone pose matches the concrete solver body with springs OFF."""
import math
import sys
from pxr import Gf, Usd, UsdPhysics, UsdSkel
from usdviewMmdPhysics import _native

stage = Usd.Stage.Open(sys.argv[1])
original = stage.GetRootLayer().ExportToString()
with Usd.EditContext(stage, stage.GetSessionLayer()):
    stage.GetPrimAtPath('/Asset/physics/rigidBodies/swing').GetAttribute(
        'mmd:physics:mode').Set('dynamic')
    # Admit visible mode-1 translation rather than depending on solver residuals.
    limit = UsdPhysics.LimitAPI.Apply(stage.GetPrimAtPath('/Asset/physics/joints/joint_0000'), 'transY')
    limit.CreateLowAttr(-.2)
    limit.CreateHighAttr(.2)
session_text = stage.GetSessionLayer().ExportToString()
skeleton = UsdSkel.Skeleton.Get(stage, '/Asset/skel/Skeleton')
session = _native.Session(stage, sys.argv[1], sys.argv[2], False, False)
replay = []
max_error = 0.
max_local_translation = 0.
try:
    for generation in (1, 2, 3):
        session.play()
        for tick in range(240):
            session.advance(.0083334)
            stats = session.stats
            assert stats['steps'] == tick + 1
            assert stats['approximateTranslationSpringAxes'] == stats['approximateRotationSpringAxes'] == 0
            cache = UsdSkel.Cache()
            query = cache.GetSkelQuery(skeleton)
            translation, rotations, scales = query.GetAnimQuery().ComputeJointLocalTransformComponents()
            world = query.ComputeJointSkelTransforms()
            bodies = session.bodyStates
            body = bodies['/Asset/physics/rigidBodies/swing']
            assert all(math.isfinite(x) for row in bodies.values() for field in row.values() for x in field)
            q = Gf.Quatd(body['rotation'][3], Gf.Vec3d(*body['rotation'][:3])).GetNormalized()
            # The unchanged pendulum's body centre is 0.6 m along X from its bone.
            expected_origin = Gf.Vec3d(*body['translation']) - q.Transform(Gf.Vec3d(.6, 0, 0))
            error = (world[1].ExtractTranslation() - expected_origin).GetLength()
            max_error = max(max_error, error)
            assert error < 2e-5, (tick, error)
            actual_q = world[1].ExtractRotationQuat().GetNormalized()
            dot = actual_q.GetReal() * q.GetReal() + Gf.Dot(actual_q.GetImaginary(), q.GetImaginary())
            assert abs(abs(dot) - 1) < 1e-5
            max_local_translation = max(max_local_translation, Gf.Vec3d(translation[1]).GetLength())
            value = (list(translation), list(rotations), bodies)
            if generation == 1:
                replay.append(value)
            else:
                assert value == replay[tick], (generation, tick)
            if tick == 59:
                session.pause()
                session.advance(1)
                assert session.stats['steps'] == 60 and session.bodyStates == bodies
                session.play()
        if generation < 3:
            session.reset()
            assert session.stats['paused'] and session.stats['steps'] == 0
            assert stage.GetSessionLayer().ExportToString() == session_text
finally:
    session.close()
assert max_local_translation > .01, max_local_translation
assert stage.GetRootLayer().ExportToString() == original
assert stage.GetSessionLayer().ExportToString() == session_text
print(f'live mode 1: 3 x 240 ticks, body/bone error {max_error:.6g} m, '
      f'nonzero local translation {max_local_translation:.6g} m, springs OFF, pause/reset/exact replay/cleanup passed')
