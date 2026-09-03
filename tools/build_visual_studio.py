#!/usr/bin/env python3
"""Execute a Visual Studio action through the canonical SCons build."""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys
from typing import Sequence


ROOT = Path(__file__).resolve().parents[1]


def parse_arguments(arguments: Sequence[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("build", "rebuild", "clean"))
    parser.add_argument("configuration", choices=("debug", "release"))
    parser.add_argument("--jobs", type=int, default=4)
    return parser.parse_args(arguments)


def _run(command: list[str], environment: dict[str, str]) -> None:
    print("+", subprocess.list2cmdline(command), flush=True)
    subprocess.run(command, cwd=ROOT, env=environment, check=True)


def main(arguments: Sequence[str] | None = None) -> int:
    args = parse_arguments(arguments)
    if args.jobs < 1:
        raise SystemExit("--jobs must be positive")
    if sys.platform != "win32":
        raise SystemExit("Visual Studio build actions require a Windows host")

    scons = shutil.which("scons")
    if scons is None:
        raise SystemExit("scons is required; install the locked version first")

    environment = os.environ.copy()
    environment["CESIUM_GODOT_BUILD_JOBS"] = str(args.jobs)
    debug_symbols = args.configuration == "debug"
    scons_arguments = [
        f"-j{args.jobs}",
        "platform=windows",
        "arch=x64",
        "compileTarget=extension",
        "target=template_release",
        "buildCesium=no",
    ]
    if debug_symbols:
        scons_arguments.append("debug_symbols=yes")

    if args.action in ("clean", "rebuild"):
        _run([scons, "-c", *scons_arguments], environment)
    if args.action in ("build", "rebuild"):
        build = [
            sys.executable,
            str(ROOT / "tools" / "build_extension.py"),
            "--platform",
            "windows",
            "--arch",
            "x64",
            "--target",
            "template_release",
            "--jobs",
            str(args.jobs),
        ]
        if debug_symbols:
            build.append("--debug-symbols")
        _run(build, environment)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
