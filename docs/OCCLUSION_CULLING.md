# Occlusion culling boundary

Cesium for Godot intentionally does not enable Cesium Native's tile-request
occlusion culling on Godot 4.6.3. This is an engine API boundary, not
an omitted null implementation.

## Two different jobs

Cesium Native asks a renderer whether an unloaded tile's bounding volume is
occluded before it commits network, decode, and realization work. Its
`TileOcclusionRendererProxyPool` therefore needs a stable three-state result for
arbitrary bounds: unavailable, visible, or occluded.

Godot's built-in occlusion culling instead operates on already-realized scene
instances against explicitly authored `OccluderInstance3D` geometry. It can
reduce draw work after content exists, but it does not answer the public
pre-load bounding-volume query Cesium needs. Godot also documents that this
CPU system has a cost and is generally not useful for large open scenes with
few blockers—the common Vanguard exterior case.

## Public API audit

The supported Godot 4.6 API and 4.6.3 runtime were checked:

- [Godot 4.6 RenderingServer](https://docs.godotengine.org/en/4.6/classes/class_renderingserver.html)
- [Godot 4.6 RenderingDevice](https://docs.godotengine.org/en/4.6/classes/class_renderingdevice.html)
- [Godot 4.6 occlusion-culling guide](https://docs.godotengine.org/en/4.6/tutorials/3d/occlusion_culling.html)

`RenderingServer` describes renderer internals as opaque. Its public occlusion
surface configures viewports, authored occluders, and instance participation;
it does not return an occlusion state for an arbitrary bound. `RenderingDevice`
can build independent low-level render passes, but exposes neither Godot's
scene depth/occlusion result nor a portable occlusion-query primitive in 4.6.
A plugin-owned depth renderer would duplicate scene submission, introduce
backend-specific synchronization and readback, fail in headless/Compatibility
renderers, and no longer be the reusable Godot-native bridge this project is
intended to provide.

Cesium for Unreal can implement this feature because an Unreal view extension
can ask renderer internals for a primitive's occlusion state. The equivalent
hook is not public in Godot.

## Runtime contract

The plugin leaves Native `enableOcclusionCulling` false and reports:

- `occlusion_culling_available = false`;
- `occlusion_culling_enabled = false`;
- `occlusion_culling_backend = "unavailable"`;
- `occlusion_culling_unavailable_reason` with the API limitation.

Returning “unavailable” from fake Native proxies indefinitely would stall
refinement; returning “visible” would add overhead without culling anything.
Disabling the option is the safe behavior.

Applications may still enable Godot's own post-load occlusion culling and
author occluders when their scene benefits. That remains independent of Cesium
streaming and does not change these diagnostics. Revisit this decision if Godot
adds a public, asynchronous per-bound visibility result that works across the
supported renderers and extension ABI.
