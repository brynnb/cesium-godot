#!/usr/bin/env python3
"""Build the locked Cesium for Godot GDExtension on the current host."""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import platform as host_platform
import shutil
import subprocess
import sys
from typing import Sequence


ROOT = Path(__file__).resolve().parents[1]


def _host_defaults() -> tuple[str, str]:
    system = host_platform.system().lower()
    machine = host_platform.machine().lower()
    if system == "windows":
        return "windows", "x64"
    if system == "darwin":
        return "macos", "arm64" if machine in ("arm64", "aarch64") else "x86_64"
    if system == "linux":
        return "linux", "arm64" if machine in ("arm64", "aarch64") else "x64"
    raise RuntimeError(f"unsupported build host: {system}/{machine}")


def _run(command: Sequence[str], environment: dict[str, str]) -> None:
    print("+", subprocess.list2cmdline(list(command)), flush=True)
    subprocess.run(list(command), cwd=ROOT, env=environment, check=True)


def _expected_output(
    platform_name: str,
    architecture: str,
    target: str = "template_release",
    precision: str = "single",
) -> Path:
    library_dir = ROOT / "godot3dtiles" / "addons" / "cesium_godot" / "lib"
    architecture_suffix = "x86_64" if architecture == "x64" else architecture
    precision_suffix = ".double" if precision == "double" else ""
    if platform_name == "windows":
        pattern = (
            f"Godot3DTiles.windows.{target}{precision_suffix}."
            f"{architecture_suffix}.dll"
        )
    elif platform_name == "macos":
        pattern = (
            f"libGodot3DTiles.macos.{target}{precision_suffix}."
            f"{architecture_suffix}.dylib"
        )
    else:
        pattern = (
            f"libGodot3DTiles.linux.{target}{precision_suffix}."
            f"{architecture_suffix}.so"
        )
    return library_dir / pattern


def parse_arguments(arguments: Sequence[str] | None = None) -> argparse.Namespace:
    default_platform, default_arch = _host_defaults()
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--platform", choices=("linux", "windows", "macos"), default=default_platform
    )
    parser.add_argument("--arch", default=default_arch)
    parser.add_argument(
        "--precision",
        choices=("single",),
        default="single",
        help="Godot 4.6.3 single precision; the only ABI locked and distributed by this repository",
    )
    parser.add_argument(
        "--target",
        choices=("editor", "template_debug", "template_release"),
        default="template_release",
    )
    parser.add_argument("--jobs", type=int, default=max(1, os.cpu_count() or 1))
    parser.add_argument("--skip-native", action="store_true")
    parser.add_argument(
        "--debug-symbols",
        action="store_true",
        help="retain native debug symbols while keeping the packaged release ABI",
    )
    parser.add_argument("--verify-only", action="store_true")
    parser.add_argument("--json", action="store_true", dest="json_output")
    return parser.parse_args(arguments)


def main(arguments: Sequence[str] | None = None) -> int:
    args = parse_arguments(arguments)
    if args.jobs < 1:
        raise SystemExit("--jobs must be positive")
    host_name, _host_arch = _host_defaults()
    if args.platform != host_name:
        raise SystemExit(
            f"cross-compilation is not configured: host is {host_name}, target is {args.platform}"
        )
    supported_targets = {("linux", "x64"), ("windows", "x64"), ("macos", "arm64")}
    if (args.platform, args.arch) not in supported_targets:
        raise SystemExit(
            f"unsupported extension target: {args.platform}/{args.arch}; "
            "supported targets are Linux x64, Windows x64, and macOS arm64"
        )
    environment = os.environ.copy()
    environment["CESIUM_GODOT_BUILD_JOBS"] = str(args.jobs)

    bootstrap = [
        sys.executable,
        str(ROOT / "tools" / "bootstrap_dependencies.py"),
        "--root",
        str(ROOT),
    ]
    if args.verify_only:
        bootstrap.append("--verify-only")
    _run(bootstrap, environment)
    if args.verify_only:
        return 0

    scons = shutil.which("scons")
    if scons is None:
        raise SystemExit("scons is required; install the locked version from dependencies.lock.json")
    command = [
        scons,
        f"-j{args.jobs}",
        f"platform={args.platform}",
        f"arch={args.arch}",
        "compileTarget=extension",
        f"target={args.target}",
        f"buildCesium={'no' if args.skip_native else 'yes'}",
    ]
    if args.debug_symbols:
        command.append("debug_symbols=yes")
    _run(command, environment)

    output = _expected_output(
        args.platform,
        args.arch,
        args.target,
        args.precision,
    )
    if not output.is_file():
        raise SystemExit(
            f"build completed without the expected GDExtension library: {output}"
        )
    report = {
        "platform": args.platform,
        "arch": args.arch,
        "precision": args.precision,
        "target": args.target,
        "outputs": [str(output)],
        "dependency_directory": str(ROOT / "build" / "dependencies"),
    }
    if args.json_output:
        print(json.dumps(report, indent=2, sort_keys=True))
    else:
        print("Locked GDExtension build completed:")
        print(f"  {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
