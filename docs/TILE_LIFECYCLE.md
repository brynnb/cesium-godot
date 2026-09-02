# Tile lifecycle and material customization

`Cesium3DTilesetLifecycleEventReceiver` provides a main-thread integration
point for applications that need to observe tile resource lifecycles or choose
materials while a tile is being realized. Its callback shape follows Cesium
for Unreal's `ICesium3DTilesetLifecycleEventReceiver`, translated to Godot
nodes, resources, signals, and `Callable` properties. The same API is available
to GDScript, C#, and other Godot languages; it does not discover specially
named methods on a GDScript subclass.

This API is intended for reusable renderer integration. Application-specific
terrain shaders, textures, metadata policies, and caches remain in the
application.

## Setup

Create a receiver as a child of your scene and assign it to a
`Cesium3DTileset` through the `lifecycle_event_receiver` property.

```gdscript
extends Cesium3DTilesetLifecycleEventReceiver


func _init() -> void:
	material_selector = Callable(self, "select_material")
	material_customizing.connect(customize_material)
	tile_mesh_primitive_loaded.connect(on_primitive_loaded)
	raster_overlay_attached.connect(on_raster_attached)
	raster_overlay_detaching.connect(on_raster_detaching)
	tile_loaded.connect(on_tile_loaded)
	tile_visibility_changed.connect(on_visibility_changed)
	tile_unloading.connect(on_tile_unloading)


func select_material(
	primitive: CesiumLoadedTilePrimitive,
	default_material: Material
) -> Material:
	# Returning the default preserves the normal glTF material.
	# Return a different Material to install a per-tile surface override.
	return default_material


func customize_material(
	primitive: CesiumLoadedTilePrimitive,
	material: Material
) -> void:
	# Standard glTF properties have already been applied to default_material.
	# Application-specific parameters can be applied here.
	pass


func on_primitive_loaded(
	primitive: CesiumLoadedTilePrimitive
) -> void:
	pass


func on_raster_attached(
	binding: CesiumRasterOverlayBinding
) -> void:
	pass


func on_raster_detaching(
	binding: CesiumRasterOverlayBinding
) -> void:
	pass


func on_tile_loaded(tile: Cesium3DTile) -> void:
	pass


func on_visibility_changed(
	tile: Cesium3DTile,
	visible: bool
) -> void:
	pass


func on_tile_unloading(tile: Cesium3DTile) -> void:
	# Release application-owned resources associated with this tile now.
	pass
```

Assign it from GDScript when constructing a scene dynamically:

```gdscript
var receiver := MyReceiver.new()
add_child(receiver)
tileset.lifecycle_event_receiver = receiver
```

`material_selector` is the decision callback because it must return a
`Material`. If it is unset, invalid, or returns a non-`Material` value, the
generated default material is used.

All notifications are emitted as signals on the receiver:

- `material_customizing(primitive, selected_material)`
- `tile_mesh_primitive_loaded(primitive)`
- `raster_overlay_attached(binding)`
- `raster_overlay_detaching(binding)`
- `tile_loaded(tile)`
- `tile_visibility_changed(tile, visible)`
- `tile_unloading(tile)`

## Ordering

For a tile that reaches renderer realization, callbacks occur in this order:

1. `material_selector` for a primitive.
2. `material_customizing` for that primitive's selected material.
3. `tile_mesh_primitive_loaded` for that primitive.
4. Repeat steps 1-3 for every primitive/surface.
5. `tile_loaded` after every primitive has finished.
6. `tile_visibility_changed(tile, true)` when selection first shows it.
7. Zero or more later visibility changes as Cesium selection changes.
8. `raster_overlay_detaching` for every attached overlay.
9. `tile_unloading` immediately before the renderer releases the tile.

All callbacks execute on the same main thread that calls Cesium's view update.
Do not block them with file I/O, network access, image decoding, or large
resource construction. Prepare CPU data asynchronously and use the callbacks
only for bounded engine-resource realization and association.

The renderer boundary has two explicit ownership stages. Cesium load workers
produce `CesiumGDPreparedModel`, which contains CPU geometry only. `ArrayMesh`,
textures, shaders, materials, collision objects, and `Cesium3DTile` nodes are
created only after Cesium calls main-thread preparation. The provider stores
the owning tileset as an `ObjectID`, never as a raw pointer retained by delayed
work. Consequently, cancellation or owner destruction can discard a prepared
result without leaking a RenderingServer resource.

Cesium CPU jobs run on the bounded `GodotTaskProcessor`, not inline on the
frame thread. `worker_thread_count` defaults to the available hardware
concurrency minus one, clamped to one through eight, and may be set from one to
64. Changing it recreates the active Native tileset so the old executor can
drain before the replacement begins.

A tile evicted after worker preparation but before main-thread realization was
never loaded from the application's perspective. It is freed without lifecycle
callbacks.

The tileset releases its active Cesium renderer resources when it exits the
scene tree, while realized Godot tile nodes are still valid. This guarantees
that `tile_unloading` subscribers can inspect and disassociate the tile before
Godot destroys it.

## Primitive context

`CesiumLoadedTilePrimitive` contains copied informational data:

- `loaded_tile`: the `Cesium3DTile`, or `null` if it has since unloaded.
- `tile_id`: Cesium's informational tile identifier string.
- `surface_index`: the stable logical renderer-primitive index. For ordinary
  primitives it historically equals the tile mesh surface; for a mixed tile it
  may differ because GPU-instanced primitives use child `MultiMesh` nodes.
- `mesh_surface_index`: the surface local to `render_node` (`0` for a
  single-surface MultiMesh primitive).
- `render_node`: the owning `Cesium3DTile` for an ordinary surface or its child
  `CesiumGltfInstancedComponent`; it becomes `null` after unload.
- `gpu_instanced` and `instance_count`: instance-batch identity and size.
- `primitive_features`: feature IDs belonging to the shared mesh surface.
- `instance_features`: node-level `EXT_instance_features`, when present.
- `primitive_metadata`: the primitive's property attributes, property-texture
  references, and exact-hit UV context.
- `model_metadata`: retained property tables, schema enums, and property
  textures used by the primitive's feature and metadata layers.
- `mesh_index`: the source glTF mesh index.
- `primitive_index`: the primitive's index within the glTF mesh.
- `material_index`: the glTF material index, or `-1` for the default material.
- `primitive_mode`: the glTF primitive mode integer.
- `attributes`: attribute semantic to glTF accessor-index mapping.
- `gltf_material`: copied standard material values, extras, unknown properties,
  extension names, and generic JSON extensions.
- `texture_coordinate_mappings`: glTF texture-coordinate semantics mapped to
  their realized Godot vertex channels.
- stable absolute origin and local-to-absolute basis values for world-aligned
  materials.

Tile identifiers are informational and are not guaranteed to be globally
unique. Associate per-instance state with the tile's Godot instance ID, and
remove it during `tile_unloading`.

The context stores the tile by `ObjectID`, not by a raw pointer. Retaining the
context after unload is safe, but `loaded_tile` will then return `null`.

The loaded `Cesium3DTile` also exposes owned `tile_bounds`, `content_bounds`,
and `viewer_request_bounds` Resources. Retain those Resources directly when an
application needs bounds after `tile_unloading`; they contain no Native
tile pointer. Shape, precision, coordinate-frame, and transformation behavior
is documented in [bounding-volume queries](BOUNDING_VOLUMES.md).

## Material ownership

The renderer first creates the ordinary glTF-derived `StandardMaterial3D`.
`material_selector` receives that resource and may return it or a replacement.
The selected resource is installed as a surface override on the individual
tile node, or as the material override on a primitive's child MultiMesh render
node. It does not mutate the underlying `ArrayMesh`, which is important when
many tiles or placements share one immutable mesh resource.

If `material_selector` returns the supplied default resource, the renderer
shallow-duplicates that material before `material_customizing`. Shader and
texture resources remain shared, while parameters and raster state are safely
per tile. A replacement returned by the application is used as supplied; the
application controls whether that replacement itself is shared.

If `material_selector` returns `null` or a non-`Material` value, the renderer
warns and uses the default material.

`material_customizing` runs after selection. When `material_selector` returns a
replacement shader material, the application is responsible for translating
any standard glTF values it still needs from `default_material` and
`primitive.gltf_material`.

Raster-overlay parameter propagation uses the separate generic
`CesiumRasterOverlayBinding` contract. See
[`RASTER_OVERLAY_MATERIALS.md`](RASTER_OVERLAY_MATERIALS.md) for shader
parameters, multiple overlays, replacement, and deterministic detach.
Stable coordinate parameters are documented in
[`WORLD_ALIGNED_MATERIALS.md`](WORLD_ALIGNED_MATERIALS.md).

### Additional application textures

Core glTF material slots do not cover every application material graph. An
exporter may list additional model texture indices in material extras under
`cesium_godot_application_texture_indices`. The adapter realizes valid indices
through the same decoded-image cache as ordinary glTF slots and exposes a
`Dictionary[int, Texture2D]` on the supplied default material as metadata named
`cesium_godot_application_textures`.

Lifecycle receivers can use those textures to build an application shader
without reading files or decoding/uploading duplicate images. Invalid,
duplicate, or non-numeric indices are ignored. The textures follow the normal
tile/model resource lifetime; applications should not retain the dictionary
after tile unload unless they deliberately want to retain those GPU resources.

## Tests

`tests/godot/fixtures/lifecycle` is a tiny local 3D Tiles 1.1 tileset containing
one glTF triangle. `tests/run_godot_tests.sh` verifies the actual streamed
sequence:

1. material creation;
2. material customization;
3. primitive completion;
4. tile completion;
5. initial visibility;
6. hiding after the camera leaves the region;
7. re-showing without reloading the tile; and
8. safely deleting an assigned receiver while its tile remains active;
9. installing a replacement receiver without replaying historical load events;
   and
10. custom-shader propagation for two simultaneous overlays;
11. independent removal, atomic replacement, and stale-detach rejection; and
12. overlay detachment followed by unload while the Godot tile is valid;
13. zero-byte cache eviction followed by reload;
14. live tileset source replacement without stale-generation callbacks;
15. malformed glTF without a partial load callback;
16. tileset destruction while an HTTP content request is blocked; and
17. whole owning-subtree destruction while an HTTP content request is blocked,
    with no late callbacks or Godot resource leaks at process exit.

The fixture is ignored by Godot's scene importer because Cesium Native must
read the original glTF bytes directly.

`examples/lifecycle_material_demo` is the corresponding interactive,
non-Vanguard example. It streams its own local fixture, selects and customizes
a shader, attaches and removes an application raster texture, moves the view
to demonstrate visibility, and recreates the tileset to show detach/unload and
reload ordering. Its `--smoke-test` mode requires every receiver hook before
exiting successfully and is part of `tests/run_godot_tests.sh`.

`tests/run_csharp_tests.sh` drives the same local lifecycle content through the
generated C# facade. It verifies material selection, primitive and tile
signals, extras, initial readiness, camera-driven visibility, unload ordering,
wrapper invalidation, and terminal load-failure delivery in Godot 4.6.3 .NET.

## Design sources

The API semantics are adapted from the Apache-2.0 Cesium for Unreal lifecycle
receiver:

- <https://github.com/CesiumGS/cesium-unreal/blob/main/Source/CesiumRuntime/Public/Cesium3DTilesetLifecycleEventReceiver.h>
- <https://github.com/CesiumGS/cesium-unreal/blob/main/Source/CesiumRuntime/Private/Cesium3DTilesetLifecycleEventReceiver.cpp>
- <https://github.com/CesiumGS/cesium-unreal/blob/main/Source/CesiumRuntime/Public/CesiumLoadedTile.h>

The renderer placement follows Cesium Native's
`IPrepareRendererResources` contract:

- <https://cesium.com/learn/cesium-native/ref-doc/rendering-3d-tiles.html>
- <https://cesium.com/learn/cesium-native/ref-doc/classCesium3DTilesSelection_1_1IPrepareRendererResources.html>
