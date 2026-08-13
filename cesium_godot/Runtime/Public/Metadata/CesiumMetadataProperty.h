#ifndef CESIUM_METADATA_PROPERTY_H
#define CESIUM_METADATA_PROPERTY_H

/**
 * Godot adaptation reviewed against Cesium for Unreal v2.29.0:
 * Source/CesiumRuntime/Public/CesiumPropertyTableProperty.h.
 *
 * Godot exposes immutable-by-API Resources instead of Unreal Blueprints and
 * wrapper structs. Values are owned snapshots, never borrowed Native views.
 */

#if defined(CESIUM_GD_MODULE)
#include "core/object/ref_counted.h"
#include "core/variant/array.h"
#include "core/variant/dictionary.h"
#elif defined(CESIUM_GD_EXT)
#include "godot_cpp/classes/ref.hpp"
#include "godot_cpp/classes/ref_counted.hpp"
#include "godot_cpp/variant/array.hpp"
#include "godot_cpp/variant/dictionary.hpp"
#include "godot_cpp/variant/string.hpp"
#include "godot_cpp/variant/variant.hpp"
using namespace godot;
#endif

#include <memory>

struct CesiumMetadataPropertySnapshot;

/** A lifetime-safe property from an EXT_structural_metadata property table. */
class CesiumMetadataProperty : public RefCounted {
	GDCLASS(CesiumMetadataProperty, RefCounted)

public:
	enum Status {
		Valid = 0,
		EmptyPropertyWithDefault = 1,
		ErrorInvalidProperty = 2,
		ErrorInvalidPropertyData = 3,
	};

	void initialize(
		const std::shared_ptr<const CesiumMetadataPropertySnapshot>& snapshot
	);

	int32_t get_status() const;
	String get_status_name() const;
	bool is_valid() const;
	String get_property_id() const;
	String get_value_type() const;
	String get_component_type() const;
	String get_enum_type() const;
	bool get_is_array() const;
	bool get_normalized() const;
	int64_t get_fixed_array_count() const;
	int64_t get_size() const;
	Variant get_value(int64_t featureId) const;
	Variant get_raw_value(int64_t featureId) const;
	Array get_values() const;
	Array get_raw_values() const;
	Dictionary get_value_transform_details() const;

protected:
	static void _bind_methods();

private:
	std::shared_ptr<const CesiumMetadataPropertySnapshot> m_snapshot;
};

VARIANT_ENUM_CAST(CesiumMetadataProperty::Status);

#endif // CESIUM_METADATA_PROPERTY_H
