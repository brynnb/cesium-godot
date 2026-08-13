// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Adapted for Godot from Cesium for Unreal under the Apache-2.0 license.

#ifndef CESIUM_GODOT_CAMERA_PROJECTION_H
#define CESIUM_GODOT_CAMERA_PROJECTION_H

/**
 * Godot adaptation reviewed against Cesium for Unreal v2.29.0:
 * - Source/CesiumRuntime/Private/Cesium3DTileset.cpp
 * - Source/CesiumRuntime/Private/CesiumViewExtension.cpp
 *
 * Captures the active Godot camera projection once per selection update and
 * creates equivalent Cesium Native ViewState instances for the current and
 * predictive camera positions. Perspective KEEP_WIDTH / KEEP_HEIGHT,
 * orthographic, and asymmetric frustum projections retain their Godot
 * semantics instead of being approximated as one vertical-FOV camera.
 * Last upstream review: Cesium for Unreal v2.29.0.
 */

#include <Cesium3DTilesSelection/ViewState.h>

#if defined(CESIUM_GD_MODULE)
#include "core/math/vector2.h"
#include "scene/3d/camera_3d.h"
#elif defined(CESIUM_GD_EXT)
#include "godot_cpp/classes/camera3d.hpp"
#include "godot_cpp/variant/vector2.hpp"
using namespace godot;
#endif

#include <glm/ext/vector_double2.hpp>
#include <glm/ext/vector_double3.hpp>

#include <cstdint>
#include <optional>

class CesiumGodotCameraProjection final {
public:
	static std::optional<CesiumGodotCameraProjection> from_camera(
		const Camera3D* camera,
		const Vector2& viewportSize
	);

	Cesium3DTilesSelection::ViewState create_view_state(
		const glm::dvec3& position,
		const glm::dvec3& direction,
		const glm::dvec3& up,
		double projectionScale = 1.0
	) const;

	double get_prediction_span() const;

	int32_t get_projection_type() const;
	const char* get_projection_type_name() const;
	bool get_keep_width() const;
	const glm::dvec2& get_viewport_size() const;
	double get_horizontal_field_of_view_radians() const;
	double get_vertical_field_of_view_radians() const;
	double get_left() const;
	double get_right() const;
	double get_bottom() const;
	double get_top() const;
	double get_near_plane() const;

private:
	CesiumGodotCameraProjection() = default;

	Camera3D::ProjectionType m_projectionType =
		Camera3D::ProjectionType::PROJECTION_PERSPECTIVE;
	bool m_keepWidth = false;
	glm::dvec2 m_viewportSize{1.0, 1.0};
	double m_horizontalFieldOfViewRadians = 0.0;
	double m_verticalFieldOfViewRadians = 0.0;
	double m_left = 0.0;
	double m_right = 0.0;
	double m_bottom = 0.0;
	double m_top = 0.0;
	double m_nearPlane = 0.1;
};

#endif // CESIUM_GODOT_CAMERA_PROJECTION_H
