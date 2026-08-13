// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Adapted for Godot from Cesium for Unreal under the Apache-2.0 license.

#include "Runtime/Public/RasterOverlays/CesiumPolygonRasterOverlay.h"

#include "Godot/Nodes/CesiumGDTileset.h"
#include "Runtime/Public/Georeference/CesiumCartographicPolygon.h"

#include <Cesium3DTilesSelection/RasterizedPolygonsTileExcluder.h>
#include <CesiumGeospatial/Projection.h>
#include <CesiumRasterOverlays/RasterizedPolygonsOverlay.h>
#include <CesiumUtility/IntrusivePointer.h>

#include <vector>

CesiumPolygonRasterOverlay::CesiumPolygonRasterOverlay() {
	this->set_material_key("clipping");
}

CesiumPolygonRasterOverlay::~CesiumPolygonRasterOverlay() {
	this->disconnect_polygon_changes();
}

void CesiumPolygonRasterOverlay::set_polygons(const Array& polygons) {
	Array validated;
	for (int32_t index = 0; index < polygons.size(); ++index) {
		Ref<CesiumCartographicPolygon> polygon = polygons[index];
		if (polygon.is_null()) {
			continue;
		}
		validated.push_back(polygon);
	}
	if (this->m_polygons == validated) {
		return;
	}
	this->disconnect_polygon_changes();
	this->m_polygons = validated;
	this->connect_polygon_changes();
	this->refresh();
}

Array CesiumPolygonRasterOverlay::get_polygons() const {
	return this->m_polygons.duplicate();
}

void CesiumPolygonRasterOverlay::set_invert_selection(bool invert) {
	if (this->m_invertSelection == invert) {
		return;
	}
	this->m_invertSelection = invert;
	this->refresh();
}

bool CesiumPolygonRasterOverlay::get_invert_selection() const {
	return this->m_invertSelection;
}

void CesiumPolygonRasterOverlay::set_exclude_selected_tiles(bool exclude) {
	if (this->m_excludeSelectedTiles == exclude) {
		return;
	}
	this->m_excludeSelectedTiles = exclude;
	this->refresh();
}

bool CesiumPolygonRasterOverlay::get_exclude_selected_tiles() const {
	return this->m_excludeSelectedTiles;
}

int32_t CesiumPolygonRasterOverlay::get_valid_polygon_count() const {
	int32_t result = 0;
	for (int32_t index = 0; index < this->m_polygons.size(); ++index) {
		Ref<CesiumCartographicPolygon> polygon = this->m_polygons[index];
		if (polygon.is_valid() && polygon->is_valid()) {
			++result;
		}
	}
	return result;
}

void CesiumPolygonRasterOverlay::_on_polygon_changed() {
	this->refresh();
}

std::unique_ptr<CesiumRasterOverlays::RasterOverlay>
CesiumPolygonRasterOverlay::create_overlay(
	const CesiumRasterOverlays::RasterOverlayOptions& options
) {
	std::vector<CesiumGeospatial::CartographicPolygon> nativePolygons;
	nativePolygons.reserve(static_cast<size_t>(this->m_polygons.size()));
	for (int32_t index = 0; index < this->m_polygons.size(); ++index) {
		Ref<CesiumCartographicPolygon> polygon = this->m_polygons[index];
		if (polygon.is_valid() && polygon->is_valid()) {
			nativePolygons.emplace_back(polygon->create_native_polygon());
		}
	}
	const CesiumGeospatial::Ellipsoid ellipsoid = options.ellipsoid;
	return std::make_unique<CesiumRasterOverlays::RasterizedPolygonsOverlay>(
		this->get_material_key().utf8().get_data(),
		nativePolygons,
		this->m_invertSelection,
		ellipsoid,
		CesiumGeospatial::GeographicProjection(ellipsoid),
		options
	);
}

String CesiumPolygonRasterOverlay::provider_type() const {
	return "rasterized_polygons";
}

void CesiumPolygonRasterOverlay::append_provider_configuration(
	Dictionary& result
) const {
	result["polygon_count"] = this->m_polygons.size();
	result["valid_polygon_count"] = this->get_valid_polygon_count();
	result["invert_selection"] = this->m_invertSelection;
	result["exclude_selected_tiles"] = this->m_excludeSelectedTiles;
	result["tile_excluder_active"] = this->m_polygonExcluder != nullptr;
}

void CesiumPolygonRasterOverlay::on_added_to_tileset(
	Cesium3DTileset* tileset,
	CesiumRasterOverlays::RasterOverlay* overlay
) {
	if (!this->m_excludeSelectedTiles || tileset == nullptr || overlay == nullptr) {
		return;
	}
	auto* polygonOverlay = static_cast<
		CesiumRasterOverlays::RasterizedPolygonsOverlay*
	>(overlay);
	CesiumUtility::IntrusivePointer<
		const CesiumRasterOverlays::RasterizedPolygonsOverlay
	> retainedOverlay(polygonOverlay);
	this->m_polygonExcluder = std::make_shared<
		Cesium3DTilesSelection::RasterizedPolygonsTileExcluder
	>(retainedOverlay);
	tileset->add_tile_excluder(this->m_polygonExcluder);
}

void CesiumPolygonRasterOverlay::on_removing_from_tileset(
	Cesium3DTileset* tileset,
	CesiumRasterOverlays::RasterOverlay* overlay
) {
	(void)overlay;
	if (tileset != nullptr && this->m_polygonExcluder != nullptr) {
		tileset->remove_tile_excluder(this->m_polygonExcluder);
	}
	this->m_polygonExcluder.reset();
}

void CesiumPolygonRasterOverlay::disconnect_polygon_changes() {
	const Callable callback(this, "_on_polygon_changed");
	for (int32_t index = 0; index < this->m_polygons.size(); ++index) {
		Ref<CesiumCartographicPolygon> polygon = this->m_polygons[index];
		if (polygon.is_valid() && polygon->is_connected("changed", callback)) {
			polygon->disconnect("changed", callback);
		}
	}
}

void CesiumPolygonRasterOverlay::connect_polygon_changes() {
	const Callable callback(this, "_on_polygon_changed");
	for (int32_t index = 0; index < this->m_polygons.size(); ++index) {
		Ref<CesiumCartographicPolygon> polygon = this->m_polygons[index];
		if (polygon.is_valid() && !polygon->is_connected("changed", callback)) {
			polygon->connect("changed", callback);
		}
	}
}

void CesiumPolygonRasterOverlay::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_polygons", "polygons"), &CesiumPolygonRasterOverlay::set_polygons);
	ClassDB::bind_method(D_METHOD("get_polygons"), &CesiumPolygonRasterOverlay::get_polygons);
	ClassDB::bind_method(D_METHOD("set_invert_selection", "invert"), &CesiumPolygonRasterOverlay::set_invert_selection);
	ClassDB::bind_method(D_METHOD("get_invert_selection"), &CesiumPolygonRasterOverlay::get_invert_selection);
	ClassDB::bind_method(D_METHOD("set_exclude_selected_tiles", "exclude"), &CesiumPolygonRasterOverlay::set_exclude_selected_tiles);
	ClassDB::bind_method(D_METHOD("get_exclude_selected_tiles"), &CesiumPolygonRasterOverlay::get_exclude_selected_tiles);
	ClassDB::bind_method(D_METHOD("get_valid_polygon_count"), &CesiumPolygonRasterOverlay::get_valid_polygon_count);
	ClassDB::bind_method(D_METHOD("_on_polygon_changed"), &CesiumPolygonRasterOverlay::_on_polygon_changed);
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "polygons", PROPERTY_HINT_ARRAY_TYPE, "CesiumCartographicPolygon"), "set_polygons", "get_polygons");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "invert_selection"), "set_invert_selection", "get_invert_selection");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "exclude_selected_tiles"), "set_exclude_selected_tiles", "get_exclude_selected_tiles");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "valid_polygon_count", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_valid_polygon_count");
}
