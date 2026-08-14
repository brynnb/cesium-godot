# Cesium request-time occlusion

Cesium for Godot can use Godot's existing Embree-backed CPU occlusion buffer
to avoid requesting and preparing 3D Tiles hidden behind authored
`OccluderInstance3D` geometry. This is request-time culling: it can prevent
work before a hidden tile is downloaded, decoded, uploaded, or instantiated.

## Engine requirement and fallback

Stock Godot 4.6.3 does not expose arbitrary-bound results from its internal
occlusion buffer. The integration therefore uses one small, generic engine API:

```gdscript
RenderingServer.viewport_query_occlusion(viewport_rid, bounds)
```

It accepts an array of world-space `AABB` values and returns one byte per
entry: `0` unavailable, `1` visible, or `2` occluded. The engine implementation
reuses the most recently completed viewport HZ buffer; it does not render a
second depth pass and does not perform GPU readback.

The plugin detects this method at runtime. On an ordinary Godot build it keeps
Cesium Native occlusion disabled and otherwise behaves normally. This allows
the same addon package to support both stock and patched engines safely.

### Required Godot engine change

The adapter does not require a private Cesium ABI, but the Godot executable
must expose the query above. The reference implementation targets Godot
4.6.3-stable (`7d41c59c4`) and makes these narrowly scoped engine changes:

1. Add `ViewportOcclusionQueryResult` and the batched
   `viewport_query_occlusion()` method to `RenderingServer`, its default
   implementation, method bindings, and class reference.
2. Save the camera transform, projection, near plane, and projection type that
   produced each completed `RendererSceneOcclusionCull::HZBuffer`.
3. Add a const HZ-buffer query that passes each world-space `AABB` through
   Godot's existing conservative `_is_occluded()` test.
4. Resolve the viewport's completed HZ buffer in `RendererViewport`, return
   `UNAVAILABLE` when the viewport or buffer cannot answer, and evaluate the
   entire input array in one call.

The implementation touches only these Godot engine areas:

- `servers/rendering/rendering_server.{h,cpp}`;
- `servers/rendering/rendering_server_default.h`;
- `servers/rendering/renderer_viewport.{h,cpp}`;
- `servers/rendering/renderer_scene_occlusion_cull.{h,cpp}`;
- `modules/raycast/raycast_occlusion_cull.cpp`; and
- `doc/classes/RenderingServer.xml` plus a focused engine test.

The public contract is intentionally renderer-agnostic:

```gdscript
enum ViewportOcclusionQueryResult {
    UNAVAILABLE = 0,
    VISIBLE = 1,
    OCCLUDED = 2,
}

func viewport_query_occlusion(
    viewport: RID,
    bounds: Array,
) -> PackedByteArray
```

The implementation must query the most recently completed buffer rather than
starting another depth pass. Invalid bounds, disabled occlusion culling, and an
uninitialized buffer must return `UNAVAILABLE`; callers conservatively treat
that result as visible. Run the batch from `RenderingServer.call_on_render_thread`
and consume it on a later frame so the main thread never synchronizes once per
tile.

After applying the engine change to a Godot 4.6.3 source checkout, build it
normally. For example, on Linux:

```bash
cd /path/to/godot-4.6.3
scons platform=linuxbsd target=editor dev_build=yes -j6
```

This creates `bin/godot.linuxbsd.editor.dev.x86_64`. Launch the project with
that executable; building it does not replace a system `godot4` installation.
The Cesium for Godot binary may remain compiled against the ordinary Godot 4.6
GDExtension API because it discovers and invokes this optional method at
runtime. A stock Godot executable remains a supported fallback, but reports
the feature as unavailable.

Finally, enable occlusion culling on the project's viewport and author useful
`OccluderInstance3D` geometry. Exposing the method alone does not create
occluders or change the tileset hierarchy.

## Runtime design

Set `occlusion_culling_enabled` on a `Cesium3DTileset`. The adapter:

1. lets Cesium Native assign bounded proxy objects during the real camera
   traversal;
2. snapshots all requested tile bounds in one batch;
3. submits that batch through `RenderingServer.call_on_render_thread`;
4. consumes the completed results on a later main-thread frame; and
5. feeds the three-state results back into Native's next traversal.

There is no render-thread synchronization per tile. Results are cached briefly
by Native tile identity so the separately weighted movement-prediction view can
reuse the proxy pool without erasing the real camera's visibility results.
Unrecognized or unavailable results are conservatively treated as visible, so
renderer unavailability cannot strand refinement.

`delay_refinement_for_occlusion` is off by default. Turning it on permits
Native to wait briefly for the first result for a new bound, which may reduce
hidden requests but can add a frame of latency. `occlusion_pool_size` bounds
the number of tile proxies and defaults to 1000.

Godot's occlusion system still needs useful authored occluders. It is most
valuable around building exteriors, cliffs, and other large blockers; it is
unlikely to help much in open terrain or while flying above the world.

Cesium Native applies request-time occlusion when deciding whether to refine a
hierarchy branch. A standalone leaf object has nothing below it to suppress and
is therefore not itself a useful preload-occlusion boundary. Object tilesets
should group interiors and other potentially hidden detail beneath tight
refinable parent bounds. The end-to-end fixture proves that a visible branch
loads while a sibling branch behind an authored wall never requests its leaf.

## Diagnostics

`get_streaming_statistics()` reports:

- `occlusion_culling_available`;
- `occlusion_culling_enabled`;
- `occlusion_culling_backend` (`godot_embree_hzbuffer` when available);
- `occlusion_culling_unavailable_reason`;
- query generation, submitted-bound count, in-flight state, and pool size;
- visible, occluded, and unavailable result counts from the latest batch;
- `tiles_occluded` and `tiles_waiting_for_occlusion_results`.

The engine HZ-buffer math has a focused native unit test.
`tests/godot/occlusion_bridge_test.gd` covers addon properties, runtime feature
detection, stock-engine fallback, and the batch API contract.
`occlusion_engine_e2e_test.gd` verifies visible and hidden bounds against an
actual authored wall, while `occlusion_tileset_e2e_test.gd` proves that the
hidden Cesium hierarchy branch does not load.

On Linux, both render-capable tests can run without opening a window on the
user's desktop:

```bash
cd /path/to/cesium-godot
CESIUM_TEST_REQUIRE_OCCLUSION=1 xvfb-run -a \
  /path/to/godot-4.6.3/bin/godot.linuxbsd.editor.dev.x86_64 \
  --rendering-method gl_compatibility --path tests/godot \
  --script res://occlusion_tileset_e2e_test.gd
```
