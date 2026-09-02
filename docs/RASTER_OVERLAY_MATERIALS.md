# Raster-overlay material bindings

Cesium raster overlays are streamed images draped onto 3D Tiles geometry. The
plugin represents each overlay-to-primitive attachment with a
`CesiumRasterOverlayBinding`. This is the Godot adaptation of Cesium for
Unreal's `FRasterOverlayTile` and `UCesiumGltfComponent::AttachRasterTile` /
`DetachRasterTile` contract.

The contract is renderer-generic. It contains no Vanguard shader names or
terrain-brush policy.

## Binding data

Each binding exposes:

- `tile_primitive`: the stable `CesiumLoadedTilePrimitive` receiving imagery;
- `overlay_key`: the stable overlay/material-layer identity;
- `texture`: the prepared Godot `Texture2D`;
- `texture_coordinate_id`: Cesium's `_CESIUMOVERLAY_n` identifier;
- `texture_coordinate_index`: the realized Godot vertex UV channel;
- `translation` and `scale`, where `raster_uv = geometry_uv * scale + translation`;
- `material`: the per-tile material instance currently containing the binding;
- `attached`: whether this binding is still current; and
- `shader_parameter_prefix`: the sanitized overlay key used for automatic
  shader parameters.

Retaining a binding is safe after tile unload. Its primitive uses a safe Godot
`ObjectID`, `attached` becomes false, and its potentially large texture and
material references are cleared after the detaching hook returns.

## Generated material convention

The plugin's generated glTF `ShaderMaterial` implements the standard
`Overlay0` layer used by Cesium for Unreal's default material. The common
provider base also defaults `key` to `Overlay0`, so one ordinary imagery or
debug-color overlay works visibly without a lifecycle receiver. Its sampled
RGB is alpha-composited over the glTF base color while the geometry's original
alpha mode remains authoritative.

Cesium Native may generate `_CESIUMOVERLAY_0` alongside an authored
`TEXCOORD_0`; both semantic names intentionally map to Godot `UV`. Raster
coordinates are transformed before their vertical image-origin conversion.
The same rule applies to `_CESIUMOVERLAY_1`, `TEXCOORD_1`, and Godot `UV2`.

Use the custom convention below for another material key, multiple visibly
composited imagery layers, or application-specific blending. Binding lifecycle
and diagnostics are identical in either path.

## Custom ShaderMaterial convention

For an overlay key such as `Aerial Imagery`, the automatic prefix is
`aerial_imagery`. A `ShaderMaterial` can declare:

```glsl
uniform sampler2D aerial_imagery_texture : source_color;
uniform vec4 aerial_imagery_translation_scale;
uniform int aerial_imagery_texture_coordinate_index;
```

The vector stores `(translation.x, translation.y, scale.x, scale.y)`. The
coordinate index currently uses `0` for Godot `UV` and `1` for `UV2`; `-1`
means that the source semantic was not realized in a Godot vertex channel.
Additional vertex channels are tracked under the glTF correctness milestone.

Multiple overlays coexist on one per-tile material instance. Reattaching the
same key first emits a detach for the old binding and then an attach for the
replacement. A late detach may include its expected texture so it cannot
remove a newer replacement with the same key.

`StandardMaterial3D` remains a compatibility fallback. Because it has one
albedo slot, it displays the first attached overlay mapped to `UV`; all
bindings remain queryable. Projects that need compositing, masking, or more
than one visible overlay should select a `ShaderMaterial` with the lifecycle
receiver's `material_selector` Callable.

## Lifecycle signals

A `Cesium3DTilesetLifecycleEventReceiver` exposes synchronous signals:

```gdscript
func _init() -> void:
	raster_overlay_attached.connect(on_raster_attached)
	raster_overlay_detaching.connect(on_raster_detaching)


func on_raster_attached(
	binding: CesiumRasterOverlayBinding
) -> void:
	pass


func on_raster_detaching(
	binding: CesiumRasterOverlayBinding
) -> void:
	# The material, texture, primitive, and attached=true state are still valid.
	pass
```

Ordering is:

1. the per-tile material is created and populated with every active binding;
2. the attach signal runs;
3. on replacement/removal, the detach signal runs while the old state
   is valid;
4. old shader state is cleared and remaining overlays are reapplied; and
5. the old binding is marked detached.

On tile unload, every remaining overlay detaches before `tile_unloading`.

## Custom raster providers

Cesium Native raster providers flow through the same contract automatically.
See [raster-overlay providers](RASTER_OVERLAY_PROVIDERS.md) for the dedicated
ion, URL-template/local-file, TMS, WMS, and WMTS nodes and their lifecycle.
An application-owned provider may use the public methods on `Cesium3DTileset`:

```gdscript
var binding := tileset.attach_raster_overlay(
	primitive,
	"TerrainBrushes",
	texture,
	0, # Cesium texture-coordinate ID
	primitive.get_overlay_texture_coordinate_index(0),
	translation,
	scale
)

tileset.detach_raster_overlay(
	primitive,
	"TerrainBrushes",
	texture # optional stale-detach guard
)
```

Use this for bounded, already-prepared Godot textures. File access, network
access, image decoding, and large data preparation must remain off the main
thread.

## Tests

The local streaming fixtures verify generated `Overlay0` compositing, authored
UV plus generated-overlay semantic coexistence, custom `ShaderMaterial` propagation,
two simultaneous overlays, per-overlay transforms, removal without disturbing
the other overlay, atomic same-key replacement, stale-detach rejection, and
overlay detachment before tile unload. The debug provider additionally proves
two independently streamed color textures, generation refresh, and final
resource release.

## Upstream reference

Last reviewed against Cesium for Unreal v2.29.0 (`71006dd`):

- `Source/CesiumRuntime/Private/CesiumGltfComponent.h`
- `Source/CesiumRuntime/Private/CesiumGltfComponent.cpp`
- `Source/CesiumRuntime/Private/UnrealPrepareRendererResources.cpp`
- `Source/CesiumRuntime/Public/CesiumRasterOverlay.h`
- `Source/CesiumRuntime/Private/CesiumRasterOverlay.cpp`
