# LOD selection and continuity

`Cesium3DTileset` delegates spatial traversal and level-of-detail selection to
Cesium Native. Godot does not choose chunks from distance rings or replace the
3D Tiles hierarchy with a parallel loader.

The principal live controls are:

- `maximum_screen_space_error`: the largest projected geometric error, in
  pixels, that Cesium may accept before refining. The Cesium Native default is
  16. Smaller values request finer detail; zero requests the finest available
  detail and can be prohibitively expensive.
- `preload_ancestors`: warms coarser parents for zooming out.
- `preload_siblings`: warms neighboring children for camera turns.
- `loading_descendant_limit`: prevents an unbounded number of descendants from
  loading behind one ancestor before Cesium chooses the ancestor as a fallback.
- `forbid_holes`: keeps a valid ancestor rendered until all required replacement
  descendants are ready.

All five update the active Native tileset and appear in
`get_streaming_statistics()`.

## Refinement semantics

`REPLACE` content is an authored LOD chain. A coarse tile remains the fallback
while required fine children load. Once the children are ready, the Godot
adapter adds every tile selected for the current frame before hiding Native's
fading parents, leaving no blank end-of-frame state.

`ADD` content is cumulative spatial content. Ready descendants are shown in
addition to their ancestors. This is useful for grouping pages and details
that are not substitutes for the parent.

The headless LOD fixture uses a real rendered parent and a child whose HTTP body
is deliberately delayed. It proves the parent remains visible during the wait,
then verifies the distinct stable states for both `REPLACE` and `ADD`. This
tests the complete Native-selection-to-Godot-publication boundary rather than
only parsing the `refine` string.

These runtime guarantees do not manufacture good source data. A production
terrain or object pipeline must still author valid coarse models, matching
terrain borders, holes, skirts where needed, and consistent geometric errors.
