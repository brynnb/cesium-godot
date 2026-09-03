# C# runtime API

Cesium for Godot includes a generated C# facade under
`godot3dtiles/addons/cesium_godot/csharp`. The streaming engine, renderer,
cache, and Cesium Native integration remain entirely in the existing C++
GDExtension. The C# files are small forwarding wrappers over the same loaded
Godot `ClassDB` API used by GDScript; they are not a second implementation and
do not bind Cesium Native directly.

Godot 4.6.3 through 4.7.2 is the tested runtime range. The canonical C# fixture
uses the Godot 4.6.3 .NET editor/runtime and .NET 8. Copy the complete
`addons/cesium_godot` directory into the project as usual. No generator is
required in an addon user's project.

```csharp
using CesiumForGodot;

var tileset = new Cesium3DTileset
{
    Url = "https://example.com/tileset.json",
    MaximumScreenSpaceError = 16.0f,
};
AddChild(tileset.NativeObject);
```

For local content, use the generated cross-platform URL helper rather than
manually assembling a `file://` prefix:

```csharp
tileset.Url = CesiumUrlUtility.LocalPathToFileUrl(
    "res://tiles/tileset.json"
);
```

It produces canonical percent-encoded URLs for Windows drives, UNC shares,
Unix paths, `res://`, `user://`, and project-relative paths.

The generated classes use composition because C# cannot attach a new managed
base type to an object instantiated by a native GDExtension. Cesium methods,
properties, enums, constants, and signals are exposed on the wrapper. Use
`NativeObject` when ordinary Godot node/resource APIs are needed.

Lifecycle integration is language-neutral. Notifications are ordinary Godot
signals, while decisions that require a return value are `Callable`
properties. For example:

```csharp
var receiver = new Cesium3DTilesetLifecycleEventReceiver();
receiver.MaterialSelector = Callable.From<GodotObject, Material, Material>(
    (primitive, defaultMaterial) => defaultMaterial
);
receiver.TileLoaded += nativeTile =>
{
    var tile = (Cesium3DTile)Variant.From(nativeTile);
    GD.Print($"Loaded {tile.TileId}");
};
tileset.SetLifecycleEventReceiver(receiver);
AddChild(receiver.NativeObject);
```

The material selector runs synchronously during main-thread tile realization.
Keep it bounded and return either the supplied default material or another
`Material`. Material customization, primitive completion, raster attachment,
tile completion, visibility changes, and unloading are signals. Tile exclusion
uses `CesiumTileExcluder.Predicate`, another synchronous `Callable` returning
`bool`. GDScript uses these same properties and signals; there is no separate
set of specially named GDScript hooks.

Godot's C# `Callable` marshaller cannot convert a native extension object
directly into a composed wrapper. Signals whose arguments are Cesium extension
objects therefore expose those arguments as `GodotObject`. Wrap one explicitly
when needed:

```csharp
georeference.EllipsoidChanged += nativeEllipsoid =>
{
    var ellipsoid = (CesiumEllipsoid)Variant.From(nativeEllipsoid);
};
```

Typed Godot arrays have the same managed inheritance constraint. Arrays whose
element type is a Cesium extension class are exposed as an untyped
`Godot.Collections.Array`; individual elements can be wrapped through
`Variant`.

## Supported surface

The committed facade contains wrappers for every registered Cesium runtime
class. It forwards constructors, properties, methods, constants, enums, and
signals from the loaded extension API. Ordinary Godot APIs remain available on
`NativeObject`; the facade does not attempt to duplicate Godot's generated C#
classes or Cesium Native in managed code.

This is runtime language support, not a separate C# editor integration. The
same experimental editor plugin and runtime feature set apply whether a project
uses GDScript or C#.

## Maintainer workflow

The generator is a pinned fork of
[`godot-csharp-gdextension-bindgen`](https://github.com/gilzoide/godot-csharp-gdextension-bindgen).
The fork repository, exact commit and tree, upstream version, namespace, and
expected class count are recorded in
`dependencies/csharp-bindgen.lock.json`. The generator is kept outside the
repository's native dependency lock so regenerating C# does not invalidate the
large Cesium Native build cache.

After building the Linux GDExtension, regenerate or verify the committed files:

```bash
python3 tools/generate_csharp_bindings.py
python3 tools/generate_csharp_bindings.py --check
```

The command loads the real extension headlessly, reflects its API through
`ClassDB`, rejects generator errors and an unexpected class count, and writes
the deterministic result into the packaged addon.

The runtime integration test requires the Godot 4.6.3 .NET executable and a
.NET 8 SDK:

```bash
GODOT_DOTNET_BIN=/path/to/Godot_v4.6.3-stable_mono_linux.x86_64 \
  tests/run_csharp_tests.sh
```

It compiles all committed wrappers, then verifies construction across Godot
node/resource base types; properties and methods; managed, typed, and untyped
arrays; named and numeric enums; resource identity; signal subscription and
extension-object arguments; `Callable` decision hooks; native `Error` results;
cross-platform local-file URLs; and input validation inside a real headless
Godot .NET process. The same test
then streams the repository's local lifecycle tileset, checks primitive and
tile signals, material selection, extras, statistics, initial readiness,
visibility changes, unloading, and a terminal missing-source error.

The streaming portion is implemented in
`tests/csharp/CSharpStreamingIntegrationTests.cs` and reuses
`tests/godot/fixtures/lifecycle`, so the GDScript and C# boundaries are checked
against the same real 3D Tiles content. The missing-source case deliberately
prints a 404 error before the final success marker.

### Optional LibGodot / 2dog compatibility lane

The separate `tests/csharp-2dog` fixture embeds Godot through
[`2dog`](https://github.com/outfox/2dog) and runs the same facade and streaming
contracts under xUnit. It is intentionally a compatibility lane, not the
supported runtime baseline: the addon remains targeted and directly tested on
Godot 4.6.3 with .NET 8, while the pinned 2dog packages use Godot 4.7.2 with
.NET 10. This 2dog/xUnit lane currently runs on Linux only.

Run it after building the Linux extension:

```bash
DOTNET10_BIN=/path/to/dotnet tests/run_2dog_tests.sh
```

The package versions and restore graphs are locked in the fixture. The test
runner creates only ignored Godot import state and ordinary .NET build output;
2dog itself is obtained from NuGet.

An embedded Godot instance does not process frames in the background. A test
that simply awaits `SceneTree.ProcessFrame` will therefore hang. The streaming
test remains synchronous on the thread that created LibGodot and explicitly
calls `GodotInstance.Iteration()` until the shared asynchronous contract
finishes. Do not add xUnit's per-test `Timeout` property to this test: xUnit may
move it to a worker thread, while Godot scene-tree mutations must remain on the
engine thread. The shell/CI job supplies the outer process timeout.
