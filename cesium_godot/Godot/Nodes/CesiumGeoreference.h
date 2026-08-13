// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Adapted for Godot from Cesium for Unreal under the Apache-2.0 license.
// Upstream counterparts:
// - Source/CesiumRuntime/Public/CesiumGeoreference.h
// - Source/CesiumRuntime/Private/CesiumGeoreference.cpp
// - Source/CesiumRuntime/Public/GeoTransforms.h
// - Source/CesiumRuntime/Private/GeoTransforms.cpp
// Last upstream review: Cesium for Unreal v2.29.0

#ifndef CESIUM_GEOREFERENCE_H
#define CESIUM_GEOREFERENCE_H

#include "Runtime/Public/Georeference/CesiumEllipsoid.h"

#if defined(CESIUM_GD_MODULE)
#include "core/object/object_id.h"
#include "core/variant/packed_float64_array.h"
#include "scene/3d/node_3d.h"
#elif defined(CESIUM_GD_EXT)
#include "godot_cpp/classes/node3d.hpp"
#include "godot_cpp/classes/ref.hpp"
#include "godot_cpp/core/object.hpp"
#include "godot_cpp/variant/basis.hpp"
#include "godot_cpp/variant/packed_float64_array.hpp"
#include "godot_cpp/variant/transform3d.hpp"
#include "godot_cpp/variant/vector3.hpp"
using namespace godot;
#endif

#include <CesiumGeospatial/LocalHorizontalCoordinateSystem.h>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <cstdint>
#include <optional>
#include <vector>

class Cesium3DTileset;
class CesiumGlobeAnchor;

/**
 * Defines the precise mapping between Cesium globe-fixed coordinates and a
 * small Godot Y-up local world.
 *
 * Cartographic mode uses East / Up / South axes, which means Godot -Z points
 * north. True-origin mode preserves Cesium's fixed frame while applying the
 * exact native-Z-up to Godot-Y-up axis conversion. All ECEF state is retained
 * as float64 independently of Godot's build precision.
 */
class CesiumGeoreference : public Node3D {
	GDCLASS(CesiumGeoreference, Node3D)

public:
	enum OriginType {
		CartographicOrigin = 0,
		TrueOrigin = 1,
	};

	CesiumGeoreference();
	~CesiumGeoreference() override;

	void _enter_tree() override;
	void _exit_tree() override;

	void set_ellipsoid(const Ref<CesiumEllipsoid>& ellipsoid);
	Ref<CesiumEllipsoid> get_ellipsoid() const;

	int32_t get_origin_type() const;
	OriginType get_origin_type_raw() const;
	void set_origin_type(int32_t type);

	double get_ecef_x() const;
	void set_ecef_x(double value);
	double get_ecef_y() const;
	void set_ecef_y(double value);
	double get_ecef_z() const;
	void set_ecef_z(double value);
	PackedFloat64Array get_origin_ecef_precise() const;
	bool set_origin_ecef_precise(const PackedFloat64Array& value);

	double get_latitude() const;
	void set_latitude(double value);
	double get_longitude() const;
	void set_longitude(double value);
	double get_altitude() const;
	void set_altitude(double value);
	PackedFloat64Array get_origin_longitude_latitude_height_precise() const;
	void set_origin_longitude_latitude_height_precise(
		const PackedFloat64Array& value
	);

	real_t get_scale_factor() const;
	void set_scale_factor(real_t value);
	int64_t get_revision() const;

	PackedFloat64Array transform_ecef_position_to_local_precise(
		const PackedFloat64Array& ecef
	) const;
	PackedFloat64Array transform_local_position_to_ecef_precise(
		const PackedFloat64Array& local
	) const;
	PackedFloat64Array transform_ecef_direction_to_local_precise(
		const PackedFloat64Array& ecef
	) const;
	PackedFloat64Array transform_local_direction_to_ecef_precise(
		const PackedFloat64Array& local
	) const;
	PackedFloat64Array transform_longitude_latitude_height_to_local_precise(
		const PackedFloat64Array& longitudeLatitudeHeight
	) const;
	PackedFloat64Array transform_local_to_longitude_latitude_height_precise(
		const PackedFloat64Array& local
	) const;

	Vector3 transform_ecef_position_to_local(const Vector3& ecef) const;
	Vector3 transform_local_position_to_ecef(const Vector3& local) const;
	Vector3 transform_ecef_direction_to_local(const Vector3& ecef) const;
	Vector3 transform_local_direction_to_ecef(const Vector3& local) const;
	Vector3 transform_global_position_to_ecef(const Vector3& global) const;
	Vector3 transform_ecef_position_to_global(const Vector3& ecef) const;

	const CesiumGeospatial::LocalHorizontalCoordinateSystem&
	get_coordinate_system() const;
	const CesiumGeospatial::Ellipsoid& get_native_ellipsoid() const;
	const glm::dvec3& get_ecef_position() const;
	glm::dvec3 get_lla() const;
	glm::dmat4 ecef_transform_to_local(const glm::dmat4& transform) const;
	glm::dmat4 local_transform_to_ecef(const glm::dmat4& transform) const;

	// Compatibility surface retained for existing scenes and C++ callers.
	Transform3D get_tx_engine_to_ecef() const;
	Transform3D get_tx_ecef_to_engine() const;
	Transform3D get_initial_tx_ecef_to_engine() const;
	Transform3D get_initial_tx_engine_to_ecef() const;
	Vector3 get_global_surface_position(
		const Vector3& cameraPosition,
		const Vector3& cameraDirection
	);
	Vector3 get_global_center_position();
	Vector3 get_mouse_pos_ecef();
	PackedFloat64Array get_mouse_pos_ecef_precise();
	Vector3 get_ellipsoid_dimensions() const;
	Vector3 ray_to_surface(
		const Vector3& globalOrigin,
		const Vector3& globalDirection
	) const;
	Basis eus_at_ecef(const Vector3& ecef) const;
	Vector3 get_normal_at_surface_pos(const Vector3& ecef) const;
	Vector3 ecef_to_lat_lon_alt_rad(const Vector3& ecef) const;
	Vector3 ecef_to_lat_lon_alt_deg(const Vector3& ecef) const;
	Vector3 lat_lon_alt_rad_to_ecef(const Vector3& latitudeLongitudeAltitude) const;
	void update_ecef_with_lla(const glm::dvec3& latitudeLongitudeAltitude);
	const glm::dvec3& get_original_origin_ecef() const;
	void set_should_update_origin(bool value);
	bool get_should_update_origin() const;

	void register_tileset_to_move_origin(Cesium3DTileset* tileset);
	void unregister_tileset_to_move_origin(Cesium3DTileset* tileset);
	void register_globe_anchor(CesiumGlobeAnchor* anchor);
	void unregister_globe_anchor(CesiumGlobeAnchor* anchor);
	void refresh_dependents();

	void _on_ellipsoid_resource_changed();

protected:
	static void _bind_methods();

private:
	void update_coordinate_system(bool notifyDependents = true);
	void disconnect_ellipsoid();
	std::optional<glm::dvec3> intersect_ellipsoid(
		const glm::dvec3& origin,
		const glm::dvec3& direction
	) const;

	Ref<CesiumEllipsoid> m_ellipsoid;
	CesiumGeospatial::LocalHorizontalCoordinateSystem m_coordinateSystem{
		glm::dmat4(1.0)
	};
	glm::dvec3 m_ecefPosition{-1292940.0, -4740030.0, 4056960.0};
	glm::dvec3 m_originalEcefPosition{-1292940.0, -4740030.0, 4056960.0};
	double m_longitude = -105.25737;
	double m_latitude = 39.736401;
	double m_height = 2250.0;
	real_t m_scaleFactor = 1.0;
	OriginType m_originType = CartographicOrigin;
	bool m_shouldUpdateOrigin = false;
	bool m_insideTree = false;
	uint64_t m_revision = 0;
	Transform3D m_initialOriginTransform;
	std::vector<ObjectID> m_trackedTilesets;
	std::vector<ObjectID> m_trackedAnchors;
};

VARIANT_ENUM_CAST(CesiumGeoreference::OriginType);

#endif
