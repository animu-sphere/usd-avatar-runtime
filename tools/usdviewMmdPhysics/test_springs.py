"""Owner spring conversion reaches live constraints, feedback and USD output."""
import math
import sys
from pxr import Gf, Usd, UsdPhysics, UsdSkel
from usdviewMmdPhysics import _native

pmx = sys.argv[1]
joint_path = "/Asset/physics/joints/joint_0000"
body_path = "/Asset/physics/rigidBodies/swing"


def make_stage(translation=(0, 0, 0), rotation=(0, 0, 0), sliding=False):
    stage = Usd.Stage.Open(pmx)
    stage.SetEditTarget(stage.GetSessionLayer())
    joint = stage.GetPrimAtPath(joint_path)
    joint.GetAttribute("mmd:physics:translationSpring").Set(Gf.Vec3f(*translation))
    joint.GetAttribute("mmd:physics:rotationSpring").Set(Gf.Vec3f(*rotation))
    if sliding:
        # A single free translation axis isolates Hookean response under gravity.
        limit = UsdPhysics.LimitAPI.Apply(joint, "transY")
        limit.CreateLowAttr().Set(-math.inf)
        limit.CreateHighAttr().Set(math.inf)
        limit = UsdPhysics.LimitAPI.Apply(joint, "rotZ")
        limit.CreateLowAttr().Set(1.)
        limit.CreateHighAttr().Set(-1.)
    return stage


def advance(session):
    session.play()
    rows = []
    for _ in range(20):
        session.advance(.025)
        rows.append(session.bodyStates[body_path])
    assert all(math.isfinite(v) for row in rows for values in row.values() for v in values)
    return rows


def response(sliding, enabled):
    stage = make_stage(translation=(0, 100, 0) if sliding else (0, 0, 0),
                       rotation=(0, 0, 10000) if not sliding else (0, 0, 0), sliding=sliding)
    before = stage.GetSessionLayer().ExportToString()
    root = stage.GetRootLayer().ExportToString()
    session = _native.Session(stage, pmx, "", enabled and sliding, enabled and not sliding)
    stats = session.stats
    assert stats["approximateTranslationSpringAxes"] == int(enabled and sliding)
    assert stats["approximateRotationSpringAxes"] == int(enabled and not sliding)
    first = advance(session)
    # Final solver rotation reaches the live animation through owner feedback.
    skeleton = UsdSkel.Skeleton.Get(stage, "/Asset/skel/Skeleton")
    target = UsdSkel.BindingAPI(skeleton).GetAnimationSourceRel().GetTargets()[0]
    q = UsdSkel.Animation.Get(stage, target).GetRotationsAttr().Get()[1]
    body_q = first[-1]["rotation"]
    dot = sum(q.GetImaginary()[i] * body_q[i] for i in range(3)) + q.GetReal() * body_q[3]
    assert abs(dot) > .99999, (q, body_q)
    session.pause()
    session.advance(1)
    assert session.bodyStates[body_path] == first[-1]
    session.reset()
    assert session.stats["steps"] == 0 and session.stats["paused"]
    assert stage.GetSessionLayer().ExportToString() == before
    assert advance(session) == first
    session.close()
    assert stage.GetSessionLayer().ExportToString() == before
    assert stage.GetRootLayer().ExportToString() == root
    return first[-1]


free_slide, spring_slide = response(True, False), response(True, True)
free_swing, spring_swing = response(False, False), response(False, True)
assert spring_slide["translation"][1] > free_slide["translation"][1] + .5, (free_slide, spring_slide)
assert abs(spring_swing["rotation"][2]) < .7 * abs(free_swing["rotation"][2]), (free_swing, spring_swing)

# Independent flags, signed/locked/disabled/non-spring activation and warnings.
for mode in ("normal", "other-type", "missing-type", "disabled", "limited"):
    stage = make_stage((5, 100, -3), (4, -2, 10000), sliding=True)
    joint = stage.GetPrimAtPath(joint_path)
    # Free Z rotation so both independent spring switches can activate.
    limit = UsdPhysics.LimitAPI.Apply(joint, "rotZ")
    limit.CreateLowAttr().Set(-90.)
    limit.CreateHighAttr().Set(90.)
    if mode == "other-type": joint.GetAttribute("mmd:physics:type").Set("hinge")
    if mode == "missing-type":
        stage = Usd.Stage.Open(stage.Flatten())
        joint = stage.GetPrimAtPath(joint_path)
        joint.RemoveProperty("mmd:physics:type")
    if mode == "disabled": UsdPhysics.Joint(joint).CreateJointEnabledAttr().Set(False)
    if mode == "limited":
        limit = UsdPhysics.LimitAPI.Apply(joint, "transY")
        limit.CreateLowAttr().Set(-2.)
        limit.CreateHighAttr().Set(2.)
    for translation, rotation in ((False, False), (True, False), (False, True), (True, True)):
        session = _native.Session(stage, pmx, "", translation, rotation)
        active = mode in ("normal", "limited")
        assert session.stats["approximateTranslationSpringAxes"] == int(active and translation), (mode, translation, rotation, session.stats)
        assert session.stats["approximateRotationSpringAxes"] == int(active and rotation), (mode, translation, rotation, session.stats)
        # Positive locked and negative source axes remain visible gaps.
        assert len(session.unappliedMetadata) == 2
        session.close()

for enabled in (False, True):
    stage = make_stage(rotation=(0, 0, 10000))
    session = _native.Session(stage, pmx, "", False, enabled)
    advance(session)
    session.pause()
    attr = stage.GetPrimAtPath(joint_path).GetAttribute("mmd:physics:rotationSpring")
    attr.Set(Gf.Vec3f(math.nan, 0, 0))
    before = stage.GetSessionLayer().ExportToString()
    try:
        session.reset()
        raise AssertionError("malformed spring accepted during Reset")
    except RuntimeError as error:
        assert "MMD_PHYSICS_METADATA_INVALID" in str(error) and joint_path in str(error), error
    assert stage.GetSessionLayer().ExportToString() == before
    attr.Set(Gf.Vec3f(0, 0, 20000))
    session.reset()
    assert session.stats["approximateRotationSpringAxes"] == int(enabled)
    rebuilt = advance(session)
    session.close()
    # Reset rereads changed source stiffness, rather than retaining a descriptor.
    fresh = make_stage(rotation=(0, 0, 20000))
    reference = _native.Session(fresh, pmx, "", False, enabled)
    assert advance(reference) == rebuilt
    reference.close()
    attr.Block()
    before = stage.GetSessionLayer().ExportToString()
    try:
        _native.Session(stage, pmx, "", False, enabled)
        raise AssertionError("blocked spring accepted during construction")
    except ValueError as error:
        assert "MMD_PHYSICS_METADATA_INVALID" in str(error) and joint_path in str(error), error
    assert stage.GetSessionLayer().ExportToString() == before

print("live spring responses:", free_slide["translation"][1], spring_slide["translation"][1],
      free_swing["rotation"][2], spring_swing["rotation"][2])
print("live MMD springs: opt-ins, activation, feedback, replay, refusal and cleanup passed")
