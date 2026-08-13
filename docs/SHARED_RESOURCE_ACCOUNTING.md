# Shared renderer-resource accounting

Cesium Native may resolve the same external glTF image into one shared
`ImageAsset` used by many concurrently loaded tiles. The Godot renderer must
therefore avoid both duplicate GPU uploads and duplicate cache charges.

Generated Godot shader variants have a related but different ownership rule.
Many independently parsed glTF models produce byte-for-byte identical shader
source. Compiling a new `Shader` for every model repeats driver work even when
the variant is unchanged, so the tileset also shares exact generated shader
variants.

Repeated tiles may also point to the same complete glTF content. In that case,
the renderer shares the immutable realized mesh, base materials, metadata,
MultiMesh data, and image leases while preserving separate tile nodes,
transforms, visibility, raster bindings, and application material overrides.

This implementation follows the responsibility boundary of Cesium for Unreal
v2.29.0's `ExtensionImageAssetUnreal` and shared texture resource, adapted to
Godot's main-thread `Resource` ownership.

## Ownership model

Each `Cesium3DTileset` owns one `CesiumGltfImageAssetResourceCache`. The cache
is consulted only while a prepared model is realized on the Godot main thread.

- The key is the exact shared Cesium Native `ImageAsset` identity.
- The first active tile uploads one Godot `ImageTexture` and receives a shared
  lease.
- Later active tiles using that image receive the same `ImageTexture` object
  and another lease.
- A tile retains one lease per unique material image until the tile's realized
  renderer resources are destroyed.
- The cache itself keeps only a weak entry. It is not an unbounded second LRU
  and cannot keep an otherwise-unused texture alive.
- Releasing the final tile lease releases the cache's `ImageTexture` reference
  and retained Native image on the Godot main thread. An application that
  deliberately retains the texture can extend its Godot lifetime outside the
  tile cache, so the live diagnostics specifically measure tile-owned leases.

The cache does not create Godot objects on a worker thread. Worker preparation
only carries the cache owner forward to the later main-thread realization
stage. This preserves the renderer-resource and cancellation boundary described
in `TILE_LIFECYCLE.md`.

## Generated shader ownership

Each exact generated shader source is compiled into one Godot `Shader` per
tileset generation. Materials created for separate models may have different
parameters and texture resources while referring to that same immutable shader.

- The key is the complete generated Godot shader source, not a shortened hash,
  glTF material index, URL fragment, or Vanguard-specific material name.
- The first realization of a source is a miss and creates the `Shader`; later
  realizations are hits and receive the same Godot object.
- The cache retains shader variants strongly until the tileset generation is
  destroyed. The generator has a finite set of feature combinations, so this
  avoids recompiling a variant after an ordinary tile eviction without
  retaining unbounded per-content resources.
- Recreating the Native tileset creates a new cache generation and releases all
  variants from the old generation on the Godot main thread.

Shader statistics report object counts, not bytes. Godot exposes neither a
portable compiled-shader allocation size nor a rendering fence with which this
plugin could measure driver memory accurately.

## Repeated model ownership

Cesium Native records the fully resolved request URL on every successfully
loaded model as `Cesium3DTiles_TileUrl`. The per-tileset model-resource cache
uses that complete value as its key. It never derives identity from a raw tile
ID, path substring, basename, or Vanguard directory convention. Query strings
remain part of the key, and a cache never crosses a tileset generation.

The cache stores weak entries. Each realized tile holds a strong lease on its
shared model resource; the final tile eviction releases the mesh, materials,
metadata, MultiMesh buffers, and embedded-image leases. Thus repeated active or
Native-retained tiles share resources, but the renderer does not add another
inactive LRU outside `maximum_cached_bytes`.

Only immutable base resources are shared. A lifecycle receiver that selects
the default material receives a shallow per-tile material duplicate before
`_customize_material`, retaining the shared Shader and Texture objects while
preventing one tile's parameter changes from affecting another placement.
Raster bindings likewise remain per tile.

The current Native `CesiumGltf::Model` and its decoded buffers are still owned
per tile and remain charged per tile in `loaded_data_bytes`. This cache removes
duplicate Godot renderer realization and resource ownership; content producers
should use `EXT_mesh_gpu_instancing` to avoid independently parsing thousands
of placements in the first place.

## Byte accounting

Downstream Cesium Native commit `541bb26` changes loaded-tile cache accounting
from a sum of independent `Tile::computeByteSize()` results to two parts:

1. glTF buffer bytes that are actually owned by each loaded tile, excluding an
   embedded image's encoded buffer-view bytes; and
2. each distinct active decoded `ImageAsset` estimate, charged once across all
   tiles that share it.

A reference count removes the shared image charge only after the last loaded
tile stops using it. This makes `maximum_cached_bytes` and
`loaded_data_bytes` agree with Cesium Native's actual shared-image lifetime.

Geometry buffers are still charged per tile because the current Native glTF
model stores them per tile; there is no cross-tile mesh allocation to
de-duplicate. Repeated geometry inside one glTF uses one mesh or a Godot
`MultiMesh` and is already measured once at its real ownership boundary.
Metadata stored in a glTF buffer remains part of that tile's buffer charge;
metadata/property images participate in Native's shared `ImageAsset` charge.
If a future renderer introduces cross-tile shared geometry or material
resources, it must add the same explicit owner/lease accounting rather than
subtracting guessed placement costs.

The Godot snapshot adds these fields:

- `shared_texture_cache_hits`
- `shared_texture_cache_misses`
- `shared_texture_live_count`
- `shared_texture_live_bytes`
- `shared_texture_maximum_live_count`
- `shared_texture_maximum_live_bytes`
- `shared_shader_cache_hits`
- `shared_shader_cache_misses`
- `shared_shader_cache_entries`
- `shared_shader_cache_maximum_entries`
- `shared_model_cache_hits`
- `shared_model_cache_misses`
- `shared_model_live_count`
- `shared_model_live_geometry_bytes`
- `shared_model_live_texture_bytes`
- `shared_model_maximum_live_count`
- `shared_model_maximum_live_geometry_bytes`
- `shared_model_maximum_live_texture_bytes`

The byte values use the stable Cesium Native `ImageAsset::getSizeBytes()` cache
estimate. They describe actively tile-leased shared material textures, not driver
allocation measured through a rendering fence. Hit and miss counters are
cumulative for the current tileset generation; live values return to zero
after the final tile lease, and maximum values are high-water marks.
Shader entries remain live for the tileset generation by design. Hits and
misses are cumulative, `shared_shader_cache_entries` is the current exact
variant count, and `shared_shader_cache_maximum_entries` is its high-water
mark.
Shared-model byte values are prepared logical geometry and texture estimates,
not physical GPU allocation. They describe the unique live renderer resources
behind tile leases and must not be added to Native `loaded_data_bytes` as if
the two values represented the same ownership layer.

## Regression fixture

`tests/godot/shared_resource_accounting_test.gd` streams two different glTF
tiles that reference the same external image. It proves that:

- both materials expose the same Godot `Texture2D` identity;
- both materials expose the same exact Godot `Shader` identity;
- one miss and one hit produce one 16-byte live texture estimate;
- one shader miss and one hit produce one live generated variant;
- Native reports two distinct 66-byte geometry buffers plus one 16-byte image,
  rather than charging the image twice; and
- forced eviction unloads both tiles and releases the live shared count and
  bytes exactly when the final lease disappears.

The corresponding Cesium Native test loads one hundred glTF models sharing two
images and checks the exact de-duplicated cache total and zero balance after
unload.

A second real tileset addresses one model through `tile_a.gltf` and
`./tile_a.gltf`, which are different raw tile IDs but the same resolved URL. It
proves that two independent placement nodes share one ArrayMesh and base
material, receive distinct per-tile customized materials, report one model
miss and one hit, and release all live model bytes after final-owner eviction.
