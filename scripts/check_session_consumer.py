#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Build and run the session's installed consumer against its library prefix.

`ost library verify-consumer` links a generated consumer and runs nothing, and
cannot configure the session on Linux (open-strata#289). This runs the repository's
consumer, including the session contract tests, from the prefix `ost library build`
installed, using the owners that build record names.
"""
from __future__ import annotations

import json
import os
import pathlib
import shutil
import subprocess

ROOT = pathlib.Path(__file__).resolve().parents[1]
LIBRARY = ROOT / "libs" / "avatarVrmSession"
SOURCE = ROOT / "adapters" / "vrm-session" / "installed_consumer"


def run(*command: str, env: dict[str, str] | None = None) -> None:
    print("+", " ".join(command), flush=True)
    subprocess.run(command, check=True, env=env)


def main() -> int:
    records = sorted(LIBRARY.glob(".strata/targets/*/library-build.json"))
    if len(records) != 1:
        print(f"expected one built target under {LIBRARY}, found {len(records)}")
        return 1
    record = json.loads(records[0].read_text(encoding="utf-8"))
    owners = {o["id"]: pathlib.Path(o["install_prefix"]) for o in record["external_dependencies"]}
    home = pathlib.Path(os.environ.get("OST_HOME") or pathlib.Path.home() / ".ost")
    runtime = home / "runtimes" / record["runtime"]["id"]
    binary = pathlib.Path(record["build_dir"]).parent / "session-consumer"
    shutil.rmtree(binary, ignore_errors=True)

    run("cmake", "-S", str(SOURCE), "-B", str(binary), "-DCMAKE_BUILD_TYPE=Release",
        f"-DCMAKE_TOOLCHAIN_FILE={records[0].parent / 'toolchain.cmake'}")
    run("cmake", "--build", str(binary), "--config", "Release")

    prefixes = [runtime, pathlib.Path(record["install_prefix"]), *owners.values()]
    env = dict(os.environ)
    env["PXR_PLUGINPATH_NAME"] = str(owners["vrmSchemaSdk"] / "plugin" / "resources" / "vrmSchema")
    if os.name == "nt":
        directories = [p / d for p in prefixes for d in ("bin", "lib")]
        env["PATH"] = os.pathsep.join([*map(str, directories), env["PATH"]])
    else:
        directories = [p / "lib" for p in prefixes]
        env["LD_LIBRARY_PATH"] = os.pathsep.join(map(str, directories))
    run("ctest", "--test-dir", str(binary), "-C", "Release", "--output-on-failure", env=env)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
