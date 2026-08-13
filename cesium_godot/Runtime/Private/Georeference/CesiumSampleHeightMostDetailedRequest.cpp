// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Adapted for Godot from Cesium for Unreal under the Apache-2.0 license.

#include "Runtime/Public/Georeference/CesiumSampleHeightMostDetailedRequest.h"

int32_t CesiumSampleHeightMostDetailedRequest::get_status() const {
	return static_cast<int32_t>(this->m_status);
}

bool CesiumSampleHeightMostDetailedRequest::is_finished() const {
	return this->m_status != Status::Pending;
}

bool CesiumSampleHeightMostDetailedRequest::is_cancelled() const {
	return this->m_status == Status::Cancelled;
}

Array CesiumSampleHeightMostDetailedRequest::get_results() const {
	return this->m_results;
}

PackedStringArray CesiumSampleHeightMostDetailedRequest::get_warnings() const {
	return this->m_warnings;
}

void CesiumSampleHeightMostDetailedRequest::initialize(
	const ObjectID& tileset
) {
	this->m_tileset = tileset;
}

void CesiumSampleHeightMostDetailedRequest::complete(
	const Array& results,
	const PackedStringArray& warnings
) {
	if (this->m_status != Status::Pending) {
		return;
	}
	this->m_results = results;
	this->m_warnings = warnings;
	this->m_status = Status::Completed;
	this->emit_signal("completed", this->m_results, this->m_warnings);
}

void CesiumSampleHeightMostDetailedRequest::cancel_from_tileset(
	const String& warning
) {
	if (this->m_status != Status::Pending) {
		return;
	}
	if (!warning.is_empty()) {
		this->m_warnings.push_back(warning);
	}
	this->m_status = Status::Cancelled;
	this->emit_signal("cancelled", this->m_warnings);
}

void CesiumSampleHeightMostDetailedRequest::_bind_methods() {
	ClassDB::bind_method(
		D_METHOD("get_status"),
		&CesiumSampleHeightMostDetailedRequest::get_status
	);
	ClassDB::bind_method(
		D_METHOD("is_finished"),
		&CesiumSampleHeightMostDetailedRequest::is_finished
	);
	ClassDB::bind_method(
		D_METHOD("is_cancelled"),
		&CesiumSampleHeightMostDetailedRequest::is_cancelled
	);
	ClassDB::bind_method(
		D_METHOD("get_results"),
		&CesiumSampleHeightMostDetailedRequest::get_results
	);
	ClassDB::bind_method(
		D_METHOD("get_warnings"),
		&CesiumSampleHeightMostDetailedRequest::get_warnings
	);

	BIND_ENUM_CONSTANT(Pending);
	BIND_ENUM_CONSTANT(Completed);
	BIND_ENUM_CONSTANT(Cancelled);

	ADD_SIGNAL(MethodInfo(
		"completed",
		PropertyInfo(Variant::ARRAY, "results"),
		PropertyInfo(Variant::PACKED_STRING_ARRAY, "warnings")
	));
	ADD_SIGNAL(MethodInfo(
		"cancelled",
		PropertyInfo(Variant::PACKED_STRING_ARRAY, "warnings")
	));

	ADD_PROPERTY(
		PropertyInfo(
			Variant::INT,
			"status",
			PROPERTY_HINT_NONE,
			"",
			PROPERTY_USAGE_NONE
		),
		"",
		"get_status"
	);
	ADD_PROPERTY(
		PropertyInfo(
			Variant::ARRAY,
			"results",
			PROPERTY_HINT_NONE,
			"",
			PROPERTY_USAGE_NONE
		),
		"",
		"get_results"
	);
	ADD_PROPERTY(
		PropertyInfo(
			Variant::PACKED_STRING_ARRAY,
			"warnings",
			PROPERTY_HINT_NONE,
			"",
			PROPERTY_USAGE_NONE
		),
		"",
		"get_warnings"
	);
}
