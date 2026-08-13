# Tileset request headers

`Cesium3DTileset.request_headers` attaches the same HTTP headers to the root
tileset request and to every dependent content request made by Cesium Native.
This matches `ACesium3DTileset::RequestHeaders` in Cesium for Unreal v2.29.0.
It is intended for private 3D Tiles servers that authenticate with a request
header instead of a query parameter.

```gdscript
$Cesium3DTileset.request_headers = {
	"Authorization": "Bearer " + session_token,
	"X-Tenant": tenant_id,
}
```

Header names and values must both be nonempty strings. Surrounding whitespace
is removed, names are normalized to lowercase because HTTP field names are
case-insensitive, and entries containing line breaks are ignored. Non-string,
empty, and names containing characters outside the HTTP field-name token syntax
are also ignored. Assigning a map that is equivalent after normalization does
not restart streaming. A real change recreates the Native tileset so the root
and all content requests use one consistent header snapshot.

The setter and getter both defensively copy the `Dictionary`. Mutating either
the assigned map or a map returned later therefore cannot silently change an
active tileset. Assign a new map through the property when rotating a token.

Header values are deliberately absent from streaming statistics, load-failure
resources, request markers, and logs. The property remains readable to the
application that owns the node, so applications must still avoid printing or
serializing it. Like Cesium for Unreal, headers apply to both URL and ion-backed
tilesets. Raster-overlay providers retain their separate `request_headers`
properties because imagery may use a different service or credential.

The deterministic runtime test requires the configured header independently on
the root JSON and its glTF content. It also covers normalization, setter/getter
copy isolation, no-op assignment without recreation, credential rotation with
recreation, and diagnostic redaction.

Upstream references:

- `Source/CesiumRuntime/Public/Cesium3DTileset.h`
- `Source/CesiumRuntime/Private/Cesium3DTileset.cpp`
- `Cesium3DTilesSelection::TilesetOptions::requestHeaders`
