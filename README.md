# Cesium for Godot

Cesium for Godot is an alpha-stage unofficial Godot 4 GDExtension for streaming,
rendering, and interacting with [3D Tiles](https://www.ogc.org/standard/3dtiles/).
It is built on [Cesium Native](https://github.com/CesiumGS/cesium-native) and is
focused on a capable, reusable runtime integration for Godot. It has been
exercised extensively in one large Linux project, but remains alpha software;
broader project, platform, and content coverage is still needed.

This project builds on
[3D Tiles for Godot](https://github.com/Battle-Road-Labs/3D-Tiles-For-Godot)
and adapts architecture and runtime behavior from
[Cesium for Unreal](https://github.com/CesiumGS/cesium-unreal).

Cesium for Godot is an independent project. It is not an official
Cesium GS or Godot Engine product and is not endorsed by either organization.

## Features

- Runtime streaming of 3D Tiles 1.0 and 1.1 content with Cesium Native's
  traversal, level-of-detail selection, culling, request scheduling, and cache
  management.
- Local Cartesian and globe-scale georeferencing, globe anchors, camera
  flights, origin shifting, and terrain height sampling.
- Cesium ion, Bing Maps, Google Map Tiles, TMS, WMS, WMTS, URL-template,
  local-file, polygon, GeoJSON, and debug raster overlays.
- Godot collision, picking, bounding-volume queries, structural metadata,
  feature styling, point and line rendering, and GPU instancing.
- Multi-camera selection, movement-aware prefetching, LOD transitions,
  language-neutral tile-lifecycle signals and material Callables, application
  material and texture integration, credits,
  and streaming diagnostics.
- Request headers, retries, cancellation, persistent HTTP caching, and
  hardware-informed runtime budgets.
- A generated C# facade for the runtime API, included with the addon and tested
  through real local-tile streaming on the Godot 4.6.3 .NET runtime. GDScript
  remains supported directly through the same underlying GDExtension API.

The integration currently targets Cesium Native 0.63.0 and Godot 4.6.3. Linux
x86_64 is the primary tested platform; Windows x86_64 and macOS arm64 are part
of the build matrix. Runtime support is substantially further along than the
editor workflow, which should be considered experimental.

## Building

Install a C++20 compiler, Git, Python 3.11, SCons 4.8.1, CMake 3.31.6, and
Ninja 1.13.0. On Linux, install the libcurl OpenSSL development package as
well. Then run:

```bash
git clone https://github.com/brynnb/cesium-godot.git
cd cesium-godot
python3 tools/build_extension.py --jobs 12
```

The build is reproducible from `dependencies.lock.json`. Dependencies and
generated files are kept in the ignored `build/` directory, while the packaged
addon is written to `godot3dtiles/addons/cesium_godot/`.

See the [reproducible build guide](docs/REPRODUCIBLE_BUILDS.md) for supported
platforms, dependency overrides, and cleanup.

## Using the addon

Download the archive for your platform from
[GitHub Releases](https://github.com/brynnb/cesium-godot/releases), or build the
addon from source. Copy the complete `cesium_godot` directory into your Godot
project's `addons` directory, then enable **Cesium for Godot** in **Project >
Project Settings > Plugins**. Keep the directory intact: the native library,
GDScript support files, and generated C# facade are distributed together.

The repository includes a
[lifecycle and material example](examples/lifecycle_material_demo/README.md)
and focused guides under [`docs/`](docs/). The
[porting map](PORTING_MAP.md) records how runtime responsibilities correspond
to Cesium for Unreal. Cesium request-time occlusion is supported through an
optional, small Godot engine API extension; the
[occlusion guide](docs/OCCLUSION_CULLING.md) documents the required engine
change, runtime behavior, fallback, hierarchy requirements, and tests.

Convert local paths through the shared utility instead of assembling a
`file://` prefix manually; the slash count differs for Windows drives and UNC
shares:

```gdscript
tileset.url = CesiumUrlUtility.local_path_to_file_url(
	"res://tiles/tileset.json"
)
```

### C# projects

The packaged addon includes generated wrappers under
`addons/cesium_godot/csharp`; users do not run the generator. Use the Godot
4.6.3 .NET runtime and add each wrapper's `NativeObject` to the scene tree:

```csharp
using CesiumForGodot;

var tileset = new Cesium3DTileset
{
    Url = "https://example.com/tileset.json",
    MaximumScreenSpaceError = 16.0f,
};
AddChild(tileset.NativeObject);
```

The [C# runtime guide](docs/C_SHARP.md) covers composition, signals,
`Callable` decision hooks, generated-code maintenance, and integration tests.

## Testing

After building the Linux extension, run:

```bash
tests/run_godot_tests.sh
```

The test suite uses local fixtures and does not require Cesium ion credentials.
The separate C# integration fixture compiles the committed facade and streams
real local 3D Tiles through a headless Godot .NET process:

```bash
GODOT_DOTNET_BIN=/path/to/Godot_v4.6.3-stable_mono_linux.x86_64 \
  tests/run_csharp_tests.sh
```

## Contributing

Issues and focused pull requests are welcome. Please keep engine-specific code
at the Godot boundary and reusable streaming behavior in the runtime layers;
the [porting architecture](docs/PORTING_ARCHITECTURE.md) describes the current
layout.

## License and attribution

Cesium for Godot is available under the Apache License 2.0. See
[`LICENSE.txt`](LICENSE.txt) and [`NOTICE.md`](NOTICE.md) for license and
upstream attribution details.
