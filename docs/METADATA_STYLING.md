# Metadata-driven feature styling

`CesiumMetadataStyle` colors or hides individual streamed features using
`EXT_mesh_features`, `EXT_instance_features`, and the linked
`EXT_structural_metadata` property table. It is a reusable `Resource` assigned
to `Cesium3DTileset.metadata_style`; it contains no Vanguard-specific policy.

The implementation follows the runtime boundary of Cesium for Unreal 2.29.0's
`CesiumFeaturesMetadataComponent` and `EncodedFeaturesMetadata`, adapted to
Godot's mesh channels, `MultiMesh` custom data, shader language, and Resource
editor. Unreal generates material-layer graph nodes. Godot instead compiles a
stable feature-ID shader variant once and evaluates ordered style rules into
small lookup textures. Rule edits replace those textures on already-loaded
materials and do not fetch tiles, rebuild geometry, or recreate the tileset.

## Basic use

```gdscript
var style := CesiumMetadataStyle.new()
style.enabled = true
style.feature_source = CesiumMetadataStyle.MeshFeatures
style.feature_id_set_name = "buildings"
style.default_color_mix = 0.0
style.rules = [
    {
        "property": "height_m",
        "operator": "greater_or_equal",
        "value": 100.0,
        "color": Color(1.0, 0.2, 0.1),
        "color_mix": 0.8,
    },
    {
        "property": "status",
        "operator": "equals",
        "value": "hidden",
        "show": false,
    },
]

$Cesium3DTileset.metadata_style = style
```

Rules are evaluated in array order and the first match wins. A rule may read
any property in the selected feature set's linked property table, or the
special `feature_id` property. Supported operators are `equals`, `not_equals`,
`less`, `less_or_equal`, `greater`, `greater_or_equal`, `between`, `contains`,
and `in`, including their common symbolic forms. A matching rule can set:

- `show`: `false` discards that feature's fragments.
- `color`: the color to blend with the glTF base color.
- `color_mix`: blend weight from `0.0` to `1.0`.

Unmatched features use `default_show`, `default_color`, and
`default_color_mix`. `evaluate_feature(feature_id, metadata)` applies the same
logic without allocating GPU resources and is useful for UI previews and
tests.

## Selecting a feature ID set

Set `feature_source` to `MeshFeatures` or `InstanceFeatures`. A non-empty
`feature_id_set_name` takes precedence over `feature_id_set_index`. Authored
labels are used directly. Unlabelled sets use the same generated names as
Cesium for Unreal:

| Source | Generated name |
| --- | --- |
| Mesh attribute | `_FEATURE_ID_<attribute index>` |
| Mesh texture | `_FEATURE_ID_TEXTURE_<texture-set order>` |
| Mesh implicit | `_IMPLICIT_FEATURE_ID` |
| Instance attribute | `_FEATURE_INSTANCE_ID_<attribute index>` |
| Instance implicit | `_IMPLICIT_FEATURE_INSTANCE_ID` |

The selected set is resolved independently for every primitive. A mixed tile
can therefore style compatible primitives and report a clear diagnostic for
others that do not contain the requested set.

Changing `enabled`, `feature_source`, the set name, or the set index changes
the vertex/instance/material layout. The tileset intentionally starts a fresh
generation so worker preparation can encode the new selection. Changing rules,
defaults, or lookup limits only refreshes currently loaded materials in place.

## GPU representation

- Attribute and implicit mesh IDs are encoded losslessly as two 16-bit pieces
  plus a validity flag in `ArrayMesh.ARRAY_CUSTOM0` (`RGB_FLOAT`).
- Attribute and implicit instance IDs use the same representation in
  `MultiMesh.INSTANCE_CUSTOM` and keep transforms in the original bulk upload.
- Texture feature IDs keep their source image, channels, nearest sampling,
  UV set, wrap modes, and `KHR_texture_transform`; they are decoded directly
  in the fragment shader.
- Two nearest-filtered lookup textures hold rule output: RGBA8 color and RG8
  show/blend controls. The shader indexes them by the resolved feature ID.

Feature IDs are limited to exact 32-bit unsigned values. The default
`maximum_features` is 1,048,576 and can be raised to 16,777,216. The default
`maximum_cache_bytes` is 64 MiB. Lookup cache entries retain only a weak model
identity, dead streamed models are pruned, and least-recently-used entries are
evicted before the byte limit is exceeded. `lookup_cache_count` and
`lookup_cache_bytes` expose the current CPU/GPU lookup payload estimate.

## Custom material contract

The generated glTF material supports metadata styling automatically. A
lifecycle receiver that returns the generated `default_material` also remains
compatible because the plugin makes the normal per-tile shallow duplicate.

A completely replacement `ShaderMaterial` must implement the metadata-style
contract. At minimum it needs the common uniforms below and must resolve the
selected input from `CUSTOM0`, `INSTANCE_CUSTOM`, or
`cesium_feature_id_texture`:

```glsl
uniform bool cesium_feature_style_enabled = false;
uniform sampler2D cesium_feature_style_colors : filter_nearest, repeat_disable;
uniform sampler2D cesium_feature_style_controls : filter_nearest, repeat_disable;
uniform int cesium_feature_style_dimension = 1;
uniform float cesium_feature_style_feature_count = 0.0;
uniform float cesium_feature_style_null_id = -1.0;
```

Texture-backed sets additionally receive
`cesium_feature_id_texture`, `cesium_feature_id_uv_set`,
`cesium_feature_id_texture_transform`,
`cesium_feature_id_texture_rotation`, `cesium_feature_id_texture_wrap`, and
`cesium_feature_id_channel_weights`. Use the generated shader available from
`primitive.default_material.shader.code` as the authoritative reference.
Incompatible replacement shaders are left unchanged and report
`active custom shader does not implement the Cesium metadata-style contract`.

## Diagnostics and validation

`CesiumLoadedTilePrimitive.get_render_diagnostics().metadata_style` reports:

- whether encoding was requested, succeeded, and was applied;
- the resolved source, set index/name, feature count, and property-table index;
- the worker encoding diagnostic and any active-material error.

`tests/godot/metadata_styling_test.gd` streams real local fixtures and verifies
mesh custom channels, direct feature textures, packed instance custom data,
property-table rule output, bounded cache accounting, selection recreation,
and rule-only live updates that preserve tile, mesh, shader, and material
identity.
