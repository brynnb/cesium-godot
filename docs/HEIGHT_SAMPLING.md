# Asynchronous terrain height sampling

`Cesium3DTileset` can query the most detailed height represented by its source
tileset without requiring that location to be visible or realized as Godot
geometry.

This is the Godot counterpart of Cesium for Unreal v2.29.0's
`CesiumSampleHeightMostDetailedAsyncAction` and delegates the actual query to
Cesium Native's `Tileset::sampleHeightMostDetailed`. Native traverses the tile
hierarchy, schedules any missing tile data through the same bounded loader used
for rendering, and intersects the source glTF geometry. The adapter does not
raycast currently visible Godot meshes and does not create collision shapes.

## Coordinates and results

Inputs and outputs are cartographic triples:

1. longitude in degrees;
2. latitude in degrees;
3. height in meters above the tileset ellipsoid, normally WGS84.

Ellipsoid height is not height above mean sea level. The input height is ignored
when sampling succeeds. If no surface is found, it is copied unchanged to the
corresponding failed result.

The output contains one `CesiumSampleHeightResult` for each input, in the same
order. `sample_success` distinguishes sampled heights from unchanged input
heights. Warnings describe tile, content, or intersection problems without
changing the batch's ordering.

`longitude_latitude_height` is a convenient `Vector3`. On Godot builds where
`Vector3` uses 32-bit components, use
`longitude_latitude_height_components` to retain the exact float64 result.

## Godot API

```gdscript
var request: CesiumSampleHeightMostDetailedRequest = \
    tileset.sample_height_most_detailed_exact(PackedFloat64Array([
        -75.612025, 40.041684, 0.0,
        -75.612088, 40.042526, 0.0,
    ]))

request.completed.connect(func(results: Array, warnings: PackedStringArray):
    for result: CesiumSampleHeightResult in results:
        if result.sample_success:
            print(result.longitude_latitude_height_components)
)
```

Two entry points are available:

- `sample_height_most_detailed(PackedVector3Array)` is convenient for ordinary
  Godot coordinates.
- `sample_height_most_detailed_exact(PackedFloat64Array)` accepts flattened
  longitude/latitude/height triples and avoids an input precision loss.

Both return immediately with a pending
`CesiumSampleHeightMostDetailedRequest`. Connect `completed` before the next
tileset update, or poll `is_finished()`, `status`, `results`, and `warnings`.
The tileset must continue receiving its normal `update_tileset` calls because
Cesium Native advances height-query tile work from its normal loader update.

If the tileset leaves the scene tree, is destroyed, or changes source before a
query completes, the request changes to `Cancelled`, retains a diagnostic
warning, and emits `cancelled`. Completed result objects contain only copied
cartographic values and remain valid after the tileset unloads.

## Scheduling and limitations

Height queries use Cesium Native's tile requester and normal request limits.
They can load source tiles that are outside the current camera view, but they do
not make those tiles visible. The downstream Native cancellation integration
also treats active height-query tiles as required work, so camera movement
cannot mistakenly cancel a query's transfer.

Cesium Native currently reports a warning rather than intersecting content that
requires `EXT_mesh_gpu_instancing`. Applications should also avoid issuing very
large batches every frame: one batch is asynchronous and bounded, but it still
represents real hierarchy traversal, downloads, decoding, and geometry tests.

The deterministic headless fixture covers a successful surface sample, a miss,
batch ordering, unchanged failed height, float64 precision, asynchronous signal
delivery, and cancellation during source recreation.
