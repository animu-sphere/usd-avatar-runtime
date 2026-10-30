"""Installed MMD properties reach the live solver; no third-party assets."""
import math
import sys
from pxr import Gf, Usd, UsdGeom, UsdPhysics
from usdviewMmdPhysics import _native

pmx = sys.argv[1]
anchor_path = "/Asset/physics/rigidBodies/anchor"
swing_path = "/Asset/physics/rigidBodies/swing"


def stage_for_drop(mask=65535, restitution=0., friction=0., damping=0., slope=0.):
    stage = Usd.Stage.Open(pmx)
    stage.SetEditTarget(stage.GetSessionLayer())
    anchor = stage.GetPrimAtPath(anchor_path)
    swing = stage.GetPrimAtPath(swing_path)
    # The same two MMD-associated bodies, with a disabled joint and a floor
    # collider, isolate properties from joint collision suppression.
    UsdPhysics.Joint.Get(stage, "/Asset/physics/joints/joint_0000").CreateJointEnabledAttr().Set(False)
    anchor.GetAttribute("xformOp:translate").Set(Gf.Vec3d(0, 0, 0))
    anchor.GetAttribute("xformOp:orient").Set(Gf.Quatf(Gf.Rotation(Gf.Vec3d(0, 0, 1), slope).GetQuat()))
    stage.GetPrimAtPath(anchor_path + "/collider").SetActive(False)
    floor = UsdGeom.Cube.Define(stage, anchor_path + "/floor")
    floor.CreateSizeAttr(2)
    floor.AddScaleOp().Set(Gf.Vec3f(5, .1, 5))
    UsdPhysics.CollisionAPI.Apply(floor.GetPrim())
    swing.GetAttribute("xformOp:translate").Set(Gf.Vec3d(.6, 2, 0))
    for body in (anchor, swing):
        body.GetAttribute("mmd:physics:friction").Set(friction)
        body.GetAttribute("mmd:physics:restitution").Set(restitution)
        body.GetAttribute("mmd:physics:linearDamping").Set(damping)
    anchor.GetAttribute("mmd:physics:collisionGroup").Set(1)
    swing.GetAttribute("mmd:physics:collisionMask").Set(mask)
    return stage


def run_drop(**properties):
    stage = stage_for_drop(**properties)
    before = stage.GetSessionLayer().ExportToString()
    session = _native.Session(stage, pmx, "")
    values = []
    session.play()
    for _ in range(80):
        session.advance(.025)
        values.append(session.bodyStates[swing_path])
    retained = session.bodyStates
    session.close()
    assert not session.bodyStates
    assert retained[swing_path] == values[-1]
    assert stage.GetSessionLayer().ExportToString() == before
    assert all(math.isfinite(v) for row in values for key in row for v in row[key])
    return values


admitted = run_drop()
rejected = run_drop(mask=0)
assert abs(admitted[-1]["translation"][1] - .18) < .03, admitted[-1]
assert abs(admitted[-1]["linearVelocity"][1]) < .05, admitted[-1]
assert rejected[-1]["translation"][1] < -10, rejected[-1]
# Only bit 1 admits the floor's group; masks are acceptance bits, not inverted.
assert run_drop(mask=1)[-1]["translation"][1] < -10
assert abs(run_drop(mask=2)[-1]["translation"][1] - .18) < .03
elastic = run_drop(restitution=.8)
assert max(row["linearVelocity"][1] for row in elastic) > 2
assert max(row["linearVelocity"][1] for row in admitted) < .1
damped = run_drop(mask=0, damping=.75)
assert abs(damped[39]["linearVelocity"][1]) < .65 * abs(rejected[39]["linearVelocity"][1])
# Clearing happens before each substep's gravity, leaving fresh acceleration.
# This distinguishes the endpoint from a huge finite post-force drag rate.
endpoint = run_drop(mask=0, damping=1.)
normalized = run_drop(mask=0, damping=2.)
assert endpoint == normalized
assert run_drop(mask=0, damping=-2.) == rejected
for row in endpoint:
    assert abs(row["linearVelocity"][1] + 9.81 / 120) < 2e-5, row
assert abs(endpoint[-1]["translation"][1] - (2 - 240 * 9.81 / 120**2)) < 2e-5
smooth = run_drop(slope=10)
rough = run_drop(slope=10, friction=1)
assert abs(smooth[-1]["linearVelocity"][0]) > abs(rough[-1]["linearVelocity"][0]) + .5


def swing(angular):
    stage = Usd.Stage.Open(pmx)
    stage.SetEditTarget(stage.GetSessionLayer())
    body = stage.GetPrimAtPath(swing_path)
    body.GetAttribute("mmd:physics:angularDamping").Set(angular)
    session = _native.Session(stage, pmx, "")
    session.play()
    for _ in range(10):
        session.advance(.025)
    q = session.bodyStates[swing_path]["rotation"]
    session.close()
    return abs(q[2])


free_angle, damped_angle = swing(0), swing(.999)
# The constrained pendulum continually receives gravity/constraint impulses;
# verify a visible response difference, rather than free-spin exponential decay.
assert damped_angle < .95 * free_angle, (free_angle, damped_angle)
# Failure is explicit and leaves no live output, including during reconstruction.
for name, value, code in (("linearDamping", float("nan"), "MMD_PHYSICS_METADATA_INVALID"),
                          ("restitution", 2., "MMD_PHYSICS_METADATA_INVALID")):
    stage = stage_for_drop()
    session = _native.Session(stage, pmx, "")
    session.play()
    session.advance(.025)
    session.pause()
    stage.GetPrimAtPath(swing_path).GetAttribute("mmd:physics:" + name).Set(value)
    before = stage.GetSessionLayer().ExportToString()
    try:
        session.reset()
        raise AssertionError("invalid metadata accepted during reset")
    except RuntimeError as error:
        assert code in str(error) and swing_path in str(error), error
    assert stage.GetSessionLayer().ExportToString() == before
    stage.GetPrimAtPath(swing_path).GetAttribute("mmd:physics:" + name).Set(0.)
    session.reset()
    assert session.stats["steps"] == 0 and session.stats["paused"]
    session.play()
    session.advance(.025)
    assert session.stats["steps"] == 3
    session.close()
    stage.GetPrimAtPath(swing_path).GetAttribute("mmd:physics:" + name).Set(value)
    before = stage.GetSessionLayer().ExportToString()
    try:
        _native.Session(stage, pmx, "")
        raise AssertionError("invalid metadata accepted during construction")
    except ValueError as error:
        assert code in str(error) and swing_path in str(error), error
    assert stage.GetSessionLayer().ExportToString() == before

print("live MMD properties: admission masks, contact, bounce, friction, linear/angular damping and refusal passed")
