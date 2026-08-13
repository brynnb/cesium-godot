// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Adapted for Godot from Cesium for Unreal under the Apache-2.0 license.

#include "Runtime/Public/Georeference/CesiumSampleHeightResult.h"

Vector3 CesiumSampleHeightResult::get_longitude_latitude_height() const {
	return Vector3(
		this->m_longitudeDegrees,
		this->m_latitudeDegrees,
		this->m_heightMeters
	);
}

PackedFloat64Array
CesiumSampleHeightResult::get_longitude_latitude_height_components() const {
	PackedFloat64Array result;
	result.resize(3);
	result.set(0, this->m_longitudeDegrees);
	result.set(1, this->m_latitudeDegrees);
	result.set(2, this->m_heightMeters);
	return result;
}

bool CesiumSampleHeightResult::get_sample_success() const {
	return this->m_sampleSuccess;
}

void CesiumSampleHeightResult::initialize(
	double longitudeDegrees,
	double latitudeDegrees,
	double heightMeters,
	bool sampleSuccess
) {
	this->m_longitudeDegrees = longitudeDegrees;
	this->m_latitudeDegrees = latitudeDegrees;
	this->m_heightMeters = heightMeters;
	this->m_sampleSuccess = sampleSuccess;
}

void CesiumSampleHeightResult::_bind_methods() {
	ClassDB::bind_method(
		D_METHOD("get_longitude_latitude_height"),
		&CesiumSampleHeightResult::get_longitude_latitude_height
	);
	ClassDB::bind_method(
		D_METHOD("get_longitude_latitude_height_components"),
		&CesiumSampleHeightResult::get_longitude_latitude_height_components
	);
	ClassDB::bind_method(
		D_METHOD("get_sample_success"),
		&CesiumSampleHeightResult::get_sample_success
	);

	ADD_PROPERTY(
		PropertyInfo(
			Variant::VECTOR3,
			"longitude_latitude_height",
			PROPERTY_HINT_NONE,
			"",
			PROPERTY_USAGE_NONE
		),
		"",
		"get_longitude_latitude_height"
	);
	ADD_PROPERTY(
		PropertyInfo(
			Variant::PACKED_FLOAT64_ARRAY,
			"longitude_latitude_height_components",
			PROPERTY_HINT_NONE,
			"",
			PROPERTY_USAGE_NONE
		),
		"",
		"get_longitude_latitude_height_components"
	);
	ADD_PROPERTY(
		PropertyInfo(
			Variant::BOOL,
			"sample_success",
			PROPERTY_HINT_NONE,
			"",
			PROPERTY_USAGE_NONE
		),
		"",
		"get_sample_success"
	);
}
