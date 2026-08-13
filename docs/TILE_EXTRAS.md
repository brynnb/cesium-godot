# Tile extras and source identity

3D Tiles nodes may carry application-defined JSON in `extras`. The integration
preserves that value without assigning a generic meaning to its keys.

`Cesium3DTile.tile_extras` exposes a deep Godot `Variant` copy while the tile is
realized. `CesiumLoadedTilePrimitive.tile_extras` stores its own deep copy, so a
game may retain source identity after the streamed scene node unloads without
retaining a Native tile pointer or a Godot node. Returned dictionaries and
arrays are also deep copies; scripts cannot mutate the renderer's copy.

This is the right place for per-placement provenance such as a Vanguard
placement ID, source chunk, authored selection distance, or source object ID.
It is not a replacement for `EXT_structural_metadata`: property tables,
feature IDs, property attributes, property textures, and mesh per-feature
picking use the typed structural-metadata API. They are deliberately not
overloaded into application-defined extras.

The Native side retains the original arbitrary JSON value on each explicit
tile. The Godot conversion preserves null, booleans, strings, arrays, objects,
signed integers, and floating-point values. JSON unsigned integers larger than
Godot's signed 64-bit integer range are returned as decimal strings rather than
silently overflowing.

The lifecycle fixture verifies nested objects and arrays, mutation isolation,
primitive access, and retained access after tile unload.

The Vanguard adapter's `d4de8ef` builder milestone emits these fields on every
renderable LOD and validates their hierarchy before publication. This keeps the
reusable integration unaware of Vanguard semantics while still giving the game
a reliable placement boundary.
