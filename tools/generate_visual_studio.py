#!/usr/bin/env python3
"""Generate the optional Visual Studio 2026 front end with locked SCons."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
import shutil
import subprocess
from typing import Sequence


ROOT = Path(__file__).resolve().parents[1]


def parse_arguments(arguments: Sequence[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--json", action="store_true", dest="json_output")
    return parser.parse_args(arguments)


def main(arguments: Sequence[str] | None = None) -> int:
    args = parse_arguments(arguments)
    lock = json.loads((ROOT / "dependencies.lock.json").read_text(encoding="utf-8"))
    expected_version = lock["toolchain"]["scons"]
    scons = shutil.which("scons")
    if scons is None:
        raise SystemExit(f"SCons {expected_version} is required")

    version_result = subprocess.run(
        [scons, "--version"],
        cwd=ROOT,
        check=True,
        capture_output=True,
        text=True,
    )
    match = re.search(r"SCons: v([^\s]+)", version_result.stdout)
    actual_version = match.group(1).split(".", 3)[:3] if match else []
    if ".".join(actual_version) != expected_version:
        raise SystemExit(
            f"locked SCons {expected_version} is required; found "
            f"{match.group(1) if match else 'an unknown version'}"
        )

    subprocess.run(
        [scons, "-Q", "-f", "SConstruct.visual_studio"],
        cwd=ROOT,
        check=True,
    )

    solution = ROOT / "CesiumForGodot.sln"
    project = ROOT / "CesiumForGodot.vcxproj"
    filters = ROOT / "CesiumForGodot.vcxproj.filters"
    for output in (solution, project, filters):
        if not output.is_file():
            raise SystemExit(f"Visual Studio generation omitted {output.name}")
    project_text = project.read_text(encoding="windows-1252")
    for required in (
        "<PlatformToolset>v145</PlatformToolset>",
        "tools\\build_visual_studio.py",
        "Debug|x64",
        "Release|x64",
    ):
        if required not in project_text:
            raise SystemExit(f"Visual Studio project is missing {required}")
    compiled_sources = project_text.count("<ClCompile Include=")
    if compiled_sources != 86:
        raise SystemExit(
            f"Visual Studio project listed {compiled_sources} compiled sources; expected 86"
        )

    report = {
        "generator": "SCons",
        "scons_version": expected_version,
        "visual_studio": "2026",
        "platform_toolset": "v145",
        "compiled_sources": compiled_sources,
        "outputs": [str(path) for path in (solution, project, filters)],
    }
    if args.json_output:
        print(json.dumps(report, indent=2, sort_keys=True))
    else:
        print("Visual Studio 2026 solution generated:")
        print(f"  {solution}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
