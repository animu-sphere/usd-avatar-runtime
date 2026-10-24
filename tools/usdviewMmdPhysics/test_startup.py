"""Non-rest VMD frame zero seeds all associated bodies without a physics tick."""
import math
from pathlib import Path
import sys
from pxr import Gf, Usd, UsdSkel
from usdviewMmdPhysics import _native

pmx, fixtures = sys.argv[1], Path(sys.argv[2])
pivot = Gf.Vec3d(0, 1.6, 0)
shift = Gf.Vec3d(1, .3, -.2)
max_speed = max_error = 0.


def quaternion(row):
    return Gf.Quatd(row['rotation'][3], Gf.Vec3d(*row['rotation'][:3])).GetNormalized()


def equivalent(a, b):
    return abs(abs(a.GetReal()*b.GetReal() + Gf.Dot(a.GetImaginary(), b.GetImaginary())) - 1) < 1e-6


control_stage = Usd.Stage.Open(pmx)
control = _native.Session(control_stage, pmx, '', False, False)
rest = control.bodyStates
reference = []
try:
    control.play()
    for _ in range(240):
        control.advance(.0083334)
        reference.append(control.bodyStates)
finally:
    control.close()

for name in ('offset', 'turn', 'bone'):
    stage = Usd.Stage.Open(pmx)
    original = stage.GetRootLayer().ExportToString()
    session_text = stage.GetSessionLayer().ExportToString()
    session = _native.Session(stage, pmx, str(fixtures/f'live-physics-start-{name}.vmd'), False, False)
    skeleton = UsdSkel.Skeleton.Get(stage, '/Asset/skel/Skeleton')
    root_q = Gf.Rotation(Gf.Vec3d(0, 1, 0), 0 if name == 'offset' else 90).GetQuat()
    child_q = Gf.Rotation(Gf.Vec3d(0, 0, 1), 30 if name == 'bone' else 0).GetQuat()
    initial = session.bodyStates
    assert session.stats['steps'] == 0 and session.stats['paused']
    for path, row in initial.items():
        is_swing = path.endswith('/swing')
        rotation = root_q * (child_q if is_swing else Gf.Quatd(1))
        expected = pivot + shift + rotation.Transform(Gf.Vec3d(*rest[path]['translation']) - pivot)
        assert (Gf.Vec3d(*row['translation']) - expected).GetLength() < 2e-6
        assert equivalent(quaternion(row), rotation * quaternion(rest[path]))
        assert row['linearVelocity'] == (0., 0., 0.)
    replay = []
    try:
        for generation in (1, 2, 3):
            assert session.bodyStates == initial
            session.play()
            session.advance(0)
            assert session.stats['steps'] == 0 and session.bodyStates == initial
            for tick in range(240):
                session.advance(.0083334)
                assert session.stats['steps'] == tick + 1
                bodies = session.bodyStates
                for path, row in bodies.items():
                    assert all(math.isfinite(x) for field in row.values() for x in field)
                    speed = math.sqrt(sum(x*x for x in row['linearVelocity']))
                    max_speed = max(max_speed, speed)
                    # Before this fix the first tick reached 120 m/s for the 1 m offset.
                    assert speed < 5., (name, tick, speed)
                    if name == 'offset':
                        expected = Gf.Vec3d(*reference[tick][path]['translation']) + shift
                        error = (Gf.Vec3d(*row['translation']) - expected).GetLength()
                        max_error = max(max_error, error)
                        assert error < 2e-5, (tick, path, error)
                query = UsdSkel.Cache().GetSkelQuery(skeleton)
                components = query.GetAnimQuery().ComputeJointLocalTransformComponents()
                value = (list(components[0]), list(components[1]), bodies)
                if generation == 1:
                    replay.append(value)
                else:
                    assert value == replay[tick], (name, generation, tick)
                if tick == 59:
                    session.pause()
                    session.advance(1)
                    assert session.stats['steps'] == 60 and session.bodyStates == bodies
                    session.play()
            if generation < 3:
                session.reset()
                assert session.stats['steps'] == 0 and session.stats['paused']
                assert stage.GetSessionLayer().ExportToString() == session_text
    finally:
        session.close()
    assert stage.GetRootLayer().ExportToString() == original
    assert stage.GetSessionLayer().ExportToString() == session_text
print(f'live startup: offset/root turn/child pose, each 3 x 240 ticks, '
      f'max speed {max_speed:.6g} m/s, translation-invariance error {max_error:.6g} m; '
      'zero-tick/zero-speed seed, pause, exact reset replay and cleanup passed')
