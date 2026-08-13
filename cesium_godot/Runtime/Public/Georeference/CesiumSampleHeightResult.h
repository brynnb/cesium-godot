// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Adapted for Godot from Cesium for Unreal under the Apache-2.0 license.

#ifndef CESIUM_SAMPLE_HEIGHT_RESULT_H
#define CESIUM_SAMPLE_HEIGHT_RESULT_H

/**
 * Godot adaptation reviewed against Cesium for Unreal v2.29.0:
 * Source/CesiumRuntime/Public/CesiumSampleHeightResult.h.
 *
 * A copied result from a most-detailed height query. No Cesium Native or
 * tileset pointer is retained, so this object remains safe after unload.
 */

#if defined(CESIUM_GD_MODULE)
#include "core/object/ref_counted.h"
#include "core/variant/packed_float64_array.h"
#elif defined(CESIUM_GD_EXT)
#include "godot_cpp/classes/ref_counted.hpp"
#include "godot_cpp/variant/packed_float64_array.hpp"
using namespace godot;
#endif

class CesiumSampleHeightResult : public RefCounted {
	GDCLASS(CesiumSampleHeightResult, RefCounted)

public:
	Vector3 get_longitude_latitude_height() const;
	PackedFloat64Array get_longitude_latitude_height_components() const;
	bool get_sample_success() const;

	void initialize(
		double longitudeDegrees,
		double latitudeDegrees,
		double heightMeters,
		bool sampleSuccess
	);

protected:
	static void _bind_methods();

private:
	double m_longitudeDegrees = 0.0;
	double m_latitudeDegrees = 0.0;
	double m_heightMeters = 0.0;
	bool m_sampleSuccess = false;
};

#endif // CESIUM_SAMPLE_HEIGHT_RESULT_H
