# LOD transitions

`Cesium3DTileset.lod_transitions_enabled` opt-in smooths the moment when Cesium
replaces a coarse tile with finer children, or finer children with a coarse
parent. Cesium Native owns the transition timing and retains both valid LODs;
the Godot adapter applies that state without changing selection or inventing a
second LOD system.

The mode is disabled by default. When disabled, generated glTF shaders contain
no transition branch or fragment discard. Changing the setting recreates that
tileset so all of its generated and shared materials use one consistent shader
variant. `lod_transition_length` defaults to 0.5 seconds and can change live.
`kick_descendants_while_fading_in` defaults to true so a newly visible ancestor
cannot pop to fully opaque merely because its descendants became ready.

## Rendering and ownership

The implementation follows Cesium for Unreal v2.29.0's `updateTileFades` and
`UCesiumGltfComponent::UpdateFade` boundary:

- Native's `TileRenderContent` supplies a percentage and whether the tile is
  entering or leaving the rendered set;
- each tile placement receives a shallow `ShaderMaterial` duplicate containing
  its two changing parameters, while repeated tiles continue sharing meshes,
  shaders, textures, and all other subresources;
- generated glTF shaders use stable screen-space stochastic discard instead of
  alpha blending, retaining opaque depth/shadow behavior and avoiding the
  slower sorting-sensitive transparent pipeline;
- a replacement owns collision immediately. The retained old LOD is visual
  only, so two overlapping physics surfaces cannot produce duplicate contacts;
- a fading tile remains a visible scene node until Native says its transition
  is complete, then follows the ordinary visibility and cache lifecycle.

Cesium Native stores one transition percentage per tile and recommends not
driving it from multiple `TilesetViewGroup`s. The plugin therefore suspends its
separate predictive-prefetch view while transitions are enabled. Current-view
selection, frustum culling, preloading, and cache retention continue normally.
Statistics expose when prediction was suppressed.

## Custom material contract

Generated glTF materials support transitions automatically. A custom spatial
shader selected through `Cesium3DTilesetLifecycleEventReceiver` opts in by
declaring the exact uniforms and applying an opaque discard. This
minimal implementation matches the built-in contract:

```glsl
uniform float cesium_lod_transition_percentage = 1.0;
uniform bool cesium_lod_fading_out = false;

float cesium_lod_dither(vec2 pixel) {
	return fract(52.9829189 * fract(dot(pixel, vec2(0.06711056, 0.00583715))));
}

bool cesium_lod_should_discard() {
	float threshold = clamp(cesium_lod_transition_percentage, 0.0, 1.0);
	float dither = cesium_lod_dither(FRAGCOORD.xy);
	return cesium_lod_fading_out
		? dither < threshold
		: dither >= threshold;
}

void fragment() {
	if (cesium_lod_should_discard()) {
		discard;
	}
	// Application shading follows.
}
```

The small per-placement material container is necessary because Godot 4.6.3's
Compatibility renderer does not support instance shader uniforms. It also
prevents one tile's transition percentage from changing another placement that
shares the same source model, without duplicating expensive resources.
The incoming and outgoing LODs test complementary halves of the same
screen-space pattern, so overlapping coverage does not introduce dither holes.

The adapter detects those declarations after lifecycle material customization
and caches the capability on the final material. Unsupported custom materials
remain fully opaque until the old node hides: they preserve coverage but still
pop. This fallback is deliberate and is reported rather than mutating arbitrary
application shader source.

Transition application is generation-local and stateless: each update derives
its compatible placements from the loaded tile's stable primitive descriptors.
It does not retain a separate registry of tile or material object IDs across
tileset destruction. Repeated source recreation can therefore release one
generation completely before Godot reuses object identities for the next.

## Diagnostics and validation

`get_streaming_statistics()` includes:

- `lod_transitions_enabled`, `lod_transition_length`, and
  `kick_descendants_while_fading_in`;
- `lod_transition_active_tiles`, the observed minimum/maximum percentage, and
  fading tile count;
- supported/unsupported primitive counts and compatible render-node count; and
- `movement_prediction_suppressed_by_lod_transitions`.

The real delayed-content ADD/REPLACE fixture proves ordinary replacement still
has no blank frame, a transition-enabled REPLACE retains its valid parent while
the child fades, isolated state reaches shared generated shaders, and the
parent hides only after the timed transition completes. The same fixture can
repeat all four cases in one process to stress tileset destruction/recreation,
and the generated shader is also compiled in an off-screen Forward+ renderer.
