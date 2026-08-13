// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Adapted for Godot from Cesium for Unreal under the Apache-2.0 license.
// Upstream counterparts:
// - Source/CesiumRuntime/Public/CesiumFlyToComponent.h
// - Source/CesiumRuntime/Private/CesiumFlyToComponent.cpp
// Last upstream review: Cesium for Unreal v2.29.0

#ifndef CESIUM_FLY_TO_H
#define CESIUM_FLY_TO_H

#if defined(CESIUM_GD_MODULE)
#include "core/object/object_id.h"
#include "core/variant/node_path.h"
#include "core/variant/packed_float64_array.h"
#include "scene/main/node.h"
#include "scene/resources/curve.h"
#elif defined(CESIUM_GD_EXT)
#include "godot_cpp/classes/curve.hpp"
#include "godot_cpp/classes/node.hpp"
#include "godot_cpp/classes/ref.hpp"
#include "godot_cpp/core/object.hpp"
#include "godot_cpp/variant/node_path.hpp"
#include "godot_cpp/variant/packed_float64_array.hpp"
#include "godot_cpp/variant/vector3.hpp"
using namespace godot;
#endif

#include <CesiumGeospatial/SimplePlanarEllipsoidCurve.h>
#include <glm/gtc/quaternion.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <optional>

class CesiumGlobeAnchor;

/**
 * Smoothly moves a CesiumGlobeAnchor along an ellipsoid-aware flight path.
 *
 * Add this component-style Node beside a CesiumGlobeAnchor beneath the target
 * Node3D, or assign globe_anchor_path explicitly. Position and flight math stay
 * in float64 ECEF even in a single-precision Godot build. The target's scale is
 * preserved while its rotation is interpolated in the moving local
 * East/Up/South frame.
 */
class CesiumFlyTo : public Node {
	GDCLASS(CesiumFlyTo, Node)

public:
	enum InterruptionReason {
		ExplicitlyInterrupted = 0,
		TargetMoved = 1,
		AnchorUnavailable = 2,
	};

	CesiumFlyTo();

	void _enter_tree() override;
	void _exit_tree() override;
	void _process(double delta) override;

	void set_globe_anchor_path(const NodePath& path);
	NodePath get_globe_anchor_path() const;
	CesiumGlobeAnchor* get_globe_anchor();

	void set_duration(double value);
	double get_duration() const;
	void set_progress_curve(const Ref<Curve>& value);
	Ref<Curve> get_progress_curve() const;
	void set_height_percentage_curve(const Ref<Curve>& value);
	Ref<Curve> get_height_percentage_curve() const;
	void set_maximum_height_by_distance_curve(const Ref<Curve>& value);
	Ref<Curve> get_maximum_height_by_distance_curve() const;
	void set_maximum_height_curve_distance(double value);
	double get_maximum_height_curve_distance() const;
	void set_default_maximum_height(double value);
	double get_default_maximum_height() const;
	void set_movement_interrupt_tolerance(double value);
	double get_movement_interrupt_tolerance() const;
	void set_automatic_update(bool value);
	bool get_automatic_update() const;

	bool fly_to_location_ecef_precise(
		const PackedFloat64Array& destination,
		double yawAtDestination,
		double pitchAtDestination,
		bool canInterruptByMoving
	);
	bool fly_to_location_longitude_latitude_height_precise(
		const PackedFloat64Array& destination,
		double yawAtDestination,
		double pitchAtDestination,
		bool canInterruptByMoving
	);
	bool fly_to_location_local_precise(
		const PackedFloat64Array& destination,
		double yawAtDestination,
		double pitchAtDestination,
		bool canInterruptByMoving
	);
	bool fly_to_location_ecef(
		const Vector3& destination,
		double yawAtDestination,
		double pitchAtDestination,
		bool canInterruptByMoving
	);
	bool fly_to_location_longitude_latitude_height(
		const Vector3& destination,
		double yawAtDestination,
		double pitchAtDestination,
		bool canInterruptByMoving
	);
	bool fly_to_location_global(
		const Vector3& destination,
		double yawAtDestination,
		double pitchAtDestination,
		bool canInterruptByMoving
	);

	bool update_flight(double delta);
	bool interrupt_flight();
	bool is_flight_in_progress() const;
	double get_flight_progress() const;
	double get_flight_elapsed_time() const;
	double get_flight_distance() const;
	double get_current_height_offset() const;
	PackedFloat64Array get_destination_ecef_precise() const;
	int32_t get_last_interruption_reason() const;

protected:
	static void _bind_methods();

private:
	CesiumGlobeAnchor* resolve_globe_anchor();
	bool apply_flight_sample(
		const glm::dvec3& position,
		const glm::dquat& localRotation
	);
	void finish_flight();
	void interrupt_flight_with_reason(InterruptionReason reason);
	void update_processing_state();

	NodePath m_globeAnchorPath;
	ObjectID m_globeAnchor;
	Ref<Curve> m_progressCurve;
	Ref<Curve> m_heightPercentageCurve;
	Ref<Curve> m_maximumHeightByDistanceCurve;
	double m_duration = 5.0;
	double m_maximumHeightCurveDistance = 1000000.0;
	double m_defaultMaximumHeight = 30000.0;
	double m_movementInterruptTolerance = 1.0e-5;
	bool m_automaticUpdate = true;

	bool m_flightInProgress = false;
	bool m_canInterruptByMoving = true;
	double m_currentFlyTime = 0.0;
	double m_flightProgress = 0.0;
	double m_length = 0.0;
	double m_maximumHeight = 0.0;
	double m_currentHeightOffset = 0.0;
	glm::dvec3 m_destinationEcef{0.0};
	glm::dvec3 m_previousPositionEcef{0.0};
	glm::dvec3 m_sourceScale{1.0};
	glm::dquat m_sourceRotation{1.0, 0.0, 0.0, 0.0};
	glm::dquat m_destinationRotation{1.0, 0.0, 0.0, 0.0};
	std::optional<CesiumGeospatial::SimplePlanarEllipsoidCurve> m_currentCurve;
	InterruptionReason m_lastInterruptionReason = ExplicitlyInterrupted;
};

VARIANT_ENUM_CAST(CesiumFlyTo::InterruptionReason);

#endif
