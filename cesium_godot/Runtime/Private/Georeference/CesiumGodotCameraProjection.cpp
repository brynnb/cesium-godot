// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Adapted for Godot from Cesium for Unreal under the Apache-2.0 license.

#include "Runtime/Private/Georeference/CesiumGodotCameraProjection.h"

#include <CesiumGeometry/Transforms.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

namespace {
constexpr double MINIMUM_VIEWPORT_DIMENSION = 1.0;
constexpr double MINIMUM_CAMERA_SIZE = 1.0e-6;
constexpr double MINIMUM_NEAR_PLANE = 1.0e-6;

double clamp_fov_radians(double fovDegrees) {
	const double clampedDegrees = std::clamp(fovDegrees, 0.01, 179.0);
	return clampedDegrees * std::numbers::pi / 180.0;
}
} // namespace

std::optional<CesiumGodotCameraProjection>
CesiumGodotCameraProjection::from_camera(
	const Camera3D* camera,
	const Vector2& viewportSize
) {
	if (camera == nullptr || !std::isfinite(static_cast<double>(viewportSize.x)) ||
		!std::isfinite(static_cast<double>(viewportSize.y))) {
		return std::nullopt;
	}

	CesiumGodotCameraProjection result;
	const double nearPlane = static_cast<double>(camera->get_near());
	if (!std::isfinite(nearPlane)) {
		return std::nullopt;
	}
	result.m_projectionType = camera->get_projection();
	result.m_keepWidth =
		camera->get_keep_aspect_mode() == Camera3D::KeepAspect::KEEP_WIDTH;
	result.m_viewportSize = glm::dvec2(
		std::max(
			MINIMUM_VIEWPORT_DIMENSION,
			static_cast<double>(viewportSize.x)
		),
		std::max(
			MINIMUM_VIEWPORT_DIMENSION,
			static_cast<double>(viewportSize.y)
		)
	);
	const double aspect =
		result.m_viewportSize.x / result.m_viewportSize.y;
	result.m_nearPlane = std::max(MINIMUM_NEAR_PLANE, nearPlane);

	if (
		result.m_projectionType ==
		Camera3D::ProjectionType::PROJECTION_PERSPECTIVE
	) {
		const double authoredFov = static_cast<double>(camera->get_fov());
		if (!std::isfinite(authoredFov)) {
			return std::nullopt;
		}
		const double fixedFov = clamp_fov_radians(authoredFov);
		if (result.m_keepWidth) {
			result.m_horizontalFieldOfViewRadians = fixedFov;
			result.m_verticalFieldOfViewRadians = 2.0 * std::atan(
				std::tan(fixedFov * 0.5) / aspect
			);
		} else {
			result.m_verticalFieldOfViewRadians = fixedFov;
			result.m_horizontalFieldOfViewRadians = 2.0 * std::atan(
				aspect * std::tan(fixedFov * 0.5)
			);
		}
		return result;
	}

	const double authoredSize = static_cast<double>(camera->get_size());
	if (!std::isfinite(authoredSize)) {
		return std::nullopt;
	}
	double fixedSize = std::max(MINIMUM_CAMERA_SIZE, authoredSize);
	// Godot's Projection::set_orthogonal / set_frustum uses `size` as the
	// fixed vertical span for KEEP_HEIGHT and the fixed horizontal span for
	// KEEP_WIDTH. Convert both modes to explicit plane extents.
	if (!result.m_keepWidth) {
		fixedSize *= aspect;
	}
	const Vector2 offset =
		result.m_projectionType == Camera3D::ProjectionType::PROJECTION_FRUSTUM
		? camera->get_frustum_offset()
		: Vector2();
	if (
		!std::isfinite(static_cast<double>(offset.x)) ||
		!std::isfinite(static_cast<double>(offset.y))
	) {
		return std::nullopt;
	}
	result.m_left = -fixedSize * 0.5 + static_cast<double>(offset.x);
	result.m_right = fixedSize * 0.5 + static_cast<double>(offset.x);
	result.m_bottom =
		-fixedSize / aspect * 0.5 + static_cast<double>(offset.y);
	result.m_top =
		fixedSize / aspect * 0.5 + static_cast<double>(offset.y);
	return result;
}

Cesium3DTilesSelection::ViewState
CesiumGodotCameraProjection::create_view_state(
	const glm::dvec3& position,
	const glm::dvec3& direction,
	const glm::dvec3& up,
	double projectionScale
) const {
	projectionScale = std::max(1.0, projectionScale);
	if (
		this->m_projectionType ==
		Camera3D::ProjectionType::PROJECTION_PERSPECTIVE
	) {
		const double horizontalFieldOfView = 2.0 * std::atan(
			std::tan(this->m_horizontalFieldOfViewRadians * 0.5) *
			projectionScale
		);
		const double verticalFieldOfView = 2.0 * std::atan(
			std::tan(this->m_verticalFieldOfViewRadians * 0.5) *
			projectionScale
		);
		return Cesium3DTilesSelection::ViewState(
			position,
			direction,
			up,
			this->m_viewportSize,
			horizontalFieldOfView,
			verticalFieldOfView
		);
	}
	const double centerX = (this->m_left + this->m_right) * 0.5;
	const double centerY = (this->m_bottom + this->m_top) * 0.5;
	const double left = centerX + (this->m_left - centerX) * projectionScale;
	const double right = centerX + (this->m_right - centerX) * projectionScale;
	const double bottom = centerY +
		(this->m_bottom - centerY) * projectionScale;
	const double top = centerY +
		(this->m_top - centerY) * projectionScale;
	if (
		this->m_projectionType ==
		Camera3D::ProjectionType::PROJECTION_ORTHOGONAL
	) {
		return Cesium3DTilesSelection::ViewState(
			position,
			direction,
			up,
			this->m_viewportSize,
			left,
			right,
			bottom,
			top
		);
	}

	const glm::dmat4 viewMatrix =
		CesiumGeometry::Transforms::createViewMatrix(position, direction, up);
	const glm::dmat4 projectionMatrix =
		CesiumGeometry::Transforms::createPerspectiveMatrix(
			left,
			right,
			bottom,
			top,
			this->m_nearPlane,
			std::numeric_limits<double>::infinity()
		);
	return Cesium3DTilesSelection::ViewState(
		viewMatrix,
		projectionMatrix,
		this->m_viewportSize
	);
}

double CesiumGodotCameraProjection::get_prediction_span() const {
	if (
		this->m_projectionType ==
		Camera3D::ProjectionType::PROJECTION_PERSPECTIVE
	) {
		return std::max(
			std::tan(this->m_horizontalFieldOfViewRadians * 0.5),
			std::tan(this->m_verticalFieldOfViewRadians * 0.5)
		);
	}
	return std::max(
		this->m_right - this->m_left,
		this->m_top - this->m_bottom
	);
}

int32_t CesiumGodotCameraProjection::get_projection_type() const {
	return static_cast<int32_t>(this->m_projectionType);
}

const char* CesiumGodotCameraProjection::get_projection_type_name() const {
	switch (this->m_projectionType) {
	case Camera3D::ProjectionType::PROJECTION_ORTHOGONAL:
		return "orthogonal";
	case Camera3D::ProjectionType::PROJECTION_FRUSTUM:
		return "frustum";
	case Camera3D::ProjectionType::PROJECTION_PERSPECTIVE:
	default:
		return "perspective";
	}
}

bool CesiumGodotCameraProjection::get_keep_width() const {
	return this->m_keepWidth;
}

const glm::dvec2& CesiumGodotCameraProjection::get_viewport_size() const {
	return this->m_viewportSize;
}

double CesiumGodotCameraProjection::get_horizontal_field_of_view_radians()
	const {
	return this->m_horizontalFieldOfViewRadians;
}

double CesiumGodotCameraProjection::get_vertical_field_of_view_radians() const {
	return this->m_verticalFieldOfViewRadians;
}

double CesiumGodotCameraProjection::get_left() const { return this->m_left; }
double CesiumGodotCameraProjection::get_right() const { return this->m_right; }
double CesiumGodotCameraProjection::get_bottom() const { return this->m_bottom; }
double CesiumGodotCameraProjection::get_top() const { return this->m_top; }
double CesiumGodotCameraProjection::get_near_plane() const {
	return this->m_nearPlane;
}
