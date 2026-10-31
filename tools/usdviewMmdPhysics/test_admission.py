"""Owner inert-joint admission, unchanged USD and Reset re-admission."""
import math
import sys
from pxr import Usd, Sdf, UsdPhysics, Gf
from usdviewMmdPhysics import _native

pmx, vmd = sys.argv[1:3]
stage = Usd.Stage.Open(pmx)
original = stage.GetRootLayer().ExportToString()
stage.SetEditTarget(stage.GetSessionLayer())
anchor = "/Asset/physics/rigidBodies/anchor"
other = "/Asset/physics/rigidBodies/otherAnchor"
Sdf.CreatePrimInLayer(stage.GetSessionLayer(), Sdf.Path(other).GetParentPath())
assert Sdf.CopySpec(stage.GetRootLayer(), anchor, stage.GetSessionLayer(), other)
path = "/Asset/physics/joints/inert"
joint = UsdPhysics.Joint.Define(stage, path)
joint.CreateBody0Rel().SetTargets([anchor])
joint.CreateBody1Rel().SetTargets([other])
prim = joint.GetPrim()
prim.CreateAttribute("mmd:physics:type", Sdf.ValueTypeNames.Token, True, Sdf.VariabilityUniform).Set("spring6Dof")
for name in ("translationSpring", "rotationSpring"):
    prim.CreateAttribute("mmd:physics:" + name, Sdf.ValueTypeNames.Vector3f,
                         True, Sdf.VariabilityUniform).Set(Gf.Vec3f(1))
authored = stage.GetSessionLayer().ExportToString()
session = _native.Session(stage, pmx, vmd, True, True)
assert session.inertJoints == [path]
retained = session.inertJoints
retained.append("copy")
assert session.inertJoints == [path]
assert path + ".mmd:physics:translationSpring" in session.unappliedMetadata
replay = []
for generation in (1, 2):
    session.play()
    for tick in range(120):
        session.advance(.0083334)
        assert session.stats["steps"] == tick + 1
        states = session.bodyStates
        assert all(math.isfinite(v) for row in states.values() for values in row.values() for v in values)
        if generation == 1:
            replay.append(states)
        else:
            assert states == replay[tick]
    session.pause()
    session.advance(1)
    assert session.bodyStates == replay[-1]
    if generation == 1:
        session.reset()
        assert session.inertJoints == [path]
# Reset changes admission when endpoint opinions change.
joint.GetBody1Rel().SetTargets(["/Asset/physics/rigidBodies/swing"])
session.reset()
assert not session.inertJoints
joint.GetBody1Rel().SetTargets([other])
session.reset()
assert session.inertJoints == [path]
# Invalid inert metadata is never hidden by omission; repair/retry works.
spring = prim.GetAttribute("mmd:physics:translationSpring")
spring.Set(Gf.Vec3f(float("inf")))
try:
    session.reset()
    raise AssertionError("malformed inert joint admitted")
except RuntimeError as error:
    assert "MMD_PHYSICS_METADATA_INVALID" in str(error) and path in str(error)
spring.Set(Gf.Vec3f(1))
session.reset()
session.play()
session.advance(.025)
assert session.stats["steps"] == 3
session.close()
assert stage.GetRootLayer().ExportToString() == original
assert stage.GetSessionLayer().ExportToString() == authored
assert retained == [path, "copy"]
print("inert joint admission: two generations, Reset re-admission, repair, copies and unchanged USD passed")
