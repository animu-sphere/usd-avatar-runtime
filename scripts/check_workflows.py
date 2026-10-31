#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Fail when a workflow drifts from what this repository's CI relies on.

Regenerating the pull request lanes never updates the hand-written workflows, so a
green generated lane says nothing about their pins. `ost ci generate` has no path
filter, so regenerating `ost-source-ci.yml` drops the one added to its trigger.
"""
from __future__ import annotations

import pathlib
import re

ROOT = pathlib.Path(__file__).resolve().parents[1]
WORKFLOWS = ROOT / ".github" / "workflows"
PINNED = ("release.yml", "session-ci.yml")
# Workflow -> triggers that skip a change touching only documentation.
FILTERED = {"ost-source-ci.yml": 1, "plain-cmake.yml": 2, "session-ci.yml": 1}
DOCUMENTATION_FILTER = """    paths-ignore:
      - "docs/**"
      - "**.md"
      - "LICENSE"
"""


def main() -> int:
    contract = (ROOT / "openstrata.ci.yaml").read_text(encoding="utf-8")
    version = re.search(r'ost:\s*\n\s*version:\s*"([^"]+)"', contract).group(1)
    pins = set(re.findall(r"runtime_artifact:\s*(sha256:[0-9a-f]{64})", contract))
    pins |= set(re.findall(r"uri:\s*(oci://\S+)", contract))
    failed = False
    for name in PINNED:
        workflow = (WORKFLOWS / name).read_text(encoding="utf-8")
        missing = sorted(pin for pin in pins if pin not in workflow)
        if f"ost {version}" not in workflow or f"/download/v{version}" not in workflow:
            missing.append(f"ost {version}")
        if missing:
            print(f"{name} does not name:\n  " + "\n  ".join(missing))
            failed = True
        else:
            print(f"{name} mirrors ost {version} and {len(pins)} runtime pin(s).")
    for name, triggers in FILTERED.items():
        found = (WORKFLOWS / name).read_text(encoding="utf-8").count(DOCUMENTATION_FILTER)
        if found != triggers:
            print(f"{name} has the documentation filter on {found} trigger(s), expected {triggers}.")
            failed = True
        else:
            print(f"{name} skips documentation-only changes.")
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
