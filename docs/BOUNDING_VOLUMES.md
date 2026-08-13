# Bounding-volume queries

The plugin exposes the authoritative Cesium Native bounds for a tileset and
for every realized tile as lifetime-safe `CesiumBoundingVolume` Resources.
This is the Godot counterpart of Cesium for Unreal's `CesiumTile` and
`CalcBounds` boundary, without retaining a Native `Tile*` in application code.

## Tileset and tile API

The tileset bound is its root tile's hierarchy bound. It can be queried as soon
as the root tileset JSON has loaded, even when no renderable leaf is visible:

```gdscript
var bounds: CesiumBoundingVolume = tileset.get_tileset_bounds()
var source_aabb: AABB = tileset.get_tileset_source_aabb()
```

Each realized `Cesium3DTile` exposes three independent Resources:

```gdscript
var hierarchy_bounds: CesiumBoundingVolume = tile.tile_bounds
var content_bounds: CesiumBoundingVolume = tile.content_bounds
var request_bounds: CesiumBoundingVolume = tile.viewer_request_bounds
```

`tile_bounds` encloses the tile and its descendants. `content_bounds` is the
optional tighter bound authored for only that tile's renderable content.
`viewer_request_bounds` is the optional volume that Cesium uses to decide
whether the viewer is close enough to request the content. An absent optional
volume is `null`; it is not replaced with the hierarchy bound.

## Shapes and precision

`CesiumBoundingVolume.volume_type` and `volume_type_name` distinguish all
Cesium Native variants:

- sphere;
- oriented box;
- geographic region;
- geographic region with loose-fitting heights;
- S2 cell;
- cylinder region.

Common fields include `center`, `oriented_box_transform`,
`oriented_box_size`, and `source_aabb`. The `details` Dictionary adds the
authored radius, region rectangle and heights, S2 token and level, or cylinder
translation/rotation/radial/angular data as appropriate.

Godot `Vector3`, `Transform3D`, and `AABB` use the engine's configured
`real_t` precision. For large Earth-centered coordinates, use the
`PackedFloat64Array` fields `center_components`,
`oriented_box_half_axes`, and the full-precision arrays in `details`. Those
arrays preserve the Native doubles even in an ordinary single-precision Godot
build.

Bounds are expressed in Cesium's tileset/source coordinate system, normally
ECEF for globe tilesets and the published local Cartesian frame for Vanguard.
The explicit conversion API avoids guessing an application's georeference or
floating-origin policy:

```gdscript
var source_to_gameplay := Transform3D(...)
var gameplay_aabb: AABB = bounds.get_transformed_aabb(source_to_gameplay)
```

The AABB is a conservative enclosure of Native's oriented-box conversion. It
is useful for navigation partitions and broad-phase tests; it does not replace
the original sphere, region, S2 cell, or cylinder. Queries in the original
source frame are also available:

```gdscript
if bounds.contains_source_position(source_position):
    print("inside")
var distance_metres := bounds.distance_to_source_position(source_position)
```

For ECEF-scale inputs in a single-precision Godot build, pass a three-element
`PackedFloat64Array` to `contains_source_position_components` or
`distance_to_source_position_components` instead of first narrowing the
position to `Vector3`.

## Ownership and coverage

The Resource owns a copy of the Native bounding-volume variant. It contains no
tile, tileset, renderer, or Godot node pointer and remains queryable after its
source tile is hidden, evicted, or destroyed. Returned Dictionaries and arrays
are new containers, so application mutation cannot change later queries.

The headless tests cover transformed root and tile boxes, content and
viewer-request spheres, geographic regions, S2 cells, cylinder regions,
full-precision data, transformed AABBs, containment/distance queries, copy
isolation, and retained Resources after tile and tileset destruction.
