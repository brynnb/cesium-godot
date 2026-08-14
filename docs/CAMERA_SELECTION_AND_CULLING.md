# Camera selection and culling

Cesium for Godot translates each active runtime `Camera3D` into a Cesium Native
`ViewState` for one combined visible-selection update and, when prediction is
active, adds one non-rendered future view with a bounded future position,
facing direction, and projection width. With no `CesiumCameraManager`, the
tileset uses only its viewport's current camera, preserving the original API.
This boundary is implemented by
`Runtime/Private/Georeference/CesiumGodotCameraProjection.*`, adapted from the
camera construction in Cesium for Unreal v2.29.0.

## Projection fidelity

The adapter preserves the projection Godot actually renders:

- perspective cameras distinguish `KEEP_WIDTH` from `KEEP_HEIGHT`;
- orthographic cameras retain their fixed width or height;
- frustum cameras retain their asymmetric left, right, top, and bottom planes;
- `h_offset` and `v_offset` move the selection eye along with the rendered eye;
- true-origin projects transform the complete camera pose through the tileset
  scene frame before selection.

Cesium Native deliberately uses an effectively infinite far plane for tileset
selection. A Godot camera's near distance determines the shape of an asymmetric
frustum, but the near and far clipping distances do not serve as general tile
distance limits. Applications that intentionally hide distant content should
use an appropriate Cesium culling policy rather than relying on the render
camera's far clip alone.

## Runtime controls

Each `Cesium3DTileset` exposes live settings corresponding to Cesium Native and
Cesium for Unreal:

- `frustum_culling_enabled` ignores tiles outside every selected render camera;
- `fog_culling_enabled` uses Cesium Native's ellipsoid-height fog table, not a
  Godot `Environment`'s visual fog;
- `enforce_culled_screen_space_error` and `culled_screen_space_error` control
  how far would-be-culled tiles refine when a culling stage is disabled;
- `render_tiles_under_camera` keeps region-bounded terrain beneath the camera
  eligible for uses such as physics.

Fog culling generally should be disabled for a local-Cartesian, non-Earth world
unless its coordinate/ellipsoid policy deliberately supports it.

Camera-turn and zoom-out prediction are independent of visible culling. They
warm content through a lower-weight `TilesetViewGroup`; only render cameras
determine visibility. In a multi-camera setup, the first valid camera (normally
the viewport camera) drives prediction. This prevents prediction from
displaying objects behind the player or widening a rendered field of view. See
the [multi-camera runtime guide](MULTI_CAMERA_STREAMING.md).

## Diagnostics

`get_streaming_statistics()` reports the configured controls and the exact
projection passed to Cesium, including projection type, aspect policy, viewport
size, eye/direction/up float64 components, perspective FOVs, or explicit plane
extents for the primary camera. Multi-camera counts and rejection reasons are
reported separately. It also reports:

- `tiles_visited` and `culled_tiles_visited`;
- `tiles_culled`, `tiles_occluded`, and `tiles_kicked`;
- tiles waiting for occlusion information;
- the maximum hierarchy depth visited.

Cesium Native request-time occlusion is available when running the optional
custom Godot build that exposes its existing Embree HZ-buffer result for a
batch of world-space bounds. The addon detects the method dynamically and
safely disables Native occlusion on stock Godot 4.6.3. See the
[occlusion bridge guide](OCCLUSION_CULLING.md).

`tests/godot/frustum_culling_test.gd` streams a real three-leaf tileset and
proves off-screen suppression, live turn-around reuse, culling disablement,
`KEEP_WIDTH` versus `KEEP_HEIGHT`, orthographic extents, and an asymmetric
frustum on the supported Godot 4.6.3 runtime.
