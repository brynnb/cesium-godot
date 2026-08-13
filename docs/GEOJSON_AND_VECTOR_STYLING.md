# GeoJSON documents and vector styling

The GeoJSON API keeps parsing, data ownership, styling, source loading, and
raster-overlay lifecycle separate. It follows Cesium for Unreal's runtime
responsibility boundaries while exposing ordinary Godot Resources and signals.
Cesium Native v0.63.0 remains authoritative for GeoJSON validation, geometry,
and worker-thread rasterization.

## Parse and inspect a document

`CesiumGeoJsonDocument.load_from_string()` is the synchronous path for text an
application already has. It preserves parser errors and warnings, attribution,
foreign members, feature IDs/properties, bounding boxes, and exact float64
coordinates.

```gdscript
var document := CesiumGeoJsonDocument.new()
var ok := document.load_from_string(
	FileAccess.get_file_as_string("res://vectors/roads.geojson"),
	[{"html": "Road survey", "show_on_screen": true}]
)
if not ok:
	push_error("GeoJSON: %s" % [document.errors])
	return

print(document.get_statistics())
for object: CesiumGeoJsonObject in document.get_objects():
	print(object.get_object_type_name())
```

`get_objects()` traverses the complete document. `get_children()` provides
explicit collection/feature traversal. Feature wrappers expose `feature_id`,
properties, and geometry; arbitrary non-standard members are available through
`get_foreign_members()`.

Geometry methods retain source precision:

- `get_points_exact()` returns flattened longitude, latitude, height triples;
- `get_line_strings_exact()` returns one packed float64 array per line; and
- `get_polygons_exact()` returns polygons containing rings of packed float64
  triples.

Returned `CesiumGeoJsonObject` Resources share ownership of the Native document
storage. They therefore remain valid after the original document Resource is
released or an overlay refresh replaces its generation. No Resource retains a
raw Godot object pointer.

Malformed documents return `false`; repaired-but-usable geometry returns
`true` with warnings. For example, Cesium closes an open polygon ring and
reports the repair rather than silently changing the input.

## Style lines and polygons

`CesiumVectorStyle` currently exposes the line and polygon subset of the
pinned Native version:

- line color, normal/random color mode, width, and pixel/meter width mode;
- polygon fill enable/color/mode; and
- polygon outline enable/color/mode/width/width mode.

```gdscript
var style := CesiumVectorStyle.new()
style.line_color = Color("ffcc33")
style.line_width = 3.0
style.line_width_mode = CesiumVectorStyle.WidthPixels
style.polygon_fill_color = Color(0.1, 0.5, 0.9, 0.35)
style.polygon_outline_enabled = true
style.polygon_outline_color = Color.WHITE
```

An overlay's `default_style` is the fallback. A style assigned to an individual
GeoJSON object with `object.set_style(style)` overrides that fallback for the
object. `get_style()` returns an editable copy; call `set_style()` to apply it.
Changing a document-owned object or the overlay's default style emits the
normal Resource `changed` signal and refreshes the attached overlay.

## Stream GeoJSON over 3D Tiles

Add a `CesiumGeoJsonDocumentRasterOverlay` beneath the `Cesium3DTileset`. The
provider supports four source modes:

| Source | Input | Work |
| --- | --- | --- |
| `FromDocument` | valid `CesiumGeoJsonDocument` | reuses parsed owned data |
| `FromString` | `geo_json` text | parses on the Cesium worker pool |
| `FromUrl` | `res://`, `user://`, `file://`, or HTTP(S) `url` | reads/parses asynchronously through the shared accessor |
| `FromCesiumIon` | asset ID, optional token/API URL | resolves, downloads, and parses asynchronously |

```gdscript
var vectors := CesiumGeoJsonDocumentRasterOverlay.new()
vectors.key = "Overlay0"
vectors.source = CesiumGeoJsonDocumentRasterOverlay.FromUrl
vectors.url = "res://vectors/roads.geojson"
vectors.request_headers = {"Authorization": "Bearer " + session_token}
vectors.default_style = style
vectors.maximum_texture_size = 1024
vectors.mip_levels = 2
vectors.document_loaded.connect(func(parsed): print(parsed.get_statistics()))
vectors.document_load_failed.connect(
	func(errors, warnings): push_error("GeoJSON: %s / %s" % [errors, warnings])
)
tileset.add_child(vectors)
```

Local Godot paths are resolved to absolute `file://` URLs and read on the same
worker pool as other local Cesium content. HTTP uses the tileset's decorated
asset accessor, so connection reuse, bounded retries, cancellation, cache
policy, headers, timing, and redacted failure diagnostics are shared with the
rest of the plugin. Ion uses the overlay token when set and otherwise the
project default; configuration diagnostics report only which token source is
active, never the token value.

Native rasterization runs on workers and creates only the imagery demanded by
selected geometry tiles. Godot texture creation and material attachment follow
the same bounded, generation-safe path as every raster provider. Refresh,
manual removal, source replacement, and node destruction invalidate pending
callbacks, so old parses cannot publish signals or attach textures later.

`document_loaded(document)` returns the parsed document for inspection.
`document_load_failed(errors, warnings)`, `last_document_errors`, and
`last_document_warnings` preserve parser diagnostics. The inherited
`load_failure` signal remains responsible for general provider construction
and transport failures.

## Current scope

The pinned Cesium Native v0.63.0 rasterizer draws lines, polygons, points, and
multipoints. The Godot document API already preserves all of those geometries,
properties, and exact coordinates, but `CesiumVectorStyle` and the GeoJSON
overlay binding currently expose only line and polygon styling. Point fill,
outline, radius, and their streamed raster regression remain Godot-port work;
the Native dependency is no longer the blocker.

This is a GeoJSON raster provider, not a general vector-tile source. MVT or
another tiled vector protocol, label placement, and icon atlases remain
separately tracked work. `mip_levels` is intentionally bounded to 0–8; it
controls raster image mip generation and does not alter source geometry or 3D
Tiles LOD.

## Verification

`tests/godot/geojson_document_test.gd` covers valid and malformed parsing,
repair warnings, nested traversal, exact coordinates, large JSON values,
properties, foreign members, attribution, style round trips, and retained
object lifetime. `tests/godot/geojson_overlay_test.gd` exercises a real
two-geometry-tile raster attachment, exact pixels, style replacement, inline,
local, and HTTP sources, headers, failures, stale generations, manual removal,
and destruction during pending work.

Native v0.63.0 includes the minimum 1x1 raster-dimension fix that prevents
empty-quadrant Blend2D crashes. The downstream lock still carries parser
warning propagation that preserves repaired-ring diagnostics through nested
objects.
