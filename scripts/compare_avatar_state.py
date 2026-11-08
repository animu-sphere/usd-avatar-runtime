#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Compare two sequences of evaluated state, as `--state-output` writes them.

Implements the parity rules in docs/architecture/OUTPUT_PATHS.md#4-parity-evidence.
Records are matched by identity, so a consumer may hold them in its own order. A
missing channel or target is a difference; a consumer that does not carry a channel
declares it with --unsupported, and the report names it. Each sequence is also checked
against the consumer contract on its own: snapshot order, fixed layout and capabilities
per instance, and the shape of every value.

The exit status is 0 when the states agree, 1 when they differ, and 2 for
invalid arguments or an unreadable/nonfinite JSON input.
"""
from __future__ import annotations

import argparse
import dataclasses
import json
import math
import struct

IDENTITY_FIELDS = ("layout", "layout_version", "generation", "frame", "input_revision")
# Channel -> the fields that identify one record of it.
CHANNELS = {
    "joints": ("skeleton", "joint"),
    "morphs": ("mesh", "target"),
    "uvs": ("mesh", "vertex"),
    "materials": ("material", "input"),
    "visibility": ("target",),
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

    def __post_init__(self) -> None:
        for field in dataclasses.fields(self):
            value = getattr(self, field.name)
            if field.name == "material" and value is None:
                continue
            if not math.isfinite(value) or value < 0:
                raise ValueError(f"{field.name} tolerance must be finite and nonnegative")


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
    if not math.isfinite(actual) or not math.isfinite(expected):
        result.differences.append(f"{path}: nonfinite value ({actual}, expected {expected})")
        return
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
        found = actual["value"][i]
        if not math.isfinite(found) or not math.isfinite(value):
            result.differences.append(f"{path}.value[{i}]: nonfinite value ({found}, expected {value})")
            continue
        try:
            narrowed, wanted = _float32(found), _float32(value)
        except OverflowError:
            result.differences.append(f"{path}.value[{i}]: value outside float32 range ({found}, expected {value})")
            continue
        result.count("material", abs(narrowed - wanted))
        if narrowed != wanted:
            result.differences.append(f"{path}.value[{i}]: {found}, expected {value}")


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
            elif channel == "uvs":
                _values(f"{where}.value", "uv", found[key]["value"], wanted[key]["value"], tolerances.weight, result)
            elif channel == "materials":
                _material(where, found[key], wanted[key], tolerances, result)
            elif channel == "visibility":
                if found[key]["visible"] != wanted[key]["visible"]:
                    result.differences.append(f"{where}.visible: {found[key]['visible']}, expected {wanted[key]['visible']}")
            else:
                _sample(where, found[key], wanted[key], tolerances, result)


# Squared-norm error a published rotation may have (docs/contracts/EVALUATED_STATE.md).
UNIT_QUATERNION = 1e-6


def validate_state(path: str, state: dict, result: Comparison, layout_order: bool) -> None:
    """The rules every snapshot keeps, whoever wrote it.

    A consumer may hold records in its own order, while `parent` stays an index into the
    producer's layout, so parent order is checked only where records are in layout order.
    """
    if not state["layout"] or state["layout_version"] < 1:
        result.differences.append(f"{path}: layout {state['layout']!r} version {state['layout_version']} is not an identity")
    if state["frame"] < 1 or state["generation"] < 1:
        result.differences.append(f"{path}: frame {state['frame']} generation {state['generation']} must be nonzero")
    if not state.get("instance"):
        result.differences.append(f"{path}.instance: {state.get('instance', 'missing')} is not an instance")
    if "capabilities" in state and len(set(state["capabilities"])) != len(state["capabilities"]):
        result.differences.append(f"{path}.capabilities: an identity appears twice")
    first: dict[str, int] = {}
    for i, joint in enumerate(state.get("joints", ())):
        where = f"{path}.joints[{joint['skeleton']}/{joint['joint']}]"
        first.setdefault(joint["skeleton"], i)
        parent = joint["parent"]
        if layout_order and parent != -1 and not (first[joint["skeleton"]] <= parent < i and state["joints"][parent]["skeleton"] == joint["skeleton"]):
            result.differences.append(f"{where}.parent: {parent} is not -1 or an earlier joint of the same skeleton")
        if abs(sum(value * value for value in joint["rotation"]) - 1) > UNIT_QUATERNION:
            result.differences.append(f"{where}.rotation: not a unit quaternion")
    for material in state.get("materials", ()):
        where = f"{path}.materials[{material['material']}/{material['input']}]"
        if material["overridden"] not in (0, 1):
            result.differences.append(f"{where}.overridden: {material['overridden']} is not 0 or 1")
        if material["type"] not in (1, 2, 3, 4):
            result.differences.append(f"{where}.type: {material['type']} is not a value type")
        elif any(material["value"][material["type"]:]):
            result.differences.append(f"{where}.value: components past type {material['type']} are not zero")
    for visibility in state.get("visibility", ()):
        if visibility["visible"] not in (0, 1):
            result.differences.append(f"{path}.visibility[{visibility['target']}].visible: {visibility['visible']} is not 0 or 1")


def _layout(state: dict) -> tuple:
    """What a binding depends on: identities, their order, joint parents and value types."""
    channels = []
    for channel, fields in CHANNELS.items():
        if channel == "samples" or channel not in state:
            continue
        extra = {"joints": ("parent",), "materials": ("type",)}.get(channel, ())
        channels.append((channel, sorted(tuple(record[f] for f in (*fields, *extra)) for record in state[channel])))
    return state["layout"], state["layout_version"], tuple(channels)


def validate_sequence(path: str, states: list[dict], result: Comparison, layout_order: bool = False) -> None:
    """Snapshots in the order a consumer received them, under the lifecycle rules.

    Per instance, (generation, frame) increases strictly, and the layout tuple, the
    channels it names and the capabilities stay fixed: a reset keeps them, and a change
    of binding needs a new instance.
    """
    seen: dict[object, tuple[int, dict]] = {}
    for i, state in enumerate(states):
        where = f"{path}[{i}]"
        validate_state(where, state, result, layout_order)
        instance = state.get("instance")
        if instance not in seen:
            seen[instance] = (i, state)
            continue
        j, last = seen[instance]
        seen[instance] = (i, state)
        if (state["generation"], state["frame"]) <= (last["generation"], last["frame"]):
            result.differences.append(f"{where}: generation {state['generation']} frame {state['frame']} does not follow "
                                      f"generation {last['generation']} frame {last['frame']} of {path}[{j}]")
        if _layout(state) != _layout(last):
            result.differences.append(f"{where}: the layout changed within instance {instance}")
        if sorted(state.get("capabilities", ())) != sorted(last.get("capabilities", ())):
            result.differences.append(f"{where}.capabilities: changed within instance {instance}")


def _instances(path: str, actual: list[dict], expected: list[dict], result: Comparison) -> None:
    """Instance handles differ between runs; they must correspond one to one, never cross."""
    forward: dict[object, object] = {}
    backward: dict[object, object] = {}
    for i, (found, wanted) in enumerate(zip(actual, expected)):
        a, e = found.get("instance"), wanted.get("instance")
        # Populate both directions even when the forward correspondence fails.
        # A reused handle at the first frame of a recreated instance must be a
        # difference, not a KeyError while formatting its diagnostic.
        mapped = forward.setdefault(a, e)
        reverse = backward.setdefault(e, a)
        if mapped != e or reverse != a:
            result.differences.append(f"{path}[{i}].instance: {a} corresponds to expected {e}, "
                                      f"but earlier {a} was {forward[a]} and {e} was {backward[e]}")


def compare_sequences(actual: list[dict], expected: list[dict], tolerances: Tolerances = Tolerances(),
                      unsupported: tuple[str, ...] = (), path: str = "state", result: Comparison | None = None) -> Comparison:
    result = result or Comparison(unsupported=unsupported)
    if len(actual) != len(expected):
        result.differences.append(f"{path}: {len(actual)} states, expected {len(expected)}")
    validate_sequence(path, actual, result)
    validate_sequence(f"expected {path}", expected, result, layout_order=True)
    _instances(path, actual, expected, result)
    for i, (found, wanted) in enumerate(zip(actual, expected)):
        compare_state(f"{path}[{i}]", found, wanted, tolerances, result)
    return result


def _reject_constant(name: str) -> None:
    raise ValueError(f"nonfinite value {name}")


def _finite_float(value: str) -> float:
    number = float(value)
    if not math.isfinite(number):
        raise ValueError(f"nonfinite value {value}")
    return number


def load(path: str) -> list[dict]:
    with open(path, encoding="utf-8") as file:
        states = json.load(file, parse_constant=_reject_constant, parse_float=_finite_float)
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
    try:
        tolerances = Tolerances() if args.tolerance is None else Tolerances(args.tolerance, args.tolerance, args.tolerance, args.tolerance)
        actual, expected = load(args.actual), load(args.expected)
    except (OSError, ValueError) as error:
        parser.error(str(error))
    result = compare_sequences(actual, expected, tolerances, tuple(args.unsupported))
    report(result)
    return 1 if result.differences else 0


if __name__ == "__main__":
    raise SystemExit(main())
