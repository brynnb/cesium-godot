// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Adapted for Godot from Cesium for Unreal under the Apache-2.0 license.

#include "Runtime/Public/Georeference/CesiumFlyTo.h"

#include "Godot/Nodes/CesiumGeoreference.h"
#include "Runtime/Public/Georeference/CesiumGlobeAnchor.h"

#if defined(CESIUM_GD_EXT)
#include "godot_cpp/core/error_macros.hpp"
#endif

#include <CesiumGeospatial/GlobeTransforms.h>
#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>

namespace {
bool unpack_triple(
	const PackedFloat64Array& values,
	glm::dvec3* result,
	const char* description
) {
	if (values.size() != 3) {
		ERR_PRINT(String(description) + " must contain three float64 values.");
		return false;
	}
	const glm::dvec3 candidate(values[0], values[1], values[2]);
	if (!std::isfinite(candidate.x) || !std::isfinite(candidate.y) ||
		!std::isfinite(candidate.z)) {
		ERR_PRINT(String(description) + " must contain only finite values.");
		return false;
	}
	*result = candidate;
	return true;
}

PackedFloat64Array pack_triple(const glm::dvec3& value) {
	PackedFloat64Array result;
	result.resize(3);
	result.set(0, value.x);
	result.set(1, value.y);
	result.set(2, value.z);
	return result;
}

bool unpack_matrix(const PackedFloat64Array& values, glm::dmat4* result) {
	if (values.size() != 16) {
		return false;
	}
	glm::dmat4 matrix(1.0);
	for (int32_t column = 0; column < 4; ++column) {
		for (int32_t row = 0; row < 4; ++row) {
			const double value = values[column * 4 + row];
			if (!std::isfinite(value)) {
				return false;
			}
			matrix[column][row] = value;
		}
	}
	*result = matrix;
	return true;
}

PackedFloat64Array pack_matrix(const glm::dmat4& matrix) {
	PackedFloat64Array result;
	result.resize(16);
	for (int32_t column = 0; column < 4; ++column) {
		for (int32_t row = 0; row < 4; ++row) {
			result.set(column * 4 + row, matrix[column][row]);
		}
	}
	return result;
}

glm::dmat3 east_up_south_to_fixed(
	const glm::dvec3& position,
	const CesiumGeospatial::Ellipsoid& ellipsoid
) {
	const glm::dmat4 eastNorthUp =
		CesiumGeospatial::GlobeTransforms::eastNorthUpToFixedFrame(
			position,
			ellipsoid
		);
	return glm::dmat3(
		glm::dvec3(eastNorthUp[0]),
		glm::dvec3(eastNorthUp[2]),
		-glm::dvec3(eastNorthUp[1])
	);
}

bool extract_rotation_and_scale(
	const glm::dmat4& matrix,
	glm::dmat3* rotation,
	glm::dvec3* scale
) {
	glm::dvec3 columns[3] = {
		glm::dvec3(matrix[0]),
		glm::dvec3(matrix[1]),
		glm::dvec3(matrix[2]),
	};
	for (int32_t index = 0; index < 3; ++index) {
		(*scale)[index] = glm::length(columns[index]);
		if (!std::isfinite((*scale)[index]) || (*scale)[index] <= 1.0e-15) {
			return false;
		}
		columns[index] /= (*scale)[index];
	}
	glm::dmat3 candidate(columns[0], columns[1], columns[2]);
	if (glm::determinant(candidate) < 0.0) {
		(*scale).x = -(*scale).x;
		candidate[0] = -candidate[0];
	}
	*rotation = candidate;
	return true;
}

double sample_curve(const Ref<Curve>& curve, double offset) {
	const double result = curve.is_valid()
		? static_cast<double>(curve->sample(std::clamp(offset, 0.0, 1.0)))
		: offset;
	return std::isfinite(result) ? result : offset;
}
}

CesiumFlyTo::CesiumFlyTo() {
	// Godot Curve resources have a normalized horizontal domain. These defaults
	// provide the same responsibilities as Unreal's packaged curve assets while
	// remaining ordinary editable/savable Godot resources.
	this->m_progressCurve.instantiate();
	this->m_progressCurve->set_min_value(0.0);
	this->m_progressCurve->set_max_value(1.0);
	this->m_progressCurve->add_point(Vector2(0.0, 0.0));
	this->m_progressCurve->add_point(Vector2(1.0, 1.0));

	this->m_heightPercentageCurve.instantiate();
	this->m_heightPercentageCurve->set_min_value(0.0);
	this->m_heightPercentageCurve->set_max_value(1.0);
	this->m_heightPercentageCurve->add_point(Vector2(0.0, 0.0));
	this->m_heightPercentageCurve->add_point(Vector2(0.5, 1.0));
	this->m_heightPercentageCurve->add_point(Vector2(1.0, 0.0));

	this->m_maximumHeightByDistanceCurve.instantiate();
	this->m_maximumHeightByDistanceCurve->set_min_value(0.0);
	this->m_maximumHeightByDistanceCurve->set_max_value(300000.0);
	this->m_maximumHeightByDistanceCurve->add_point(Vector2(0.0, 0.0));
	this->m_maximumHeightByDistanceCurve->add_point(Vector2(0.02, 30000.0));
	this->m_maximumHeightByDistanceCurve->add_point(Vector2(1.0, 300000.0));
}

void CesiumFlyTo::_enter_tree() {
	this->resolve_globe_anchor();
	this->update_processing_state();
}

void CesiumFlyTo::_exit_tree() {
	this->m_flightInProgress = false;
	this->m_currentCurve.reset();
	this->m_globeAnchor = ObjectID();
	this->set_process(false);
}

void CesiumFlyTo::_process(double delta) {
	if (this->m_automaticUpdate) {
		this->update_flight(delta);
	}
}

void CesiumFlyTo::set_globe_anchor_path(const NodePath& path) {
	if (this->m_globeAnchorPath == path) {
		return;
	}
	if (this->m_flightInProgress) {
		this->interrupt_flight_with_reason(AnchorUnavailable);
	}
	this->m_globeAnchorPath = path;
	this->m_globeAnchor = ObjectID();
	if (this->is_inside_tree()) {
		this->resolve_globe_anchor();
	}
}

NodePath CesiumFlyTo::get_globe_anchor_path() const {
	return this->m_globeAnchorPath;
}

CesiumGlobeAnchor* CesiumFlyTo::resolve_globe_anchor() {
	CesiumGlobeAnchor* cached = this->m_globeAnchor.is_null()
		? nullptr
		: Object::cast_to<CesiumGlobeAnchor>(
			ObjectDB::get_instance(this->m_globeAnchor)
		);
	if (cached != nullptr) {
		return cached;
	}
	CesiumGlobeAnchor* result = nullptr;
	if (!this->m_globeAnchorPath.is_empty()) {
		result = Object::cast_to<CesiumGlobeAnchor>(
			this->get_node_or_null(this->m_globeAnchorPath)
		);
	} else {
		Node* parent = this->get_parent();
		if (parent != nullptr) {
			for (int32_t index = 0; index < parent->get_child_count(); ++index) {
				result = Object::cast_to<CesiumGlobeAnchor>(
					parent->get_child(index)
				);
				if (result != nullptr) {
					break;
				}
			}
		}
	}
	if (result != nullptr) {
		this->m_globeAnchor = ObjectID(result->get_instance_id());
	}
	return result;
}

CesiumGlobeAnchor* CesiumFlyTo::get_globe_anchor() {
	return this->resolve_globe_anchor();
}

void CesiumFlyTo::set_duration(double value) {
	this->m_duration = std::isfinite(value) ? std::max(0.0, value) : 0.0;
}

double CesiumFlyTo::get_duration() const { return this->m_duration; }

void CesiumFlyTo::set_progress_curve(const Ref<Curve>& value) {
	this->m_progressCurve = value;
}

Ref<Curve> CesiumFlyTo::get_progress_curve() const {
	return this->m_progressCurve;
}

void CesiumFlyTo::set_height_percentage_curve(const Ref<Curve>& value) {
	this->m_heightPercentageCurve = value;
}

Ref<Curve> CesiumFlyTo::get_height_percentage_curve() const {
	return this->m_heightPercentageCurve;
}

void CesiumFlyTo::set_maximum_height_by_distance_curve(
	const Ref<Curve>& value
) {
	this->m_maximumHeightByDistanceCurve = value;
}

Ref<Curve> CesiumFlyTo::get_maximum_height_by_distance_curve() const {
	return this->m_maximumHeightByDistanceCurve;
}

void CesiumFlyTo::set_maximum_height_curve_distance(double value) {
	this->m_maximumHeightCurveDistance = std::isfinite(value)
		? std::max(1.0, value)
		: 1.0;
}

double CesiumFlyTo::get_maximum_height_curve_distance() const {
	return this->m_maximumHeightCurveDistance;
}

void CesiumFlyTo::set_default_maximum_height(double value) {
	this->m_defaultMaximumHeight = std::isfinite(value)
		? std::max(0.0, value)
		: 0.0;
}

double CesiumFlyTo::get_default_maximum_height() const {
	return this->m_defaultMaximumHeight;
}

void CesiumFlyTo::set_movement_interrupt_tolerance(double value) {
	this->m_movementInterruptTolerance = std::isfinite(value)
		? std::max(0.0, value)
		: 0.0;
}

double CesiumFlyTo::get_movement_interrupt_tolerance() const {
	return this->m_movementInterruptTolerance;
}

void CesiumFlyTo::set_automatic_update(bool value) {
	this->m_automaticUpdate = value;
	this->update_processing_state();
}

bool CesiumFlyTo::get_automatic_update() const {
	return this->m_automaticUpdate;
}

bool CesiumFlyTo::fly_to_location_ecef_precise(
	const PackedFloat64Array& destination,
	double yawAtDestination,
	double pitchAtDestination,
	bool canInterruptByMoving
) {
	if (this->m_flightInProgress) {
		WARN_PRINT("Cannot start a Cesium flight while another flight is in progress.");
		return false;
	}
	if (!std::isfinite(yawAtDestination) ||
		!std::isfinite(pitchAtDestination)) {
		ERR_PRINT("CesiumFlyTo yaw and pitch must be finite angles.");
		return false;
	}
	glm::dvec3 destinationEcef;
	if (!unpack_triple(destination, &destinationEcef, "An ECEF destination")) {
		return false;
	}
	CesiumGlobeAnchor* anchor = this->resolve_globe_anchor();
	if (anchor == nullptr || !anchor->sync_from_target()) {
		WARN_PRINT("CesiumFlyTo requires an initialized CesiumGlobeAnchor.");
		return false;
	}
	CesiumGeoreference* georeference = anchor->get_georeference();
	if (georeference == nullptr) {
		WARN_PRINT("CesiumFlyTo requires a globe anchor with a CesiumGeoreference.");
		return false;
	}
	glm::dmat4 sourceMatrix;
	if (!unpack_matrix(anchor->get_anchor_to_ecef_matrix_precise(), &sourceMatrix)) {
		return false;
	}
	const glm::dvec3 sourceEcef(sourceMatrix[3]);
	this->m_currentCurve =
		CesiumGeospatial::SimplePlanarEllipsoidCurve::
			fromEarthCenteredEarthFixedCoordinates(
				georeference->get_native_ellipsoid(),
				sourceEcef,
				destinationEcef
			);
	if (!this->m_currentCurve.has_value()) {
		WARN_PRINT("CesiumFlyTo could not create an ellipsoid path for the requested destination.");
		return false;
	}

	glm::dmat3 sourceFixedRotation(1.0);
	if (!extract_rotation_and_scale(
		sourceMatrix,
		&sourceFixedRotation,
		&this->m_sourceScale
	)) {
		WARN_PRINT("CesiumFlyTo cannot animate a target with a degenerate scale.");
		this->m_currentCurve.reset();
		return false;
	}
	const glm::dmat3 sourceEastUpSouth = east_up_south_to_fixed(
		sourceEcef,
		georeference->get_native_ellipsoid()
	);
	this->m_sourceRotation = glm::normalize(glm::quat_cast(
		glm::transpose(sourceEastUpSouth) * sourceFixedRotation
	));
	const double pitch = glm::radians(std::clamp(
		pitchAtDestination,
		-89.99,
		89.99
	));
	const double yaw = glm::radians(yawAtDestination);
	this->m_destinationRotation = glm::normalize(
		glm::angleAxis(yaw, glm::dvec3(0.0, 1.0, 0.0)) *
		glm::angleAxis(pitch, glm::dvec3(1.0, 0.0, 0.0))
	);

	this->m_length = glm::distance(sourceEcef, destinationEcef);
	this->m_maximumHeight = 0.0;
	if (this->m_heightPercentageCurve.is_valid()) {
		this->m_maximumHeight = this->m_defaultMaximumHeight;
		if (this->m_maximumHeightByDistanceCurve.is_valid()) {
			const double configuredMaximum = static_cast<double>(
				this->m_maximumHeightByDistanceCurve->sample(
					std::clamp(
						this->m_length /
							this->m_maximumHeightCurveDistance,
						0.0,
						1.0
					)
				)
			);
			this->m_maximumHeight = std::isfinite(configuredMaximum)
				? std::max(0.0, configuredMaximum)
				: this->m_defaultMaximumHeight;
		}
	}

	this->m_destinationEcef = destinationEcef;
	this->m_previousPositionEcef = sourceEcef;
	this->m_canInterruptByMoving = canInterruptByMoving;
	this->m_currentFlyTime = 0.0;
	this->m_flightProgress = 0.0;
	this->m_currentHeightOffset = 0.0;
	this->m_flightInProgress = true;
	this->update_processing_state();
	this->emit_signal(
		"flight_started",
		pack_triple(destinationEcef),
		this->m_length
	);
	return true;
}

bool CesiumFlyTo::fly_to_location_longitude_latitude_height_precise(
	const PackedFloat64Array& destination,
	double yawAtDestination,
	double pitchAtDestination,
	bool canInterruptByMoving
) {
	glm::dvec3 llh;
	if (!unpack_triple(
		destination,
		&llh,
		"A longitude/latitude/height destination"
	)) {
		return false;
	}
	CesiumGlobeAnchor* anchor = this->resolve_globe_anchor();
	CesiumGeoreference* georeference = anchor != nullptr
		? anchor->get_georeference()
		: nullptr;
	if (georeference == nullptr) {
		WARN_PRINT("CesiumFlyTo requires a globe anchor with a CesiumGeoreference.");
		return false;
	}
	const glm::dvec3 ecef =
		georeference->get_native_ellipsoid().cartographicToCartesian(
			CesiumGeospatial::Cartographic(
				glm::radians(llh.x),
				glm::radians(std::clamp(llh.y, -90.0, 90.0)),
				llh.z
			)
		);
	return this->fly_to_location_ecef_precise(
		pack_triple(ecef),
		yawAtDestination,
		pitchAtDestination,
		canInterruptByMoving
	);
}

bool CesiumFlyTo::fly_to_location_local_precise(
	const PackedFloat64Array& destination,
	double yawAtDestination,
	double pitchAtDestination,
	bool canInterruptByMoving
) {
	glm::dvec3 local;
	if (!unpack_triple(destination, &local, "A local destination")) {
		return false;
	}
	CesiumGlobeAnchor* anchor = this->resolve_globe_anchor();
	CesiumGeoreference* georeference = anchor != nullptr
		? anchor->get_georeference()
		: nullptr;
	if (georeference == nullptr) {
		WARN_PRINT("CesiumFlyTo requires a globe anchor with a CesiumGeoreference.");
		return false;
	}
	return this->fly_to_location_ecef_precise(
		pack_triple(
			georeference->get_coordinate_system().localPositionToEcef(local)
		),
		yawAtDestination,
		pitchAtDestination,
		canInterruptByMoving
	);
}

bool CesiumFlyTo::fly_to_location_ecef(
	const Vector3& destination,
	double yawAtDestination,
	double pitchAtDestination,
	bool canInterruptByMoving
) {
	return this->fly_to_location_ecef_precise(
		pack_triple(glm::dvec3(destination.x, destination.y, destination.z)),
		yawAtDestination,
		pitchAtDestination,
		canInterruptByMoving
	);
}

bool CesiumFlyTo::fly_to_location_longitude_latitude_height(
	const Vector3& destination,
	double yawAtDestination,
	double pitchAtDestination,
	bool canInterruptByMoving
) {
	return this->fly_to_location_longitude_latitude_height_precise(
		pack_triple(glm::dvec3(destination.x, destination.y, destination.z)),
		yawAtDestination,
		pitchAtDestination,
		canInterruptByMoving
	);
}

bool CesiumFlyTo::fly_to_location_global(
	const Vector3& destination,
	double yawAtDestination,
	double pitchAtDestination,
	bool canInterruptByMoving
) {
	CesiumGlobeAnchor* anchor = this->resolve_globe_anchor();
	CesiumGeoreference* georeference = anchor != nullptr
		? anchor->get_georeference()
		: nullptr;
	if (georeference == nullptr) {
		WARN_PRINT("CesiumFlyTo requires a globe anchor with a CesiumGeoreference.");
		return false;
	}
	return this->fly_to_location_ecef(
		georeference->transform_global_position_to_ecef(destination),
		yawAtDestination,
		pitchAtDestination,
		canInterruptByMoving
	);
}

bool CesiumFlyTo::apply_flight_sample(
	const glm::dvec3& position,
	const glm::dquat& localRotation
) {
	CesiumGlobeAnchor* anchor = this->resolve_globe_anchor();
	CesiumGeoreference* georeference = anchor != nullptr
		? anchor->get_georeference()
		: nullptr;
	if (anchor == nullptr || georeference == nullptr) {
		return false;
	}
	const glm::dmat3 frame = east_up_south_to_fixed(
		position,
		georeference->get_native_ellipsoid()
	);
	const glm::dmat3 fixedRotation = frame * glm::mat3_cast(localRotation);
	glm::dmat4 matrix(1.0);
	for (int32_t column = 0; column < 3; ++column) {
		matrix[column] = glm::dvec4(
			fixedRotation[column] * this->m_sourceScale[column],
			0.0
		);
	}
	matrix[3] = glm::dvec4(position, 1.0);
	if (!anchor->set_anchor_to_ecef_matrix_precise(pack_matrix(matrix))) {
		return false;
	}
	this->m_previousPositionEcef = position;
	return true;
}

bool CesiumFlyTo::update_flight(double delta) {
	if (!this->m_flightInProgress || !this->m_currentCurve.has_value()) {
		return false;
	}
	CesiumGlobeAnchor* anchor = this->resolve_globe_anchor();
	if (anchor == nullptr || anchor->get_georeference() == nullptr) {
		this->interrupt_flight_with_reason(AnchorUnavailable);
		return false;
	}
	// Observe gameplay movement that occurred after the anchor's last physics
	// sync without round-tripping every flight sample through Godot's possibly
	// single-precision transform. Origin shifts call sync_to_target and update
	// the anchor's remembered transform, so they are not mistaken for movement.
	if (
		this->m_canInterruptByMoving &&
		anchor->target_transform_changed_since_last_sync() &&
		!anchor->sync_from_target()
	) {
		this->interrupt_flight_with_reason(AnchorUnavailable);
		return false;
	}
	glm::dvec3 currentEcef;
	if (!unpack_triple(
		anchor->get_ecef_position_precise(),
		&currentEcef,
		"The globe anchor ECEF position"
	)) {
		this->interrupt_flight_with_reason(AnchorUnavailable);
		return false;
	}
	if (
		this->m_canInterruptByMoving &&
		glm::distance(currentEcef, this->m_previousPositionEcef) >
			this->m_movementInterruptTolerance
	) {
		this->interrupt_flight_with_reason(TargetMoved);
		return false;
	}

	this->m_currentFlyTime += std::isfinite(delta)
		? std::max(0.0, delta)
		: 0.0;
	const bool reachedDuration = this->m_duration <= 0.0 ||
		this->m_currentFlyTime >= this->m_duration;
	const double timePercentage = reachedDuration
		? 1.0
		: this->m_currentFlyTime / this->m_duration;
	this->m_flightProgress = reachedDuration
		? 1.0
		: std::clamp(
			sample_curve(this->m_progressCurve, timePercentage),
			0.0,
			1.0
		);

	if (this->m_flightProgress >= 1.0) {
		this->m_currentHeightOffset = 0.0;
		if (!this->apply_flight_sample(
			this->m_destinationEcef,
			this->m_destinationRotation
		)) {
			this->interrupt_flight_with_reason(AnchorUnavailable);
			return false;
		}
		this->emit_signal(
			"flight_progressed",
			1.0,
			pack_triple(this->m_destinationEcef),
			0.0
		);
		this->finish_flight();
		return true;
	}

	const double heightPercentage = this->m_heightPercentageCurve.is_valid()
		? static_cast<double>(this->m_heightPercentageCurve->sample(
			this->m_flightProgress
		))
		: 0.0;
	this->m_currentHeightOffset = std::isfinite(heightPercentage)
		? this->m_maximumHeight * std::clamp(heightPercentage, 0.0, 1.0)
		: 0.0;
	const glm::dvec3 position = this->m_currentCurve->getPosition(
		this->m_flightProgress,
		this->m_currentHeightOffset
	);
	const glm::dquat rotation = glm::normalize(glm::slerp(
		this->m_sourceRotation,
		this->m_destinationRotation,
		this->m_flightProgress
	));
	if (!this->apply_flight_sample(position, rotation)) {
		this->interrupt_flight_with_reason(AnchorUnavailable);
		return false;
	}
	this->emit_signal(
		"flight_progressed",
		this->m_flightProgress,
		pack_triple(position),
		this->m_currentHeightOffset
	);
	return true;
}

void CesiumFlyTo::finish_flight() {
	this->m_flightInProgress = false;
	this->m_currentFlyTime = 0.0;
	this->m_currentCurve.reset();
	this->update_processing_state();
	this->emit_signal("flight_completed");
}

void CesiumFlyTo::interrupt_flight_with_reason(InterruptionReason reason) {
	if (!this->m_flightInProgress) {
		return;
	}
	this->m_flightInProgress = false;
	this->m_currentCurve.reset();
	this->m_lastInterruptionReason = reason;
	this->update_processing_state();
	this->emit_signal("flight_interrupted", static_cast<int32_t>(reason));
}

bool CesiumFlyTo::interrupt_flight() {
	if (!this->m_flightInProgress) {
		return false;
	}
	this->interrupt_flight_with_reason(ExplicitlyInterrupted);
	return true;
}

void CesiumFlyTo::update_processing_state() {
	this->set_process(this->is_inside_tree() && this->m_automaticUpdate &&
		this->m_flightInProgress);
}

bool CesiumFlyTo::is_flight_in_progress() const {
	return this->m_flightInProgress;
}

double CesiumFlyTo::get_flight_progress() const {
	return this->m_flightProgress;
}

double CesiumFlyTo::get_flight_elapsed_time() const {
	return this->m_currentFlyTime;
}

double CesiumFlyTo::get_flight_distance() const { return this->m_length; }

double CesiumFlyTo::get_current_height_offset() const {
	return this->m_currentHeightOffset;
}

PackedFloat64Array CesiumFlyTo::get_destination_ecef_precise() const {
	return pack_triple(this->m_destinationEcef);
}

int32_t CesiumFlyTo::get_last_interruption_reason() const {
	return static_cast<int32_t>(this->m_lastInterruptionReason);
}

void CesiumFlyTo::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_globe_anchor_path", "path"), &CesiumFlyTo::set_globe_anchor_path);
	ClassDB::bind_method(D_METHOD("get_globe_anchor_path"), &CesiumFlyTo::get_globe_anchor_path);
	ClassDB::bind_method(D_METHOD("get_globe_anchor"), &CesiumFlyTo::get_globe_anchor);
	ClassDB::bind_method(D_METHOD("set_duration", "value"), &CesiumFlyTo::set_duration);
	ClassDB::bind_method(D_METHOD("get_duration"), &CesiumFlyTo::get_duration);
	ClassDB::bind_method(D_METHOD("set_progress_curve", "curve"), &CesiumFlyTo::set_progress_curve);
	ClassDB::bind_method(D_METHOD("get_progress_curve"), &CesiumFlyTo::get_progress_curve);
	ClassDB::bind_method(D_METHOD("set_height_percentage_curve", "curve"), &CesiumFlyTo::set_height_percentage_curve);
	ClassDB::bind_method(D_METHOD("get_height_percentage_curve"), &CesiumFlyTo::get_height_percentage_curve);
	ClassDB::bind_method(D_METHOD("set_maximum_height_by_distance_curve", "curve"), &CesiumFlyTo::set_maximum_height_by_distance_curve);
	ClassDB::bind_method(D_METHOD("get_maximum_height_by_distance_curve"), &CesiumFlyTo::get_maximum_height_by_distance_curve);
	ClassDB::bind_method(D_METHOD("set_maximum_height_curve_distance", "value"), &CesiumFlyTo::set_maximum_height_curve_distance);
	ClassDB::bind_method(D_METHOD("get_maximum_height_curve_distance"), &CesiumFlyTo::get_maximum_height_curve_distance);
	ClassDB::bind_method(D_METHOD("set_default_maximum_height", "value"), &CesiumFlyTo::set_default_maximum_height);
	ClassDB::bind_method(D_METHOD("get_default_maximum_height"), &CesiumFlyTo::get_default_maximum_height);
	ClassDB::bind_method(D_METHOD("set_movement_interrupt_tolerance", "value"), &CesiumFlyTo::set_movement_interrupt_tolerance);
	ClassDB::bind_method(D_METHOD("get_movement_interrupt_tolerance"), &CesiumFlyTo::get_movement_interrupt_tolerance);
	ClassDB::bind_method(D_METHOD("set_automatic_update", "value"), &CesiumFlyTo::set_automatic_update);
	ClassDB::bind_method(D_METHOD("get_automatic_update"), &CesiumFlyTo::get_automatic_update);
	ClassDB::bind_method(D_METHOD("fly_to_location_ecef_precise", "destination", "yaw_at_destination", "pitch_at_destination", "can_interrupt_by_moving"), &CesiumFlyTo::fly_to_location_ecef_precise, DEFVAL(0.0), DEFVAL(0.0), DEFVAL(true));
	ClassDB::bind_method(D_METHOD("fly_to_location_longitude_latitude_height_precise", "destination", "yaw_at_destination", "pitch_at_destination", "can_interrupt_by_moving"), &CesiumFlyTo::fly_to_location_longitude_latitude_height_precise, DEFVAL(0.0), DEFVAL(0.0), DEFVAL(true));
	ClassDB::bind_method(D_METHOD("fly_to_location_local_precise", "destination", "yaw_at_destination", "pitch_at_destination", "can_interrupt_by_moving"), &CesiumFlyTo::fly_to_location_local_precise, DEFVAL(0.0), DEFVAL(0.0), DEFVAL(true));
	ClassDB::bind_method(D_METHOD("fly_to_location_ecef", "destination", "yaw_at_destination", "pitch_at_destination", "can_interrupt_by_moving"), &CesiumFlyTo::fly_to_location_ecef, DEFVAL(0.0), DEFVAL(0.0), DEFVAL(true));
	ClassDB::bind_method(D_METHOD("fly_to_location_longitude_latitude_height", "destination", "yaw_at_destination", "pitch_at_destination", "can_interrupt_by_moving"), &CesiumFlyTo::fly_to_location_longitude_latitude_height, DEFVAL(0.0), DEFVAL(0.0), DEFVAL(true));
	ClassDB::bind_method(D_METHOD("fly_to_location_global", "destination", "yaw_at_destination", "pitch_at_destination", "can_interrupt_by_moving"), &CesiumFlyTo::fly_to_location_global, DEFVAL(0.0), DEFVAL(0.0), DEFVAL(true));
	ClassDB::bind_method(D_METHOD("update_flight", "delta"), &CesiumFlyTo::update_flight);
	ClassDB::bind_method(D_METHOD("interrupt_flight"), &CesiumFlyTo::interrupt_flight);
	ClassDB::bind_method(D_METHOD("is_flight_in_progress"), &CesiumFlyTo::is_flight_in_progress);
	ClassDB::bind_method(D_METHOD("get_flight_progress"), &CesiumFlyTo::get_flight_progress);
	ClassDB::bind_method(D_METHOD("get_flight_elapsed_time"), &CesiumFlyTo::get_flight_elapsed_time);
	ClassDB::bind_method(D_METHOD("get_flight_distance"), &CesiumFlyTo::get_flight_distance);
	ClassDB::bind_method(D_METHOD("get_current_height_offset"), &CesiumFlyTo::get_current_height_offset);
	ClassDB::bind_method(D_METHOD("get_destination_ecef_precise"), &CesiumFlyTo::get_destination_ecef_precise);
	ClassDB::bind_method(D_METHOD("get_last_interruption_reason"), &CesiumFlyTo::get_last_interruption_reason);

	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "globe_anchor_path", PROPERTY_HINT_NODE_PATH_VALID_TYPES, "CesiumGlobeAnchor"), "set_globe_anchor_path", "get_globe_anchor_path");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "duration", PROPERTY_HINT_RANGE, "0,3600,0.01,or_greater,suffix:s"), "set_duration", "get_duration");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "progress_curve", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_progress_curve", "get_progress_curve");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "height_percentage_curve", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_height_percentage_curve", "get_height_percentage_curve");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "maximum_height_by_distance_curve", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_maximum_height_by_distance_curve", "get_maximum_height_by_distance_curve");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "maximum_height_curve_distance", PROPERTY_HINT_RANGE, "1,100000000,1,or_greater,exp,suffix:m"), "set_maximum_height_curve_distance", "get_maximum_height_curve_distance");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "default_maximum_height", PROPERTY_HINT_RANGE, "0,100000000,1,or_greater,suffix:m"), "set_default_maximum_height", "get_default_maximum_height");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "movement_interrupt_tolerance", PROPERTY_HINT_RANGE, "0,1000,0.000001,or_greater,exp,suffix:m"), "set_movement_interrupt_tolerance", "get_movement_interrupt_tolerance");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "automatic_update"), "set_automatic_update", "get_automatic_update");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "flight_in_progress", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "is_flight_in_progress");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "flight_progress", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_flight_progress");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "flight_elapsed_time", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_flight_elapsed_time");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "flight_distance", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_flight_distance");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "current_height_offset", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_current_height_offset");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_FLOAT64_ARRAY, "destination_ecef_precise", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_destination_ecef_precise");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "last_interruption_reason", PROPERTY_HINT_ENUM, "Explicitly Interrupted,Target Moved,Anchor Unavailable", PROPERTY_USAGE_NONE), "", "get_last_interruption_reason");

	BIND_ENUM_CONSTANT(ExplicitlyInterrupted);
	BIND_ENUM_CONSTANT(TargetMoved);
	BIND_ENUM_CONSTANT(AnchorUnavailable);
	ADD_SIGNAL(MethodInfo(
		"flight_started",
		PropertyInfo(Variant::PACKED_FLOAT64_ARRAY, "destination_ecef"),
		PropertyInfo(Variant::FLOAT, "straight_line_distance")
	));
	ADD_SIGNAL(MethodInfo(
		"flight_progressed",
		PropertyInfo(Variant::FLOAT, "progress"),
		PropertyInfo(Variant::PACKED_FLOAT64_ARRAY, "position_ecef"),
		PropertyInfo(Variant::FLOAT, "height_offset")
	));
	ADD_SIGNAL(MethodInfo("flight_completed"));
	ADD_SIGNAL(MethodInfo(
		"flight_interrupted",
		PropertyInfo(Variant::INT, "reason", PROPERTY_HINT_ENUM, "Explicitly Interrupted,Target Moved,Anchor Unavailable")
	));
}
