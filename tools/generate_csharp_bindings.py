#!/usr/bin/env python3
"""Generate the committed C# facade from the loaded Cesium GDExtension API."""

from __future__ import annotations

import argparse
import filecmp
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
from typing import Any, Sequence


ROOT = Path(__file__).resolve().parents[1]
LOCK_PATH = ROOT / "dependencies/csharp-bindgen.lock.json"
CHECKOUT = ROOT / "build/dependencies/sources/csharp-gdextension-bindgen"
GENERATED = ROOT / "build/csharp-bindings-generated"
OUTPUT = ROOT / "addons/cesium_godot/csharp"
TEST_PROJECT = ROOT / "tests/godot"


class GenerationError(RuntimeError):
    """The pinned generator or generated output is invalid."""


def run(
    command: Sequence[str],
    *,
    cwd: Path | None = None,
    capture: bool = False,
) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        list(command),
        cwd=cwd,
        check=True,
        text=True,
        stdout=subprocess.PIPE if capture else None,
        stderr=subprocess.STDOUT if capture else None,
    )


def git_output(repository: Path, *arguments: str) -> str:
    return run(("git", *arguments), cwd=repository, capture=True).stdout.strip()


def load_lock() -> dict[str, Any]:
    data = json.loads(LOCK_PATH.read_text(encoding="utf-8"))
    if data.get("schema_version") != 1:
        raise GenerationError("unsupported C# bindgen lock schema")
    return data


def provision_generator(lock: dict[str, Any]) -> None:
    if not CHECKOUT.exists():
        CHECKOUT.parent.mkdir(parents=True, exist_ok=True)
        run(("git", "init", str(CHECKOUT)))
        run(("git", "remote", "add", "origin", lock["repository"]), cwd=CHECKOUT)
    if not (CHECKOUT / ".git").exists():
        raise GenerationError(f"generator path is not a Git checkout: {CHECKOUT}")

    dirty = git_output(CHECKOUT, "status", "--porcelain")
    if dirty:
        raise GenerationError(f"generator checkout has local changes: {CHECKOUT}")
    actual_remote = git_output(CHECKOUT, "remote", "get-url", "origin").removesuffix(".git")
    expected_remote = lock["repository"].removesuffix(".git")
    if actual_remote != expected_remote:
        raise GenerationError(
            f"generator origin is {actual_remote}, expected {expected_remote}"
        )

    if git_output(CHECKOUT, "rev-parse", "HEAD") != lock["commit"]:
        run(("git", "fetch", "--depth=1", "origin", lock["commit"]), cwd=CHECKOUT)
        run(("git", "checkout", "--detach", lock["commit"]), cwd=CHECKOUT)
    actual_tree = git_output(CHECKOUT, "rev-parse", "HEAD^{tree}")
    if actual_tree != lock["tree"]:
        raise GenerationError(
            f"generator tree is {actual_tree}, expected {lock['tree']}"
        )


def generate(lock: dict[str, Any], godot: str) -> list[Path]:
    extension = ROOT / "addons/cesium_godot/CesiumGodot.gdextension"
    if not extension.is_file():
        raise GenerationError(f"packaged extension descriptor is missing: {extension}")
    library = ROOT / (
        "addons/cesium_godot/lib/"
        "libCesiumGodot.linux.template_release.x86_64.so"
    )
    if not library.is_file():
        raise GenerationError(
            "build the Linux extension before generating bindings; "
            f"missing {library}"
        )

    import_dir = TEST_PROJECT / ".godot"
    import_dir.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(TEST_PROJECT / "extension_list.cfg", import_dir / "extension_list.cfg")
    if GENERATED.exists():
        shutil.rmtree(GENERATED)
    GENERATED.mkdir(parents=True)

    entrypoint = (
        CHECKOUT
        / "addons/csharp_gdextension_bindgen/cli_entrypoint.gd"
    )
    environment = os.environ.copy()
    environment["XDG_DATA_HOME"] = str(ROOT / "build/csharp-bindings-user-data")
    result = subprocess.run(
        (
            godot,
            "--headless",
            "--path",
            str(TEST_PROJECT),
            "--script",
            str(entrypoint),
            "--",
            str(GENERATED),
            lock["namespace"],
        ),
        cwd=ROOT,
        env=environment,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )
    if result.returncode != 0 or "ERROR:" in result.stdout or "SCRIPT ERROR:" in result.stdout:
        raise GenerationError("binding generation failed:\n" + result.stdout)

    files = sorted(GENERATED.glob("*.cs"))
    if len(files) != lock["expected_class_count"]:
        raise GenerationError(
            f"generated {len(files)} classes, expected {lock['expected_class_count']}"
        )
    return files


def outputs_match(generated: list[Path]) -> bool:
    existing = sorted(OUTPUT.glob("*.cs")) if OUTPUT.is_dir() else []
    if [path.name for path in generated] != [path.name for path in existing]:
        return False
    return all(
        filecmp.cmp(source, OUTPUT / source.name, shallow=False)
        for source in generated
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="fail if committed bindings differ")
    parser.add_argument("--godot", default=os.environ.get("GODOT_BIN", "godot4"))
    arguments = parser.parse_args()

    try:
        lock = load_lock()
        provision_generator(lock)
        generated = generate(lock, arguments.godot)
        if arguments.check:
            if not outputs_match(generated):
                raise GenerationError("committed C# bindings are stale")
        else:
            OUTPUT.mkdir(parents=True, exist_ok=True)
            for old_file in OUTPUT.glob("*.cs"):
                old_file.unlink()
            for source in generated:
                shutil.copyfile(source, OUTPUT / source.name)
        print(
            f"C# bindings {'verified' if arguments.check else 'generated'}: "
            f"{len(generated)} classes from {lock['commit']}"
        )
        return 0
    except (GenerationError, OSError, subprocess.CalledProcessError, json.JSONDecodeError) as error:
        print(f"C# binding generation failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
