#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""What the state comparison must catch and must allow, over the committed fixture state."""
from __future__ import annotations

import copy
import json
import pathlib
import unittest

from compare_avatar_state import OPTIONAL_CHANNELS, Tolerances, compare_sequences

EXPECTED = pathlib.Path(__file__).resolve().parents[1] / "tests" / "assets" / "vrm-session" / "expected.json"


class CompareAvatarState(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.fixture = json.loads(EXPECTED.read_text(encoding="utf-8"))["bone"]

    def setUp(self) -> None:
        self.expected = copy.deepcopy(self.fixture)
        self.actual = copy.deepcopy(self.fixture)
        self.state = self.actual[-1]

    def differences(self, tolerances: Tolerances = Tolerances(), unsupported: tuple[str, ...] = ()) -> list[str]:
        return compare_sequences(self.actual, self.expected, tolerances, unsupported).differences

    def assertDiffers(self, text: str, differences: list[str] | None = None) -> None:
        differences = self.differences() if differences is None else differences
        self.assertTrue(any(text in d for d in differences), f"no difference names {text!r}: {differences}")

    def overridden_material(self) -> dict:
        return next(m for m in self.state["materials"] if m["overridden"])

    def test_the_same_state_agrees(self) -> None:
        self.assertEqual(self.differences(), [])

    def test_the_fixture_exercises_every_channel(self) -> None:
        # A VRM session publishes no primary UVs or visibility; tests below add them.
        for channel in set(OPTIONAL_CHANNELS) - {"uvs", "visibility"}:
            self.assertTrue(self.state[channel], channel)
        self.overridden_material()

    def add_to_layout(self, channel: str, records: list[dict]) -> None:
        for state in (*self.actual, *self.expected):
            state[channel] = copy.deepcopy(records)

    def test_a_uv_is_compared(self) -> None:
        self.add_to_layout("uvs", [{"mesh": "/m", "vertex": 0, "value": [0.25, 0.5]}, {"mesh": "/m", "vertex": 1, "value": [0.5, 0.5]}])
        self.state["uvs"].reverse()
        self.assertEqual(self.differences(), [])
        self.state["uvs"][0]["value"][1] += 0.01
        self.assertDiffers("uvs[/m/1].value[1]")

    def test_record_order_does_not_matter(self) -> None:
        for channel in ("joints", "morphs", "uvs", "materials", "visibility", "samples", "capabilities"):
            self.state[channel].reverse()
        self.assertEqual(self.differences(), [])

    def test_a_negated_quaternion_is_the_same_rotation(self) -> None:
        joint = self.state["joints"][0]
        joint["rotation"] = [-value for value in joint["rotation"]]
        self.assertEqual(self.differences(), [])

    def test_each_value_kind_is_compared(self) -> None:
        def perturb(mutate) -> list[str]:
            self.setUp()
            mutate(self.state)
            return self.differences()

        def add(record: dict, field: str, index: int | None = None) -> None:
            if index is None:
                record[field] += 0.01
            else:
                record[field][index] += 0.01

        self.assertDiffers("translation[0]", perturb(lambda s: add(s["joints"][0], "translation", 0)))
        self.assertDiffers("rotation[0]", perturb(lambda s: add(s["joints"][0], "rotation", 0)))
        self.assertDiffers("scale[0]", perturb(lambda s: add(s["joints"][0], "scale", 0)))
        self.assertDiffers("weight", perturb(lambda s: add(s["morphs"][0], "weight")))
        self.assertDiffers("value[0]", perturb(lambda s: add(next(m for m in s["materials"] if m["overridden"]), "value", 0)))
        self.assertDiffers("source_seconds", perturb(lambda s: add(s["samples"][0], "source_seconds")))
        self.assertDiffers("seconds", perturb(lambda s: add(s, "seconds")))

    def test_tolerances_apply_per_value_kind(self) -> None:
        self.state["morphs"][0]["weight"] += 1e-6
        self.assertEqual(self.differences(), [])
        self.state["morphs"][0]["weight"] += 3e-6
        self.assertEqual(len(self.differences()), 1)
        self.assertEqual(self.differences(Tolerances(weight=1e-5)), [])

    def test_a_material_matches_after_float_conversion_only(self) -> None:
        material = self.overridden_material()
        value = material["value"][0]
        material["value"][0] = value + 1e-12
        self.assertEqual(self.differences(), [])
        material["value"][0] = value + 1e-4
        self.assertEqual(len(self.differences()), 1)

    def test_a_value_without_an_override_is_ignored(self) -> None:
        material = self.overridden_material()
        material["overridden"] = 0
        self.expected = copy.deepcopy(self.actual)
        material["value"][0] += 1
        self.assertEqual(self.differences(), [])

    def test_a_changed_override_flag_differs(self) -> None:
        material = self.overridden_material()
        material["overridden"] = 0
        self.assertDiffers("overridden")

    def test_missing_and_unexpected_targets_differ(self) -> None:
        removed = self.state["morphs"].pop()
        self.assertDiffers("is missing")
        self.state["morphs"].append(removed | {"target": "unexpected"})
        self.assertDiffers("is missing")
        self.assertDiffers("is unexpected")

    def test_a_duplicate_identity_differs(self) -> None:
        self.state["morphs"].append(copy.deepcopy(self.state["morphs"][0]))
        self.assertDiffers("appears twice")

    def test_a_missing_channel_differs_until_declared(self) -> None:
        for state in self.actual:
            del state["materials"]
        self.assertDiffers("not declared unsupported")
        result = compare_sequences(self.actual, self.expected, unsupported=("materials",))
        self.assertEqual(result.differences, [])
        self.assertEqual(result.unsupported, ("materials",))

    def test_frame_identity_differs(self) -> None:
        for field in ("layout", "layout_version", "generation", "frame"):
            self.setUp()
            self.state[field] = 99
            self.assertDiffers(field)

    def test_a_missing_state_differs(self) -> None:
        self.actual.pop()
        self.assertDiffers("states")

    # Identity: instance, frame order and layout, under the consumer contract.

    def test_a_visibility_is_compared(self) -> None:
        self.add_to_layout("visibility", [{"target": "/a", "visible": 1}, {"target": "/b", "visible": 0}])
        self.assertEqual(self.differences(), [])
        self.state["visibility"][1]["visible"] = 1
        self.assertDiffers("visibility[/b].visible")
        self.state["visibility"][1]["visible"] = 2
        self.assertDiffers("not 0 or 1")

    def test_the_input_revision_is_compared(self) -> None:
        self.state["input_revision"] += 1
        self.assertDiffers("input_revision")

    def test_instances_correspond_but_need_not_be_equal(self) -> None:
        for state in self.actual:
            state["instance"] = 7
        self.assertEqual(self.differences(), [])

    def test_a_missing_or_zero_instance_differs(self) -> None:
        del self.state["instance"]
        self.assertDiffers("instance: missing")
        self.state["instance"] = 0
        self.assertDiffers("instance: 0")

    def two_instances(self) -> None:
        """The fixture's frames, alternating between two instances, as one thread sees them."""
        for states in (self.actual, self.expected):
            states[:] = [copy.deepcopy(s) | {"instance": instance} for s in states for instance in (1, 2)]
        self.state = self.actual[-1]

    def test_two_instances_on_one_thread_agree(self) -> None:
        self.two_instances()
        self.assertEqual(self.differences(), [])

    def test_instances_must_not_cross(self) -> None:
        self.two_instances()
        self.actual[-1]["instance"], self.actual[-2]["instance"] = 1, 2
        self.assertDiffers("corresponds to expected")

    def test_frames_of_an_instance_must_advance(self) -> None:
        self.actual[1]["frame"] = self.actual[0]["frame"]
        self.assertDiffers("does not follow")
        self.setUp()
        self.actual[0]["frame"] = 0
        self.assertDiffers("must be nonzero")

    def test_a_reset_restarts_frames_in_the_next_generation(self) -> None:
        for states in (self.actual, self.expected):
            for i, state in enumerate(states[2:]):
                state["generation"] += 1
                state["frame"] = 1 + 30 * i
        self.assertEqual(self.differences(), [])

    def test_an_older_generation_differs(self) -> None:
        for states in (self.actual, self.expected):
            states[-1]["generation"] = 0
        self.assertDiffers("must be nonzero")
        self.setUp()
        for states in (self.actual, self.expected):
            states[0]["generation"] = 2
        self.assertDiffers("does not follow")

    def test_a_layout_change_within_an_instance_differs(self) -> None:
        for change in (lambda s: s.update(layout_version=2), lambda s: s["morphs"].pop(),
                       lambda s: s["joints"][1].update(parent=-1), lambda s: s["materials"][0].update(type=4)):
            self.setUp()
            for states in (self.actual, self.expected):
                change(states[-1])
            self.assertDiffers("layout changed")

    def test_a_layout_change_with_a_new_instance_is_allowed(self) -> None:
        for states in (self.actual, self.expected):
            states[-1]["instance"] = 2
            states[-1]["layout_version"] = 2
            states[-1]["morphs"].pop()
        self.assertEqual(self.differences(), [])

    def test_capabilities_are_fixed_per_instance(self) -> None:
        for states in (self.actual, self.expected):
            states[-1]["capabilities"].pop()
        self.assertDiffers("capabilities: changed")

    # Absence: an empty channel, a missing one, zero and no override stay apart.

    def test_an_empty_channel_differs_from_a_populated_one(self) -> None:
        self.state["morphs"] = []
        self.assertDiffers("is missing")

    def test_an_empty_channel_differs_from_a_missing_one(self) -> None:
        for states in (self.actual, self.expected):
            for state in states:
                state["visibility"] = []
        self.assertEqual(self.differences(), [])
        del self.state["visibility"]
        self.assertDiffers("visibility: missing")

    def test_zero_is_a_value_not_an_absence(self) -> None:
        morph = next(m for m in self.state["morphs"] if m["weight"])
        morph["weight"] = 0
        self.assertDiffers("weight")

    # Shape: what no snapshot may hold.

    def test_invalid_values_differ(self) -> None:
        cases = {
            "not a unit quaternion": lambda s: s["joints"][0].update(rotation=[0, 0, 0, 2]),
            "earlier joint": lambda s: s["joints"][0].update(parent=5),
            "is not 0 or 1": lambda s: s["materials"][0].update(overridden=2),
            "past type": lambda s: s["materials"][0]["value"].__setitem__(3, 1),
            "is not a value type": lambda s: s["materials"][0].update(type=5),
            "appears twice": lambda s: s["capabilities"].append(s["capabilities"][0]),
            "is not an identity": lambda s: s.update(layout=""),
        }
        for text, change in cases.items():
            self.setUp()
            for states in (self.actual, self.expected):
                change(states[-1])
            self.assertDiffers(text)


if __name__ == "__main__":
    unittest.main()
