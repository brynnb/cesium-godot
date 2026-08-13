# Georeferencing, globe anchors, flights, and floating origins

The plugin keeps Earth-scale coordinates in float64 and presents Godot with a
small Y-up local world. This is the Godot-native counterpart of Cesium for
Unreal's ellipsoid, georeference, globe-anchor, and origin-shift runtime.

## Coordinate frames

`CesiumGeoreference` owns one Cesium Native
`LocalHorizontalCoordinateSystem`. Every runtime consumer uses that same
transform:

- cartographic mode places the authored longitude, latitude, and ellipsoid
  height at Godot local zero;
- Godot +X is east, +Y is up, and -Z is north;
- true-origin mode maps Godot X/Y/Z to Cesium fixed X/Z/-Y without a hidden
  rotation on the scene node;
- `scale_factor` is Godot units per meter. Its default is `1.0`.

The georeference node's own normal Godot transform is an outer application
frame. This permits an application to place the entire globe frame in a larger
scene without changing its geospatial coordinates.

`CesiumEllipsoid` defaults to WGS84 and can be shared by multiple
georeferences. Its `radii` inspector property is convenient, while
`set_radii_precise` retains exact float64 values. Changing a shared ellipsoid
updates its georeferences and preserves their authored longitude, latitude,
and height.

Ellipsoid height is not height above mean sea level. A geoid model is a
separate application/data-provider responsibility.

## Precision-safe APIs

Earth-centered, Earth-fixed values are about 6.4 million meters from zero and
cannot be represented precisely in a single-precision `Vector3`. Use the
`*_precise` APIs for authoritative state. They accept or return a
`PackedFloat64Array` containing exactly three values:

```gdscript
var llh := PackedFloat64Array([-105.25737, 39.736401, 2250.0])
var ecef: PackedFloat64Array = ellipsoid.longitude_latitude_height_to_ecef_precise(llh)
var local: PackedFloat64Array = georeference.transform_ecef_position_to_local_precise(ecef)
var round_trip: PackedFloat64Array = georeference.transform_local_position_to_ecef_precise(local)
```

Longitude precedes latitude in new precise APIs, matching Cesium for Unreal
and Cesium Native. Compatibility functions whose names explicitly say
`lat_lon_alt` retain the legacy latitude-first ordering. The `Vector3`
conversion helpers are suitable for small values and UI, but are intentionally
documented as lossy for ECEF.

The ellipsoid also exposes precise geodetic surface projection, surface
normals, and forward ray intersection. A miss returns an empty array; it is not
reported as `(0, 0, 0)`.

## Anchoring an ordinary node

Add `CesiumGlobeAnchor` as a child of any ordinary `Node3D`:

```text
CesiumGeoreference
  Player (CharacterBody3D)
    CesiumGlobeAnchor
```

With empty paths, the anchor targets its `Node3D` parent and finds the nearest
ancestor `CesiumGeoreference`. Explicit `target_node_path` and
`georeference_path` properties support other scene layouts.

The component stores the complete object-to-ECEF matrix in float64 through
Cesium Native `GlobeAnchor`. It then realizes only a small transform in Godot.
When gameplay moves the target, automatic synchronization detects the change
on the physics tick. Call `sync_from_target()` immediately after a scripted or
teleport move when the authoritative ECEF value is needed in the same tick.
Call `sync_to_target()` after changing the matrix externally.

`adjust_orientation_for_globe_when_moving` keeps the node upright relative to
the ellipsoid as it travels. Disable it when gameplay already applies its own
curvature rotation.

## Ellipsoid-aware flights

Add `CesiumFlyTo` beside `CesiumGlobeAnchor` to move the anchored node along a
Cesium Native `SimplePlanarEllipsoidCurve` rather than a straight line through
the globe:

```text
CameraRig
  CesiumGlobeAnchor
  CesiumFlyTo
```

With an empty `globe_anchor_path`, the component discovers its sibling anchor.
The precise flight methods accept three-value `PackedFloat64Array` coordinates:

```gdscript
fly_to.fly_to_location_longitude_latitude_height_precise(
    PackedFloat64Array([-73.9857, 40.7484, 1500.0]),
    0.0, # destination yaw in the local East/Up/South frame
    -25.0,
    true # interrupt when gameplay moves the target
)
```

Equivalent precise entry points accept ECEF or georeference-local positions.
`fly_to_location_global` and the `Vector3` ECEF/cartographic conveniences are
useful for ordinary Godot input but are intentionally lossy for authoritative
Earth-scale coordinates.

`duration` and three ordinary Godot `Curve` resources control progress, the
fraction of arc height, and maximum height versus distance. A Godot `Curve`
has a normalized horizontal domain, so
`maximum_height_curve_distance` defines the distance represented by its right
edge; its vertical values are meters. Clearing `height_percentage_curve`
disables the extra arc height. Clearing only
`maximum_height_by_distance_curve` uses `default_maximum_height`, matching
upstream's fallback responsibility.

Position and path state remain float64 ECEF. Rotation is interpolated in the
moving East/Up/South frame, pitch is clamped away from its singular poles, and
the target's non-uniform scale is retained. Ordinary Godot transform shear is
not retained during a rotating flight. A floating-origin shift changes only
the realized local transform and does not interrupt the flight.

By default the component advances in `_process`. Set `automatic_update=false`
and call `update_flight(delta)` for deterministic replay or application-owned
time. `flight_started`, `flight_progressed`, `flight_completed`, and
`flight_interrupted` provide lifecycle hooks. An interruption reports whether
it was explicit, caused by target movement, or caused by the anchor becoming
unavailable. Starting a second flight does not replace the current one; cancel
the current flight explicitly first.

## Floating origin

Add `CesiumOriginShift` beside the anchor beneath the target:

```text
Player
  CesiumGlobeAnchor
  CesiumOriginShift
```

Set `mode` to `ChangeCesiumGeoreference` and choose `distance` in local Godot
units. The component is disabled by default so merely adding it cannot move a
scene unexpectedly. A distance of zero shifts continuously; a positive value
waits until the target crosses the threshold.

Before a shift, the component captures the target's precise anchor transform.
It then moves the cartographic origin to that ECEF point. Registered globe
anchors and already-realized tiles are transformed synchronously. Tile content,
materials, metadata, and loaded primitive contexts are not recreated.

The component changes `Node3D` transforms but does not rewrite physics-body
velocities or application-owned navigation data. Install origin shifting at a
physics-safe point and keep all persistent world objects globe anchored. Use
the `origin_shifted` signal to update application systems that store unanchored
local coordinates.

## Regression coverage

`geospatial_foundation_test.gd` covers WGS84/custom ellipsoids, float64
round-trips, true-origin axes, scale, ray hits, ordinary-node anchors, deletion,
and explicit origin shifts. `cartographic_streaming_test.gd` streams a real
ECEF tile with rotation and non-uniform scale, then proves an origin shift
recenters the same live tile and retains its primitive identity.
`fly_to_test.gd` deterministically covers exact cartographic/local endpoints,
arc height, scale retention, floating-origin continuity, terminal signal order,
movement cancellation, explicit cancellation, and zero-duration completion.
