// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Adapted for Godot from Cesium for Unreal under the Apache-2.0 license.

#ifndef CESIUM_METADATA_ENUM_H
#define CESIUM_METADATA_ENUM_H

/**
 * Godot adaptation reviewed against Cesium for Unreal v2.29.0:
 * Source/CesiumRuntime/Public/Private CesiumMetadataEnum.*.
 */

#if defined(CESIUM_GD_MODULE)
#include "core/object/ref_counted.h"
#include "core/variant/array.h"
#include "core/variant/dictionary.h"
#include "core/variant/string.h"
#elif defined(CESIUM_GD_EXT)
#include "godot_cpp/classes/ref_counted.hpp"
#include "godot_cpp/variant/array.hpp"
#include "godot_cpp/variant/dictionary.hpp"
#include "godot_cpp/variant/string.hpp"
using namespace godot;
#endif

#include <memory>

struct CesiumMetadataEnumSnapshot;

class CesiumMetadataEnum : public RefCounted {
	GDCLASS(CesiumMetadataEnum, RefCounted)

public:
	void initialize(
		const std::shared_ptr<const CesiumMetadataEnumSnapshot>& snapshot
	);

	String get_enum_id() const;
	String get_enum_name() const;
	String get_description() const;
	String get_value_type() const;
	int32_t get_value_count() const;
	Array get_values() const;
	bool has_value(int64_t value) const;
	String get_name_for_value(int64_t value) const;
	Dictionary get_value_details(int64_t value) const;

protected:
	static void _bind_methods();

private:
	std::shared_ptr<const CesiumMetadataEnumSnapshot> m_snapshot;
};

#endif // CESIUM_METADATA_ENUM_H
