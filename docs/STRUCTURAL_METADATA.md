# Structural metadata

The plugin exposes streamed `EXT_structural_metadata` property tables, schema
enums, property attributes, and property textures plus `EXT_mesh_features` and
`EXT_instance_features` feature-ID sets as lifetime-safe Godot Resources.

## Godot API

Each loaded `Cesium3DTile` has a `model_metadata` property containing a
`CesiumModelMetadata` Resource. Its API is:

```gdscript
var model_metadata: CesiumModelMetadata = loaded_tile.model_metadata
var table: CesiumPropertyTable = model_metadata.get_property_table(0)
var property: CesiumMetadataProperty = table.find_property("height")
var enum_definition: CesiumMetadataEnum = (
    model_metadata.find_enum(property.enum_type)
)

var all_values_for_feature: Dictionary = (
    table.get_metadata_values_for_feature(feature_id)
)
var transformed_value: Variant = property.get_value(feature_id)
var encoded_value: Variant = property.get_raw_value(feature_id)

var property_texture: CesiumPropertyTexture = (
    model_metadata.get_property_texture(0)
)
```

`CesiumPropertyTable` preserves the table name, schema class ID, row count,
and properties. `CesiumModelMetadata` also preserves every schema enum in
deterministic enum-ID order. `CesiumMetadataEnum` exposes its ID, name,
description, integer component type, authored values, and exact name/details
lookups for signed 64-bit values. `CesiumMetadataProperty` preserves:

- status, property ID, value type, component type, and enum type ID;
- fixed or variable array shape and normalization;
- transformed and raw values for every feature row;
- offset, scale, minimum, maximum, no-data, and default details.

Scalars become Godot scalar Variants. Vectors, matrices, and metadata arrays
become nested Arrays. Objects returned to scripts are independent containers;
mutating one result cannot change later queries.

### Property attributes

`CesiumLoadedTilePrimitive.primitive_metadata` contains the structural metadata
referenced by that glTF primitive. Property attributes remain per-primitive
because their values come from its vertex accessors:

```gdscript
var primitive_metadata: CesiumPrimitiveMetadata = primitive.primitive_metadata
var attribute: CesiumPropertyAttribute = (
    primitive_metadata.get_property_attribute(0)
)
var height_property: CesiumPropertyAttributeProperty = (
    attribute.find_property("height")
)

var all_vertex_values := attribute.get_metadata_values_at_index(vertex_index)
var height := height_property.get_value(vertex_index)
var encoded_height := height_property.get_raw_value(vertex_index)
```

The attribute Resource preserves its status, authored name, class ID, element
count, and deterministic property list. Each property exposes the same type,
array, normalization, raw/transformed value, and value-transform API as a
property-table property. Numeric scalar, vector, and matrix attributes follow
Cesium Native's supported shapes. Unsupported or malformed definitions remain
visible with an error status; they are not silently reinterpreted. An optional
schema property omitted from the glTF attribute still returns its authored
default for every source vertex.

The primitive also preserves the original property-attribute indices. An
out-of-range index is kept in that list for diagnostics but cannot create a
Resource. An in-range attribute with an invalid class creates an invalid
Resource, preserving the distinction between a broken reference and a broken
definition.

### Property textures

Property textures live on `CesiumModelMetadata`, while a primitive's
`property_texture_indices` says which of them apply to that primitive. This
matches the ownership boundary in the glTF extension and Cesium for Unreal:

```gdscript
var texture: CesiumPropertyTexture = model_metadata.get_property_texture(0)
var category: CesiumPropertyTextureProperty = texture.find_property("category")

# Uses the same supplied UV for every property.
var direct_values := texture.get_metadata_values_for_uv(Vector2(0.25, 0.75))
var raw_category := category.get_raw_value(Vector2(0.25, 0.75))
```

Each property preserves its value/component/enum type, fixed-array shape,
normalization, channels, effective glTF texture-coordinate set, sampler wrap
modes, `KHR_texture_transform`, and offset/scale/min/max/no-data/default
details. Sampling is CPU-side and uses Cesium Native's required nearest texel,
little-endian channel assembly, wrapping, texture-coordinate transform, value
normalization, no-data replacement, and scale/offset behavior. These images
are metadata storage; they are not uploaded as visible Godot textures.

Different properties in one property texture may select different UV sets.
For an exact surface hit, let the primitive interpolate each property's own UV:

```gdscript
var values := primitive_metadata.get_property_texture_values_from_hit(
    model_metadata,
    property_texture_index,
    surface_face_index,
    primitive_local_hit_position
)

# Convenience form for a Godot physics hit Dictionary:
var picked_values := CesiumMetadataPicking.get_property_texture_values_from_hit(
    ray_hit,
    property_texture_index
)
```

Only property textures explicitly referenced by the primitive may be queried
through the hit API. Invalid indices, classes, accessors, images, channel
layouts, and unsupported shapes return explicit status or an empty exact-hit
result rather than sampling unrelated memory.

`Cesium3DTile.get_metadata_table(index)` and `get_structural_metadata()` remain
compatibility conveniences. They return a dictionary whose values are
`CesiumMetadataProperty` Resources. New code should use `model_metadata` so
table identity, validity, class, and row count are not discarded.

### Primitive feature IDs

Each mesh surface has an aligned `CesiumPrimitiveFeatures` Resource:

```gdscript
var primitive := loaded_tile.get_loaded_tile_primitive(surface_index)
var features: CesiumPrimitiveFeatures = primitive.primitive_features
var feature_set: CesiumFeatureIdSet = features.get_feature_id_set(0)

var feature_id := features.get_feature_id_from_face(face_index, 0)
var feature_values := features.get_metadata_values_for_face(
    loaded_tile.model_metadata,
    face_index,
    0
)
```

`Cesium3DTile.get_primitive_features(surface_index)` provides the same Resource
while the tile is present. The retained `CesiumLoadedTilePrimitive` context is
the safer application-facing handle because it keeps the Resource alive after
the source tile unloads.

`CesiumFeatureIdSet` exposes its attribute, texture, or implicit type; validity;
feature count; null feature ID; property-table association; label; source
attribute or texture-coordinate index; and per-vertex lookup. Face lookup uses
the first source vertex, matching Cesium for Unreal's face-query semantics.
Texture-backed sets apply `KHR_texture_transform` and snapshot the sampled ID
for each source vertex. They also retain an owned CPU image, sampler, source
positions, and UVs only when exact texture picking needs them.

### Instance feature IDs

An instanced primitive exposes its node-level feature sets separately from the
feature sets on its shared mesh:

```gdscript
var primitive := loaded_tile.get_loaded_tile_primitive(surface_index)
var instance_features: CesiumPrimitiveFeatures = primitive.instance_features
var instance_set: CesiumFeatureIdSet = instance_features.get_feature_id_set(0)

var feature_id := instance_features.get_feature_id_from_instance(
    instance_index,
    0
)
var feature_values := instance_features.get_metadata_values_for_instance(
    primitive.model_metadata,
    instance_index,
    0
)
```

`CesiumGltfInstancedComponent.instance_features` exposes the same Resource.
Feature IDs may come from a `_FEATURE_ID_n` instance accessor or implicitly
from the instance index. Accessor counts must match the render batch. Invalid
definitions remain exposed with an error status rather than silently changing
instance identity. A node's instance feature snapshot and Godot Resource are
shared by all of that node's mesh primitives and charged once during worker
preparation.

### Godot physics hits

Pass the Dictionary returned by `PhysicsDirectSpaceState3D.intersect_ray()` to
the static picking helper:

```gdscript
var resolved := CesiumMetadataPicking.resolve_hit(ray_hit, feature_set_index)
if resolved.valid:
    print(resolved.feature_id, resolved.metadata)
```

The helper walks from the collision object to its streamed tile, converts the
global collision face to the correct mesh surface and surface-local face,
converts the world hit position into tile-local coordinates, and resolves the
retained primitive, feature ID, property-table values, tile extras, and source
node/mesh/primitive indices. For a texture-backed set it computes barycentric
coordinates on the source triangle, interpolates UVs, applies
`KHR_texture_transform`, honors sampler wrapping, and samples the exact texel.

Godot's `MultiMeshInstance3D` is render-only and does not report a physics
instance index. A nearby-collision or selection adapter therefore supplies the
stable primitive and instance index it already used to create the hit:

```gdscript
var resolved := CesiumMetadataPicking.resolve_hit({
    "primitive": primitive,
    "instance_index": instance_index,
    "face_index": shared_mesh_face_index,
    "position": world_hit_position,
})
```

`resolve_instance(primitive, instance_index, feature_set_index)` is the direct
form when only instance metadata is needed. `resolve_hit` first queries
`EXT_instance_features`. If that extension has no feature sets, it falls back
to the shared primitive's mesh features using the face and position. This is
the Godot counterpart of Unreal's `Hit.Item` behavior without creating distant
collision for every rendered instance.

## Loading and ownership

Property-table, accessor, and texture views point into a glTF model, so
retaining borrowed Native views after tile unload would be unsafe. The renderer
instead copies tables, attribute values, UV hit geometry, and feature sets into
CPU-only snapshots on a load worker. Property-texture samplers and schemas have
a minimal retained owner, while decoded images use Cesium's intrusive shared
image ownership. No geometry buffers or Godot rendering objects are retained by
that owner. Main-thread realization creates at most one Godot metadata or
feature Resource per incremental renderer step, under the same loading time
budget as mesh and material realization. A tile is not published until its
mesh, metadata, and primitive-feature Resources are complete.

Owned feature arrays, property attributes, property-hit geometry, and metadata
texture pixels are included in prepared-memory accounting. A decoded property
image used by several properties is retained and charged once. Node-level
instance feature arrays are charged only on first use. Source positions and UV
sets are retained only for texture-backed exact-hit queries, avoiding a second
position copy on ordinary geometry.

`CesiumModelMetadata.retained_memory_bytes` reports its worker-owned metadata
snapshot, while `property_texture_bytes` reports the retained decoded image
payload. `CesiumPrimitiveMetadata.retained_memory_bytes` reports the
primitive-local attributes, references, and exact-hit geometry. These bytes are
also charged once to the tile's bounded prepared payload before publication.

The Resources own the snapshots through reference counting. Applications may
retain a model, table, or property after the source `Cesium3DTile` is unloaded;
no Resource contains a pointer into the destroyed glTF or Cesium tile.

## Coverage

The headless integration test streams Cesium Native's `ComplexTypes` fixture
through the real tileset pipeline. It verifies normalized variable-length
`UINT8` arrays, packed fixed-length boolean arrays, variable-length strings,
fixed enum arrays, enum definitions and value lookup, attribute feature IDs,
face-to-table joins, copy isolation, and access after source-tile destruction.
A local fixture verifies implicit and texture feature IDs, per-vertex property
attributes, default-only attributes/textures, property texture channels,
samplers, UV transforms, normalization and source overrides, unsupported and
invalid definitions, invalid references, direct texel lookup, barycentric
exact-hit lookup, an actual Godot physics ray-to-metadata query, accounting,
copy isolation, and every Resource after source-tile destruction. The
GPU-instancing fixture verifies shuffled accessor IDs, implicit IDs,
invalid-definition status, property-table joins, instance-first hit resolution,
one shared node-level Resource across mesh primitives, and retained queries
after tile destruction.

Run all integration tests with:

```bash
./tests/run_godot_tests.sh
```

## Remaining metadata work

The remaining broad metadata feature is metadata-driven GPU styling. Mesh
physics and explicit instance-index picking are complete. Application collision
policies still decide which nearby instances receive physical shapes and carry
their stable instance indices.
