#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Run the committed MMD fixture through the importer and the installed session.

`avatarMmdSessionCheck` compares the session with the owner, and the committed state is
compared with the expected file. Morph, UV, IK, append and physics feedback must each
change that state, and each input in `broken/` must fail the comparison on what it took
away, which shows the comparison would see that feature break.

`ost library build` must have built avatarMmdSession, with a Jolt SDK on
CMAKE_PREFIX_PATH. The run's state is written to .ost-ci for measuring agreement across
platforms. With --update, this rewrites the expected file instead of comparing with it.
"""
from __future__ import annotations

import argparse
import json
import pathlib
import re
import subprocess
import sys
import tempfile

from check_session_fixture import environment, importers
from compare_avatar_state import Comparison, Tolerances, compare_sequences, report

ROOT = pathlib.Path(__file__).resolve().parents[1]
LIBRARY = ROOT / "libs" / "avatarMmdSession"
ASSETS = ROOT / "tests" / "assets" / "mmd-session"
TICKS = "240"
TOLERANCE = 1e-5
# What the run's report must count, for each feature the fixture exercises.
CHANGED = ("pose_changed", "feedback_changed", "morphs_changed", "uvs_changed")
# Broken input -> (PMX, VMD, text every feature's differences must include).
BROKEN = {
    "IK": ("broken/no-ik.pmx", "motion.vmd", ["upper"]),
    "append": ("broken/no-append.pmx", "motion.vmd", ["follower"]),
    "physics feedback": ("broken/no-feedback.pmx", "motion.vmd", ["swing"]),
    "the later body on a bone": ("broken/earlier-body.pmx", "motion.vmd", ["swing"]),
    "morph and UV": ("avatar.pmx", "broken/no-morph.vmd", ["morphs[", "uvs["]),
}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--update", action="store_true", help="rewrite the expected state")
    args = parser.parse_args()
    fixture = subprocess.run([sys.executable, str(ROOT / "tools" / "make_mmd_session_fixture.py"), "--check"])
    if fixture.returncode:
        return fixture.returncode
    expected_path = ASSETS / "expected.json"
    tolerances = Tolerances(TOLERANCE, TOLERANCE, TOLERANCE, TOLERANCE)
    failed = False
    with tempfile.TemporaryDirectory(ignore_cleanup_errors=True) as temporary:
        directory = pathlib.Path(temporary)
        host, env = environment(*importers(directory, ASSETS), LIBRARY, "avatarMmdSessionCheck")

        def run(pmx: str, vmd: str, name: str) -> tuple[str, list[dict]]:
            output = directory / f"{name}.json"
            command = [str(host), str(ASSETS / pmx), str(ASSETS / vmd), TICKS, "--state-output", str(output)]
            print("+", host.name, pmx, vmd, TICKS, flush=True)
            done = subprocess.run(command, env=env, stdout=subprocess.PIPE, text=True, errors="replace")
            print(done.stdout, end="")
            if done.returncode:
                raise SystemExit(f"{host.name} {pmx} {vmd}: exit {done.returncode}")
            return done.stdout, json.loads(output.read_text(encoding="utf-8"))

        printed, actual = run("avatar.pmx", "motion.vmd", "avatar")
        for count in CHANGED:
            found = re.search(rf"\b{count}=(\d+)", printed)
            if not found or not int(found.group(1)):
                print(f"the fixture did not exercise {count}")
                failed = True
        evidence = ROOT / ".ost-ci"
        evidence.mkdir(exist_ok=True)
        (evidence / "mmd-session-state.json").write_text(json.dumps(actual, indent=1, sort_keys=True) + "\n", encoding="utf-8", newline="\n")
        if args.update:
            expected_path.write_text(json.dumps(actual, indent=1, sort_keys=True) + "\n", encoding="utf-8", newline="\n")
            print(f"wrote {expected_path}")
            return 1 if failed else 0
        expected = json.loads(expected_path.read_text(encoding="utf-8"))
        result = compare_sequences(actual, expected, tolerances, path="avatar", result=Comparison())
        report(result)
        print(f"compared with {expected_path.name} at tolerance {TOLERANCE}")
        failed = failed or bool(result.differences)
        for index, (feature, (pmx, vmd, wanted)) in enumerate(BROKEN.items()):
            _, broken = run(pmx, vmd, f"broken-{index}")
            differences = compare_sequences(broken, expected, tolerances).differences
            missing = [text for text in wanted if not any(text in d for d in differences)]
            print(f"without {feature}: {len(differences)} differences" + (f", none naming {missing}" if missing else ""))
            failed = failed or bool(missing)
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
