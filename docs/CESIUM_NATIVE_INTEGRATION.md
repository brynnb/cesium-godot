# Cesium Native integration

This branch is developed against Cesium Native v0.63.0
(`b4137763914820b01a42b52ee886a55b036cd6ac`). This is the exact Cesium Native
revision used by the Cesium for Unreal v2.29.0 reference checkout. Native
changes are carried as a locked patch series and provisioned into `build/`, so
the original Battle Road dependency is not silently modified. Older Cesium
Native APIs are not a supported compatibility target.

## Downstream patches currently required

- Resume bounded main-thread renderer preparation across frames while
  preserving Native's one-shot renderer interface.
- Count a decoded image shared by multiple loaded tiles once, while retaining
  exact unload references and excluding embedded encoded bytes from the active
  decoded-image estimate.
- Report terminal individual-tile content failures on the main thread with
  stable tile/request context, while excluding cancellation and `RetryLater`;
  root HTTP status codes are also preserved in load-failure details.
- Render ready descendants beneath empty additive tiles.
- Bound SQLite cache response bodies by bytes as well as item
  count, always remove expired entries during prune, and compact/truncate the
  database when explicitly cleared.
- Pin Native tile-load urgency and numeric-priority ordering with a
  regression test.
- Preserve each explicit 3D Tiles node's arbitrary JSON `extras`
  value through parsing and hierarchy moves so renderer adapters can expose
  stable source identity.
- Preserve nested GeoJSON parser warnings, including automatically closed
  polygon rings, when child results are returned through parent objects.
- Retain frame-local demand independently from consumed queues and
  cooperatively cancel stale tile HTTP, cache, decompression, external glTF,
  postprocess, and renderer-preparation work.
- Let an accessor decorator opt into receiving canceled Curl
  transfers as requests with null responses. The default throwing behavior is
  unchanged; the Godot retry decorator uses the opt-in mode so libcurl can
  physically abort without rejecting an async continuation during unwinding.
- Provide the same opt-in null-response contract for connection-
  level Curl failures, allowing decorators to classify and retry status-zero
  failures without propagating an exception through another continuation.
- Keep shared-image accounting exact when a glTF modifier replaces a loaded
  tile's model repeatedly, including transactional release and acquisition of
  image usage for each replacement.
- Initialize the secondary asynchronous tilesets in Native's incremental
  renderer tests before their roots are inspected.

Two changes formerly carried downstream are present upstream in v0.63.0 and
are deliberately no longer patched here: external glTF buffers resolve before
the compression/image/quantization postprocess pass, and empty GeoJSON raster
quadrants clamp to a valid minimum image size. Native v0.63.0 also moves image
types into `CesiumImage` and GeoJSON raster overlays into
`CesiumVectorOverlays`; the Godot bridge targets those current APIs directly.

The Native test suite covers the remaining downstream changes. The Godot
integration additionally streams real external-Draco, meshopt, and KTX2
fixtures end to end. Adversarial tests turn away during a live slow tile
transfer, cancel after partial main-thread surface realization, and destroy an
incomplete Native renderer preparation to prove canceled content is never
published and all intermediate resources are released. A deterministic local
HTTP fixture additionally verifies the root/tile failure details and that the
Godot adapter does not retry malformed successful responses. Curl tests cover
both pre-transfer and active-transfer cancellation in the decorator-safe mode
while retaining the standalone exception contract.

## Durable patch and build contract

These changes no longer depend on the development-only checkout path. The 13
patches are preserved as an ordered, SHA-256-locked mail patch series under
`dependencies/cesium-native-patches`. The bootstrap starts from the exact
v0.63.0 commit, applies that series, and requires the final Git tree recorded
in `dependencies.lock.json`.

```bash
python3 tools/bootstrap_dependencies.py --only cesium-native
python3 tools/build_extension.py --jobs 12
```

The default source is `build/dependencies/sources/cesium-native`; compiled
libraries are separate under
`build/dependencies/native-build/<triplet>-release`. The build never edits a
generated Native header. On Windows, `NOGDI` prevents the Win32 `OPAQUE` macro
from colliding with glTF's `Material::AlphaMode::OPAQUE` at the consumer
boundary.

An alternate development checkout remains possible with
`CESIUM_GODOT_NATIVE_ROOT`, but it must have the exact expected tree unless the
caller explicitly opts out with
`CESIUM_GODOT_ALLOW_UNPINNED_DEPENDENCIES=1`. Select an out-of-source library
tree with `CESIUM_GODOT_NATIVE_BUILD_ROOT`; a legacy override without that
variable is treated as an existing in-source build for backward compatibility.

See `docs/REPRODUCIBLE_BUILDS.md` for the full dependency and CI contract.
