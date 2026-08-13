# glTF rendering contract

`CesiumGDModelLoader` is the Cesium Native-to-Godot mesh boundary.
`Runtime/Private/Materials/CesiumGltfMaterialLoader` owns glTF material,
texture, and sampler translation. This mirrors the responsibility split in
Cesium for Unreal v2.29.0 without importing Unreal component or material APIs.

## Implemented behavior

- Accessors are read with Cesium Native `AccessorView` rather than unchecked
  pointer arithmetic. Sparse accessors, byte strides, normalized unsigned-byte
  and unsigned-short UVs/colors, and all legal glTF index widths therefore use
  one validated path.
- Invalid indices, mismatched attribute counts, missing required UV sets, and
  malformed primitive counts fail explicitly. The renderer never repairs
  malformed triangles by duplicating the final value.
- Triangle strips and fans are converted to explicit triangle lists. Line loops
  are converted to explicit line segments because Godot has no matching
  primitive mode.
- Point, line, line-loop, and line-strip primitives retain their native Godot
  topology and skip triangle collision generation. Point clouds use a live,
  GPU-driven Cesium attenuation shader and optional Bentley circular point
  style; see [`POINTS_AND_LINES.md`](POINTS_AND_LINES.md).
- Only primitives reachable from the glTF default scene are realized. Complete
  parent/child node transforms, `CESIUM_RTC`, and the declared glTF up-axis are
  flattened into each primitive before its Godot surface is created. Reusing
  one mesh from multiple nodes creates distinct placed surfaces without
  duplicating immutable material or texture resources.
- Negative-scale node transforms preserve front-face winding, normals, tangent
  direction, and tangent handedness. The lifecycle context retains the source
  node, mesh, and primitive indices for every realized placement.
- `EXT_mesh_gpu_instancing` keeps one source mesh instead of expanding repeated
  geometry. Translation, rotation, and scale accessors are validated on the
  worker and packed as local TRS into Godot's 12-float row-major format. The
  common node/up-axis/RTC transform remains double precision until it is
  assigned once to the standalone `MultiMeshInstance3D`, avoiding repeated
  large translations in the 32-bit instance buffer. Mixed ordinary and
  instanced primitives keep stable logical indices and lifecycle material
  overrides.
- Instanced bounds are calculated from the transformed source bounding box on
  the worker and installed as an explicit custom AABB. Batches containing a
  mirrored transform receive an unculled default-material variant so mixed
  transform handedness cannot make valid instances disappear.
- Missing normals on lit triangle content produce duplicated corners and flat
  normals as required by glTF. Normal-mapped triangle content without authored
  tangents receives a validated tangent basis; replacing this implementation
  with a shared MikkTSpace implementation remains on the parity list.
- Core metallic/roughness PBR retains base color, metallic/roughness, normal,
  occlusion, emissive, alpha mask/blend, double-sided, unlit, and vertex-color
  behavior. glTF numeric factors stay linear. Base-color and emissive textures
  are declared `source_color`; data textures remain linear.
- Each source `BLEND` primitive owns an independently sorted child renderer
  using its AABB center, while opaque and masked primitives remain batched.
  An optional alpha depth prepass, live tileset priority, all-transparent
  models, combined collision, and exact face-to-primitive picking are covered
  in [`TRANSLUCENCY.md`](TRANSLUCENCY.md).
- `KHR_texture_transform` remains attached to each texture slot. The shader
  applies the specified UV override, scale, clockwise glTF sampling rotation,
  and offset independently, so two textures may transform one source UV set in
  different ways.
- A decoded `ImageAsset` is uploaded once per active tileset resource identity
  and shared by every material slot and loaded tile that references it. Tiles
  retain explicit leases; the final tile release destroys the weakly cached
  Godot texture. Exact generated shader-source variants are compiled once and
  strongly retained for the tileset generation; ordinary material instances
  remain per model so their parameters and application customization stay
  independent.
- Independently selected tiles with the same complete resolved content URL
  share their immutable realized ArrayMesh, base materials, MultiMesh data,
  metadata Resources, and image leases. Their scene nodes, transforms,
  visibility, raster bindings, and material overrides remain independent. The
  weak cache retains nothing after the final tile lease and never guesses
  identity from a raw tile ID or path fragment.
- KTX2/Basis transcoding targets are derived from Godot's active rendering
  backend. S3TC advertises BC1/BC3, the independent RGTC feature advertises
  BC4/BC5, BPTC advertises BC7, ETC2 advertises ETC/EAC, and ASTC advertises
  ASTC 4x4. Compatible payloads retain their corresponding compressed Godot
  `Image::Format` through `ImageTexture` creation. Formats without a Godot
  upload representation, including PVRTC, are never advertised to Native and
  receive its decoded fallback. Source format and byte-size metadata remain
  available for diagnostics.
- Draco and meshopt are decoded after external buffers have resolved. Embedded
  payloads retain Cesium Native's immediate path, so postprocessing occurs
  exactly once in either case.
- Required material extensions that are not implemented return
  `ERR_UNAVAILABLE`; optional unknown extensions are named in the material's
  `cesium_unsupported_material_extensions` metadata.

## Sampler adaptation

Shader samplers use `repeat_disable`; a coordinate helper implements clamp,
repeat, and mirrored repeat independently for S and T. Godot ShaderMaterial
does not expose independent minification and magnification filters, nor both of
glTF's mixed texel/mip interpolation combinations. Exact Godot equivalents are
used where available. A material using an inexpressible combination receives
the closest filter and `cesium_sampler_exact=false` metadata so diagnostics and
content validation can identify the adaptation.

## Deliberate limits still on the roadmap

- Godot ArrayMesh exposes UV and UV2 directly. Texture coordinate sets above
  one currently fail explicitly; future custom-attribute support must not map
  them silently to UV0.
- Tile-level ECEF orientation and the generic georeference/floating-origin
  implementation still need their dedicated conformance fixtures. Large tile
  translations remain double precision until main-thread placement.
- A single primitive mesh or MultiMesh-buffer upload remains indivisible in
  Godot, so configured per-primitive geometry/texture limits reject oversized
  leaves before upload. Material, mesh, MultiMesh, metadata, and feature
  realization otherwise yield as separate main-thread steps.

The lifecycle fixture loads an actual local tileset and embedded PNG. It checks
all five core texture roles, factors, alpha/unlit/double-sided shader state,
per-texture transform values, mixed wrapping, sampler approximation reporting,
decoded-image sharing, the spec-default missing material, nested nodes,
repeated mesh placement, reflection transforms, generated normals/tangents,
and stable node/mesh/primitive lifecycle identity.

A second streamed-content fixture uses Cesium Native's external Draco model,
meshopt duck, and KTX2 balloon. It verifies realized Godot surfaces, non-empty
decoded vertices, texture creation, and retained transcode diagnostics.

The GPU-instancing fixture mixes an ordinary primitive with an
`EXT_mesh_gpu_instancing` node. It verifies nested transforms, normalized
integer quaternion rotation, nonuniform and mirrored scale, bulk buffer layout,
shared geometry, explicit culling bounds, logical-to-mesh surface mapping,
lifecycle material routing, node-shared `EXT_instance_features`, property-table
picking, and safe retained context after unload.

The translucency fixture streams mixed and all-BLEND models. It verifies that
opaque batching is preserved without duplicating blended geometry, each blend
primitive sorts independently, generated shader policy is configurable,
shared collision retains all triangles, and physics hits map back to the
correct logical primitive.

The separate shared-resource fixture streams two glTF tiles that resolve one
external image through Cesium Native's shared-asset system. It verifies exact
Godot texture and generated-shader identity, de-duplicated Native cache bytes,
live/high-water Godot resource estimates, and final-owner texture release after
forced eviction. See
`SHARED_RESOURCE_ACCOUNTING.md` for the ownership contract.
