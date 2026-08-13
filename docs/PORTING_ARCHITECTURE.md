# Porting architecture and source layout

The plugin follows Cesium for Unreal's responsibility boundaries without
copying Unreal's engine types. `PORTING_MAP.md` is the authoritative mapping to
the upstream v2.29.0 sources; this document defines where Godot counterparts
belong.

## Current responsibility tree

```text
cesium_godot/
  Runtime/
    Public/                  application-facing nodes and Resources
      Bounds/
      Credits/
      Diagnostics/
      Georeference/
      Metadata/
      Renderer/
      TileSelection/
    Private/                 implementation details
      Async/
      Bounds/
      Credits/
      Diagnostics/
      Georeference/
      Materials/
      Metadata/
      Networking/
      RasterOverlays/
      Renderer/
      TileSelection/
  Godot/
    Nodes/                   ordinary registered Godot scene nodes
  Models/                    legacy Godot nodes/Resources, migrated when touched
  Implementations/           remaining legacy adapters, migrated when touched
  Utils/                     remaining legacy helpers, migrated when touched
```

New runtime work goes directly into `Runtime/Public/<Responsibility>` when it
is part of the stable application API and `Runtime/Private/<Responsibility>`
when it is an implementation detail. Future editor-only code belongs under
`Godot/Editor`; ordinary Godot scene nodes and Resources that do not mirror a
Cesium runtime API belong under `Godot/Nodes` and `Godot/Resources`.

The initial migration deliberately moved coherent private subsystems rather
than renaming the repository at once:

- Cesium CPU scheduling is in `Runtime/Private/Async`.
- Native HTTP and cache instrumentation are in `Runtime/Private/Networking`.
- glTF preparation, texture upload, resource preparation, and renderer caches
  are together in `Runtime/Private/Renderer`.
  Worker handoff state uses ordinary C++ storage; engine-owned packed arrays and
  Variant arrays are created only during bounded main-thread realization.
- materials, raster-overlay bindings, metadata, bounds, and height queries were
  already introduced in responsibility-specific Runtime directories.
- immutable diagnostic state is public under `Runtime/Public/Diagnostics`, while
  selection-time snapshot construction stays in `Runtime/Private/Diagnostics`;
  the ordinary Godot debug-drawing node lives under `Godot/Nodes`.
- immutable attribution snapshots live under `Runtime/Public/Credits`; their
  construction is private, while the scene-wide collector and Godot-native
  presenter live together under `Godot/Nodes`.
- active Godot camera projection translation and Cesium view-state construction
  live under `Runtime/Private/Georeference`; tileset selection owns the policy
  controls and lifetime while the adapter owns only immutable per-frame math.
- reusable ellipsoid, globe-anchor, and origin-shift APIs mirror their Cesium
  runtime counterparts under `Runtime/Public/Private/Georeference`; the scene
  georeference and internal streamed-mesh node live under `Godot/Nodes`. One
  Native local-horizontal transform is shared by selection, placement, bounds,
  anchors, and origin shifts instead of duplicating engine-axis arithmetic.
- exact polygon Resources remain under `Runtime/Public/Georeference`; polygon
  raster masks share the ordinary provider lifecycle, while generic selection
  contexts and lifetime-safe Native policy adapters live under
  `Runtime/Public/Private/TileSelection`. Application predicates are ordinary
  component-style Nodes under `Godot/Nodes`.
- Native LOD fade state is translated under `Runtime/Private/Materials` beside
  generated glTF shader variants; tileset traversal owns timing/visibility and
  applications own any matching custom-shader implementation.
- the substantially changed tileset scene node is under `Godot/Nodes`. Raster
  providers now mirror Cesium for Unreal beneath
  `Runtime/Public/Private/RasterOverlays`: one abstract lifecycle/options node
  and one recognizable concrete class per Native provider. The original
  combined ion/TMS class remains source- and scene-compatible while new TMS
  content uses its dedicated node. URL-template/local-file, WMS, and WMTS each
  have a dedicated provider node and share no service-specific policy with the
  abstract lifecycle base.

The legacy folders remain valid only for existing code. When a legacy area is
substantially changed, move that cohesive area and update `PORTING_MAP.md` in
the same commit. Do not perform a cosmetic all-at-once rename.

## Dependency direction

- Application and Vanguard adapter code use registered Godot classes and
  `Runtime/Public`; they must not include `Runtime/Private` headers.
- `Runtime/Private` may implement `Runtime/Public` and call Cesium Native.
- Godot adaptation code owns Godot object creation, signals, scene nodes, and
  main-thread renderer calls. Cesium Native remains renderer-agnostic.
- Vanguard terrain brushes, shaders, collision policy, actors, ocean, and game
  data stay outside this repository.

Every substantially ported source names its Cesium for Unreal counterpart and
last reviewed release in a header comment. Copied or substantially translated
Apache-2.0 logic retains attribution.

## Regression guard

`tests/validate_source_layout.py` verifies that migrated paths exist, legacy
paths and stale includes do not return, every source listed by `SCsub` exists,
and the porting map names each migrated responsibility. It runs before the
headless streaming suite:

```bash
python3 tests/validate_source_layout.py
tests/run_godot_tests.sh
```
