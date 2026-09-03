# Editor asset catalogs

> **Status:** parked foundation. The catalog model and its focused compatibility
> test are complete, but it is intentionally not connected to the editor dock.
> Runtime and Vanguard-facing work currently take priority.

`CesiumEditorAssetCatalog` is the credential-free data model for browsing local
and private-server 3D Tiles sources in the Godot editor. It is intentionally
separate from the scene builder and from HTTP transport. Projects can therefore
use the same parser for a local file, a private catalog response, or a catalog
created in memory without coupling the add-on to Cesium ion.

The implementation is located at:

```text
addons/cesium_godot/editor/catalog/editor_asset_catalog.gd
```

## Catalog v1

A catalog is a UTF-8 JSON object with this shape:

```json
{
  "schema": "cesium-godot-asset-catalog-v1",
  "assets": [
    {
      "name": "Campus buildings",
      "type": "3d_tiles",
      "url": "../tiles/campus/tileset.json",
      "description": "Private photogrammetry tileset",
      "attribution": "Example Survey Group"
    },
    {
      "name": "Unconfigured tileset",
      "type": "blank",
      "url": "",
      "description": "A scene placeholder",
      "attribution": ""
    }
  ]
}
```

All five normalized entry fields are strings:

- `name` is required and non-empty.
- `type` is currently either `3d_tiles` or `blank`.
- `url` is required for `3d_tiles` and must be empty for `blank`.
- `description` and `attribution` are optional and normalize to empty strings.

Unknown fields are not retained. Credential-like fields are rejected rather
than ignored so a token cannot accidentally appear to be supported or safe.
The whole parse is atomic: one invalid entry rejects the document and clears
the model instead of exposing a partial catalog.

## URL resolution

Pass the catalog document's location—not its directory—as the second argument
to `parse_json` or `parse_dictionary`. Relative references use normal document
resolution:

```gdscript
var catalog := CesiumEditorAssetCatalog.new()
var ok := catalog.parse_json(
    response_body,
    "https://world.example.test/catalogs/vanguard/catalog.json"
)
```

For that catalog, `../tiles/campus/tileset.json` resolves to
`https://world.example.test/catalogs/tiles/campus/tileset.json`. A catalog at
`/srv/world/catalog.json` resolves the same relative reference to a canonical,
percent-encoded `file:///srv/tiles/campus/tileset.json` URL.

The resolver supports:

- absolute HTTP and HTTPS URLs;
- HTTP root-relative, path-relative, and scheme-relative references;
- absolute `file://` URLs;
- Unix, Windows-drive, and UNC local paths;
- `res://`, `user://`, and project-relative paths for direct editor entries;
- percent-encoded spaces and non-ASCII local filenames.

`local_path_to_file_url` delegates to the runtime-wide
`CesiumUrlUtility.local_path_to_file_url`; `make_direct_entry` remains the
catalog-specific helper for a file picker or direct URL field. Unsupported
schemes fail explicitly.

## Browsing and filtering

`get_entries(search_text, type_filter, sort_field, ascending)` returns defensive
copies. Search is case-insensitive across all normalized fields. Type filtering
accepts `blank`, `3d_tiles`, or an empty string for all entries. Sorting accepts
`name`, `type`, or `url` and has deterministic name/URL tie breakers.

## Credentials and private servers

The catalog model has no networking code and has nowhere to store request
headers, cookies, passwords, or access tokens. It rejects credential-shaped
fields, URL user-info (`user:password@host`), and common credential-bearing
query keys. It also never prints the catalog JSON or its URLs; parse errors only
describe the field and entry number.

If a private catalog needs authentication, the editor transport should keep the
credential in its own in-memory request configuration, fetch the JSON, and pass
only the response body and credential-free request URL to this model. Asset
requests should receive authentication through the same separate transport
policy rather than embedding secrets in catalog data.

## Why there is no HTTP directory scraping

The model only accepts an explicit v1 catalog or a direct source URL. HTTP does
not define a portable directory-listing API, and the 3D Tiles specification
defines a tileset entry document rather than a server asset-directory protocol.
Parsing arbitrary server-generated HTML would be server-specific, fragile, and
could silently cross authentication or navigation boundaries. Private servers
should expose a v1 JSON catalog (which may be generated from their own database
or filesystem), while a local editor file picker may create direct entries.

This is an intentional limitation, not an unimplemented discovery fallback.

The parked model has a credential-free focused test which can be run without
enabling the editor plugin:

```bash
godot4 --headless --path tests/godot \
  --script res://editor_asset_catalog_test.gd
```
