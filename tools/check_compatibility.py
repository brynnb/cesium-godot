#!/usr/bin/env python3
"""Check that the declared Godot ABI floor matches the locked bindings."""

from __future__ import annotations

import argparse
import configparser
import json
from pathlib import Path
import sys
from typing import Sequence


ROOT = Path(__file__).resolve().parents[1]


def _read_extension(path: Path) -> configparser.ConfigParser:
    parser = configparser.ConfigParser()
    with path.open("r", encoding="utf-8-sig") as source:
        parser.read_file(source)
    return parser


def parse_arguments(arguments: Sequence[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--godot-cpp-root", type=Path)
    return parser.parse_args(arguments)


def main(arguments: Sequence[str] | None = None) -> int:
    args = parse_arguments(arguments)
    root = args.root.resolve()
    godot_cpp = (
        args.godot_cpp_root
        or root / "build" / "dependencies" / "sources" / "godot-cpp"
    ).resolve()
    lock = json.loads((root / "dependencies.lock.json").read_text(encoding="utf-8"))
    compatibility = lock["godot_compatibility"]
    minimum = compatibility["minimum"]
    bindings_version = compatibility["bindings_version"]
    api_version_name = compatibility["api_version"].replace(".", "-")
    api_path = godot_cpp / "gdextension" / f"extension_api-{api_version_name}.json"
    if not api_path.is_file():
        print(f"compatibility check failed: bindings API is missing: {api_path}", file=sys.stderr)
        return 1
    api = json.loads(api_path.read_text(encoding="utf-8"))
    header = api["header"]
    api_version = (
        f"{header['version_major']}.{header['version_minor']}.{header['version_patch']}"
    )
    api_floor = f"{header['version_major']}.{header['version_minor']}"
    errors: list[str] = []
    if api_version != bindings_version:
        errors.append(
            f"locked bindings say {bindings_version}, but extension_api.json is {api_version}"
        )
    if api_floor != compatibility["api_version"]:
        errors.append(
            f"declared bindings API is Godot {compatibility['api_version']}, "
            f"but the locked API targets {api_floor}"
        )

    manifests = [
        root / "godot3dtiles/addons/cesium_godot/Godot3DTiles.gdextension",
        root / "examples/lifecycle_material_demo/Godot3DTiles.gdextension",
    ]
    required_library_keys = {
        "windows.debug.x86_64",
        "windows.release.x86_64",
        "linux.debug.x86_64",
        "linux.release.x86_64",
        "macos.debug.arm64",
        "macos.release.arm64",
        "android.debug.arm64",
        "android.release.arm64",
        "android.debug.x86_64",
        "android.release.x86_64",
    }
    for manifest_path in manifests:
        manifest = _read_extension(manifest_path)
        declared = manifest.get(
            "configuration", "compatibility_minimum", fallback=""
        ).strip('"')
        if declared != minimum:
            errors.append(
                f"{manifest_path.relative_to(root)} declares {declared!r}, expected {minimum!r}"
            )
        keys = set(manifest["libraries"])
        missing = sorted(required_library_keys - keys)
        if missing:
            errors.append(
                f"{manifest_path.relative_to(root)} is missing library keys: {', '.join(missing)}"
            )

    if errors:
        print("compatibility check failed:", file=sys.stderr)
        for error in errors:
            print(f"  - {error}", file=sys.stderr)
        return 1
    print(
        f"Godot compatibility check passed: runtime minimum {minimum}, "
        f"bindings API {api_version}, "
        f"{len(manifests)} manifests"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
