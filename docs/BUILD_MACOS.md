# Building on macOS Apple Silicon

macOS arm64 is part of the locked build matrix. The same build wrapper used on
Linux and Windows provisions exact sources, uses an out-of-source Native build,
and emits a packaged dylib.

## Prerequisites

Install Xcode command-line tools, Git, Python 3.11.9, and `pkg-config`. Install
the remaining locked Python build tools with:

```bash
brew install pkg-config
python3 -m pip install scons==4.8.1 cmake==3.31.6 ninja==1.12.1
```

Cesium's locked vcpkg baseline builds its own OpenSSL. No system LibreSSL
fallback or interactive prompt is used.

## Build

From the repository root on an arm64 Mac:

```bash
python3 tools/build_extension.py --platform macos --arch arm64 --jobs 8
```

The output is:

```text
godot3dtiles/addons/cesium_godot/lib/libGodot3DTiles.macos.template_release.arm64.dylib
```

Both editor/debug and release manifest keys intentionally load this release
library, as the Linux and Windows packages already do. Copy
`godot3dtiles/addons/cesium_godot` into a project's `addons/` directory and
enable its editor plugin.

All generated sources, vcpkg packages, and build products are under the visible
`build/` directory. See `docs/REPRODUCIBLE_BUILDS.md` for its exact layout,
verification commands, cleanup, overrides, and CI contract.
