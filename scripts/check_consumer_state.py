#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Compare hydra-toon's accepted-input export with an independent fixture oracle.

Pass a built toon-avatar-state-test executable. It exercises the installed-style
consumer API without OpenUSD, format providers or a GPU. Renderer mapping tests
belong to hydra-toon; this host owns file-format and identity comparison.
"""
from __future__ import annotations

import argparse
import copy
import json
import math
import pathlib
import subprocess

from compare_avatar_state import compare_sequences, load, report

ROOT = pathlib.Path(__file__).resolve().parents[1]


def expected_states() -> list[dict]:
    states = []
    for step in range(6):
        animated = step in (1, 4)
        t = 1 if animated else 0
        states.append({
            "instance": (1 << 64) - (2 if step < 3 else 3),
            "layout": 'capture."\\日', "layout_version": 1,
            "frame": (1, 2, 1, 1, 2, 3)[step], "generation": 2 if step == 2 else 1,
            "input_revision": (1, 2, 1, 1, 2, 3)[step] * 10,
            "seconds": t, "capabilities": [{"id": "test.resolved", "version": 7}],
            "joints": [
                {"skeleton": "rig", "joint": 'root"\\', "parent": -1,
                 "translation": [.05 * t, 0, 0], "rotation": [0, 0, math.sin(.1 * t), math.cos(.1 * t)], "scale": [1, 1, 1]},
                {"skeleton": "rig", "joint": "eye", "parent": 0,
                 "translation": [.02 * t, .1, 0], "rotation": [0, 0, 0, 1], "scale": [1, 1, 1]},
            ],
            "morphs": [{"mesh": "face", "target": "resolved.a", "weight": -.2 * t},
                       {"mesh": "face", "target": "resolved.b", "weight": .3 * t}],
            "uvs": [{"mesh": "face", "vertex": vertex, "value": [vertex * .25 + t * .125, vertex * .5]}
                    for vertex in (2, 0, 1)],
            "materials": [
                {"material": "surface", "input": "owner:base", "type": 4, "overridden": t, "value": [.7 - .1 * t, .3, .2, 1]},
                {"material": "surface", "input": "owner:shade", "type": 3, "overridden": t, "value": [.2, .1 + .05 * t, .05, 0]},
            ],
            "visibility": [{"target": "face", "visible": 0 if animated else 1}],
            "samples": [
                {"source": 'source"\\', "actor": "actor", "channel": 'pose:clip"\\', "evaluator": 'test.provider"\\日',
                 "kind": 3, "validity": 1, "resolution": 3, "source_seconds": .25,
                 "clock_scale": 2, "clock_offset": -.125, "runtime_seconds": .375},
                {"source": "other", "actor": "actor", "channel": 'pose:clip"\\', "evaluator": "test.other",
                 "kind": 3, "validity": 1, "resolution": 2, "source_seconds": .5,
                 "clock_scale": .5, "clock_offset": -2, "runtime_seconds": -1.75},
                {"source": 'source"\\', "actor": "other actor", "channel": 'pose:clip"\\', "evaluator": "",
                 "kind": 1, "validity": 1, "resolution": 1, "source_seconds": 0,
                 "clock_scale": 1, "clock_offset": 0, "runtime_seconds": 0},
            ],
        })
    return states


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("consumer", type=pathlib.Path, help="built toon-avatar-state-test")
    parser.add_argument("--output", type=pathlib.Path, default=ROOT / ".ost-ci" / "direct-consumer-state.json")
    args = parser.parse_args()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run([str(args.consumer.resolve()), "--state-output", str(args.output.resolve())], check=True)
    actual, expected = load(args.output), expected_states()
    args.output.with_name(args.output.stem + "-expected.json").write_text(
        json.dumps(expected, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    result = compare_sequences(actual, expected)
    report(result)
    if result.differences:
        return 1
    # Deliberately break delivery while preserving the other values. Every
    # omission must be detected; shared writer bugs cannot supply the oracle.
    for channel in ("joints", "morphs", "uvs", "materials", "visibility", "samples", "capabilities"):
        broken = copy.deepcopy(actual)
        del broken[1][channel]
        if not compare_sequences(broken, expected).differences:
            raise AssertionError(f"lost {channel} was not detected")
    for step in (2, 3):
        broken = copy.deepcopy(actual)
        broken[step] = copy.deepcopy(actual[0])
        if not compare_sequences(broken, expected).differences:
            raise AssertionError("stale reset or recreated instance was not detected")
    broken = copy.deepcopy(actual)
    broken[1]["visibility"][0]["visible"] = 1
    if not compare_sequences(broken, expected).differences:
        raise AssertionError("changed visibility was not detected")
    controls = 10
    for field in ("source", "actor", "evaluator", "clock_scale", "clock_offset", "runtime_seconds"):
        broken = copy.deepcopy(actual)
        record = broken[1]["samples"][0]
        record[field] = "changed" if isinstance(record[field], str) else record[field] + .125
        if not compare_sequences(broken, expected).differences:
            raise AssertionError(f"changed sample {field} was not detected")
        controls += 1
    broken = copy.deepcopy(actual)
    broken[1]["capabilities"][0]["version"] += 1
    if not compare_sequences(broken, expected).differences:
        raise AssertionError("changed capability version was not detected")
    controls += 1
    print(f"6 accepted consumer states agree; {controls} delivery/identity/provenance controls refused")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
