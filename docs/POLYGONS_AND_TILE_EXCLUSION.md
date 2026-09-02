# Cartographic polygons, clipping, and tile exclusion

The plugin exposes two related but deliberately separate mechanisms:

- `CesiumPolygonRasterOverlay` rasterizes exact cartographic polygons into a
  streamed monochrome mask. The generated default glTF material discards white
  mask pixels, so the selected region is clipped without a Vanguard-specific
  shader.
- `CesiumTileExcluder` calls an application predicate during Cesium Native
  selection. Returning `true` rejects the current tile and its complete
  descendant subtree before content requests or Godot mesh realization.

This follows Cesium for Unreal v2.29.0's
`CesiumCartographicPolygon`, `CesiumPolygonRasterOverlay`, and
`CesiumTileExcluder` responsibility boundaries. The Godot APIs use Resources,
component-style child Nodes, lifetime-safe `ObjectID`s, and `Callable`s instead
of Unreal actors, components, splines, and Blueprint events.

## Exact polygon data

`CesiumCartographicPolygon` stores alternating float64 longitude/latitude
degrees as its authoritative data:

```gdscript
var polygon := CesiumCartographicPolygon.new()
polygon.vertices_degrees_exact = PackedFloat64Array([
	-122.52, 37.70,
	-122.35, 37.70,
	-122.35, 37.83,
	-122.52, 37.83,
])
```

`vertices_degrees` is the inspector-friendly `PackedVector2Array` convenience.
In a single-precision Godot build it is intentionally less exact. ECEF triples
can be converted with `set_vertices_from_ecef_exact`, using any
`CesiumEllipsoid` rather than assuming WGS84.

The Resource exposes Native triangulation, bounding-rectangle, completely
inside, and completely outside queries. Fewer than three vertices or a shape
that Native cannot triangulate is invalid. Editing a Resource used by a live
polygon overlay automatically creates a clean new overlay generation.

## Polygon masks and clipping

Add a polygon overlay as a direct child of its tileset:

```gdscript
var overlay := CesiumPolygonRasterOverlay.new()
overlay.polygons = [polygon]
overlay.invert_selection = false
overlay.exclude_selected_tiles = true
tileset.add_child(overlay)
```

The Native raster provider produces white selected pixels and black unselected
pixels. `invert_selection` swaps which geographic area is selected. The plugin
reserves the stable material key `clipping`; generated default materials sample
that mask with Cesium's per-tile UV translation/scale, correct Godot image-row
orientation, and discard selected pixels.

`exclude_selected_tiles` is a performance optimization, not the pixel-precise
clip itself. When enabled, a tile wholly inside the selected area is excluded
before loading. Boundary tiles still load so the mask can clip them precisely.
When inverted, wholly outside tiles are excluded instead. Disable this option
when the mask drives a non-clipping effect and selected geometry must remain.

A custom lifecycle material must implement the same stable shader contract:

```glsl
uniform bool clipping_enabled = false;
uniform sampler2D clipping_texture;
uniform vec4 clipping_translation_scale;
uniform int clipping_texture_coordinate_index = -1;
```

Choose `UV` for index `0` and `UV2` for index `1`, apply
`uv * clipping_translation_scale.zw + clipping_translation_scale.xy`, flip the
resulting Y coordinate for Godot image sampling, and discard when the red mask
channel is greater than `0.5`. The ordinary raster binding lifecycle populates
and clears these parameters.

Cesium Native v0.63.0's `RasterizedPolygonsOverlay` still documents that its
triangle-mask path ignores the antimeridian. This adapter does not hide that
current limitation or silently rewrite user geometry. Native's newer GeoJSON
vector rasterizer is separate and does have its own antimeridian coverage.

## Application tile predicates

`CesiumTileExcluder` is also a direct tileset child:

```gdscript
func exclude_tile(context: CesiumTileExclusionContext) -> bool:
	return (
		context.depth > 4
		and context.tile_bounds.distance_to_source_position_components(
			important_ecef_position
		) > 50_000.0
	)

var excluder := CesiumTileExcluder.new()
excluder.predicate = Callable(self, "exclude_tile")
tileset.add_child(excluder)
```

The `predicate` property is the only exclusion callback path in every Godot
language. The context contains the Native tile ID (which may legitimately be empty), depth,
child count, geometric error, ADD/REPLACE mode, an owned bounding-volume
snapshot, and the complete float64 tile transform.

The adapter reuses one context object to avoid allocating a Godot Object for
every visited tile. Do not retain it. Copy `context.to_dictionary()` during the
call if a diagnostic snapshot is needed. Bounding volumes inside that copied
dictionary are independently owned and remain valid after tile eviction.

Predicates run synchronously during main-thread Native traversal. They must be
fast, deterministic, non-blocking, and must not modify the scene tree or their
tileset. Exceptions, missing callbacks, and non-boolean results fail open so a
bad application predicate cannot make world data disappear. Evaluation,
exclusion, invalid-result, and per-Native-frame counters are exposed for
diagnostics.

Disabling or removing the component removes the exact shared Native adapter
from both the current generation and future tileset options. Re-enabling it
does not recreate the tileset. Source recreation retains enabled generic
excluders, while polygon overlay generations remove their coupled excluder
before releasing the old mask.

## Validation

`tests/godot/tile_exclusion_polygon_test.gd` covers exact LLH/ECEF conversion,
Native triangulation and rectangle queries, typed predicate context, invalid
predicate fail-open behavior, disabling/re-enabling selection, whole-tile
polygon culling, real streamed mask attachment, generated-material clipping
parameters, selected/unselected mask pixels, and live polygon recreation.
