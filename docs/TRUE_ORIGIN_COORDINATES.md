# True-origin coordinate boundary

`CesiumGeoreference.OriginType.TrueOrigin` is the local-Cartesian mode used by
tilesets such as Vanguard. 3D Tiles and Cesium Native use a Z-up Cartesian
frame, while Godot uses Y-up coordinates and looks forward along negative Z.

The scene hierarchy owns that axis conversion:

```text
Godot world
  CesiumGeoreference     native Z-up -> Godot Y-up rotation
    Cesium3DTileset      application-authored local transform
      Cesium3DTile       complete native 3D Tiles transform
```

Consequently, a tile transform must remain local to `Cesium3DTileset`. The
renderer must not separately rotate its translation or discard its linear
part. Doing either causes selection and rendering to disagree: Cesium can
request content from one side of a local world while Godot draws it on the
other, and root rotation or scale can be lost.

`Cesium3DTileset.update_tileset` accepts a global Godot camera transform. In
true-origin mode it transforms that complete camera frame through the inverse
global tileset transform before constructing Cesium Native's Z-up
`ViewState`. This keeps position, look direction, and up direction in the same
coordinate system as tile bounds and respects an application transform on the
tileset node.

The renderer assigns the complete Native tile transform to the realized tile
node. It separately captures the resulting global engine-space origin and
basis for `CesiumLoadedTilePrimitive`'s stable world-aligned material
parameters. Thus ordinary Godot rendering and high/low split custom-material
coordinates describe the same placement.

## Regression coverage

`tests/godot/true_origin_axis_test.gd` streams a local fixture whose root has:

- a translation along Native Y, which must appear along negative Godot Z;
- a Native Z rotation; and
- non-uniform scale.

The test requires the correctly positioned camera to select the tile, verifies
the realized global translation, rotation, and scale, and verifies that the
world-aligned material frame exactly matches the realized tile frame. This is
also part of `tests/run_godot_tests.sh`.

## Upstream relationship

The responsibility follows Cesium for Unreal v2.29.0
`Cesium3DTileset.cpp` / `CesiumViewExtension.cpp` for constructing Native view
states and `UnrealPrepareRendererResources.cpp` for applying complete tile
transforms. Godot's scene-node conversion is intentionally engine-specific;
the selection and renderer coordinate frames must still agree exactly.
