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
        cls.expected = json.loads(EXPECTED.read_text(encoding="utf-8"))["bone"]

    def setUp(self) -> None:
        self.actual = copy.deepcopy(self.expected)
        self.state = self.actual[-1]

    def differences(self, tolerances: Tolerances = Tolerances(), unsupported: tuple[str, ...] = ()) -> list[str]:
        return compare_sequences(self.actual, self.expected, tolerances, unsupported).differences

    def overridden_material(self) -> dict:
        return next(m for m in self.state["materials"] if m["overridden"])

    def test_the_same_state_agrees(self) -> None:
        self.assertEqual(self.differences(), [])

    def test_the_fixture_exercises_every_channel(self) -> None:
        # A VRM session publishes no primary UVs; test_a_uv_is_compared adds them.
        for channel in set(OPTIONAL_CHANNELS) - {"uvs"}:
            self.assertTrue(self.state[channel], channel)
        self.overridden_material()

    def test_a_uv_is_compared(self) -> None:
        for state in (self.state, self.expected[-1]):
            state["uvs"] = [{"mesh": "/m", "vertex": 0, "value": [0.25, 0.5]}, {"mesh": "/m", "vertex": 1, "value": [0.5, 0.5]}]
        self.state["uvs"].reverse()
        self.assertEqual(self.differences(), [])
        self.state["uvs"][0]["value"][1] += 0.01
        self.assertIn("uvs[/m/1].value[1]", self.differences()[0])

    def test_record_order_does_not_matter(self) -> None:
        for channel in ("joints", "morphs", "uvs", "materials", "samples", "capabilities"):
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

        self.assertIn("translation[0]", perturb(lambda s: add(s["joints"][0], "translation", 0))[0])
        self.assertIn("rotation[0]", perturb(lambda s: add(s["joints"][0], "rotation", 0))[0])
        self.assertIn("scale[0]", perturb(lambda s: add(s["joints"][0], "scale", 0))[0])
        self.assertIn("weight", perturb(lambda s: add(s["morphs"][0], "weight"))[0])
        self.assertIn("value[0]", perturb(lambda s: add(next(m for m in s["materials"] if m["overridden"]), "value", 0))[0])
        self.assertIn("source_seconds", perturb(lambda s: add(s["samples"][0], "source_seconds"))[0])
        self.assertIn("seconds", perturb(lambda s: add(s, "seconds"))[0])

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
        self.assertIn("overridden", self.differences()[0])

    def test_missing_and_unexpected_targets_differ(self) -> None:
        removed = self.state["morphs"].pop()
        self.assertIn("is missing", self.differences()[0])
        self.state["morphs"].append(removed | {"target": "unexpected"})
        self.assertEqual(sorted("missing" in d for d in self.differences()), [False, True])

    def test_a_duplicate_identity_differs(self) -> None:
        self.state["morphs"].append(copy.deepcopy(self.state["morphs"][0]))
        self.assertIn("appears twice", self.differences()[0])

    def test_a_missing_channel_differs_until_declared(self) -> None:
        del self.state["materials"]
        self.assertIn("not declared unsupported", self.differences()[0])
        result = compare_sequences(self.actual, self.expected, unsupported=("materials",))
        self.assertEqual(result.differences, [])
        self.assertEqual(result.unsupported, ("materials",))

    def test_frame_identity_differs(self) -> None:
        for field in ("layout", "layout_version", "generation", "frame"):
            self.setUp()
            self.state[field] = 99
            self.assertIn(field, self.differences()[0])

    def test_a_missing_state_differs(self) -> None:
        self.actual.pop()
        self.assertIn("states", self.differences()[0])


if __name__ == "__main__":
    unittest.main()
