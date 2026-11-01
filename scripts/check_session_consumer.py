#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Build and run a session's installed consumer against its library prefix.

`ost library verify-consumer` links a generated consumer and runs nothing. This runs
the repository's consumer, including the session contract tests, from the prefix
`ost library build` installed, using the owners that build record names. The MMD
session also needs its Jolt SDK on CMAKE_PREFIX_PATH, and the owner's published
importer to open its generated model.
"""
from __future__ import annotations

import argparse
import json
import os
import pathlib
import shutil
import subprocess
import tempfile

from check_session_fixture import importers

ROOT = pathlib.Path(__file__).resolve().parents[1]
MMD_ASSETS = ROOT / "tests" / "assets" / "mmd-session"


def run(*command: str, env: dict[str, str] | None = None) -> None:
    print("+", " ".join(command), flush=True)
    subprocess.run(command, check=True, env=env)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--session", choices=("vrm", "mmd"), default="vrm")
    args = parser.parse_args()
    library = ROOT / "libs" / f"avatar{args.session.capitalize()}Session"
    source = ROOT / "adapters" / f"{args.session}-session" / "installed_consumer"
    records = sorted(library.glob(".strata/targets/*/library-build.json"))
    if len(records) != 1:
        print(f"expected one built target under {library}, found {len(records)}")
        return 1
    record = json.loads(records[0].read_text(encoding="utf-8"))
    owners = {o["id"]: pathlib.Path(o["install_prefix"]) for o in record["external_dependencies"]}
    home = pathlib.Path(os.environ.get("OST_HOME") or pathlib.Path.home() / ".ost")
    runtime = home / "runtimes" / record["runtime"]["id"]
    binary = pathlib.Path(record["build_dir"]).parent / "session-consumer"
    shutil.rmtree(binary, ignore_errors=True)

    configure = ["cmake", "-S", str(source), "-B", str(binary), "-DCMAKE_BUILD_TYPE=Release",
                 f"-DCMAKE_TOOLCHAIN_FILE={records[0].parent / 'toolchain.cmake'}"]
    if args.session == "mmd":
        configure.append(f"-DPMX={(MMD_ASSETS / 'avatar.pmx').as_posix()}")
    run(*configure)
    run("cmake", "--build", str(binary), "--config", "Release")

    prefixes = [runtime, pathlib.Path(record["install_prefix"]), *owners.values()]
    env = dict(os.environ)
    with tempfile.TemporaryDirectory(ignore_cleanup_errors=True) as temporary:
        if args.session == "mmd":
            plugins, libraries = importers(pathlib.Path(temporary), MMD_ASSETS)
        else:
            plugins, libraries = [str(owners["vrmSchemaSdk"] / "plugin" / "resources" / "vrmSchema")], []
        env["PXR_PLUGINPATH_NAME"] = os.pathsep.join(plugins)
        if os.name == "nt":
            directories = [*libraries, *[p / d for p in prefixes for d in ("bin", "lib")]]
            env["PATH"] = os.pathsep.join([*map(str, directories), env["PATH"]])
        else:
            directories = [*libraries, *[p / "lib" for p in prefixes]]
            env["LD_LIBRARY_PATH"] = os.pathsep.join(map(str, directories))
        run("ctest", "--test-dir", str(binary), "-C", "Release", "--output-on-failure", env=env)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
