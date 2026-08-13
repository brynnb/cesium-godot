# Point and line primitives

The generic renderer supports all glTF 2.0 point and line topologies inside
ordinary streamed 3D Tiles content. No application adapter is required.

## Topology mapping

| glTF mode | Godot realization |
| --- | --- |
| `POINTS` | `Mesh.PRIMITIVE_POINTS` |
| `LINES` | `Mesh.PRIMITIVE_LINES` |
| `LINE_LOOP` | Explicit closing segments in `Mesh.PRIMITIVE_LINES` |
| `LINE_STRIP` | `Mesh.PRIMITIVE_LINE_STRIP` |

Indexed and non-indexed input use the same validated accessor path as triangle
meshes. Vertex colors, material factors, texture coordinates, source node
transforms, lifecycle callbacks, metadata, and unload ownership remain part of
the ordinary glTF renderer contract. Point and line primitives deliberately do
not generate triangle collision shapes, matching Cesium for Unreal's dedicated
non-physics components.

`CesiumLoadedTilePrimitive.primitive_mode_name`, `point_primitive`,
`line_primitive`, and `triangle_primitive` expose the logical source topology.
For points, the lifecycle context also copies `point_count`,
`primitive_dimensions`, `tile_geometric_error`, `uses_additive_refinement`, and
the optional Bentley diameter. These facts remain safe to inspect after unload.

## Point-cloud shading

Every `Cesium3DTileset` owns a `CesiumPointCloudShading` Resource with the same
four controls as Cesium for Unreal v2.29.0:

- `attenuation`: size points from the tile's geometric error and view depth;
- `geometric_error_scale`: multiply the selected geometric error;
- `maximum_attenuation`: explicit maximum point size in pixels, or zero to use
  Cesium's additive/replacement defaults;
- `base_resolution`: dataset spacing in meters when a tile has zero geometric
  error, or zero to estimate it from the point bounds and count.

The fallback order is tile geometric error, configured base resolution, then
the cube root of `bounding volume / point count`. An attenuated additive tile is
capped at five pixels; a replacement tile defaults to the tileset's maximum
screen-space error. `maximum_attenuation` overrides either cap. This matches
the upstream calculation.

Godot does not need Unreal's custom vertex factory. A point stays one GPU
vertex in `PRIMITIVE_POINTS`; the generated spatial shader writes `POINT_SIZE`
from `MODELVIEW_MATRIX`, `PROJECTION_MATRIX`, and `VIEWPORT_SIZE`. It therefore
responds to camera depth, FOV, viewport resolution, and stereo views entirely
on the renderer without CPU quad expansion or per-frame script updates.

`BENTLEY_materials_point_style.diameter` is read from the source material. A
positive diameter overrides attenuation with a fixed pixel size, and
`POINT_COORD` clips the square point sprite to a circle as upstream does.

Edits to the settings Resource and to `maximum_screen_space_error` update
uniforms on already-realized point primitives. Geometry, shaders, textures,
tiles, and lifecycle identities are retained. Per-tile uniform containers are
shallow copies; immutable shader and texture resources remain shared.

Custom lifecycle materials may support the same generated uniform contract:

```text
cesium_point_attenuation_enabled: bool
cesium_point_geometric_error: float (meters)
cesium_point_maximum_size: float (pixels)
cesium_point_diameter: float (pixels)
```

`CesiumPointCloudShading.apply_to_material` verifies all four uniforms and
returns `false` for a non-shader material or a custom shader without the full
contract. This is a capability result, not a stream failure, and unsupported
custom shaders are left untouched without renderer warnings.

## Coverage

`tests/godot/line_point_renderer_test.gd` streams a local ADD-refined fixture
containing all four non-triangle topologies. It covers non-indexed points,
indexed lines, line-loop conversion, Godot surface modes, upstream sizing
fallbacks, Bentley circular points, live edits without reload, diagnostics,
and the absence of point/line collision bodies on Godot 4.6.3.
