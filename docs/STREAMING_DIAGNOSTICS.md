# Streaming budgets and diagnostics

`Cesium3DTileset` keeps Cesium Native's loading and cache limits available as
live Godot properties:

- `main_thread_loading_time_limit_ms` is the soft aggregate time budget passed
  to `Tileset::loadTiles()` for main-thread renderer realization each frame.
  The default is 5 ms. Zero drains every pending main-thread load in one call.
- `tile_cache_unload_time_limit_ms` is the soft time budget for evicting cached
  Native content each frame. The default is zero, which performs all required
  eviction without throttling and prevents sustained loading from outrunning
  cleanup. Setting a positive value can reduce eviction work in an individual
  frame, but makes `maximum_cached_bytes` a soft limit that may be exceeded
  while eviction catches up.
- `maximum_cached_bytes` is Cesium Native's in-memory tile and raster-data
  budget. It is independent of the HTTP disk cache. Optional hardware-derived
  sizing and explicit-override precedence are documented in
  [`HARDWARE_BUDGETS.md`](HARDWARE_BUDGETS.md).
- `worker_thread_count` controls the bounded CPU executor. Its default is
  hardware concurrency minus one, clamped to one through eight.
- `stale_request_cancellation_enabled` controls whether active tile downloads
  and later preparation stages are stopped when no current view, predictive
  view, or height query needs the tile anymore. It defaults to enabled.
- `maximum_primitive_geometry_upload_bytes` and
  `maximum_primitive_texture_upload_bytes` cap the two indivisible payload
  classes in one surface realization. Both default to 16 MiB; zero disables
  that guard. Changing either recreates the tileset so every pending worker
  uses one consistent policy.

The time, cache, and cancellation properties update a live tileset. Changing
`worker_thread_count` or either primitive cap recreates the Native tileset
because its executor or renderer adapter is a lifetime dependency.

`is_initial_loading_finished()` is a readiness latch for the current tileset
generation. It becomes true the first time Cesium Native reports 100 percent
load progress for the selected view and remains true while later camera motion
requests more tiles. Recreating the tileset because its source or a
generation-bound setting changed resets it to false.

Main-thread preparation is resumable at Godot material/texture and mesh-surface
boundaries. Each invocation realizes at least one whole step, then continues
with additional small steps only while the remaining frame budget permits.
Material/texture creation deliberately yields before its primitive's mesh
upload when the material is first encountered. The tile is not published to
the scene or reported through lifecycle callbacks until every step is complete.
A zero time budget uses the compatibility path and drains the whole tile
without yielding.

## Priority and movement prediction

Native selection always gives urgent visible work precedence over normal and
speculative preload work. Within an urgency class, work facing the camera and
closer to it receives the better numeric priority. Queue entries from multiple
views are consumed with weighted round robin rather than concatenated.

Optional movement prediction adds one second Cesium view at a smoothed,
bounded estimate of the camera's future position, facing direction, and
projection width. This view selects and loads content but never controls
rendering. The real camera therefore cannot render predicted tiles
accidentally, and its default scheduler weight of `1.0` remains above the
prediction default of `0.25`.

- `movement_prediction_enabled` defaults to true.
- `movement_prediction_seconds` is the prediction horizon (default 1 second).
- `movement_prediction_minimum_speed` suppresses idle jitter (default 1 m/s).
- `movement_prediction_maximum_distance` caps a bad velocity sample or very
  fast camera (default 1000 m).
- `movement_prediction_weight` controls the future view's share of otherwise
  available requests (default 0.25).
- `turn_prediction_enabled` continues a detected camera turn (default true).
- `turn_prediction_minimum_angular_speed_degrees` suppresses orientation jitter
  (default 5 degrees/second), and `turn_prediction_maximum_angle_degrees` caps
  the extrapolated turn (default 90 degrees).
- `zoom_out_prediction_enabled` widens the future projection while the camera
  is zooming out (default true). `zoom_out_prediction_minimum_rate` suppresses
  projection jitter and `zoom_out_prediction_maximum_scale` caps its width
  (default 2x).

Linear velocity, world-space angular velocity, and logarithmic projection
growth use a short exponential filter. Zoom-in is not extrapolated because
the current wider view already covers it. Samples after a stall longer than
one second are rejected instead of projecting a debugger pause or suspended
window into the world. When every prediction becomes inactive, its view is
unregistered in that same update, which also lets stale cancellation stop its
obsolete work.

Statistics distinguish `movement_prediction_translation_active`,
`turn_prediction_active`, and `zoom_out_prediction_active`, and report their
speed/distance, angular speed/angle, projection rate/scale, and final predicted
direction. The feature remains automatically suppressed while optional LOD
fades are active because Cesium Native owns one fade percentage per tile across
view groups.

## HTTP disk cache

The persistent HTTP cache is independent of the loaded-tile RAM budget. Its
default location is `user://cache/cesium-request-cache.sqlite`; use
`get_resolved_http_cache_path()` or the statistics snapshot to see the exact
absolute path for the current Godot project. It is a normal disk file, not a
RAM-backed `/tmp` directory.

- `http_cache_enabled` enables persistent standards-based HTTP caching.
- `http_cache_path` accepts a Godot (`user://`) or absolute SQLite path.
- `http_cache_maximum_items` limits retained responses after pruning.
- `http_cache_maximum_data_bytes` limits logical response-body bytes. SQLite
  indexes, headers, and bookkeeping add some physical overhead.
- `http_cache_prune_interval_requests` schedules LRU/expiry maintenance.
- `prune_http_cache()` performs maintenance immediately.
- `clear_http_cache()` deletes all entries, compacts the database, and
  truncates its write-ahead log so the disk space is actually returned.

Changing a disk-cache setting recreates that tileset's Native loader because
the accessor and database are lifetime dependencies. It does not touch the
separate loaded-tile RAM cache. The transport is Cesium Native's maintained
libcurl accessor, so response cache headers, connection reuse, and TLS
verification are preserved instead of reimplemented in Godot-specific code.
Bounded non-blocking retries and typed failure signals are described in
`LOAD_FAILURES_AND_RETRIES.md`.

## Runtime snapshot

`get_streaming_statistics()` returns a `Dictionary` intended for compact debug
UI, logging, automated routes, and benchmark capture. It contains:

- queue and selection state: `worker_queue`, `main_thread_queue`, `selected`,
  `fading_out`, `tiles_visited`, `culled_tiles_visited`, `tiles_culled`,
  `tiles_occluded`, tiles waiting for occlusion, `tiles_kicked`, and maximum
  hierarchy depth visited;
- camera/culling state: active frustum/fog/culled-SSE/under-camera controls,
  explicit occlusion availability, and the exact perspective, orthographic, or
  asymmetric-frustum projection of the primary camera passed to Native;
  `selection_render_view_count`, total `selection_view_count`, manager
  configured/resolved state, and invalid/wrong-world/duplicate camera counts
  describe multi-camera selection. See `CAMERA_SELECTION_AND_CULLING.md` and
  `MULTI_CAMERA_STREAMING.md`;
- scheduling state: `stale_request_cancellation_enabled`, cumulative
  `canceled_tile_loads`, and movement-prediction enable/active state, settings,
  measured speed/distance, separate worker/main queues, and whether the
  predictive view is deliberately suppressed by single-view LOD transitions;
- renderer state: `realized`, `visible`, `hidden`, `failed`, and cumulative
  `unloaded`;
- Native state: `loaded`, `load_progress_percent`, `loaded_data_bytes`, and
  `cache_limit_bytes`. `load_progress_percent` is the current view's dynamic
  progress, unlike the one-way initial-readiness latch. Loaded data charges
  each per-tile buffer once and each actively shared Native image once rather
  than once per placement;
- LOD policy: `maximum_screen_space_error`, `preload_ancestors`,
  `preload_siblings`, `loading_descendant_limit`, `forbid_holes`, opt-in
  transition settings, active transition/percentage state, and compatible or
  unsupported material coverage. See `LOD_TRANSITIONS.md`;
- current work: `selection_us` and `load_tiles_us`;
- logical requests, attempts, successes/failures/cancellations, in-flight and
  queued-retry high-water values, retry/exhaustion counts, response bytes, and
  total/average/maximum request time under `request_*` / `requests_*`;
- glTF decode count, failures, and total/average/maximum time under `decode_*`;
- CPU preparation totals, averages, and maximums under
  `worker_preparation_*`;
- Godot resource-realization totals, averages, and maximums under
  `main_thread_realization_*`, plus per-operation step values and cumulative
  `incremental_realization_yields`;
- cumulative and maximum-per-tile prepared geometry and decoded/compressed
  texture byte estimates under `prepared_geometry_*` and
  `prepared_texture_*`, maximum-per-primitive values, the active primitive
  caps, and `oversized_payload_rejections`;
- shared Godot material-texture ownership under `shared_texture_cache_hits`,
  `shared_texture_cache_misses`, `shared_texture_live_count`,
  `shared_texture_live_bytes`, `shared_texture_maximum_live_count`, and
  `shared_texture_maximum_live_bytes`, plus cumulative
  `released_cpu_texture_count` and `released_cpu_texture_bytes` proving that
  post-upload source buffers were actually deallocated;
- exact generated shader reuse under `shared_shader_cache_hits`,
  `shared_shader_cache_misses`, `shared_shader_cache_entries`, and
  `shared_shader_cache_maximum_entries`; and
- repeated resolved-model renderer ownership under `shared_model_cache_hits`,
  `shared_model_cache_misses`, `shared_model_live_count`, live geometry/texture
  bytes, and their `shared_model_maximum_*` high-water values; and
- the active budget and worker settings; and
- hardware capability facts, recommendation profile/share, cache-budget
  source, and automatic-versus-explicit worker source; and
- HTTP cache configuration, explicit residency state, resolved path, physical
  file bytes, lookups, hits, misses, store attempts/successes/bytes, prunes,
  clears, failures, and prune timing; and
- retry configuration plus the bounded structured-failure queue's pending and
  dropped counts.

The main-thread realization time includes synchronous Godot mesh, texture,
shader, material, scene-node, metadata, collision, and lifecycle-callback work.
Godot does not expose a portable fence that isolates physical GPU transfer
time, so this metric is deliberately not labeled “GPU upload time.” A single
surface and its newly introduced textures remain indivisible; the configurable
payload validator makes that limit enforceable and observable. Production
tilers should use benchmark measurements to choose a smaller cap when their
target hardware cannot realize the default within its frame target.

Statistics reset when a source or worker-count change recreates the tileset.
They contain no application-specific assumptions and can be displayed by a
Vanguard adapter or any other Godot project.

Shared-texture live bytes use Cesium Native's stable logical image cache
estimate. They measure tile-owned leases, are de-duplicated across concurrently
loaded tiles, and return to zero after the final tile lease unloads. An
application may retain a returned texture beyond that ownership boundary. The
values are not a claim about physical GPU allocation, which Godot does not
expose through a portable accounting API.
The ownership and exact eviction fixture are documented in
`SHARED_RESOURCE_ACCOUNTING.md`.

Generated shaders are retained for one tileset generation because the exact
source-variant set is finite and recompiling an evicted variant would repeat
driver work. Their statistics deliberately expose counts only: Godot has no
portable API for the physical memory used by a compiled shader.

Shared-model bytes count each unique live Godot renderer resource once even
when multiple tiles lease it. Native `loaded_data_bytes` remains separately
correct: Native currently owns and charges each tile's parsed glTF buffers.
These statistics expose two different ownership layers and are not additive
physical-memory measurements.

## Independent tileset budgets

Every `Cesium3DTileset` owns its Native loader, request limit, task processor,
main-thread realization allowance, memory cache, disk-cache adapter, and
cancellation state. A project should therefore represent terrain and static
objects with separate tileset nodes and configure each node's
`maximum_simultaneous_tile_loads`, `worker_thread_count`,
`main_thread_loading_time_limit_ms`, and `maximum_cached_bytes` independently.
This follows Cesium for Unreal's per-tileset boundary and avoids introducing a
Vanguard-specific global scheduler into the reusable plugin.

The headless adversarial suite updates a 24-leaf object tileset before a small
terrain tileset on every frame. It requires terrain to finish while object work
is still queued and verifies that neither node's live worker, request,
main-thread, or cache settings leak into the other.
