# Geocoding

`CesiumGeocoderService` performs search and autocomplete requests through the
Cesium ion geocoder API. It is a standalone runtime `Node`: it does not require
a `Cesium3DTileset`, editor tooling, Cesium ion cloud hosting, or Vanguard code.
Set `api_url` to use the public ion API or a compatible private/self-hosted ion
endpoint.

The implementation delegates application-data discovery, request construction,
provider selection, JSON decoding, feature destinations, and attribution to
the pinned `CesiumIonClient::Connection`. Godot owns only the engine-facing
async lifetime and copied result Resources.

## Basic use

Add the service to the scene tree before starting requests so it can pump its
Native network and main-thread queues.

```gdscript
var geocoder := CesiumGeocoderService.new()
add_child(geocoder)

# Inject this from application secrets at runtime. Do not save it in a scene.
geocoder.access_token = runtime_credentials.cesium_ion_token

var request := geocoder.search(
	"Kojan Harbor",
	CesiumGeocoderService.Default
)
request.completed.connect(func(result: CesiumGeocoderResult) -> void:
	for feature in result.features:
		print(feature.display_name)
		print(feature.longitude_latitude_height_components)
)
request.failed.connect(func(code: String, message: String, status: int) -> void:
	push_error("Geocoder failed [%s/%d]: %s" % [code, status, message])
)
```

Use `autocomplete(query, provider)` for partial text entered interactively, or
`geocode(query, provider, request_type)` when the request type is dynamic.
Providers are `Google`, `Bing`, and `Default`; `Default` lets the ion server
choose. Search and autocomplete use the corresponding Cesium ion endpoints.

The request handle exposes `status`, `is_finished()`, its copied query/provider/
request type, HTTP status, error fields, and the final `result`. Invalid empty
queries return an already-failed request with `InvalidQuery`, so callers that do
not connect signals may still inspect a deterministic result.

## Results and precision

`CesiumGeocoderResult` contains defensive arrays of:

- `CesiumGeocoderFeature`, with a display name, center location, bounding globe
  rectangle, and whether the source destination was a point; and
- `CesiumGeocoderAttribution`, with provider HTML and its mandatory
  `show_on_screen` policy.

`longitude_latitude_height_components` is an authoritative
`PackedFloat64Array[longitude degrees, latitude degrees, ellipsoid-height
meters]`. `globe_rectangle_components` is an authoritative
`PackedFloat64Array[west, south, east, north]` in degrees. The `Vector3`
longitude/latitude/height convenience is lossy in a single-precision Godot
build.

A rectangle result reports its center as longitude/latitude/height. A point
result reports a zero-area globe rectangle. Many geocoders omit height, in
which case Cesium Native returns zero meters.

Geocoder credits are request results, not tileset-frame credits. Applications
must display every attribution according to `show_on_screen` and the selected
provider's terms. They are deliberately returned to the caller rather than
silently inserted into a scene credit collector that may disappear before the
search UI finishes using the result.

## Runtime ownership and cancellation

The service owns one bounded worker executor and Cesium's Curl-backed asset
accessor. It caches `/appData` once per API endpoint, which avoids repeating
server discovery for every autocomplete keystroke. Concurrent first requests
share that discovery operation. Changing `api_url` cancels current requests,
drains and destroys the old runtime, and clears the cache. A request submitted
from a cancellation callback or while a worker/retry-setting change is
draining is queued in the service, then starts on the rebuilt runtime with the
new endpoint and settings. Configuration diagnostics report both the total
pending and temporarily deferred counts.

Each request snapshots the access token when submitted. Changing
`access_token` affects later requests without invalidating results already in
flight. `cancel()` and `cancel_request()` are logical per-request cancellation:
they immediately release service ownership, emit `cancelled`, and prevent a
late response from changing the terminal state. Cesium Native v0.63.0's
geocoder method does not accept a per-request cancellation token, so its
underlying shared HTTP transfer may finish in the background. Destroying the
service or changing endpoints calls transport-wide cancellation and drains
Native continuations on the Godot thread before releasing its worker pool.

The service reuses the plugin's bounded, non-sleeping network retry layer.
`worker_thread_count`, `maximum_network_retries`, and initial/maximum retry
delays are bounded in the inspector. Runtime-setting changes take effect the
next time the service reaches a safe idle boundary; active work is never
silently restarted. `get_configuration().runtime_recreation_pending` reports
whether the current runtime is draining before those settings take effect.

## Credentials and diagnostics

The public ion geocoder requires a token with the appropriate geocoding scope.
Private single-user ion instances may accept an empty token. The token is sent
only in the `Authorization` request header. `get_configuration()` reports only
`access_token_configured`; it never returns the token.

The password inspector hint obscures the token visually but does not make a
saved `.tscn` secret. Set credentials from a runtime secret source and do not
commit them. Request and result Resources also contain no token.

`tests/godot/geocoder_service_test.gd` uses a deterministic local ion emulator
to verify app-data caching, concurrent search/autocomplete, UTF-8 and reserved
query encoding, provider and authorization selection, point/rectangle float64
results, attribution, defensive arrays, HTTP failure, queued/in-flight
cancellation, late-response suppression, callback-time endpoint replacement,
deferred runtime recreation, and service shutdown on Godot 4.6.3
without external credentials.
