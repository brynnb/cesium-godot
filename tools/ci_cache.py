"""Content-cache namespaces for CI; caches never replace dependency verification."""
import argparse
import hashlib
import json
import os
from pathlib import Path


def namespaces(lock, platform, arch, image):
    # Native patches and application source are deliberately NOT outer cache
    # keys: SCons/sccache/vcpkg validate their own input/command/ABI signatures.
    common = {"platform": platform, "arch": arch, "image": image,
              "toolchain": lock["toolchain"], "precision": "single", "target": "template_release"}
    def digest(value):
        return hashlib.sha256(json.dumps(value, sort_keys=True).encode()).hexdigest()[:24]
    return {
        "compiler": digest(common),
        "vcpkg": digest({**common, "vcpkg": lock["dependencies"]["vcpkg"]}),
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--platform", required=True)
    parser.add_argument("--arch", required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    lock = json.loads((root / "dependencies.lock.json").read_text())
    keys = namespaces(lock, args.platform, args.arch, os.environ.get("ImageVersion", "local"))
    for name in ["vcpkg", "scons", "sccache"]:
        (root / "build/ci-cache" / name).mkdir(parents=True, exist_ok=True)
    with open(os.environ["GITHUB_OUTPUT"], "a") as output:
        for name, key in keys.items():
            print(f"{name}={key}", file=output)
    with open(os.environ["GITHUB_ENV"], "a") as output:
        cache_root = root / "build/ci-cache"
        print(f"VCPKG_DEFAULT_BINARY_CACHE={cache_root / 'vcpkg'}", file=output)
        print(f"VCPKG_BINARY_SOURCES=clear;files,{cache_root / 'vcpkg'},readwrite", file=output)
        print(f"SCONS_CACHE={cache_root / 'scons'}", file=output)
        print(f"SCCACHE_DIR={cache_root / 'sccache'}", file=output)
        print("SCCACHE_CACHE_SIZE=2G", file=output)
        # Native compilation finishes before the much longer Godot/SCons build.
        # Keep this job-owned server alive until the explicit flush; otherwise
        # its default idle timeout expires and --stop-server fails on Windows.
        print("SCCACHE_IDLE_TIMEOUT=0", file=output)
