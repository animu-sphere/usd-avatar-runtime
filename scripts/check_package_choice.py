#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Check what a consumer gets with both library prefixes on its search path.

`avatarVrmSession` includes the core and `avatarRuntime` is the core alone, so both
provide the `AvatarRuntime` CMake package. Each find order below must resolve the
core from the session's prefix or refuse with a message naming the choice. These
configure only: `scripts/check_session_consumer.py` builds and runs the session.
"""
from __future__ import annotations

import json
import pathlib
import subprocess
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]
VERSION = (ROOT / "VERSION").read_text(encoding="utf-8").strip()
SESSION_PACKAGE = f"find_package(AvatarVrmSession {VERSION} EXACT CONFIG REQUIRED)"
CORE_PACKAGE = f"find_package(AvatarRuntime {VERSION} EXACT CONFIG REQUIRED)"
SESSION_COMPONENT = f"find_package(AvatarRuntime {VERSION} EXACT CONFIG REQUIRED COMPONENTS vrm_session)"
# Scenario -> find calls, and the refusal expected, if any.
SCENARIOS = {
    "the session package": ([SESSION_PACKAGE], None),
    "the session package, then the core package": ([SESSION_PACKAGE, CORE_PACKAGE], None),
    "the session component": ([SESSION_COMPONENT], "This prefix has no vrm_session component."),
    "the core package, then the session package": ([CORE_PACKAGE, SESSION_PACKAGE], "pin one package"),
}
PROJECT = """cmake_minimum_required(VERSION 3.24)
project(package_choice LANGUAGES CXX)
list(PREPEND CMAKE_PREFIX_PATH "{core}")
{finds}
if(NOT TARGET AvatarRuntime::avatarVrmSession)
    message(FATAL_ERROR "AvatarRuntime::avatarVrmSession was not imported")
endif()
message(STATUS "package-choice-core=${{AvatarRuntime_DIR}}")
"""


def record(name: str) -> tuple[pathlib.Path, dict]:
    library = ROOT / "libs" / name
    records = sorted(library.glob(".strata/targets/*/library-build.json"))
    if len(records) != 1:
        raise SystemExit(f"expected one built target under {library}, found {len(records)}")
    return records[0], json.loads(records[0].read_text(encoding="utf-8"))


def main() -> int:
    session_path, session = record("avatarVrmSession")
    _, core = record("avatarRuntime")
    session_prefix = pathlib.Path(session["install_prefix"]).resolve()
    failed = False
    with tempfile.TemporaryDirectory(prefix="package-choice-") as scratch:
        for index, (name, (finds, refusal)) in enumerate(SCENARIOS.items()):
            source = pathlib.Path(scratch) / str(index)
            source.mkdir()
            (source / "CMakeLists.txt").write_text(
                PROJECT.format(core=core["install_prefix"], finds="\n".join(finds)), encoding="utf-8"
            )
            done = subprocess.run(
                ["cmake", "-S", str(source), "-B", str(source / "build"), "-DCMAKE_BUILD_TYPE=Release",
                 f"-DCMAKE_TOOLCHAIN_FILE={session_path.parent / 'toolchain.cmake'}"],
                capture_output=True, text=True, errors="replace",
            )
            output = " ".join((done.stdout + done.stderr).split())
            if refusal is not None:
                passed = done.returncode != 0 and refusal in output
                outcome = f"refuses: {refusal}" if passed else f"exit {done.returncode}, expected the refusal: {refusal}"
            else:
                found = output.split("package-choice-core=")[-1].split(" -- ")[0] if "package-choice-core=" in output else ""
                passed = done.returncode == 0 and bool(found) and pathlib.Path(found).resolve().is_relative_to(session_prefix)
                outcome = "core from the session's prefix" if passed else f"exit {done.returncode}, core from {found or 'nowhere'}"
            print(f"{name}: {outcome}")
            if not passed:
                print(done.stdout + done.stderr)
                failed = True
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
