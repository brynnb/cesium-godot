# Reproducible builds and compatibility

Godot 4.6.3 is both the minimum supported and tested GDExtension runtime.
Release builds compile against the locked Godot 4.6 `godot-cpp` API (bindings
version 4.6.0) and use typed APIs introduced by Godot 4.6, so older Godot 4.x
runtimes are not supported. This follows Godot's documented
[GDExtension compatibility rules](https://docs.godotengine.org/en/stable/tutorials/scripting/gdextension/gdextension_compatibility.html).

The locked native streaming implementation is Cesium Native v0.63.0 plus the
ordered downstream patches recorded in the dependency lock.

## One-command build

Install Git and a C++20 compiler, then install the tool versions recorded in
`dependencies.lock.json`:

```bash
python3 -m pip install scons==4.8.1 cmake==3.31.6 ninja==1.12.1
python3 tools/build_extension.py --jobs 12
```

PyPI does not publish Ninja 1.12.1 for Apple Silicon. The macOS CI job instead
downloads Ninja's official `ninja-mac.zip` release and verifies its pinned
SHA-256 before use. Local macOS builds may install the same 1.12.1 release by
any trusted method.

The wrapper verifies every locked input, provisions missing sources, builds
Cesium Native out of source, and builds the release GDExtension. Useful
variants are:

```bash
# Re-check already provisioned sources and bundled files without building.
python3 tools/build_extension.py --verify-only

# Re-link after an already successful Native build.
python3 tools/build_extension.py --skip-native --jobs 12

# Build the double-precision extension ABI.
python3 tools/build_extension.py --precision double --jobs 12
```

The supported build hosts/targets are Linux x86_64, Windows x86_64, and macOS
arm64. The wrapper rejects accidental cross-target combinations because the
Native triplet, bundled archives, and host compiler would otherwise disagree.

## What is locked

`dependencies.lock.json` is the source of truth for:

- the exact Godot 4.6 `godot-cpp` commit and Git tree;
- the exact Cesium Native v0.63.0 base commit and expected patched tree;
- the ordered, hashed downstream Native patches under
  `dependencies/cesium-native-patches`;
- the vcpkg baseline;
- the MikkTSpace source hashes and upstream commit;
- every bundled litehtml/gumbo archive hash; and
- the CI Python, SCons, CMake, and Ninja versions.

`tools/bootstrap_dependencies.py` refuses an unexpected origin, commit/tree,
tracked modification, missing submodule, or changed vendored hash. It never
resets or overwrites a mismatched checkout. The exceptional
`CESIUM_GODOT_ALLOW_UNPINNED_DEPENDENCIES=1` escape hatch is for deliberate
dependency development only and disables this protection visibly.

## Visible disk layout

All default generated dependency data is deliberately kept under the visible,
ignored `build/` directory:

```text
build/
  dependencies/
    sources/godot-cpp/
    sources/cesium-native/
    native-build/<triplet>-release/
    vcpkg/<locked-baseline>/
  test-engines/
```

Nothing in this build path uses `/tmp`, a RAM-backed filesystem, or a randomly
selected home-cache directory. `du -sh build` reports its complete local size.
Deleting `build/` removes all provisioned sources, Native build products,
vcpkg packages, and downloaded test engines; the next build recreates them
from the lock. The distributable addon remains under
`godot3dtiles/addons/cesium_godot`.

An existing exact vcpkg checkout can be reused explicitly without creating a
second package tree:

```bash
CESIUM_GODOT_VCPKG_ROOT=/absolute/path/to/vcpkg-at-the-locked-baseline \
  python3 tools/build_extension.py --jobs 12
```

Development overrides are also explicit:

| Variable | Meaning |
| --- | --- |
| `CESIUM_GODOT_GODOT_CPP_ROOT` | Alternate exact `godot-cpp` checkout |
| `CESIUM_GODOT_NATIVE_ROOT` | Alternate exact patched Native source checkout |
| `CESIUM_GODOT_NATIVE_BUILD_ROOT` | Explicit Native library build tree; otherwise every source, including a development override, builds visibly under this repository's `build/dependencies/native-build` |
| `CESIUM_GODOT_VCPKG_ROOT` | Exact vcpkg baseline checkout |
| `CESIUM_GODOT_BUILD_JOBS` | Native CMake parallelism |
| `CESIUM_GODOT_NATIVE_TESTS` | Build Native's test targets when true |

## Compatibility and CI gates

`tools/check_compatibility.py` compares the locked bindings API, declared ABI
floor, packaged/example `.gdextension` manifests, and platform library keys.
The normal test runner invokes it before runtime tests.

`.github/workflows/build-matrix.yml` uses immutable action SHAs and fixed
Ubuntu 24.04, Windows 2022, and macOS 15 arm64 runner labels. It builds Linux,
Windows single/double precision, and macOS arm64 artifacts. Linux additionally
runs Cesium Native tests, resolves the final shared library, and runs the full
Godot suite on the documented Godot 4.6.3 runtime.

The OS compiler and SDK are supplied by those versioned runner images. A
runner-image refresh can still change patch-level system tools; the dependency
lock and CI logs make that boundary explicit rather than claiming bit-for-bit
reproducibility across unrelated host toolchains.
