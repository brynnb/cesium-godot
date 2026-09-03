# Visual Studio 2026 contributor workflow

Cesium for Godot keeps SCons as its single authoritative extension build.
Windows contributors may generate a Visual Studio 2026 makefile solution that
uses the v145 toolset for code browsing and delegates Build, Rebuild, and Clean
back to that same SCons workflow.

## Generate the solution

Install Visual Studio 2026 with the Desktop development with C++ workload,
Python 3.11.9, and the build tools pinned in `dependencies.lock.json`:

```powershell
python -m pip install scons==4.11.1 cmake==4.4.3 ninja==1.13.0
python tools/generate_visual_studio.py
```

Open `CesiumForGodot.sln`. The solution and its `.vcxproj`, `.filters`, and
user settings are generated local files and are intentionally ignored by Git.
Regenerate them after adding or removing native source files.

Both solution configurations build the packaged release ABI used by the addon.
The Debug configuration additionally retains native debug symbols; it does not
create a competing debug-only addon ABI. Build actions use four workers by
default and invoke `tools/build_extension.py`, so command-line, CI, and Visual
Studio builds share dependency locking, platform validation, and output paths.

## Debug through Godot

Define the `GODOT4_BIN` environment variable before starting Visual Studio:

```powershell
$env:GODOT4_BIN = "C:\path\to\Godot_v4.6.3-stable_win64.exe"
devenv CesiumForGodot.sln
```

The generated debugger settings launch the credential-free lifecycle/material
example. Breakpoints in the extension can then be reached as Godot loads and
streams its local fixture.

This solution is optional. Users installing a released addon, including normal
Godot C# projects, do not need Visual Studio, SCons, CMake, or Ninja.
