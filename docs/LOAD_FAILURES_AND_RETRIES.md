# Load failures and network retries

`Cesium3DTileset` exposes failures as typed, deferred Godot events instead of
requiring an application to parse console text:

```gdscript
func _ready() -> void:
	$Cesium3DTileset.load_failure.connect(_on_cesium_load_failure)

func _on_cesium_load_failure(failure: CesiumLoadFailure) -> void:
	print(failure.category_name, ": ", failure.message)
	if failure.terminal:
		show_streaming_error(failure.to_dictionary())
```

`CesiumIonRasterOverlay` has the same `load_failure(failure)` signal. Overlay
failures are delivered to both the overlay signal and its owning tileset's
unified signal, using the same `CesiumLoadFailure` instance.

## Delivery and lifetime

Network, decode, and Native callbacks may originate on worker threads. They
write plain records into a bounded thread-safe queue; `Cesium3DTileset` creates
the Godot `CesiumLoadFailure` Resources on the main thread and defers each
signal. A listener may therefore remove the source node without unwinding a
Cesium Native callback. `source` is a weak lookup from `source_instance_id` and
may be `null` if the source was already freed. Retain the failure Resource, not
the source node, when keeping diagnostic history.

The queue retains at most 1,024 pending records, drops the oldest record on
overflow, and exposes that count as `load_failure_queue_dropped`. This bounds a
failure storm without hiding that information was lost.

## Failure fields

Every failure provides read-only properties and `to_dictionary()`:

- `category` / `category_name`: `tileset`, `tile_content`, `raster_overlay`,
  `network`, `decode`, `material`, `renderer`, or `cache`;
- `stage` / `stage_name`: the narrower Ion endpoint, tileset JSON, tile content,
  raster provider/request, network request, glTF decode, material creation,
  renderer preparation, texture upload, cache open, or cache maintenance step;
- `message`, redacted `url`, `http_status_code`, `tile_id`, and `overlay_key`;
- `terminal`: this attempt or processing operation will not continue;
- `retryable`: the transport status is safe for the configured policy to retry;
- `retry_scheduled`: a later attempt has actually been queued; and
- `attempt`, `maximum_attempts`, and `retry_delay_seconds`.

`terminal` describes this reported operation. A nonterminal material fallback
can still produce a visible tile, while a terminal `tile_content` failure means
that individual tile cannot be realized. A retry notification is nonterminal
because the same logical request is still active. Malformed data is permanent
even when its HTTP response was successful.

Diagnostic URLs redact values for case-insensitive `token`, `access_token`,
`key`, `api_key`, `signature`, and `sig` query keys. Applications should still
avoid putting credentials in path segments or diagnostic messages.

## Retry policy

Retries are configured per `Cesium3DTileset`:

- `maximum_network_retries`: additional attempts after the first; default 3;
- `network_retry_initial_delay_seconds`: default 0.25 seconds; and
- `network_retry_maximum_delay_seconds`: default 4 seconds.

Changing a retry property recreates that tileset so all active requests use
one consistent policy. Retries apply only to idempotent HTTP reads. The current
Curl transport uses GET for Cesium content; POST and PUT are never retried.
Retryable responses are status 0, 408, 425, 429, 500, 502, 503, 504, and
520 through 527. Other HTTP failures and successful responses containing
malformed data fail permanently.

Delay doubles after each failed attempt and is capped by the maximum. A
non-negative numeric `Retry-After` delta is honored, still bounded by that
maximum. Delays live in a due-time queue advanced from `update_tileset()`; no
Godot or worker thread sleeps. Cancellation removes queued retries and the
logical request completes once. An active transfer is aborted by libcurl; the
downstream Curl accessor's opt-in decorator mode represents that abort as a
null response so the retry layer can consume it without an async rejection
race. The persistent HTTP cache wraps this accessor, so a cache hit does no
network/retry work and a cache miss stores only the final response.

## Statistics

`get_streaming_statistics()` exposes the retry configuration and current
cache state plus:

- `requests`, `request_attempts`, `request_successes`, `request_failures`, and
  `request_cancellations`;
- `request_retries`, `request_retries_exhausted`,
  `request_retries_queued`, and `request_retries_maximum_queued`;
- `requests_in_flight` and `requests_maximum_in_flight`;
- `request_total_us`, `request_average_us`, `request_maximum_us`, and
  `request_response_bytes`;
- `decode_count`, `decode_failures`, and decode total/average/maximum time;
- `http_cache_residency_state` (`disabled`, `not_initialized`, `ready`, or
  `unavailable`) and cache prune total/average/maximum time; and
- `load_failure_queue_pending` and `load_failure_queue_dropped`.

`requests` counts logical accessor requests, while `request_attempts` includes
retry attempts. Timing covers a logical request from submission through final
success, failure, or cancellation, including backoff. Cache-hit requests are
served outside the network accessor and remain represented by the separate
`http_cache_*` counters.

The headless `load_failure_retry_test.gd` uses a local deterministic server to
verify two transient 503 retries followed by success, permanent 404 handling,
two connection-abort retries followed by success, malformed root and leaf
content, credential redaction, individual tile identity, overlay fan-out, and
the associated statistics.
