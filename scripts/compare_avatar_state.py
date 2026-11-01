#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Compare two sequences of evaluated state, as `--state-output` writes them.

Implements the parity rules in docs/architecture/OUTPUT_PATHS.md#4-parity-evidence.
Records are matched by identity, so a consumer may hold them in its own order. A
missing channel or target is a difference; a consumer that does not carry a channel
declares it with --unsupported, and the report names it.

The exit status is 0 when the states agree, 1 when they differ.
"""
from __future__ import annotations

import argparse
import dataclasses
import json
import math
import struct

IDENTITY_FIELDS = ("layout", "layout_version", "generation", "frame")
# Channel -> the fields that identify one record of it.
CHANNELS = {
    "joints": ("skeleton", "joint"),
    "morphs": ("mesh", "target"),
    "materials": ("material", "input"),
    "samples": ("channel",),
}
OPTIONAL_CHANNELS = (*CHANNELS, "capabilities")


@dataclasses.dataclass(frozen=True)
class Tolerances:
    """Relative to max(1, |expected|). A material of None compares as float32, exactly."""

    transform: float = 2e-6
    weight: float = 2e-6
    material: float | None = None
    seconds: float = 2e-6


@dataclasses.dataclass
class Comparison:
    differences: list[str] = dataclasses.field(default_factory=list)
    compared: dict[str, int] = dataclasses.field(default_factory=dict)
    largest: dict[str, float] = dataclasses.field(default_factory=dict)
    unsupported: tuple[str, ...] = ()

    def count(self, kind: str, error: float) -> None:
        self.compared[kind] = self.compared.get(kind, 0) + 1
        self.largest[kind] = max(self.largest.get(kind, 0.0), error)


def _close(path: str, kind: str, actual: float, expected: float, tolerance: float, result: Comparison) -> None:
    error = abs(actual - expected) / max(1.0, abs(expected))
    result.count(kind, error)
    if error > tolerance:
        result.differences.append(f"{path}: {actual}, expected {expected}")


def _values(path: str, kind: str, actual: list[float], expected: list[float], tolerance: float, result: Comparison) -> None:
    if len(actual) != len(expected):
        result.differences.append(f"{path}: {len(actual)} components, expected {len(expected)}")
        return
    for i, value in enumerate(expected):
        _close(f"{path}[{i}]", kind, actual[i], value, tolerance, result)


def _joint(path: str, actual: dict, expected: dict, tolerances: Tolerances, result: Comparison) -> None:
    if actual["parent"] != expected["parent"]:
        result.differences.append(f"{path}.parent: {actual['parent']}, expected {expected['parent']}")
    # A quaternion and its negation are the same rotation.
    rotation = actual["rotation"]
    if sum(a * b for a, b in zip(rotation, expected["rotation"])) < 0:
        rotation = [-value for value in rotation]
    _values(f"{path}.translation", "translation", actual["translation"], expected["translation"], tolerances.transform, result)
    _values(f"{path}.rotation", "rotation", rotation, expected["rotation"], tolerances.transform, result)
    _values(f"{path}.scale", "scale", actual["scale"], expected["scale"], tolerances.transform, result)


def _float32(value: float) -> float:
    return struct.unpack("f", struct.pack("f", value))[0]


def _material(path: str, actual: dict, expected: dict, tolerances: Tolerances, result: Comparison) -> None:
    for field in ("type", "overridden"):
        if actual[field] != expected[field]:
            result.differences.append(f"{path}.{field}: {actual[field]}, expected {expected[field]}")
            return
    # Without an override the value means nothing.
    if not expected["overridden"]:
        return
    if tolerances.material is not None:
        _values(f"{path}.value", "material", actual["value"], expected["value"], tolerances.material, result)
        return
    if len(actual["value"]) != len(expected["value"]):
        result.differences.append(f"{path}.value: {len(actual['value'])} components, expected {len(expected['value'])}")
        return
    for i, value in enumerate(expected["value"]):
        result.count("material", abs(_float32(actual["value"][i]) - _float32(value)))
        if _float32(actual["value"][i]) != _float32(value):
            result.differences.append(f"{path}.value[{i}]: {actual['value'][i]}, expected {value}")


def _sample(path: str, actual: dict, expected: dict, tolerances: Tolerances, result: Comparison) -> None:
    for field in ("kind", "validity", "resolution"):
        if actual[field] != expected[field]:
            result.differences.append(f"{path}.{field}: {actual[field]}, expected {expected[field]}")
    _close(f"{path}.source_seconds", "seconds", actual["source_seconds"], expected["source_seconds"], tolerances.seconds, result)


def _keyed(path: str, channel: str, records: list[dict], result: Comparison) -> dict[tuple, dict]:
    keyed: dict[tuple, dict] = {}
    for record in records:
        key = tuple(record[field] for field in CHANNELS[channel])
        if key in keyed:
            result.differences.append(f"{path}.{channel}: {key} appears twice")
        keyed[key] = record
    return keyed


def compare_state(path: str, actual: dict, expected: dict, tolerances: Tolerances, result: Comparison) -> None:
    for field in IDENTITY_FIELDS:
        if actual[field] != expected[field]:
            result.differences.append(f"{path}.{field}: {actual[field]}, expected {expected[field]}")
    _close(f"{path}.seconds", "seconds", actual["seconds"], expected["seconds"], tolerances.seconds, result)
    for channel in OPTIONAL_CHANNELS:
        if channel in result.unsupported:
            continue
        if channel not in actual or channel not in expected:
            result.differences.append(f"{path}.{channel}: missing, and not declared unsupported")
            continue
        if channel == "capabilities":
            if sorted(actual[channel]) != sorted(expected[channel]):
                result.differences.append(f"{path}.capabilities: {sorted(actual[channel])}, expected {sorted(expected[channel])}")
            continue
        found = _keyed(path, channel, actual[channel], result)
        wanted = _keyed(path, channel, expected[channel], result)
        for key in sorted(wanted.keys() - found.keys()):
            result.differences.append(f"{path}.{channel}: {key} is missing")
        for key in sorted(found.keys() - wanted.keys()):
            result.differences.append(f"{path}.{channel}: {key} is unexpected")
        for key in sorted(found.keys() & wanted.keys()):
            where = f"{path}.{channel}[{'/'.join(map(str, key))}]"
            if channel == "joints":
                _joint(where, found[key], wanted[key], tolerances, result)
            elif channel == "morphs":
                _close(f"{where}.weight", "weight", found[key]["weight"], wanted[key]["weight"], tolerances.weight, result)
            elif channel == "materials":
                _material(where, found[key], wanted[key], tolerances, result)
            else:
                _sample(where, found[key], wanted[key], tolerances, result)


def compare_sequences(actual: list[dict], expected: list[dict], tolerances: Tolerances = Tolerances(),
                      unsupported: tuple[str, ...] = (), path: str = "state", result: Comparison | None = None) -> Comparison:
    result = result or Comparison(unsupported=unsupported)
    if len(actual) != len(expected):
        result.differences.append(f"{path}: {len(actual)} states, expected {len(expected)}")
    for i, (found, wanted) in enumerate(zip(actual, expected)):
        compare_state(f"{path}[{i}]", found, wanted, tolerances, result)
    return result


def _reject_constant(name: str) -> None:
    raise ValueError(f"nonfinite value {name}")


def load(path: str) -> list[dict]:
    with open(path, encoding="utf-8") as file:
        states = json.load(file, parse_constant=_reject_constant)
    return states if isinstance(states, list) else [states]


def report(result: Comparison, limit: int = 40) -> None:
    for difference in result.differences[:limit]:
        print(difference)
    if len(result.differences) > limit:
        print(f"... and {len(result.differences) - limit} more")
    for kind in sorted(result.compared):
        print(f"{kind}: {result.compared[kind]} values, largest error {result.largest[kind]:.3g}")
    if result.unsupported:
        print("not compared, declared unsupported:", ", ".join(result.unsupported))
    print(f"{len(result.differences)} differences")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("actual", help="the consumer's state: one state or a list")
    parser.add_argument("expected", help="the state it is compared with")
    parser.add_argument("--unsupported", action="append", default=[], choices=OPTIONAL_CHANNELS,
                        help="a channel the consumer does not carry")
    parser.add_argument("--tolerance", type=float, help="use this relative tolerance for every value, materials included")
    args = parser.parse_args()
    tolerances = Tolerances() if args.tolerance is None else Tolerances(args.tolerance, args.tolerance, args.tolerance, args.tolerance)
    result = compare_sequences(load(args.actual), load(args.expected), tolerances, tuple(args.unsupported))
    report(result)
    return 1 if result.differences else 0


if __name__ == "__main__":
    raise SystemExit(main())
