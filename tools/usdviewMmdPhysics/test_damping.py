"""Source-policy admission -> keyed constraints -> exact Reset replay."""
import math
import sys
from pxr import Usd
from usdviewMmdPhysics import _native

pmx, vmd = sys.argv[1:3]
root = "/Asset/physics/rigidBodies/"
anchor, swing = root + "anchor", root + "swing"


def run(linear, angular):
    stage = Usd.Stage.Open(pmx)
    original = stage.GetRootLayer().ExportToString()
    stage.SetEditTarget(stage.GetSessionLayer())
    for path in (anchor, swing):
        body = stage.GetPrimAtPath(path)
        body.GetAttribute("mmd:physics:linearDamping").Set(linear)
        body.GetAttribute("mmd:physics:angularDamping").Set(angular)
    authored = stage.GetSessionLayer().ExportToString()
    session = _native.Session(stage, pmx, vmd)
    normalizations = session.dampingNormalizations
    expected = {(path + ".mmd:physics:" + name, value, max(0., min(1., value)))
                for path in (anchor, swing)
                for name, value in (("linearDamping", linear), ("angularDamping", angular))
                if value < 0 or value > 1}
    assert {(r["attributePath"], r["original"], r["coefficient"]) for r in normalizations} == expected
    replay = []
    for generation in (1, 2, 3):
        assert session.dampingNormalizations == normalizations
        session.play()
        for tick in range(120):
            session.advance(.0083334)
            assert session.stats["steps"] == tick + 1
            state = session.bodyStates
            assert all(math.isfinite(v) for row in state.values() for values in row.values() for v in values)
            seconds = (tick + 1) / 120
            expected_x = .2 * (seconds if seconds <= .5 else max(0., 1 - seconds))
            # A clearing declaration never overrides a kinematic command.
            assert abs(state[anchor]["translation"][0] - expected_x) < 1e-5
            if generation == 1:
                replay.append(state)
            else:
                assert state == replay[tick], (linear, angular, generation, tick)
        session.pause()
        session.advance(2)
        assert session.bodyStates == replay[-1]
        if generation < 3:
            session.reset()
            assert session.stats["steps"] == 0 and session.stats["paused"]
            assert stage.GetSessionLayer().ExportToString() == authored
    session.close()
    assert not session.bodyStates
    assert stage.GetSessionLayer().ExportToString() == authored
    assert stage.GetRootLayer().ExportToString() == original
    return replay


# Independent channels, finite-rate coexistence, normalization and exact replay.
runs = {(l, a): run(l, a) for l in (0., 1.) for a in (0., 1.)}
assert run(2., -2.) == runs[1., 0.]
assert run(-2., 2.) == runs[0., 1.]
assert run(1., .5) != runs[1., 0.]
assert run(.5, 1.) != runs[0., 1.]
assert len({tuple(r[swing]["rotation"]) for r in runs[1., 1.]}) > 1
assert max(abs(r[swing]["rotation"][2]) for r in runs[1., 1.]) > .001
assert any(any(abs(v) > .01 for v in r[swing]["linearVelocity"]) for r in runs[1., 1.])
assert runs[1., 0.] != runs[0., 0.] and runs[0., 1.] != runs[0., 0.]

# Reset must re-admit changed source opinions, including diagnostics, rather
# than keeping the old descriptor. A failed admission can be repaired/retried.
stage = Usd.Stage.Open(pmx)
stage.SetEditTarget(stage.GetSessionLayer())
body = stage.GetPrimAtPath(swing)
session = _native.Session(stage, pmx, vmd)
assert not session.dampingNormalizations
body.GetAttribute("mmd:physics:linearDamping").Set(2.)
body.GetAttribute("mmd:physics:angularDamping").Set(-2.)
session.reset()
assert len(session.dampingNormalizations) == 2
retained = session.dampingNormalizations
for name in ("linearDamping", "angularDamping"):
    attr = body.GetAttribute("mmd:physics:" + name)
    attr.Set(float("inf"))
    authored = stage.GetSessionLayer().ExportToString()
    try:
        session.reset()
        raise AssertionError("nonfinite damping admitted on Reset")
    except RuntimeError as error:
        assert "MMD_PHYSICS_METADATA_INVALID" in str(error) and swing in str(error)
    assert stage.GetSessionLayer().ExportToString() == authored
    assert not session.bodyStates
    attr.Set(0.)
    session.reset()
assert not session.dampingNormalizations and len(retained) == 2
session.play()
session.advance(.025)
assert session.stats["steps"] == 3
session.close()
print("source damping: 8 policies x 3 generations x 120 ticks; keyed constraints, "
      "independent channels, normalization, nonmutation and Reset re-admission passed")
