// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Adapted for Godot from Cesium for Unreal under the Apache-2.0 license.

#ifndef CESIUM_PROPERTY_TEXTURE_H
#define CESIUM_PROPERTY_TEXTURE_H

/**
 * Godot adaptation reviewed against Cesium for Unreal v2.29.0:
 * Source/CesiumRuntime/Public/CesiumPropertyTexture.h.
 */

#include "Runtime/Public/Metadata/CesiumPropertyTextureProperty.h"

#if defined(CESIUM_GD_MODULE)
#include "core/object/ref_counted.h"
#include "core/variant/array.h"
#include "core/variant/dictionary.h"
#elif defined(CESIUM_GD_EXT)
#include "godot_cpp/classes/ref.hpp"
#include "godot_cpp/classes/ref_counted.hpp"
#include "godot_cpp/variant/array.hpp"
#include "godot_cpp/variant/dictionary.hpp"
using namespace godot;
#endif

#include <memory>

struct CesiumPropertyTextureSnapshot;

class CesiumPropertyTexture : public RefCounted {
	GDCLASS(CesiumPropertyTexture, RefCounted)

public:
	enum Status {
		Valid = 0,
		ErrorInvalidPropertyTexture = 1,
		ErrorInvalidPropertyTextureClass = 2,
	};

	void initialize(
		const std::shared_ptr<const CesiumPropertyTextureSnapshot>& snapshot
	);
	void add_property(const Ref<CesiumPropertyTextureProperty>& property);
	int32_t get_status() const;
	String get_status_name() const;
	bool is_valid() const;
	String get_texture_name() const;
	String get_class_name() const;
	Array get_properties() const;
	Array get_property_names() const;
	Ref<CesiumPropertyTextureProperty> find_property(
		const String& propertyId
	) const;
	Dictionary get_metadata_values_for_uv(
		const Vector2& textureCoordinates
	) const;

protected:
	static void _bind_methods();

private:
	std::shared_ptr<const CesiumPropertyTextureSnapshot> m_snapshot;
	Array m_properties;
};

VARIANT_ENUM_CAST(CesiumPropertyTexture::Status);

#endif // CESIUM_PROPERTY_TEXTURE_H
