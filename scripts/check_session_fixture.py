#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Run the committed VRM fixture through the importers and the installed session.

This is the end-to-end check: a VRM and a VRMA file go through the owner's published
importers and `avatarVrmSessionCheck`, which compares the session with the owner, and
the committed state is compared with tests/assets/session/expected.json.

`ost library build` must have built avatarVrmSession. With --update, this rewrites
the expected file instead of comparing with it.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
import pathlib
import platform
import subprocess
import sys
import tempfile
import urllib.request

ROOT = pathlib.Path(__file__).resolve().parents[1]
LIBRARY = ROOT / "libs" / "avatarVrmSession"
ASSETS = ROOT / "tests" / "assets" / "session"
TOLERANCE = 1e-5
CLIP_INPUTS = [
    "--clip-expression", "/Animation/Expressions/happy.vrm:expressionName", "/Animation/Expressions/happy.vrm:expressionWeight",
    "--clip-expression", "/Animation/Expressions/blink.vrm:expressionName", "/Animation/Expressions/blink.vrm:expressionWeight",
    "--clip-gaze", "/Animation/LookAt.vrm:lookAtTarget",
]
# The run compared with the expected file: both gaze types, with clip inputs and springs.
EXPECTED_RUNS = {"bone": "avatar_bone.vrm", "expression": "avatar_expression.vrm"}


def importers(directory: pathlib.Path) -> tuple[list[str], list[str]]:
    """Downloads the pinned importer bundles and returns their plugin and library paths."""
    pins = json.loads((ASSETS / "importers.json").read_text(encoding="utf-8"))
    system = {"Windows": "windows", "Linux": "linux"}[platform.system()]
    plugins: list[str] = []
    libraries: list[str] = []
    for name, digests in pins["bundles"].items():
        asset = f"{name}-{pins['version']}-cy2026-{system}-x86_64-py313-usd.tar.zst"
        archive = directory / asset
        url = f"https://github.com/{pins['repository']}/releases/download/v{pins['version']}/{asset}"
        print("+ download", url, flush=True)
        with urllib.request.urlopen(url) as response:
            data = response.read()
        digest = hashlib.sha256(data).hexdigest()
        if digest != digests[system]:
            raise SystemExit(f"{asset}: sha256 {digest}, expected {digests[system]}")
        archive.write_bytes(data)
        bundle = directory / name
        bundle.mkdir()
        subprocess.run(["cmake", "-E", "tar", "xf", str(archive)], check=True, cwd=bundle)
        activation = json.loads((bundle / "openstrata.activation.json").read_text(encoding="utf-8"))
        plugins += [str(bundle / p) for p in activation["plugin_paths"]]
        libraries += [str(bundle / p) for p in activation["library_paths"]]
    return plugins, libraries


def environment(plugins: list[str], libraries: list[str]) -> tuple[pathlib.Path, dict[str, str]]:
    records = sorted(LIBRARY.glob(".strata/targets/*/library-build.json"))
    if len(records) != 1:
        raise SystemExit(f"expected one built target under {LIBRARY}, found {len(records)}")
    record = json.loads(records[0].read_text(encoding="utf-8"))
    home = pathlib.Path(os.environ.get("OST_HOME") or pathlib.Path.home() / ".ost")
    prefix = pathlib.Path(record["install_prefix"])
    owners = [pathlib.Path(o["install_prefix"]) for o in record["external_dependencies"]]
    prefixes = [home / "runtimes" / record["runtime"]["id"], prefix, *owners]
    env = dict(os.environ)
    env["PXR_PLUGINPATH_NAME"] = os.pathsep.join(plugins)
    if os.name == "nt":
        env["PATH"] = os.pathsep.join([*libraries, *[str(p / d) for p in prefixes for d in ("bin", "lib")], env["PATH"]])
        return prefix / "bin" / "avatarVrmSessionCheck.exe", env
    env["LD_LIBRARY_PATH"] = os.pathsep.join([*libraries, *[str(p / "lib") for p in prefixes]])
    return prefix / "bin" / "avatarVrmSessionCheck", env


def compare(path: str, actual, expected, failures: list[str]) -> None:
    if isinstance(expected, dict):
        if set(actual) != set(expected):
            failures.append(f"{path}: fields differ")
            return
        for key in expected:
            compare(f"{path}.{key}", actual[key], expected[key], failures)
    elif isinstance(expected, list):
        if len(actual) != len(expected):
            failures.append(f"{path}: {len(actual)} entries, expected {len(expected)}")
            return
        # A quaternion and its negation are the same rotation.
        if path.endswith(".rotation"):
            if sum(a * b for a, b in zip(actual, expected)) < 0:
                actual = [-a for a in actual]
        for i, entry in enumerate(expected):
            compare(f"{path}[{i}]", actual[i], entry, failures)
    elif isinstance(expected, float) or isinstance(actual, float):
        if not math.isclose(actual, expected, abs_tol=TOLERANCE):
            failures.append(f"{path}: {actual}, expected {expected}")
    elif actual != expected:
        failures.append(f"{path}: {actual}, expected {expected}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--update", action="store_true", help="rewrite the expected state")
    args = parser.parse_args()
    fixture = subprocess.run([sys.executable, str(ROOT / "tools" / "make_session_fixture.py"), "--check"])
    if fixture.returncode:
        return fixture.returncode
    with tempfile.TemporaryDirectory(ignore_cleanup_errors=True) as temporary:
        directory = pathlib.Path(temporary)
        plugins, libraries = importers(directory)
        host, env = environment(plugins, libraries)
        clip = str(ASSETS / "clip.vrma")

        def run(*arguments: str) -> None:
            print("+", host.name, *arguments, flush=True)
            subprocess.run([str(host), *arguments], check=True, env=env)

        actual = {}
        for name, avatar in EXPECTED_RUNS.items():
            path = str(ASSETS / avatar)
            for springs in ([], ["--no-springs"]):
                run(path, clip, "0.016666666666666666", "90", "--face", *springs)
            output = directory / f"{name}.json"
            run(path, clip, "0.016666666666666666", "90", *CLIP_INPUTS, "--state-output", str(output))
            actual[name] = json.loads(output.read_text(encoding="utf-8"))
    expected_path = ASSETS / "expected.json"
    if args.update:
        expected_path.write_text(json.dumps(actual, indent=1, sort_keys=True) + "\n", encoding="utf-8", newline="\n")
        print(f"wrote {expected_path}")
        return 0
    failures: list[str] = []
    compare("state", actual, json.loads(expected_path.read_text(encoding="utf-8")), failures)
    for failure in failures[:40]:
        print(failure)
    print(f"{len(failures)} differences from {expected_path.name} at tolerance {TOLERANCE}")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
