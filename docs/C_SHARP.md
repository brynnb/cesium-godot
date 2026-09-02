# C# runtime API

Cesium for Godot includes a generated C# facade under
`godot3dtiles/addons/cesium_godot/csharp`. The streaming engine, renderer,
cache, and Cesium Native integration remain entirely in the existing C++
GDExtension. The C# files are small forwarding wrappers over the same loaded
Godot `ClassDB` API used by GDScript; they are not a second implementation and
do not bind Cesium Native directly.

Use the Godot 4.6.3 .NET editor/runtime for C# projects. Copy the complete
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

The optional runtime smoke test requires the Godot 4.6.3 .NET executable and a
.NET 8 SDK:

```bash
GODOT_DOTNET_BIN=/path/to/Godot_v4.6.3-stable_mono_linux.x86_64 \
  tests/run_csharp_tests.sh
```

It compiles all committed wrappers, then verifies construction across Godot
node/resource base types; properties and methods; managed, typed, and untyped
arrays; named and numeric enums; resource identity; signal subscription and
extension-object arguments; `Callable` decision hooks; native `Error` results;
and input validation inside a real headless Godot .NET process. The same test
then streams the repository's local lifecycle tileset, checks primitive and
tile signals, material selection, extras, statistics, initial readiness,
visibility changes, unloading, and a terminal missing-source error.
