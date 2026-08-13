// Copyright 2020-2026 CesiumGS, Inc. and Contributors

#include "Runtime/Private/Georeference/CesiumCameraPredictor.h"

#include <glm/ext/matrix_double3x3.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cmath>

namespace {
constexpr double MINIMUM_DELTA_SECONDS = 0.000001;
constexpr double MAXIMUM_DELTA_SECONDS = 1.0;
constexpr double MINIMUM_VECTOR_LENGTH = 1.0e-12;
constexpr double FILTER_TIME_CONSTANT_SECONDS = 0.25;

glm::dquat camera_orientation(
	const glm::dvec3& sourceDirection,
	const glm::dvec3& sourceUp
) {
	glm::dvec3 direction = glm::normalize(sourceDirection);
	glm::dvec3 right = glm::cross(direction, sourceUp);
	if (glm::dot(right, right) <= MINIMUM_VECTOR_LENGTH) {
		const glm::dvec3 fallbackUp =
			std::abs(direction.y) < 0.99
			? glm::dvec3(0.0, 1.0, 0.0)
			: glm::dvec3(1.0, 0.0, 0.0);
		right = glm::cross(direction, fallbackUp);
	}
	right = glm::normalize(right);
	const glm::dvec3 up = glm::normalize(glm::cross(right, direction));
	return glm::normalize(glm::quat_cast(glm::dmat3(right, up, -direction)));
}

glm::dvec3 angular_velocity(
	const glm::dquat& previous,
	const glm::dquat& current,
	double deltaSeconds
) {
	glm::dquat delta = glm::normalize(current * glm::conjugate(previous));
	if (delta.w < 0.0) {
		delta = -delta;
	}
	const double sinHalfAngle = glm::length(
		glm::dvec3(delta.x, delta.y, delta.z)
	);
	if (sinHalfAngle <= MINIMUM_VECTOR_LENGTH) {
		return glm::dvec3(0.0);
	}
	const double angle = 2.0 * std::atan2(sinHalfAngle, delta.w);
	return glm::dvec3(delta.x, delta.y, delta.z) *
		(angle / (sinHalfAngle * deltaSeconds));
}
} // namespace

CesiumCameraPrediction CesiumCameraPredictor::sample(
	const glm::dvec3& position,
	const glm::dvec3& direction,
	const glm::dvec3& up,
	double projectionSpan,
	const CesiumCameraPredictionSettings& settings
) {
	CesiumCameraPrediction result;
	result.position = position;
	result.direction = glm::normalize(direction);
	result.up = glm::normalize(up);
	projectionSpan = std::max(projectionSpan, MINIMUM_VECTOR_LENGTH);
	const glm::dquat orientation = camera_orientation(
		result.direction,
		result.up
	);
	const auto sampleTime = std::chrono::steady_clock::now();

	if (this->m_hasPreviousSample) {
		result.deltaSeconds = std::chrono::duration<double>(
			sampleTime - this->m_previousTime
		).count();
		if (
			result.deltaSeconds >= MINIMUM_DELTA_SECONDS &&
			result.deltaSeconds <= MAXIMUM_DELTA_SECONDS
		) {
			const double alpha = 1.0 - std::exp(
				-result.deltaSeconds / FILTER_TIME_CONSTANT_SECONDS
			);
			const glm::dvec3 rawLinearVelocity =
				(position - this->m_previousPosition) /
				result.deltaSeconds;
			this->m_smoothedLinearVelocity +=
				(rawLinearVelocity - this->m_smoothedLinearVelocity) * alpha;
			const glm::dvec3 rawAngularVelocity = angular_velocity(
				this->m_previousOrientation,
				orientation,
				result.deltaSeconds
			);
			this->m_smoothedAngularVelocity +=
				(rawAngularVelocity - this->m_smoothedAngularVelocity) * alpha;
			const double rawZoomRate = std::log(
				projectionSpan / this->m_previousProjectionSpan
			) / result.deltaSeconds;
			this->m_smoothedZoomRate +=
				(rawZoomRate - this->m_smoothedZoomRate) * alpha;
		} else {
			result.deltaSeconds = 0.0;
			this->m_smoothedLinearVelocity = glm::dvec3(0.0);
			this->m_smoothedAngularVelocity = glm::dvec3(0.0);
			this->m_smoothedZoomRate = 0.0;
		}
	}

	this->m_hasPreviousSample = true;
	this->m_previousPosition = position;
	this->m_previousOrientation = orientation;
	this->m_previousProjectionSpan = projectionSpan;
	this->m_previousTime = sampleTime;

	result.linearSpeed = glm::length(this->m_smoothedLinearVelocity);
	glm::dvec3 displacement = this->m_smoothedLinearVelocity *
		settings.horizonSeconds;
	result.distance = glm::length(displacement);
	if (
		settings.maximumDistance > 0.0 &&
		result.distance > settings.maximumDistance
	) {
		displacement *= settings.maximumDistance / result.distance;
		result.distance = settings.maximumDistance;
	}
	result.translationActive =
		settings.enabled &&
		settings.maximumDistance > 0.0 &&
		result.linearSpeed >= settings.minimumLinearSpeed &&
		std::isfinite(result.distance) && result.distance > 0.001;
	if (result.translationActive) {
		result.position += displacement;
	}

	result.angularSpeedRadians = glm::length(
		this->m_smoothedAngularVelocity
	);
	result.angleRadians = result.angularSpeedRadians * settings.horizonSeconds;
	if (settings.maximumAngleRadians > 0.0) {
		result.angleRadians = std::min(
			result.angleRadians,
			settings.maximumAngleRadians
		);
	}
	result.turnActive =
		settings.enabled &&
		settings.turnPredictionEnabled &&
		settings.maximumAngleRadians > 0.0 &&
		result.angularSpeedRadians >= settings.minimumAngularSpeedRadians &&
		std::isfinite(result.angleRadians) && result.angleRadians > 0.0001;
	if (result.turnActive) {
		const glm::dvec3 axis = glm::normalize(
			this->m_smoothedAngularVelocity
		);
		const glm::dquat predictedOrientation = glm::angleAxis(
			result.angleRadians,
			axis
		) * orientation;
		result.direction = glm::normalize(
			predictedOrientation * glm::dvec3(0.0, 0.0, -1.0)
		);
		result.up = glm::normalize(
			predictedOrientation * glm::dvec3(0.0, 1.0, 0.0)
		);
	}

	result.zoomOutRate = std::max(0.0, this->m_smoothedZoomRate);
	result.projectionScale = std::exp(
		result.zoomOutRate * settings.horizonSeconds
	);
	result.projectionScale = std::clamp(
		result.projectionScale,
		1.0,
		std::max(1.0, settings.maximumZoomOutScale)
	);
	result.zoomOutActive =
		settings.enabled &&
		settings.zoomOutPredictionEnabled &&
		settings.maximumZoomOutScale > 1.0 &&
		result.zoomOutRate >= settings.minimumZoomOutRate &&
		std::isfinite(result.projectionScale) &&
		result.projectionScale > 1.0001;
	if (!result.zoomOutActive) {
		result.projectionScale = 1.0;
	}

	result.active =
		settings.enabled && result.deltaSeconds > 0.0 &&
		settings.horizonSeconds > 0.0 &&
		(
			result.translationActive || result.turnActive ||
			result.zoomOutActive
		);
	return result;
}

void CesiumCameraPredictor::reset() {
	this->m_hasPreviousSample = false;
	this->m_previousPosition = glm::dvec3(0.0);
	this->m_previousOrientation = glm::dquat(1.0, 0.0, 0.0, 0.0);
	this->m_previousProjectionSpan = 0.0;
	this->m_smoothedLinearVelocity = glm::dvec3(0.0);
	this->m_smoothedAngularVelocity = glm::dvec3(0.0);
	this->m_smoothedZoomRate = 0.0;
	this->m_previousTime = std::chrono::steady_clock::time_point{};
}
