# Stable coordinates for world-aligned materials

World-aligned materials must not restart their texture pattern at each tile,
change interpretation when Cesium swaps LODs, or slide when a floating origin
moves rendered nodes closer to the camera.

`CesiumLoadedTilePrimitive` therefore captures an absolute, pre-origin-shift
coordinate frame when a tile is realized:

- `absolute_origin`: the primitive tile's stable absolute origin;
- `absolute_origin_high` and `absolute_origin_low`: a split representation for
  shader precision; and
- `local_to_absolute_basis`: rotation and scale from local vertex coordinates
  into the stable absolute frame.

The absolute frame is the tileset's engine-space coordinate system before a
floating-origin offset. It may represent ECEF-derived engine coordinates for a
globe tileset or local Cartesian coordinates for a world such as Vanguard. It
is deliberately not named ECEF because local tilesets are valid.

## Automatic ShaderMaterial parameters

After `_create_material` selects a `ShaderMaterial`, the renderer sets these
parameters before `_customize_material` runs:

```glsl
uniform vec3 cesium_absolute_origin_high;
uniform vec3 cesium_absolute_origin_low;
uniform mat3 cesium_local_to_absolute_basis;
```

A shader can reconstruct its stable position in vertex processing:

```glsl
varying vec3 stable_position_high;
varying vec3 stable_position_low;

void vertex() {
	stable_position_high = cesium_absolute_origin_high;
	stable_position_low = (
		cesium_local_to_absolute_basis * VERTEX +
		cesium_absolute_origin_low
	);
}
```

Keep the high and low terms separate for calculations that are sensitive to
large-world float precision. Adding them immediately into one `vec3` loses much
of the reason for splitting them. Periodic material coordinates can reduce the
terms modulo the material repeat size before combining them.

The shader should use these stable coordinates for material lookup while still
letting Godot use its ordinary `MODEL_MATRIX` for rasterization. Cesium LOD
replacement then changes geometry density without changing the coordinate
frame used by the material.

## Explicit application

The standard `cesium` prefix is automatic. A project that wants a second
parameter namespace may call:

```gdscript
primitive.apply_world_coordinate_parameters(material, "terrain")
```

This sets `terrain_absolute_origin_high`,
`terrain_absolute_origin_low`, and `terrain_local_to_absolute_basis`.

## Scope

This contract supplies stable coordinates; it does not prescribe triplanar
mapping, terrain brush blending, texture arrays, or application-specific world
units. Vanguard's control maps, brush rules, and shader stay in its thin
adapter.

The current transform is captured from the realized tile node and its stable
absolute origin. Full nested glTF node-transform parity is tracked separately
under the glTF correctness work.
