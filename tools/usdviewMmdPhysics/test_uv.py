"""Generated primary UV + vertex/group + Jolt -> retained state -> live st."""
import math
import sys
from pxr import Usd, UsdGeom, UsdSkel
from usdviewMmdPhysics import _native

stage = Usd.Stage.Open(sys.argv[1])
mesh = UsdGeom.Mesh.Get(stage, "/Asset/geo/Mesh")
st = UsdGeom.PrimvarsAPI(mesh).GetPrimvar("st")
base = list(st.Get())
root = stage.GetRootLayer().ExportToString()
session_layer = stage.GetSessionLayer().ExportToString()
max_error = 0
for mixed, motion in enumerate(sys.argv[2:]):
    session = _native.Session(stage, sys.argv[1], motion)
    replay = []
    try:
        for generation in range(1, 4):
            session.play()
            for tick in range(240):
                session.advance(.0083334)
                assert session.stats["steps"] == tick + 1
                frame = (tick + 1) / 4
                if frame <= 15:
                    weight = frame / 15 * 1.5
                elif frame <= 30:
                    weight = 1.5 - (frame - 15) / 15 * 2
                else:
                    weight = -.5 + (frame - 30) / 30 * .5
                uv_weight = weight * (.5 if mixed else 1)
                values = list(st.Get())
                for vertex, (original, actual) in enumerate(zip(base, values)):
                    expected = (original[0] + (.25 * uv_weight if vertex == 0 else 0),
                                original[1] - (.5 * uv_weight if vertex == 0 else 0))
                    for a in range(2):
                        error = abs(actual[a] - expected[a])
                        max_error = max(max_error, error)
                        assert error < 1e-6, (tick, vertex, a, actual, expected)
                query = UsdSkel.Cache().GetSkelQuery(UsdSkel.Skeleton.Get(stage, "/Asset/skel/Skeleton"))
                weights = list(query.GetAnimQuery().ComputeBlendShapeWeights())
                assert len(weights) == 1 and abs(weights[0] - (weight if mixed else 0)) < 1e-6
                assert st.GetInterpolation() == UsdGeom.Tokens.vertex and not st.IsIndexed()
                assert not st.GetAttr().GetTimeSamples()
                for row in session.bodyStates.values():
                    assert all(math.isfinite(x) for field in row.values() for x in field)
                current = (values, weights, session.bodyStates)
                if generation == 1:
                    replay.append(current)
                else:
                    assert current == replay[tick], (generation, tick)
                if tick == 59:
                    session.pause()
                    session.advance(1)
                    assert session.stats["steps"] == 60 and list(st.Get()) == values
                    session.play()
            assert list(st.Get()) == base
            session.reset()
            assert session.stats["steps"] == 0 and session.stats["paused"]
            assert list(st.Get()) == base
            assert stage.GetSessionLayer().ExportToString() == session_layer
    finally:
        session.close()
    assert stage.GetRootLayer().ExportToString() == root
    assert stage.GetSessionLayer().ExportToString() == session_layer
    assert list(st.Get()) == base
print(f"live primary UV: direct/mixed each 3 x 240 ticks, signed/unclamped weights, "
      f"base reset, vertex coexistence, pause/replay/cleanup; max st error {max_error:.6g}")
