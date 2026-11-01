#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Write the MMD session fixture in tests/assets/mmd-session with the owner's encoders.

One model and one motion exercise morph, primary UV, IK, append and physics feedback, with a
self-joint, which the importer disables, and a bone two simulated bodies name.
Each `broken` input takes away exactly one of them, so that the check can show the
comparison failing for that reason. The encoders come from the owner's source archive
at the tag and digest in importers.json, so neither mode needs an owner checkout.

  make_mmd_session_fixture.py          rewrite the files
  make_mmd_session_fixture.py --check  fail if a committed file differs
"""
from __future__ import annotations

import argparse
import copy
import hashlib
import importlib.util
import json
import pathlib
import sys
import tarfile
import tempfile
import urllib.request

ROOT = pathlib.Path(__file__).resolve().parents[1]
ASSETS = ROOT / "tests" / "assets" / "mmd-session"


def encoders(directory: pathlib.Path):
    """Downloads the pinned owner source and loads its two fixture encoders."""
    pins = json.loads((ASSETS / "importers.json").read_text(encoding="utf-8"))
    asset = f"usd-mmd-plugins-{pins['version']}-src.tar.gz"
    url = f"https://github.com/{pins['repository']}/releases/download/v{pins['version']}/{asset}"
    print("+ download", url, flush=True)
    with urllib.request.urlopen(url) as response:
        data = response.read()
    digest = hashlib.sha256(data).hexdigest()
    if digest != pins["source"]:
        raise SystemExit(f"{asset}: sha256 {digest}, expected {pins['source']}")
    archive = directory / asset
    archive.write_bytes(data)
    with tarfile.open(archive) as tar:
        names = [m for m in tar.getmembers() if m.name.endswith(("/generate_fixtures.py", "/generate_vmd_fixtures.py"))]
        tar.extractall(directory, members=names, filter="data")

    def load(stem: str):
        path = next(directory.rglob(f"tests/fixtures/{stem}.py"))
        spec = importlib.util.spec_from_file_location(stem, path)
        module = importlib.util.module_from_spec(spec)
        # The encoders' dataclasses resolve their module by name.
        sys.modules[stem] = module
        spec.loader.exec_module(module)
        return module

    return load("generate_fixtures"), load("generate_vmd_fixtures")


def model(pmx) -> dict:
    flags = pmx.ROTATABLE | pmx.TRANSLATABLE
    result = pmx.empty_model(2.0, pmx.UTF16LE, "MMD session fixture")
    result["bones"] = [
        pmx.bone("anchor", "anchor", (0.0, 20.0, 0.0), -1, flags, (1.0, 0.0, 0.0)),
        # Physics feedback: a dynamic body swings this bone.
        pmx.bone("swing", "swing", (0.0, 20.0, 0.0), 0, flags, (15.0, 0.0, 0.0)),
        # Append: the follower takes half of the keyed driver's rotation.
        pmx.bone("driver", "driver", (5.0, 15.0, 0.0), 0, flags, (0.0, -2.0, 0.0)),
        pmx.bone("follower", "follower", (5.0, 10.0, 0.0), 0, flags | pmx.APPEND_ROTATION,
                 (0.0, -2.0, 0.0), append=(2, 0.5)),
        # IK: the keyed target pulls a two-link chain.
        pmx.bone("upper", "upper", (-5.0, 15.0, 0.0), 0, flags, (0.0, -5.0, 0.0)),
        pmx.bone("lower", "lower", (-5.0, 10.0, 0.0), 4, flags, (0.0, -5.0, 0.0)),
        pmx.bone("tip", "tip", (-5.0, 5.0, 0.0), 5, flags, (0.0, -1.0, 0.0)),
        pmx.bone("target", "target", (-5.0, 5.0, 0.0), 0, flags | pmx.IK, (0.0, -1.0, 0.0),
                 ik={"target": 6, "loopCount": 40, "limitAngle": 2.0, "links": [(5, None), (4, None)]}),
        # After physics: this append reads the swing the body fed back, not the keyed one.
        pmx.bone("afterSwing", "afterSwing", (5.0, 20.0, 0.0), 0, flags | pmx.APPEND_ROTATION | pmx.DEFORM_AFTER_PHYSICS,
                 (0.0, -1.0, 0.0), append=(1, 1.0)),
    ]
    faces = []
    for bone, (x, y) in ((1, (7.5, 20.0)), (3, (5.0, 10.0)), (5, (-5.0, 10.0))):
        start = len(result["vertices"])
        for dx, dy in ((-1.0, -1.0), (1.0, -1.0), (1.0, 1.0), (-1.0, 1.0)):
            vertex = pmx.vertex(0.0, pmx.deform("BDEF1", [bone]), 0)
            vertex["position"] = (x + dx, y + dy, 0.0)
            result["vertices"].append(vertex)
        faces += [start, start + 1, start + 2, start, start + 2, start + 3]
    result["faces"] = faces
    result["materials"] = [pmx.material("quads", "quads", len(faces), ("shared", 0), flags=0x01)]
    result["morphs"] = [
        {"name": "wide", "englishName": "wide", "panel": 4, "type": "vertex",
         "offsets": [(1, (2.0, 0.0, 0.0)), (2, (2.0, 0.0, 0.0))]},
        {"name": "wideGroup", "englishName": "wideGroup", "panel": 4, "type": "group", "offsets": [(0, 0.5)]},
        {"name": "uv", "englishName": "uv", "panel": 4, "type": "uv", "offsets": [(0, (0.25, 0.5, 4.0, 8.0))]},
        {"name": "uvGroup", "englishName": "uvGroup", "panel": 4, "type": "group", "offsets": [(2, 0.5)]},
    ]
    result["rigidBodies"] = [
        pmx.rigid_body("anchor", "anchor", 0, 0, 0, (1.0, 0.0, 0.0), (0.0, 20.0, 0.0), (0.0, 0.0, 0.0), 1.0, 0),
        pmx.rigid_body("swing", "swing", 1, 0, 1, (7.5, 1.0, 1.0), (7.5, 20.0, 0.0), (0.0, 0.0, 0.0), 1.0, 2),
        # Later in source order than "swing", so this body's rotation reaches the bone.
        pmx.rigid_body("swingLater", "swingLater", 1, 0, 1, (3.0, 1.0, 1.0), (3.0, 20.0, 0.0), (0.0, 0.0, 0.0), 1.0, 2),
    ]
    for body in result["rigidBodies"]:
        body["nonCollisionMask"] = 0xFFFF
        body["linearDamping"] = body["angularDamping"] = 0.0
        body["friction"] = body["restitution"] = 0.0
    limits = ((0.0, 0.0, 0.0), (0.0, 0.0, 0.0)), ((0.0, 0.0, -3.0), (0.0, 0.0, 3.0)), ((0.0, 0.0, 0.0), (0.0, 0.0, 0.0))
    result["joints"] = [pmx.joint(name, 0, bodies, (0.0, 20.0, 0.0), (0.0, 0.0, 0.0), *limits)
                        for name, bodies in (("pivot", (0, 1)), ("pivotLater", (0, 2)), ("self", (1, 1)))]
    return result


def motion(vmd):
    result = vmd.Motion("MMD session fixture")
    turn = (0.0, 0.0, 0.3826834323650898, 0.9238795325112867)
    identity = (0.0, 0.0, 0.0, 1.0)
    for frame, x, rotation, reach, weight in ((0, 0.0, identity, 0.0, 0.0), (15, 1.25, turn, 4.0, 1.0),
                                              (30, 0.0, identity, 0.0, -0.5), (60, 0.0, identity, 0.0, 0.0)):
        result.bones.append(("anchor", frame, (x, 0.0, 0.0), identity))
        result.bones.append(("driver", frame, (0.0, 0.0, 0.0), rotation))
        result.bones.append(("target", frame, (reach, reach, 0.0), identity))
        result.morphs.extend((("wideGroup", frame, 2.0 * weight), ("wide", frame, -0.5 * weight),
                              ("uvGroup", frame, 2.0 * weight), ("uv", frame, -0.5 * weight)))
    return result


def outputs(pmx, vmd) -> dict[str, bytes]:
    avatar = model(pmx)
    clip = motion(vmd)
    no_ik = copy.deepcopy(avatar)
    no_ik["bones"][7]["flags"] &= ~pmx.IK
    no_append = copy.deepcopy(avatar)
    no_append["bones"][3]["flags"] &= ~pmx.APPEND_ROTATION
    no_after_physics = copy.deepcopy(avatar)
    no_after_physics["bones"][8]["flags"] &= ~pmx.DEFORM_AFTER_PHYSICS
    no_feedback = copy.deepcopy(avatar)
    no_feedback["rigidBodies"][1]["physicsMode"] = no_feedback["rigidBodies"][2]["physicsMode"] = 0
    # Without the later body, the earlier one drives the bone.
    earlier_body = copy.deepcopy(avatar)
    del earlier_body["rigidBodies"][2]
    earlier_body["joints"] = [j for j in earlier_body["joints"] if 2 not in j["rigidBodies"]]
    no_morph = copy.deepcopy(clip)
    no_morph.morphs = []
    return {
        "avatar.pmx": pmx.encode(avatar)[0],
        "motion.vmd": clip.encode(),
        "broken/no-ik.pmx": pmx.encode(no_ik)[0],
        "broken/no-append.pmx": pmx.encode(no_append)[0],
        "broken/no-after-physics.pmx": pmx.encode(no_after_physics)[0],
        "broken/no-feedback.pmx": pmx.encode(no_feedback)[0],
        "broken/earlier-body.pmx": pmx.encode(earlier_body)[0],
        "broken/no-morph.vmd": no_morph.encode(),
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--check", action="store_true", help="fail if a committed file differs")
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(ignore_cleanup_errors=True) as temporary:
        files = outputs(*encoders(pathlib.Path(temporary)))
    failed = False
    for name, data in files.items():
        path = ASSETS / name
        if args.check:
            if not path.is_file() or path.read_bytes() != data:
                print(f"{path} differs from the generator")
                failed = True
        else:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
