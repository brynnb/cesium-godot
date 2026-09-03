# Debugging, benchmarking, and content validation

The diagnostics in this plugin are designed to answer three separate questions:

1. What is Cesium selecting and loading right now?
2. What renderer material and imagery actually reached one primitive?
3. Is a repeatable route or authored tileset structurally healthy?

They are opt-in or offline so ordinary streaming does not scan the full tile
tree, retain unloaded renderer resources, or generate hidden runtime artifacts.

## Tile-state snapshots and visualization

Enable bounded snapshots on a `Cesium3DTileset`:

```gdscript
tileset.debug_tile_state_capture_enabled = true
tileset.debug_tile_state_limit = 512

for state: CesiumTileDebugState in tileset.debug_tile_states:
	print(
		state.hierarchy_path,
		" ", state.selection_role_name,
		" ", state.load_state_name,
		" GE=", state.geometric_error
	)
```

Each update captures selected and fading tiles first, then their ancestors and
immediate children up to the configured limit. It does not traverse every tile
in a large tileset. `debug_tile_states_truncated` reports when the cap was hit.

`CesiumTileDebugState` is immutable from scripts. It owns copied scalar values,
stable hierarchy paths, and `CesiumBoundingVolume` snapshots; it never retains
a Cesium Native tile. A state retained by an application remains safe after
source replacement or tileset destruction.

For an in-world view, add a `CesiumTileDebugVisualizer` as a sibling or child
and assign its `tileset` property. When it is a direct child of the tileset it
finds the parent automatically. It draws oriented box edges, optional labels,
and optional parent-child refinement links. Labels include textual role and
load-state names so the view does not depend on color perception.

The default color meanings are:

| State | Color |
| --- | --- |
| Selected | green |
| Fading | yellow |
| Ancestor | cyan |
| Immediate child | gray |
| Worker loading | orange |
| Main-thread realization | magenta |
| Temporary or terminal failure | red |

The visualizer pools its labels and rebuilds one `ImmediateMesh`; destroying it
during active requests is safe. Assignments are weak `ObjectID`s, and drawing
uses only the copied snapshots.

## Primitive, material, UV, and overlay inspection

Any `CesiumLoadedTilePrimitive` returned by a lifecycle callback or loaded tile
offers a JSON-friendly diagnostic snapshot:

```gdscript
var diagnostic := primitive.get_render_diagnostics()
print(JSON.stringify(diagnostic, "  "))
```

The result reports:

- source tile, primitive, material, attribute, and UV identities;
- generated default, application-selected base, and currently active material;
- whether the active material is generated, lifecycle-customized, or an overlay
  composite;
- every core glTF texture slot, its effective `KHR_texture_transform` UV set,
  mapped Godot UV index, generated texture, and fallback reason;
- every attached raster overlay, key, UV mapping, transform, shader prefix,
  texture/material description, active state, and fallback; and
- explicit `realized`, `renderer_unavailable`, or `unloaded` renderer state.

Descriptions contain IDs, names, paths, dimensions, and state—not strong
`Resource` references. The primitive holds materials and scene nodes by weak
`ObjectID`, so retaining diagnostics cannot pin GPU resources. After unload,
copied source facts remain available while material/texture availability is
false and overlay bindings are empty.

## Repeatable streaming benchmarks

Copy or preload
`res://addons/cesium_godot/scripts/cesium_streaming_benchmark.gd`, then call its
coroutine with a tileset, camera, and fixed route:

```gdscript
var runner := CesiumStreamingBenchmark.new()
var report: Dictionary = await runner.run(tileset, camera, [
	{
		"position": Vector3(0, 100, 300),
		"target": Vector3.ZERO,
		"travel_frames": 30,
		"settle_frames": 30,
	},
])
```

Pause the application's ordinary tileset-update loop while `run` owns updates.
The `cesium-godot-benchmark-v1` result includes mean/p50/p95/max frame and
explicit update times; worker/main queue peaks; Native loaded/cache bytes;
shared renderer-resource and process-memory high-water values; visual-state
transitions; empty-visible frames; terminal failures; final queues; and bounded
route samples. The route is deterministic; wall-clock measurements still vary
with machine load, GPU/backend, Godot version, and cache warmth, so record those
conditions when comparing reports.

The credential-free example includes a ready-made benchmark command:

```bash
godot4 --headless --path examples/lifecycle_material_demo -- --benchmark-route
```

## Offline local tileset validation

Validate an authored local tileset before launching Godot:

```bash
python3 tools/validate_tileset.py /absolute/path/to/tileset.json
python3 tools/validate_tileset.py /absolute/path/to/tileset.json \
  --max-payload-bytes 16777216 --json \
  --write-report /absolute/path/to/validation-report.json
```

The validator follows local external tilesets, glTF/GLB files, external buffers,
and images. It detects missing files and textures, malformed JSON/GLB, invalid
bounds and transforms, invalid refine or geometric-error values, child errors
larger than parent errors, broken child/material/mesh links, external-tileset
cycles, and payloads above the configured warning threshold. Referenced files
and payload bytes are counted once by resolved path.

HTTP/HTTPS and unfamiliar URI schemes are reported as warnings because this is
an offline tool. It does not decode mesh compression, transcode textures,
validate every extension schema, or prove visual correctness; runtime fixtures
remain authoritative for renderer behavior. Errors return exit status 1.
`--strict-warnings` also makes warnings fail CI.

## Regression coverage

`tests/run_godot_tests.sh` runs source-layout and validator checks, the complete
headless streaming suite, the example lifecycle smoke test, the benchmark
route, and deterministic local-network cancellation/retry fixtures. The suite
specifically destroys diagnostic consumers during an active request, cancels
in-flight and partially-realized work, validates unload callback order, tears
down whole owner subtrees, forces tile-cache eviction, and verifies that the
last shared texture/model lease releases its resources.

`tests/run_csharp_tests.sh` is the corresponding Godot .NET boundary test. It
compiles the committed generated facade and exercises its ordinary API plus
real local streaming, lifecycle callbacks, visibility, unloading, and failure
delivery. Set `GODOT_DOTNET_BIN` to the Godot 4.6.3 .NET executable before
running it.

The hosted Windows build also launches the packaged addon with the lifecycle
demo's `--smoke-test` mode. This catches platform-specific DLL loading and
local-file URL failures that a successful Windows link alone cannot detect.

For Android, `tests/run_android_emulator_tests.sh` installs the exported x86_64
APK into an accelerated Android 35 emulator on an isolated Xvfb display. It
requires successful packaged-file and HTTPS streaming, lifecycle callbacks,
material application, unloading, and two independent launches. It also saves a
rendered screenshot for visual inspection and fails on Android crashes, native
library loading failures, explicit smoke failures, or rendering-device loss.
The Android guide documents setup and explains why this complements rather
than replaces ARM64 physical-device testing.
