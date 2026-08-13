// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Adapted for Godot from Cesium for Unreal under the Apache-2.0 license.

#include "Runtime/Public/TileSelection/CesiumTileExclusionContext.h"

const String& CesiumTileExclusionContext::get_tile_id() const {
	return this->m_tileId;
}

int32_t CesiumTileExclusionContext::get_depth() const {
	return this->m_depth;
}

int32_t CesiumTileExclusionContext::get_child_count() const {
	return this->m_childCount;
}

double CesiumTileExclusionContext::get_geometric_error() const {
	return this->m_geometricError;
}

int32_t CesiumTileExclusionContext::get_refine_mode() const {
	return this->m_refineMode;
}

String CesiumTileExclusionContext::get_refine_mode_name() const {
	return this->m_refineMode == 0 ? "add" : "replace";
}

Ref<CesiumBoundingVolume> CesiumTileExclusionContext::get_tile_bounds() const {
	return this->m_tileBounds;
}

PackedFloat64Array
CesiumTileExclusionContext::get_tile_transform_components() const {
	return this->m_tileTransformComponents;
}

Dictionary CesiumTileExclusionContext::to_dictionary() const {
	Dictionary result;
	result["tile_id"] = this->m_tileId;
	result["depth"] = this->m_depth;
	result["child_count"] = this->m_childCount;
	result["geometric_error"] = this->m_geometricError;
	result["refine_mode"] = this->m_refineMode;
	result["refine_mode_name"] = this->get_refine_mode_name();
	result["tile_bounds"] = this->m_tileBounds.is_valid()
		? this->m_tileBounds->create_owned_copy()
		: Ref<CesiumBoundingVolume>();
	result["tile_transform_components"] = this->m_tileTransformComponents;
	return result;
}

void CesiumTileExclusionContext::initialize(
	const String& tileId,
	int32_t depth,
	int32_t childCount,
	double geometricError,
	int32_t refineMode,
	const Ref<CesiumBoundingVolume>& tileBounds,
	const PackedFloat64Array& tileTransformComponents
) {
	this->m_tileId = tileId;
	this->m_depth = depth;
	this->m_childCount = childCount;
	this->m_geometricError = geometricError;
	this->m_refineMode = refineMode;
	this->m_tileBounds = tileBounds;
	this->m_tileTransformComponents = tileTransformComponents;
}

void CesiumTileExclusionContext::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_tile_id"), &CesiumTileExclusionContext::get_tile_id);
	ClassDB::bind_method(D_METHOD("get_depth"), &CesiumTileExclusionContext::get_depth);
	ClassDB::bind_method(D_METHOD("get_child_count"), &CesiumTileExclusionContext::get_child_count);
	ClassDB::bind_method(D_METHOD("get_geometric_error"), &CesiumTileExclusionContext::get_geometric_error);
	ClassDB::bind_method(D_METHOD("get_refine_mode"), &CesiumTileExclusionContext::get_refine_mode);
	ClassDB::bind_method(D_METHOD("get_refine_mode_name"), &CesiumTileExclusionContext::get_refine_mode_name);
	ClassDB::bind_method(D_METHOD("get_tile_bounds"), &CesiumTileExclusionContext::get_tile_bounds);
	ClassDB::bind_method(D_METHOD("get_tile_transform_components"), &CesiumTileExclusionContext::get_tile_transform_components);
	ClassDB::bind_method(D_METHOD("to_dictionary"), &CesiumTileExclusionContext::to_dictionary);

#define CESIUM_READ_ONLY_PROPERTY(type, name) \
	ADD_PROPERTY(PropertyInfo(type, #name, PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_" #name)
	CESIUM_READ_ONLY_PROPERTY(Variant::STRING, tile_id);
	CESIUM_READ_ONLY_PROPERTY(Variant::INT, depth);
	CESIUM_READ_ONLY_PROPERTY(Variant::INT, child_count);
	CESIUM_READ_ONLY_PROPERTY(Variant::FLOAT, geometric_error);
	CESIUM_READ_ONLY_PROPERTY(Variant::INT, refine_mode);
	CESIUM_READ_ONLY_PROPERTY(Variant::STRING, refine_mode_name);
	CESIUM_READ_ONLY_PROPERTY(Variant::OBJECT, tile_bounds);
	CESIUM_READ_ONLY_PROPERTY(Variant::PACKED_FLOAT64_ARRAY, tile_transform_components);
#undef CESIUM_READ_ONLY_PROPERTY
}
