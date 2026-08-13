// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Adapted for Godot from Cesium for Unreal under the Apache-2.0 license.

#ifndef CESIUM_SAMPLE_HEIGHT_MOST_DETAILED_REQUEST_H
#define CESIUM_SAMPLE_HEIGHT_MOST_DETAILED_REQUEST_H

/**
 * Godot adaptation reviewed against Cesium for Unreal v2.29.0:
 * Source/CesiumRuntime/Public/CesiumSampleHeightMostDetailedAsyncAction.h.
 *
 * Godot uses a RefCounted request and signal in place of an Unreal async
 * Blueprint action. Results are copied before this object is completed.
 */

#if defined(CESIUM_GD_MODULE)
#include "core/object/object_id.h"
#include "core/object/ref_counted.h"
#include "core/variant/array.h"
#include "core/variant/packed_string_array.h"
#elif defined(CESIUM_GD_EXT)
#include "godot_cpp/classes/ref_counted.hpp"
#include "godot_cpp/variant/array.hpp"
#include "godot_cpp/variant/packed_string_array.hpp"
using namespace godot;
#endif

class CesiumSampleHeightMostDetailedRequest : public RefCounted {
	GDCLASS(CesiumSampleHeightMostDetailedRequest, RefCounted)

public:
	enum Status {
		Pending = 0,
		Completed = 1,
		Cancelled = 2,
	};

	int32_t get_status() const;
	bool is_finished() const;
	bool is_cancelled() const;
	Array get_results() const;
	PackedStringArray get_warnings() const;

	void initialize(const ObjectID& tileset);
	void complete(const Array& results, const PackedStringArray& warnings);
	void cancel_from_tileset(const String& warning);

protected:
	static void _bind_methods();

private:
	ObjectID m_tileset;
	Status m_status = Status::Pending;
	Array m_results;
	PackedStringArray m_warnings;
};

VARIANT_ENUM_CAST(CesiumSampleHeightMostDetailedRequest::Status);

#endif // CESIUM_SAMPLE_HEIGHT_MOST_DETAILED_REQUEST_H
