#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Generate the small VRM avatars and VRMA clip that CI runs through the session.

The geometry and motion here are written by this script, with no third-party art,
and are covered by this repository's license. They exist to exercise pose, spring,
expression and LookAt through the real importers, not to look like an avatar.

    python tools/make_session_fixture.py          # rewrite tests/assets/session
    python tools/make_session_fixture.py --check  # fail if the files differ
"""
from __future__ import annotations

import argparse
import json
import math
import pathlib
import struct
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
ASSETS = ROOT / "tests" / "assets" / "session"
FLOAT, U16, U8 = 5126, 5123, 5121
COUNT = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4, "MAT4": 16}
FORMAT = {FLOAT: "f", U16: "H", U8: "B"}
LICENSE_URL = "https://www.apache.org/licenses/LICENSE-2.0"


class Glb:
    """Accessors and buffer views in one binary chunk."""

    def __init__(self) -> None:
        self.binary = bytearray()
        self.views: list[dict] = []
        self.accessors: list[dict] = []

    def add(self, component: int, kind: str, rows: list, bounds: bool = False) -> int:
        offset = len(self.binary)
        for row in rows:
            row = row if isinstance(row, (tuple, list)) else (row,)
            self.binary += struct.pack("<" + FORMAT[component] * COUNT[kind], *row)
        self.views.append({"buffer": 0, "byteOffset": offset, "byteLength": len(self.binary) - offset})
        self.binary += b"\0" * (-len(self.binary) % 4)
        accessor = {"bufferView": len(self.views) - 1, "componentType": component, "count": len(rows), "type": kind}
        if bounds:
            columns = list(zip(*[r if isinstance(r, (tuple, list)) else (r,) for r in rows]))
            accessor["min"] = [min(c) for c in columns]
            accessor["max"] = [max(c) for c in columns]
        self.accessors.append(accessor)
        return len(self.accessors) - 1

    def build(self, document: dict) -> bytes:
        document["accessors"] = self.accessors
        document["bufferViews"] = self.views
        document["buffers"] = [{"byteLength": len(self.binary)}]
        text = json.dumps(document, separators=(",", ":"), sort_keys=True).encode("utf-8")
        text += b" " * (-len(text) % 4)
        chunks = struct.pack("<I4s", len(text), b"JSON") + text + struct.pack("<I4s", len(self.binary), b"BIN\0") + bytes(self.binary)
        return struct.pack("<4sII", b"glTF", 2, 12 + len(chunks)) + chunks


# name, parent, translation from the parent. Humanoid bones keep their VRM names.
JOINTS = [
    ("hips", None, (0.0, 0.9, 0.0)),
    ("spine", "hips", (0.0, 0.1, 0.0)),
    ("head", "spine", (0.0, 0.5, 0.0)),
    ("leftEye", "head", (0.03, 0.05, 0.08)),
    ("rightEye", "head", (-0.03, 0.05, 0.08)),
    ("hair1", "head", (0.0, 0.1, -0.05)),
    ("hair2", "hair1", (0.0, -0.1, 0.0)),
    ("hair3", "hair2", (0.0, -0.1, 0.0)),
    ("leftUpperArm", "spine", (0.15, 0.35, 0.0)),
    ("leftLowerArm", "leftUpperArm", (0.25, 0.0, 0.0)),
    ("leftHand", "leftLowerArm", (0.25, 0.0, 0.0)),
    ("rightUpperArm", "spine", (-0.15, 0.35, 0.0)),
    ("rightLowerArm", "rightUpperArm", (-0.25, 0.0, 0.0)),
    ("rightHand", "rightLowerArm", (-0.25, 0.0, 0.0)),
    ("leftUpperLeg", "hips", (0.1, -0.05, 0.0)),
    ("leftLowerLeg", "leftUpperLeg", (0.0, -0.4, 0.0)),
    ("leftFoot", "leftLowerLeg", (0.0, -0.4, 0.0)),
    ("rightUpperLeg", "hips", (-0.1, -0.05, 0.0)),
    ("rightLowerLeg", "rightUpperLeg", (0.0, -0.4, 0.0)),
    ("rightFoot", "rightLowerLeg", (0.0, -0.4, 0.0)),
]
BONES = [name for name, _, _ in JOINTS if not name.startswith("hair")]
TARGETS = ["smile", "close", "up", "down", "left", "right"]
EXPRESSIONS = {"happy": 0, "blink": 1, "lookUp": 2, "lookDown": 3, "lookLeft": 4, "lookRight": 5}


def joint_nodes(first: int) -> list[dict]:
    """One node per joint, from node index `first`, with its children named."""
    index = {name: first + i for i, (name, _, _) in enumerate(JOINTS)}
    nodes = []
    for name, _, translation in JOINTS:
        node = {"name": name, "translation": list(translation)}
        children = [index[child] for child, parent, _ in JOINTS if parent == name]
        if children:
            node["children"] = children
        nodes.append(node)
    return nodes


def world_positions() -> dict[str, tuple[float, float, float]]:
    world: dict[str, tuple[float, float, float]] = {}
    for name, parent, t in JOINTS:
        base = world[parent] if parent else (0.0, 0.0, 0.0)
        world[name] = (base[0] + t[0], base[1] + t[1], base[2] + t[2])
    return world


def avatar(look_type: str) -> bytes:
    """A VRM 1.0 avatar: one quad per joint, rigidly skinned, with six morph targets."""
    glb = Glb()
    world = world_positions()
    positions, normals, joints, weights, indices = [], [], [], [], []
    for slot, (name, _, _) in enumerate(JOINTS):
        x, y, z = world[name]
        start = len(positions)
        for dx, dy in ((-0.02, -0.02), (0.02, -0.02), (0.02, 0.02), (-0.02, 0.02)):
            positions.append((x + dx, y + dy, z))
            normals.append((0.0, 0.0, 1.0))
            joints.append((slot, 0, 0, 0))
            weights.append((1.0, 0.0, 0.0, 0.0))
        indices += [start, start + 1, start + 2, start, start + 2, start + 3]
    head = [i for i, (name, _, _) in enumerate(JOINTS) if name == "head"][0] * 4
    deltas = {"smile": (0.0, 0.01, 0.0), "close": (0.0, -0.01, 0.0), "up": (0.0, 0.0, 0.01),
              "down": (0.0, 0.0, -0.01), "left": (0.01, 0.0, 0.0), "right": (-0.01, 0.0, 0.0)}
    targets = []
    for target in TARGETS:
        rows = [(0.0, 0.0, 0.0)] * len(positions)
        rows[head:head + 4] = [deltas[target]] * 4
        targets.append({"POSITION": glb.add(FLOAT, "VEC3", rows, bounds=True)})
    attributes = {
        "POSITION": glb.add(FLOAT, "VEC3", positions, bounds=True),
        "NORMAL": glb.add(FLOAT, "VEC3", normals),
        "JOINTS_0": glb.add(U8, "VEC4", joints),
        "WEIGHTS_0": glb.add(FLOAT, "VEC4", weights),
    }
    index_accessor = glb.add(U16, "SCALAR", indices)
    inverse_binds = [(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, -world[n][0], -world[n][1], -world[n][2], 1)
                     for n, _, _ in JOINTS]
    inverse_accessor = glb.add(FLOAT, "MAT4", inverse_binds)
    node = {name: 2 + i for i, (name, _, _) in enumerate(JOINTS)}
    presets = {
        name: {"isBinary": False, "morphTargetBinds": [{"node": 0, "index": slot, "weight": 1.0}]}
        for name, slot in EXPRESSIONS.items()
    }
    presets["happy"]["overrideBlink"] = "blend"
    presets["happy"]["materialColorBinds"] = [
        {"material": 0, "type": "color", "targetValue": [0.9, 0.7, 0.5, 0.6]},
        {"material": 0, "type": "emissionColor", "targetValue": [0.3, 0.1, 0.1, 1.0]},
    ]
    look = {"type": look_type, "offsetFromHeadBone": [0.0, 0.06, 0.0]}
    for side in ("rangeMapHorizontalInner", "rangeMapHorizontalOuter", "rangeMapVerticalDown", "rangeMapVerticalUp"):
        look[side] = {"inputMaxValue": 90.0, "outputScale": 10.0 if look_type == "bone" else 1.0}
    vrm = {
        "specVersion": "1.0",
        "meta": {"name": "usd-avatar-runtime session fixture", "version": "1.0", "authors": ["usd-avatar-runtime"],
                 "licenseUrl": LICENSE_URL, "avatarPermission": "everyone"},
        "humanoid": {"humanBones": {name: {"node": node[name]} for name in BONES}},
        "expressions": {"preset": presets},
        "lookAt": look,
    }
    spring = {
        "specVersion": "1.0",
        "colliders": [{"node": node["head"], "shape": {"sphere": {"offset": [0.0, 0.0, 0.0], "radius": 0.05}}}],
        "colliderGroups": [{"name": "Head", "colliders": [0]}],
        "springs": [{"name": "Hair", "colliderGroups": [0], "joints": [
            {"node": node[name], "hitRadius": 0.01, "stiffness": 0.6, "gravityPower": 0.4,
             "gravityDir": [0.0, -1.0, 0.0], "dragForce": 0.3}
            for name in ("hair1", "hair2", "hair3")]}],
    }
    return glb.build({
        "asset": {"version": "2.0", "generator": "usd-avatar-runtime make_session_fixture.py"},
        "scene": 0, "scenes": [{"nodes": [0, 1]}],
        "nodes": [
            {"name": "Body", "mesh": 0, "skin": 0},
            {"name": "Armature", "children": [node["hips"]]},
            *joint_nodes(2),
        ],
        "meshes": [{"name": "Body", "extras": {"targetNames": TARGETS}, "primitives": [
            {"attributes": attributes, "indices": index_accessor, "material": 0, "targets": targets}]}],
        "skins": [{"joints": [node[n] for n, _, _ in JOINTS], "inverseBindMatrices": inverse_accessor, "skeleton": node["hips"]}],
        "materials": [{
            "name": "Body_Mat", "doubleSided": True,
            "pbrMetallicRoughness": {"baseColorFactor": [0.2, 0.3, 0.4, 0.8], "metallicFactor": 0.0, "roughnessFactor": 0.7},
            "emissiveFactor": [0.05, 0.05, 0.05],
            "extensions": {"VRMC_materials_mtoon": {"specVersion": "1.0", "shadeColorFactor": [0.1, 0.1, 0.1]}},
        }],
        "extensionsUsed": ["VRMC_vrm", "VRMC_springBone", "VRMC_materials_mtoon"],
        "extensions": {"VRMC_vrm": vrm, "VRMC_springBone": spring},
    })


def quaternion(axis: tuple[float, float, float], degrees: float) -> tuple[float, float, float, float]:
    half = math.radians(degrees) / 2
    return (axis[0] * math.sin(half), axis[1] * math.sin(half), axis[2] * math.sin(half), math.cos(half))


def clip() -> bytes:
    """A VRMA clip over one second with body motion, two expressions and a moving gaze."""
    glb = Glb()
    nodes = joint_nodes(0)
    bones = {name: i for i, (name, _, _) in enumerate(JOINTS)}
    # The clip's rig is the avatar's, without its hair.
    for entry in nodes:
        if entry["name"] == "head":
            entry["children"] = [c for c in entry["children"] if not JOINTS[c][0].startswith("hair")]
    times = [0.0, 0.25, 0.5, 0.75, 1.0]
    time_accessor = glb.add(FLOAT, "SCALAR", times, bounds=True)
    samplers, channels = [], []

    def track(node: int, path: str, rows: list, kind: str, input_accessor: int = time_accessor) -> None:
        samplers.append({"input": input_accessor, "output": glb.add(FLOAT, kind, rows), "interpolation": "LINEAR"})
        channels.append({"sampler": len(samplers) - 1, "target": {"node": node, "path": path}})

    track(bones["hips"], "translation", [(0.0, 0.9, 0.3 * t) for t in times], "VEC3")
    track(bones["hips"], "rotation", [quaternion((0, 1, 0), 20 * math.sin(2 * math.pi * t)) for t in times], "VEC4")
    track(bones["spine"], "rotation", [quaternion((1, 0, 0), 10 * math.sin(2 * math.pi * t)) for t in times], "VEC4")
    track(bones["head"], "rotation", [quaternion((0, 0, 1), 25 * math.sin(2 * math.pi * t)) for t in times], "VEC4")
    track(bones["leftUpperArm"], "rotation", [quaternion((0, 0, 1), -60 * t) for t in times], "VEC4")
    track(bones["rightUpperArm"], "rotation", [quaternion((0, 0, 1), 60 * t) for t in times], "VEC4")
    track(bones["leftUpperLeg"], "rotation", [quaternion((1, 0, 0), 25 * math.sin(2 * math.pi * t)) for t in times], "VEC4")
    track(bones["rightUpperLeg"], "rotation", [quaternion((1, 0, 0), -25 * math.sin(2 * math.pi * t)) for t in times], "VEC4")
    extra = len(nodes)
    nodes += [{"name": "expression_happy"}, {"name": "expression_blink"}, {"name": "lookAt_target", "translation": [-0.5, 1.5, 1.0]}]
    track(extra, "translation", [(w, 0.0, 0.0) for w in (0.0, 1.0, 0.5, 0.25, 0.0)], "VEC3")
    # A sparse channel, absent before its first key.
    sparse = glb.add(FLOAT, "SCALAR", [0.5, 0.75], bounds=True)
    track(extra + 1, "translation", [(0.0, 0.0, 0.0), (1.0, 0.0, 0.0)], "VEC3", sparse)
    track(extra + 2, "translation", [(-0.5 + t, 1.5 + 0.2 * math.sin(2 * math.pi * t), 1.0) for t in times], "VEC3")
    return glb.build({
        "asset": {"version": "2.0", "generator": "usd-avatar-runtime make_session_fixture.py"},
        "extensionsUsed": ["VRMC_vrm_animation"],
        "extensions": {"VRMC_vrm_animation": {
            "specVersion": "1.0",
            "humanoid": {"humanBones": {name: {"node": bones[name]} for name in BONES}},
            "expressions": {"preset": {"happy": {"node": extra}, "blink": {"node": extra + 1}}},
            "lookAt": {"node": extra + 2, "offsetFromHeadBone": [0.0, 0.06, 0.0]},
        }},
        "scene": 0, "scenes": [{"nodes": [0, extra, extra + 1, extra + 2]}],
        "nodes": nodes,
        "animations": [{"name": "session_fixture", "samplers": samplers, "channels": channels}],
    })


FIXTURES = {
    "avatar_bone.vrm": lambda: avatar("bone"),
    "avatar_expression.vrm": lambda: avatar("expression"),
    "clip.vrma": clip,
}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="fail if a file differs from what this writes")
    args = parser.parse_args()
    stale = False
    for name, build in FIXTURES.items():
        path = ASSETS / name
        data = build()
        if args.check:
            if not path.exists() or path.read_bytes() != data:
                print(f"stale: {path}", file=sys.stderr)
                stale = True
            continue
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
        print(f"wrote {path} ({len(data)} bytes)")
    return 1 if stale else 0


if __name__ == "__main__":
    raise SystemExit(main())
