
#include "CesiumGDAssetBuilder.h"
#include "Godot/Nodes/CesiumGeoreference.h"
#include "Utils/AssetManipulation.h"
#include "godot_cpp/core/class_db.hpp"


void CesiumGDAssetBuilder::instantiate_tileset(int32_t assetId, const String& assetType, const String& assetName) {
	//Create a new Globe
	CesiumGodot::AssetManipulation::instantiate_tileset(this, assetId, assetType, assetName);
}
	

void CesiumGDAssetBuilder::instantiate_dynamic_cam() {
  CesiumGodot::AssetManipulation::instantiate_dynamic_cam(this);
}

void CesiumGDAssetBuilder::instantiate_orbit_cam() {
  CesiumGodot::AssetManipulation::instantiate_orbit_cam(this);
}

Variant CesiumGDAssetBuilder::get_georeference_camera_script() const {
	return this->m_georeferenceCameraScript;
}

void CesiumGDAssetBuilder::set_georeference_camera_script(Variant cameraScript) {
	this->m_georeferenceCameraScript = cameraScript;
}

CesiumGeoreference* CesiumGDAssetBuilder::find_or_create_globe() {
	return CesiumGodot::AssetManipulation::find_or_create_globe(this);
}

Array CesiumGDAssetBuilder::find_all_tilesets() {
	return CesiumGodot::AssetManipulation::find_all_tilesets(this);
}

void CesiumGDAssetBuilder::_bind_methods() {
	ClassDB::bind_method(D_METHOD("instantiate_dynamic_cam"), &CesiumGDAssetBuilder::instantiate_dynamic_cam);
	ClassDB::bind_method(D_METHOD("instantiate_orbit_cam"), &CesiumGDAssetBuilder::instantiate_orbit_cam);
	ClassDB::bind_method(D_METHOD("instantiate_tileset", "assetId", "assetType"), &CesiumGDAssetBuilder::instantiate_tileset);
	ClassDB::bind_method(D_METHOD("find_or_create_globe"), &CesiumGDAssetBuilder::find_or_create_globe);	

	ClassDB::bind_method(D_METHOD("get_georeference_camera_script"), &CesiumGDAssetBuilder::get_georeference_camera_script);
	ClassDB::bind_method(D_METHOD("set_georeference_camera_script", "script"), &CesiumGDAssetBuilder::set_georeference_camera_script);

	ClassDB::bind_method(D_METHOD("find_all_tilesets"), &CesiumGDAssetBuilder::find_all_tilesets);
	
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "georef_camera_script"), "set_georeference_camera_script", "get_georeference_camera_script");
}
