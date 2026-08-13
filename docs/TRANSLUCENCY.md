# Translucent glTF rendering

The generic renderer follows the glTF material contract and isolates every
source primitive whose `alphaMode` is `BLEND`. Opaque and alpha-masked
primitives remain batched as surfaces on the parent `Cesium3DTile`; each
ordinary blended primitive becomes a one-surface child `MeshInstance3D`.
`EXT_mesh_gpu_instancing` already owns a child `MultiMeshInstance3D`, so a
blended instanced primitive uses that existing boundary.

This mirrors Cesium for Unreal v2.29.0's important behavior: it selects a
translucent base material for `BLEND` and realizes independently sortable
primitive components. It does not copy Unreal component APIs into Godot.

## Why primitives are isolated

Godot sorts transparent `VisualInstance3D` objects back-to-front. It cannot
sort separate surfaces inside one `MeshInstance3D`, and it does not provide
order-independent transparency in the supported Godot 4.6.3 renderer API.
Leaving all glTF primitives in one tile mesh therefore gives the whole tile
one sort position, even when its windows, foliage, or facade layers occupy
different places.

Each isolated BLEND renderer enables `sorting_use_aabb_center`. Godot then
uses that primitive's bounds center rather than the tile/node origin for its
transparent sort distance. Opaque batching, immutable shader and texture
sharing, per-tile visibility, raster overlays, LOD transitions, and lifecycle
material overrides continue through the same logical primitive map.

Godot's relevant engine behavior is documented in
[Transparency sorting](https://docs.godotengine.org/en/stable/tutorials/3d/3d_rendering_limitations.html#transparency-sorting),
[`VisualInstance3D.sorting_use_aabb_center`](https://docs.godotengine.org/en/stable/classes/class_visualinstance3d.html#class-visualinstance3d-property-sorting-use-aabb-center),
and [`Material.render_priority`](https://docs.godotengine.org/en/stable/classes/class_material.html#class-material-property-render-priority).

## Tileset controls

`Cesium3DTileset.translucency_sort_priority` defaults to `0` and is clamped to
Godot's `-128..127` material range. It is assigned to every generated or
application-selected material for the tileset, matching Cesium for Unreal's
tileset-wide component priority. Changing it updates loaded materials in
place; it does not rebuild tiles or shaders. Leave it at zero unless the
application must explicitly order the entire tileset relative to another
transparent system. A nonzero value does not solve intersections within one
primitive.

`Cesium3DTileset.translucency_depth_prepass_enabled` defaults to `true`.
Generated `BLEND` shaders then include Godot's `depth_prepass_alpha` render
mode, allowing fully opaque texels in mostly-opaque blended assets to populate
depth before their transparent pass while partial-alpha fragments remain
blended. This commonly improves foliage and facade layering but cannot make
arbitrary intersecting transparency order-independent. Changing the setting
recreates the tileset because it changes generated shader variants.

`MASK` remains an alpha-scissor/coverage path and is not moved into transparent
child renderers. `OPAQUE` remains fully batched.

## Collision, picking, and ownership

Separating render nodes does not remove physics. When physics meshes are
enabled, all non-instanced triangle primitives—opaque, masked, and blended—are
combined into one tile-local concave collision shape. The tile records the
face range belonging to each logical renderer primitive, so
`CesiumMetadataPicking.resolve_hit` still returns the correct primitive,
surface-local face, feature IDs, and metadata even when its visual mesh is a
child node.

The shared-model cache retains the parent mesh and every standalone primitive
mesh under one content lease. Multiple tile placements may share those
immutable resources while owning independent child nodes, visibility,
material overrides, raster bindings, and collision bodies. An all-BLEND model
is valid: its parent `ArrayMesh` has zero surfaces and its child renderers carry
all visual geometry.

## Diagnostics and custom materials

`CesiumLoadedTilePrimitive` exposes:

- `translucent` — the source glTF primitive uses `alphaMode: BLEND`;
- `translucency_isolated` — that primitive owns an independently sortable
  renderer;
- `render_diagnostics.sorting_uses_aabb_center`;
- material diagnostics for `alpha_mode`, `source_translucent`,
  `translucency_depth_prepass`, and `render_priority`.

Isolation is intentionally based on source glTF alpha semantics, like the
upstream implementation. A lifecycle callback that replaces an authored
`OPAQUE` material with an arbitrary transparent shader does not retroactively
move geometry between renderer nodes. Content authors should mark blended
primitives as `BLEND`; application replacement shaders should preserve that
source contract.

## Remaining renderer limits

Primitive isolation materially improves the common case but does not remove
Godot's transparency limitations. Triangles that intersect inside one source
primitive are still ordered by that primitive's draw call, and independently
sorted primitives can still have ambiguous cyclic intersections. Splitting
content at meaningful transparent object boundaries is the available correct
content-side remedy. Weighted or per-pixel order-independent transparency is
not exposed by the supported public Godot renderer API and is not simulated
with an application-specific compositor in this reusable plugin.

The streamed regressions cover mixed opaque/BLEND content, all-BLEND content,
independent AABB-centered nodes, live priority changes, enabled and disabled
depth-prepass shader variants, combined collision, face-to-primitive picking,
and an empty parent mesh on Godot 4.6.3.
