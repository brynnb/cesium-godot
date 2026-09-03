# Raster-overlay providers

Raster-overlay provider nodes stream imagery through Cesium Native and attach
it to the matching 3D Tiles geometry through the generic
`CesiumRasterOverlayBinding` material contract. Provider nodes are children of
the `Cesium3DTileset` they decorate.

The provider API mirrors Cesium for Unreal v2.29.0 where its responsibilities
translate to Godot. Provider-specific nodes only describe where imagery comes
from; the abstract `CesiumRasterOverlay` base owns lifecycle, budgets, failure
delivery, and safe attachment to each Native tileset generation.

## Available providers

| Godot node | Source | Important properties |
| --- | --- | --- |
| `CesiumIonRasterOverlay` | Cesium ion raster asset | `asset_id`, optional overlay token/API URL/asset options |
| `CesiumBingMapsRasterOverlay` | Bing Maps imagery | API key, map style, culture, optional service URL |
| `CesiumGoogleMapTilesRasterOverlay` | Google Map Tiles API | API key, map/image/scale/layer/style options and session endpoint |
| `CesiumUrlTemplateRasterOverlay` | HTTP(S) or local `file:///` template | `template_url`, projection, levels, optional tiling scheme and coverage |
| `CesiumTileMapServiceRasterOverlay` | Tile Map Service and `tilemapresource.xml` | `url`, optional explicit zoom range |
| `CesiumWebMapServiceRasterOverlay` | OGC WMS | `base_url`, `layers`, `version`, `format`, dimensions and levels |
| `CesiumWebMapTileServiceRasterOverlay` | OGC WMTS | layer/style/matrix set, labels, projection, dimensions, subdomains and tiling scheme |
| `CesiumPolygonRasterOverlay` | In-memory `CesiumCartographicPolygon` Resources | polygons, inverted selection, optional whole-tile exclusion |
| `CesiumGeoJsonDocumentRasterOverlay` | Parsed Resource, inline JSON, local/HTTP URL, or Cesium ion | vector style, headers/credentials, raster mip count, parse diagnostics |
| `CesiumDebugColorizeTilesRasterOverlay` | Cesium Native diagnostic image | random half-transparent color per geometry tile; no network source |

`CesiumIonRasterOverlay.data_source = FromUrl` remains available only for old
Battle Road scenes that used the former combined ion/TMS node. New content
should use `CesiumTileMapServiceRasterOverlay` so provider intent remains
explicit.

## Common options and lifecycle

Every concrete provider inherits:

- `key`: stable material/overlay identity. It defaults to upstream's
  `Overlay0`, which generated plugin materials display automatically; other
  keys must match the receiving custom shader's sanitized parameter prefix;
- `maximum_screen_space_error`: imagery detail target;
- `maximum_texture_size`: maximum decoded tile dimension;
- `maximum_simultaneous_tile_loads`: independent imagery request cap;
- `sub_tile_cache_bytes`: bounded Cesium Native imagery sub-tile cache;
- `show_credits_on_screen`: requests on-screen attribution from the common
  credit system; and
- `generate_mipmaps`: generate the complete mip chain before upload.

Changing a provider property calls `refresh()`: the old Native provider is
removed and a new one is attached. Changing a tileset property that recreates
the Native tileset first detaches every provider, releases old imagery, and
then attaches a fresh provider instance to the new generation. This prevents a
Godot node from retaining a pointer to the previous Native collection.

`get_provider_type()` and `get_configuration()` provide inspectable state.
Configuration dictionaries include whether the provider is currently attached
but deliberately never include an ion access-token value. HTTP headers are
defensively copied. Provider and tileset `load_failure` signals receive the
same immutable `CesiumLoadFailure` for provider-creation failures.

## URL-template example

```gdscript
var imagery := CesiumUrlTemplateRasterOverlay.new()
imagery.name = "AerialImagery"
imagery.key = "AerialImagery"
imagery.template_url = "https://tiles.example.com/{z}/{x}/{y}.png"
imagery.minimum_level = 0
imagery.maximum_level = 18
imagery.request_headers = {"Authorization": "Bearer " + session_token}
tileset.add_child(imagery)
```

URL templates support Cesium Native's placeholders: `{x}`, `{y}`, `{z}`,
`{reverseX}`, `{reverseY}`, `{reverseZ}`, geographic bounds, projected bounds,
`{width}`, and `{height}`.

For a local template, use the shared path converter to produce a
standards-compliant absolute file URI on every supported platform:

```gdscript
imagery.template_url = CesiumUrlUtility.local_path_to_file_url(
	"res://imagery/{z}/{x}/{y}.png"
)
```

Local files are read on the Cesium worker pool and returned through the same
asset-response contract as HTTP imagery. Only `file:///absolute/path` and
`file://localhost/absolute/path` are accepted; remote file hosts are rejected
instead of being silently treated as local paths. Local reads are not placed in
the HTTP cache. `CesiumUrlUtility.local_path_to_file_url` also accepts native
Unix, Windows-drive, `user://`, and project-relative paths and applies the
required percent encoding. It can represent a UNC path canonically, but the
default accessor rejects remote hosts unless an application supplies an
explicit network-share access policy.

## WMS and WMTS

WMS creates `GetMap` requests for the comma-separated `layers`, requested MIME
`format`, WMS `version`, dimensions, and zoom range. WMS 1.3.0 is the default.

WMTS supports both REST templates and service URLs understood by Cesium
Native. Use the projection matching the matrix set: Web Mercator for common
EPSG:3857 sets and Geographic for EPSG:4326. Matrix labels may be supplied
explicitly or generated from `tile_matrix_set_label_prefix`. Set
`specify_tiling_scheme` only when the service's root tile counts or coverage
differ from the projection defaults. `dimensions` supplies static WMTS
dimensions such as time, while `subdomains` supplies the `{s}` template value.

## Bing Maps and Google Map Tiles

`CesiumBingMapsRasterOverlay` delegates the complete service-specific flow to
Cesium Native: it fetches imagery metadata, chooses quadkey/subdomain URLs,
applies optional culture, and collects both provider-specific and Bing logo
credits. All eight styles exposed by Cesium for Unreal v2.29.0 are available.

```gdscript
var bing := CesiumBingMapsRasterOverlay.new()
bing.api_key = credentials.bing_maps_key
bing.map_style = CesiumBingMapsRasterOverlay.AerialWithLabelsOnDemand
bing.culture = "en-US"
tileset.add_child(bing)
```

`CesiumGoogleMapTilesRasterOverlay` creates a Google session and then uses its
returned token and tile dimensions for imagery, viewport availability, and
required attribution. It exposes satellite, roadmap, and terrain maps; PNG,
JPEG, or automatic image format; 1x/2x/4x label scale; high DPI; roadmap,
Street View, and traffic layers; overlay-only mode; and Google JSON style
objects. Each `styles` entry must be one complete JSON object. Invalid entries
are omitted and reported through `last_style_errors` rather than turning a
valid remainder into malformed session JSON.

```gdscript
var google := CesiumGoogleMapTilesRasterOverlay.new()
google.api_key = credentials.google_map_tiles_key
google.map_type = CesiumGoogleMapTilesRasterOverlay.Roadmap
google.layer_types = (
	CesiumGoogleMapTilesRasterOverlay.LayerRoadmap |
	CesiumGoogleMapTilesRasterOverlay.LayerTraffic
)
google.styles = PackedStringArray([
	'{"featureType":"road","stylers":[{"saturation":-50}]}',
])
tileset.add_child(google)
```

The service URLs are configurable for compatible private gateways and
deterministic tests. Ordinary applications should keep their official HTTPS
defaults. Neither `get_configuration()` nor load-failure diagnostics exposes
the API key; query credentials are redacted by the common diagnostics path.
Applications remain responsible for the providers' current terms, enabled API
products, credentials, display requirements, and attribution.

Cesium Native v0.63.0 now supplies the Azure Maps provider/session API used by
Cesium for Unreal v2.29.0. The dependency blocker is gone, but the Godot node,
properties, credential redaction, attribution wiring, lifecycle integration,
and deterministic service tests have not been ported. Azure Maps is therefore
a P2 provider gap and is not listed as available above.

## Polygon masks

`CesiumPolygonRasterOverlay` has no network source. Cesium Native rasterizes
exact longitude/latitude polygons on workers and streams the resulting masks
through the same binding lifecycle. Generated plugin materials clip selected
pixels automatically; custom materials use the documented `clipping` shader
contract. Optional whole-tile exclusion avoids loading tiles entirely within
the clipped region. See [cartographic polygons, clipping, and tile
exclusion](POLYGONS_AND_TILE_EXCLUSION.md) for exact coordinates, inversion,
custom materials, and Native limitations.

## Debug tile colors

`CesiumDebugColorizeTilesRasterOverlay` is a zero-credential diagnostic for
seeing the geometry tiles Cesium has selected. Add it as a child of a tileset:

```gdscript
var tile_colors := CesiumDebugColorizeTilesRasterOverlay.new()
tileset.add_child(tile_colors)
```

Cesium Native creates an independent 1x1 RGBA image for every geometry tile.
Its random RGB value is drawn over the source material with alpha byte 127
(approximately 50%). The provider explicitly reports that no additional
imagery detail is available, so it never causes geometry to refine merely to
serve the diagnostic. Parent/child replacements, simultaneously selected
leaves, and tile boundaries therefore appear as separate translucent colors.

Keep the inherited key at its `Overlay0` default when using generated plugin
materials. A custom material may use another key by implementing the normal
raster-binding convention. This provider is intended for debugging, not for a
stable semantic color assignment: colors may change when the provider is
refreshed or a tile is reloaded.

## GeoJSON

`CesiumGeoJsonDocumentRasterOverlay` parses vector features once, then asks
Cesium Native to rasterize only the imagery tiles requested by the active 3D
Tiles geometry. It accepts an owned `CesiumGeoJsonDocument`, inline text,
`res://`, `user://`, `file://`, HTTP(S), or Cesium ion sources. Parsing and URL
loading are asynchronous for provider sources; live document or style edits
replace the provider generation without publishing stale results. See
[GeoJSON documents and vector styling](GEOJSON_AND_VECTOR_STYLING.md) for the
complete API, exact-coordinate access, lifetime contract, examples, and the
current Godot point-styling gap.

## Materials and filtering

Providers do not choose application shaders. Generated plugin glTF materials
implement upstream's standard `Overlay0` layer, while a tileset lifecycle
receiver may select a custom material implementing any named binding described
in [raster-overlay materials](RASTER_OVERLAY_MATERIALS.md). Godot shader sampler
hints control filtering. `generate_mipmaps` controls whether mip data exists;
it does not pretend Godot exposes Unreal's independent texture-group,
minification, and magnification controls.

## Credentials and attribution

Do not commit provider tokens, API keys, or authorization headers in `.tscn`
files. Add them at runtime from project secrets or the authenticated session.
Ion uses an overlay token only when explicitly set and otherwise uses the
project default.
Enable `show_credits_on_screen` when a source requires on-screen attribution
and render the shared `CesiumGDCreditSystem` output as described in
[credits and attribution](CREDITS_AND_ATTRIBUTION.md).

## Verification

`tests/godot/raster_provider_test.gd` covers the network provider configuration APIs,
normalization, defensive copies and credential redaction. It also streams a
real HTTP URL-template image with a custom header, attaches it to real geometry,
recreates the Native tileset, proves a fresh provider attaches, streams a local
file template, and verifies raster textures are fully released at shutdown. A
deterministic local service also exercises Bing metadata/quadkey imagery and
Google POST session creation, JSON-style filtering, viewport credits, coarse
imagery attachment, key redaction, and removal without using real credentials.
`tests/godot/tile_exclusion_polygon_test.gd` covers the polygon provider's
Native mask pixels, material parameters, whole-tile culling, and live refresh.
`tests/godot/debug_color_overlay_test.gd` streams two geometry tiles, verifies
their independent 1x1 half-transparent images and generated `Overlay0` shader
bindings, then proves refresh/removal detach old bindings and release their
renderer resources.
`tests/godot/geojson_document_test.gd` covers parsing, warnings, traversal,
exact coordinates, metadata, attribution, styles, and retained object
lifetimes. `tests/godot/geojson_overlay_test.gd` rasterizes a real two-tile
fixture and covers every source mode except credentialed ion, HTTP headers,
style refresh, malformed input, source replacement, manual removal, and
provider destruction while work is still pending.
