#!/usr/bin/env python3
"""Provision and verify the exact native inputs used by the GDExtension build.

Generated sources and build products live below ``build/dependencies`` by
default. Nothing is written to a system temporary directory or a hidden home
cache. Existing checkouts are never reset: an unexpected or dirty checkout is
reported and left untouched.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
from typing import Any, Sequence


class DependencyError(RuntimeError):
    """A dependency does not match the repository lock."""


def _run(
    command: Sequence[str],
    *,
    cwd: Path | None = None,
    capture: bool = False,
    check: bool = True,
    env: dict[str, str] | None = None,
) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        list(command),
        cwd=cwd,
        check=check,
        text=True,
        stdout=subprocess.PIPE if capture else None,
        stderr=subprocess.PIPE if capture else None,
        env=env,
    )


def _git_output(repository: Path, *arguments: str) -> str:
    result = _run(("git", *arguments), cwd=repository, capture=True)
    return result.stdout.strip()


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def _require_hash(path: Path, expected: str) -> None:
    if not path.is_file():
        raise DependencyError(f"locked file is missing: {path}")
    actual = _sha256(path)
    if actual != expected:
        raise DependencyError(
            f"locked file hash differs for {path}: expected {expected}, got {actual}"
        )


def _load_lock(lock_path: Path) -> dict[str, Any]:
    try:
        data = json.loads(lock_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise DependencyError(f"cannot read dependency lock {lock_path}: {error}") from error
    if data.get("schema_version") != 1:
        raise DependencyError(
            f"unsupported dependency lock schema: {data.get('schema_version')!r}"
        )
    return data


def _is_git_repository(path: Path) -> bool:
    if not path.is_dir():
        return False
    result = _run(
        ("git", "rev-parse", "--is-inside-work-tree"),
        cwd=path,
        capture=True,
        check=False,
    )
    return result.returncode == 0 and result.stdout.strip() == "true"


def _require_clean(repository: Path) -> None:
    changed = _git_output(repository, "status", "--porcelain", "--untracked-files=no")
    if changed:
        raise DependencyError(
            f"dependency checkout has tracked changes and will not be modified: {repository}"
        )


def _remote_matches(repository: Path, expected_url: str) -> bool:
    result = _run(
        ("git", "remote", "get-url", "origin"),
        cwd=repository,
        capture=True,
        check=False,
    )
    if result.returncode != 0:
        return False
    actual = result.stdout.strip().removesuffix(".git")
    expected = expected_url.removesuffix(".git")
    return actual == expected


def _checkout_exact(
    destination: Path,
    *,
    repository_url: str,
    ref: str | None,
    commit: str,
    verify_only: bool,
) -> None:
    if not destination.exists():
        if verify_only:
            raise DependencyError(f"dependency checkout is missing: {destination}")
        destination.parent.mkdir(parents=True, exist_ok=True)
        _run(("git", "init", str(destination)))
        _run(("git", "remote", "add", "origin", repository_url), cwd=destination)
        fetch_target = f"refs/tags/{ref}" if ref else commit
        _run(("git", "fetch", "--depth=1", "origin", fetch_target), cwd=destination)
        _run(("git", "checkout", "--detach", commit), cwd=destination)

    if not _is_git_repository(destination):
        raise DependencyError(f"dependency path is not a Git checkout: {destination}")
    if not _remote_matches(destination, repository_url):
        raise DependencyError(
            f"dependency checkout has an unexpected origin (wanted {repository_url}): "
            f"{destination}"
        )
    _require_clean(destination)


def _update_submodules(repository: Path, verify_only: bool) -> None:
    gitmodules = repository / ".gitmodules"
    if not gitmodules.is_file():
        return
    if not verify_only:
        _run(
            ("git", "submodule", "update", "--init", "--recursive", "--depth=1"),
            cwd=repository,
        )
    status = _git_output(repository, "submodule", "status", "--recursive")
    bad = [line for line in status.splitlines() if line.startswith(("-", "+", "U"))]
    if bad:
        raise DependencyError(
            "dependency submodules do not match the locked superproject:\n" + "\n".join(bad)
        )


def _verify_godot_cpp(
    repository: Path, specification: dict[str, Any], verify_only: bool
) -> dict[str, str]:
    _checkout_exact(
        repository,
        repository_url=specification["repository"],
        ref=specification["ref"],
        commit=specification["commit"],
        verify_only=verify_only,
    )
    head = _git_output(repository, "rev-parse", "HEAD")
    if head != specification["commit"]:
        raise DependencyError(
            f"godot-cpp commit differs at {repository}: expected "
            f"{specification['commit']}, got {head}"
        )
    tree = _git_output(repository, "rev-parse", "HEAD^{tree}")
    if tree != specification["tree"]:
        raise DependencyError(
            f"godot-cpp tree differs at {repository}: expected "
            f"{specification['tree']}, got {tree}"
        )
    _update_submodules(repository, verify_only)
    return {"path": str(repository), "commit": head, "tree": tree}


def _verify_patch_files(root: Path, specification: dict[str, Any]) -> list[Path]:
    patches: list[Path] = []
    for entry in specification["patches"]:
        path = root / entry["path"]
        _require_hash(path, entry["sha256"])
        patches.append(path)
    return patches


def _verify_vcpkg(
    repository: Path, specification: dict[str, Any], verify_only: bool
) -> dict[str, str]:
    baseline = specification["baseline"]
    _checkout_exact(
        repository,
        repository_url=specification["repository"],
        ref=None,
        commit=baseline,
        verify_only=verify_only,
    )
    head = _git_output(repository, "rev-parse", "HEAD")
    tree = _git_output(repository, "rev-parse", "HEAD^{tree}")
    if head != baseline or tree != specification["tree"]:
        raise DependencyError(
            f"vcpkg differs at {repository}: expected {baseline}/"
            f"{specification['tree']}, got {head}/{tree}"
        )

    executable = repository / ("vcpkg.exe" if os.name == "nt" else "vcpkg")
    if not executable.is_file():
        if verify_only:
            raise DependencyError(
                f"locked vcpkg checkout is not bootstrapped: {executable}"
            )
        bootstrap = repository / (
            "bootstrap-vcpkg.bat" if os.name == "nt" else "bootstrap-vcpkg.sh"
        )
        if not bootstrap.is_file():
            raise DependencyError(f"vcpkg bootstrap script is missing: {bootstrap}")
        command = (
            (str(bootstrap), "-disableMetrics")
            if os.name == "nt"
            else ("bash", str(bootstrap), "-disableMetrics")
        )
        _run(command, cwd=repository)
    if not executable.is_file():
        raise DependencyError(
            f"vcpkg bootstrap completed without its executable: {executable}"
        )
    _run((str(executable), "version"), cwd=repository, capture=True)
    return {"path": str(repository), "commit": head, "tree": tree}


def _apply_native_patches(repository: Path, patches: Sequence[Path]) -> None:
    environment = os.environ.copy()
    environment.update(
        {
            "GIT_COMMITTER_NAME": "Cesium for Godot dependency bootstrap",
            "GIT_COMMITTER_EMAIL": "dependency-bootstrap@invalid.local",
        }
    )
    result = _run(
        ("git", "am", "--committer-date-is-author-date", *map(str, patches)),
        cwd=repository,
        check=False,
        env=environment,
    )
    if result.returncode != 0:
        _run(("git", "am", "--abort"), cwd=repository, check=False)
        raise DependencyError(
            f"could not apply the locked Cesium Native patch series in {repository}"
        )


def _verify_cesium_native(
    root: Path,
    repository: Path,
    specification: dict[str, Any],
    verify_only: bool,
) -> dict[str, str]:
    patches = _verify_patch_files(root, specification)
    _checkout_exact(
        repository,
        repository_url=specification["repository"],
        ref=specification["ref"],
        commit=specification["base_commit"],
        verify_only=verify_only,
    )
    tree = _git_output(repository, "rev-parse", "HEAD^{tree}")
    if tree == specification["base_tree"] and patches:
        if verify_only:
            raise DependencyError(
                f"Cesium Native has the upstream base but not the required patches: {repository}"
            )
        _apply_native_patches(repository, patches)
        tree = _git_output(repository, "rev-parse", "HEAD^{tree}")
    if tree != specification["patched_tree"]:
        raise DependencyError(
            f"Cesium Native tree differs at {repository}: expected "
            f"{specification['patched_tree']}, got {tree}"
        )
    _require_clean(repository)
    _update_submodules(repository, verify_only)
    return {
        "path": str(repository),
        "commit": _git_output(repository, "rev-parse", "HEAD"),
        "tree": tree,
    }


def _verify_bundled(root: Path, dependencies: dict[str, Any]) -> dict[str, int]:
    checked = 0
    for dependency_name in ("mikktspace",):
        for relative_path, expected in dependencies[dependency_name]["files"].items():
            _require_hash(root / relative_path, expected)
            checked += 1
    return {"files": checked}


def parse_arguments(arguments: Sequence[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    root_default = Path(__file__).resolve().parents[1]
    parser.add_argument("--root", type=Path, default=root_default)
    parser.add_argument("--lock", type=Path)
    parser.add_argument("--godot-cpp-root", type=Path)
    parser.add_argument("--native-root", type=Path)
    parser.add_argument("--vcpkg-root", type=Path)
    parser.add_argument(
        "--only",
        choices=("all", "godot-cpp", "cesium-native", "vcpkg", "bundled"),
        default="all",
    )
    parser.add_argument("--verify-only", action="store_true")
    parser.add_argument("--json", action="store_true", dest="json_output")
    return parser.parse_args(arguments)


def main(arguments: Sequence[str] | None = None) -> int:
    args = parse_arguments(arguments)
    root = args.root.resolve()
    lock_path = (args.lock or root / "dependencies.lock.json").resolve()
    try:
        lock = _load_lock(lock_path)
        dependencies = lock["dependencies"]
        default_sources = root / "build" / "dependencies" / "sources"
        godot_cpp_root = (args.godot_cpp_root or default_sources / "godot-cpp").resolve()
        native_root = (args.native_root or default_sources / "cesium-native").resolve()
        configured_vcpkg = os.environ.get("CESIUM_GODOT_VCPKG_ROOT", "").strip()
        default_vcpkg = (
            root
            / "build"
            / "dependencies"
            / "vcpkg"
            / dependencies["vcpkg"]["baseline"]
        )
        vcpkg_root = (
            args.vcpkg_root
            or (Path(configured_vcpkg).expanduser() if configured_vcpkg else default_vcpkg)
        ).resolve()
        result: dict[str, Any] = {
            "lock": str(lock_path),
            "mode": "verify" if args.verify_only else "provision",
        }
        if args.only in ("all", "bundled"):
            result["bundled"] = _verify_bundled(root, dependencies)
        if args.only in ("all", "godot-cpp"):
            result["godot_cpp"] = _verify_godot_cpp(
                godot_cpp_root, dependencies["godot_cpp"], args.verify_only
            )
        if args.only in ("all", "cesium-native"):
            result["cesium_native"] = _verify_cesium_native(
                root, native_root, dependencies["cesium_native"], args.verify_only
            )
        if args.only in ("all", "vcpkg"):
            result["vcpkg"] = _verify_vcpkg(
                vcpkg_root, dependencies["vcpkg"], args.verify_only
            )
    except (DependencyError, KeyError, TypeError, subprocess.CalledProcessError) as error:
        print(f"dependency bootstrap failed: {error}", file=sys.stderr)
        return 1

    if args.json_output:
        print(json.dumps(result, indent=2, sort_keys=True))
    else:
        print("Dependency lock verified.")
        for name in ("godot_cpp", "cesium_native", "vcpkg"):
            if name in result:
                print(f"  {name}: {result[name]['path']} ({result[name]['tree']})")
        if "bundled" in result:
            print(f"  bundled files: {result['bundled']['files']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
