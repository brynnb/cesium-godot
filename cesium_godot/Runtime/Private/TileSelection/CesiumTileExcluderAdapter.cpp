// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Adapted for Godot from Cesium for Unreal under the Apache-2.0 license.

#include "Runtime/Private/TileSelection/CesiumTileExcluderAdapter.h"

#include "Godot/Nodes/CesiumTileExcluder.h"
#include "Runtime/Private/Bounds/CesiumBoundingVolumeSnapshot.h"
#include "Runtime/Public/Bounds/CesiumBoundingVolume.h"
#include "Runtime/Public/TileSelection/CesiumTileExclusionContext.h"

#include <Cesium3DTilesSelection/Tile.h>
#include <Cesium3DTilesSelection/TileID.h>

#include <glm/mat4x4.hpp>

namespace {
CesiumTileExcluder* resolve_excluder(const ObjectID& objectId) {
	return Object::cast_to<CesiumTileExcluder>(ObjectDB::get_instance(objectId));
}

void pack_matrix(
	const glm::dmat4& matrix,
	PackedFloat64Array* result
) {
	int32_t output = 0;
	for (int32_t column = 0; column < 4; ++column) {
		for (int32_t row = 0; row < 4; ++row) {
			result->set(output++, matrix[column][row]);
		}
	}
}

int32_t tile_depth(const Cesium3DTilesSelection::Tile& tile) {
	int32_t result = 0;
	const Cesium3DTilesSelection::Tile* parent = tile.getParent();
	while (parent != nullptr) {
		++result;
		parent = parent->getParent();
	}
	return result;
}
} // namespace

CesiumTileExcluderAdapter::CesiumTileExcluderAdapter(const ObjectID& excluder)
	: m_excluder(excluder) {
	this->m_context.instantiate();
	this->m_bounds.instantiate();
	this->m_tileTransformComponents.resize(16);
}

void CesiumTileExcluderAdapter::startNewFrame() noexcept {
	CesiumTileExcluder* excluder = resolve_excluder(this->m_excluder);
	if (excluder != nullptr) {
		excluder->begin_native_frame();
	}
}

bool CesiumTileExcluderAdapter::shouldExclude(
	const Cesium3DTilesSelection::Tile& tile
) const noexcept {
	CesiumTileExcluder* excluder = resolve_excluder(this->m_excluder);
	if (excluder == nullptr || this->m_context.is_null()) {
		return false;
	}

	try {
		if (this->m_boundsSnapshot == nullptr) {
			this->m_boundsSnapshot = create_cesium_bounding_volume_snapshot(
				tile.getBoundingVolume()
			);
			this->m_bounds->initialize(this->m_boundsSnapshot);
		} else {
			this->m_boundsSnapshot->volume = tile.getBoundingVolume();
		}
		pack_matrix(tile.getTransform(), &this->m_tileTransformComponents);
		this->m_context->initialize(
			String::utf8(
				Cesium3DTilesSelection::TileIdUtilities::createTileIdString(
					tile.getTileID()
				).c_str()
			),
			tile_depth(tile),
			static_cast<int32_t>(tile.getChildren().size()),
			tile.getGeometricError(),
			static_cast<int32_t>(tile.getRefine()),
			this->m_bounds,
			this->m_tileTransformComponents
		);
		return excluder->evaluate_native_context(this->m_context);
	} catch (...) {
		return false;
	}
}
