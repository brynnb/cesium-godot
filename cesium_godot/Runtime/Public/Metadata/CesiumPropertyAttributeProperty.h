// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Adapted for Godot from Cesium for Unreal under the Apache-2.0 license.

#ifndef CESIUM_PROPERTY_ATTRIBUTE_PROPERTY_H
#define CESIUM_PROPERTY_ATTRIBUTE_PROPERTY_H

/**
 * Godot adaptation reviewed against Cesium for Unreal v2.29.0:
 * Source/CesiumRuntime/Public/CesiumPropertyAttributeProperty.h.
 */

#include "Runtime/Public/Metadata/CesiumMetadataProperty.h"

/** Per-vertex EXT_structural_metadata with retained raw/transformed values. */
class CesiumPropertyAttributeProperty : public CesiumMetadataProperty {
	GDCLASS(CesiumPropertyAttributeProperty, CesiumMetadataProperty)

protected:
	static void _bind_methods();
};

#endif // CESIUM_PROPERTY_ATTRIBUTE_PROPERTY_H
