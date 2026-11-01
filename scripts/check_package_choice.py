#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Check what a consumer gets with several library prefixes on its search path.

A session package includes the core and `avatarRuntime` is the core alone, so each
provides the `AvatarRuntime` CMake package. Each find order below must resolve the
core from the session's prefix or refuse with a message naming the choice. The other
session's prefix gets the same refusal, because one process cannot use both. These
configure only: `scripts/check_session_consumer.py` builds and runs a session.
"""
from __future__ import annotations

import argparse
import json
import pathlib
import subprocess
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]
VERSION = (ROOT / "VERSION").read_text(encoding="utf-8").strip()
OTHER = {"vrm": "mmd", "mmd": "vrm"}
OTHER_FIRST = "the other session's package, then this one"
PROJECT = """cmake_minimum_required(VERSION 3.24)
project(package_choice LANGUAGES CXX)
list(PREPEND CMAKE_PREFIX_PATH "{core}" "{other}" "{session}")
{finds}
if(NOT TARGET AvatarRuntime::{target})
    message(FATAL_ERROR "AvatarRuntime::{target} was not imported")
endif()
message(STATUS "package-choice-core=${{AvatarRuntime_DIR}}")
"""


def package(session: str) -> str:
    return f"Avatar{session.capitalize()}Session"


def find(name: str, *components: str) -> str:
    wanted = f" COMPONENTS {' '.join(components)}" if components else ""
    return f"find_package({name} {VERSION} EXACT CONFIG REQUIRED{wanted})"


def scenarios(session: str) -> dict[str, tuple[list[str], str | None]]:
    """Scenario -> find calls, and the refusal expected, if any."""
    own = find(package(session))
    core = find("AvatarRuntime")
    other = find(package(OTHER[session]))
    return {
        "the session package": ([own], None),
        "the session package, then the core package": ([own, core], None),
        "the session component": ([find("AvatarRuntime", f"{session}_session")], f"This prefix has no {session}_session component."),
        "the core package, then the session package": ([core, own], "pin one package"),
        OTHER_FIRST: ([other, own], "pin one package"),
    }


def record(name: str) -> tuple[pathlib.Path, dict]:
    library = ROOT / "libs" / name
    records = sorted(library.glob(".strata/targets/*/library-build.json"))
    if len(records) != 1:
        raise SystemExit(f"expected one built target under {library}, found {len(records)}")
    return records[0], json.loads(records[0].read_text(encoding="utf-8"))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--session", choices=sorted(OTHER), default="vrm")
    args = parser.parse_args()
    target = f"avatar{args.session.capitalize()}Session"
    session_path, session = record(target)
    _, core = record("avatarRuntime")
    other_path, other = record(f"avatar{OTHER[args.session].capitalize()}Session")
    session_prefix = pathlib.Path(session["install_prefix"]).resolve()
    failed = False
    with tempfile.TemporaryDirectory(prefix="package-choice-") as scratch:
        for index, (name, (finds, refusal)) in enumerate(scenarios(args.session).items()):
            source = pathlib.Path(scratch) / str(index)
            source.mkdir()
            (source / "CMakeLists.txt").write_text(
                PROJECT.format(core=core["install_prefix"], other=other["install_prefix"], session=session["install_prefix"],
                               finds="\n".join(finds), target=target), encoding="utf-8"
            )
            # A toolchain names one session's owners, so the session found first selects it.
            toolchain = (other_path if name == OTHER_FIRST else session_path).parent / "toolchain.cmake"
            done = subprocess.run(
                ["cmake", "-S", str(source), "-B", str(source / "build"), "-DCMAKE_BUILD_TYPE=Release",
                 f"-DCMAKE_TOOLCHAIN_FILE={toolchain}"],
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
