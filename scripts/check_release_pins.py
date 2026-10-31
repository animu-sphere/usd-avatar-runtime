#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Fail when release.yml drifts from the pins openstrata.ci.yaml declares.

release.yml is hand-written, so regenerating the pull request lanes never
updates it and a green pull request says nothing about it.
"""
from __future__ import annotations

import pathlib
import re

ROOT = pathlib.Path(__file__).resolve().parents[1]


def main() -> int:
    contract = (ROOT / "openstrata.ci.yaml").read_text(encoding="utf-8")
    release = (ROOT / ".github" / "workflows" / "release.yml").read_text(encoding="utf-8")
    version = re.search(r'ost:\s*\n\s*version:\s*"([^"]+)"', contract).group(1)
    pins = set(re.findall(r"runtime_artifact:\s*(sha256:[0-9a-f]{64})", contract))
    pins |= set(re.findall(r"uri:\s*(oci://\S+)", contract))
    missing = sorted(pin for pin in pins if pin not in release)
    if f"ost {version}" not in release or f"/download/v{version}" not in release:
        missing.append(f"ost {version}")
    if missing:
        print("release.yml does not name:\n  " + "\n  ".join(missing))
        return 1
    print(f"release.yml mirrors ost {version} and {len(pins)} runtime pin(s).")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
