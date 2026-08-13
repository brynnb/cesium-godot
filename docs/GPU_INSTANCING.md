# GPU instancing

The plugin adapts Cesium for Unreal v2.29.0's
`CesiumGltfInstancedComponent` boundary to Godot's `MultiMeshInstance3D`.
glTF nodes using `EXT_mesh_gpu_instancing` keep one copy of each primitive's
vertices and draw every placement through one packed instance buffer. The
renderer never expands a repeated tree or prop into duplicated geometry.

## Loading path

On a Cesium worker, the model loader:

1. validates matching `TRANSLATION`, `ROTATION`, and `SCALE` accessor counts;
2. accepts float translations/scales and float or normalized signed-byte/short
   quaternion rotations, matching the applicable Unreal path and extension;
3. keeps the complete shared nested-node, up-axis, and RTC transform in double
   precision until it becomes the child component's transform;
4. packs only each local instance TRS in Godot's documented 12-float row-major
   Transform3D layout, avoiding needless precision loss from repeating a large
   common translation in Godot's 32-bit MultiMesh buffer;
5. computes a combined AABB from eight source-mesh box corners per instance;
6. includes transform storage in the primitive's bounded geometry accounting.

The main thread separately realizes the material, one-surface `ArrayMesh`, and
`MultiMesh`. The common node/up-axis/RTC transform is assigned once to the child
render node, while local placement is one bulk `set_buffer` upload rather than
thousands of per-instance RenderingServer calls. The explicit local custom AABB
keeps the batch cullable even before a rendering backend has derived automatic
bounds. A batch containing any mirrored combined transform uses an unculled
default material variant because one MultiMesh may contain both handednesses.

Each batch is exposed as a child `CesiumGltfInstancedComponent`. Its associated
`CesiumLoadedTilePrimitive` exposes:

- `gpu_instanced`, `instance_count`, and `render_node`;
- a logical `surface_index` stable among all primitives in the tile;
- `mesh_surface_index`, which is `0` for the batch's private source mesh;
- the same source node/mesh/primitive identity, materials, feature context,
  extras, lifecycle callbacks, and overlay routing as ordinary primitives.
- node-level `EXT_instance_features`, shared across every mesh primitive on the
  node, plus attribute/implicit instance lookup and property-table joins.

Mixed tiles retain ordinary surfaces on `Cesium3DTile` while instance batches
live in child components. A parent-mesh-surface map keeps ordinary physics-face
picking aligned even when an instanced logical primitive precedes it.

## Metadata and collision boundary

`CesiumMetadataPicking.resolve_instance()` resolves a stable instance index to
its `EXT_instance_features` ID and property-table row. `resolve_hit()` prefers
that instance row and falls back to the shared mesh's feature IDs when the node
has no instance feature sets. Godot MultiMesh rendering does not emit physics
instance indices, so a nearby collision or selection adapter supplies the
primitive and instance index it owns. Generic distant instance collision is
intentionally not generated: applications such as Vanguard need a separate
nearby object-collision policy so thousands of distant render instances never
block visible streaming or create thousands of physics shapes.

Godot documents the bulk buffer layout in
[RenderingServer.multimesh_set_buffer](https://docs.godotengine.org/en/stable/classes/class_renderingserver.html#class-renderingserver-method-multimesh-set-buffer),
and the source data contract is the
[EXT_mesh_gpu_instancing specification](https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Vendor/EXT_mesh_gpu_instancing/README.md).

Run the real mixed-content fixture with the complete suite:

```bash
./tests/run_godot_tests.sh
```

## Downstream authoring validation

The Vanguard adapter is a large real-world producer of this generic contract;
none of its source-format or batching policy lives in the plugin. Its
`objects-world-v3` builder groups only exact mesh/cull/LOD signatures, bounds
groups by count and spatial extent, keeps non-decomposable transforms as exact
singletons, and writes instance catalog identity through
`EXT_instance_features` plus `EXT_structural_metadata`.

The published 2026-08-12 artifact instanced 858,360 of 940,105 placements. A
full wrapper validator checked 196,201 glTF contents and all 858,360 unique
catalog identities; a real Godot smoke test required a
`CesiumGltfInstancedComponent`, joined its first and last instance back to
catalog metadata, and observed zero tile failures. On the corrected four-region
route, that artifact reduced peak process RSS by 28.4% and Cesium loaded-data
bytes by 34.9% versus the individual-content predecessor. The downstream
implementation is commit `1adffa0` in the Vanguard Cesium demo worktree.
