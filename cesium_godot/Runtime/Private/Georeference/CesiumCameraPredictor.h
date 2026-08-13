// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Godot-specific predictive-view adaptation of Cesium Native multi-view
// selection and Cesium for Unreal camera-driven streaming.
// Last upstream review: Cesium for Unreal v2.29.0.

#ifndef CESIUM_CAMERA_PREDICTOR_H
#define CESIUM_CAMERA_PREDICTOR_H

#include <glm/ext/quaternion_double.hpp>
#include <glm/ext/vector_double3.hpp>

#include <chrono>

struct CesiumCameraPredictionSettings final {
	bool enabled = true;
	double horizonSeconds = 1.0;
	double minimumLinearSpeed = 1.0;
	double maximumDistance = 1000.0;
	bool turnPredictionEnabled = true;
	double minimumAngularSpeedRadians = 0.08726646259971647;
	double maximumAngleRadians = 1.5707963267948966;
	bool zoomOutPredictionEnabled = true;
	double minimumZoomOutRate = 0.1;
	double maximumZoomOutScale = 2.0;
};

struct CesiumCameraPrediction final {
	double deltaSeconds = 0.0;
	bool active = false;
	bool translationActive = false;
	bool turnActive = false;
	bool zoomOutActive = false;
	double linearSpeed = 0.0;
	double distance = 0.0;
	double angularSpeedRadians = 0.0;
	double angleRadians = 0.0;
	double zoomOutRate = 0.0;
	double projectionScale = 1.0;
	glm::dvec3 position{0.0};
	glm::dvec3 direction{0.0, 0.0, -1.0};
	glm::dvec3 up{0.0, 1.0, 0.0};
};

/**
 * Maintains a short filtered camera history and produces one bounded future
 * selection pose. The result is suitable for a low-weight, non-rendered
 * Cesium TilesetViewGroup; it never changes the visible camera view.
 */
class CesiumCameraPredictor final {
public:
	CesiumCameraPrediction sample(
		const glm::dvec3& position,
		const glm::dvec3& direction,
		const glm::dvec3& up,
		double projectionSpan,
		const CesiumCameraPredictionSettings& settings
	);

	void reset();

private:
	bool m_hasPreviousSample = false;
	glm::dvec3 m_previousPosition{0.0};
	glm::dquat m_previousOrientation{1.0, 0.0, 0.0, 0.0};
	double m_previousProjectionSpan = 0.0;
	glm::dvec3 m_smoothedLinearVelocity{0.0};
	glm::dvec3 m_smoothedAngularVelocity{0.0};
	double m_smoothedZoomRate = 0.0;
	std::chrono::steady_clock::time_point m_previousTime{};
};

#endif // CESIUM_CAMERA_PREDICTOR_H
