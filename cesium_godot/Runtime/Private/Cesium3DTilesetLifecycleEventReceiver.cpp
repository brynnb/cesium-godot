// The lifecycle shape and callback ordering in this file are adapted from
// Cesium for Unreal's ICesium3DTilesetLifecycleEventReceiver (Apache-2.0).
// Copyright 2020-2026 CesiumGS, Inc. and Contributors.

#include "Runtime/Public/Cesium3DTilesetLifecycleEventReceiver.h"

#include "Models/Cesium3DTile.h"
#include "Runtime/Public/CesiumLoadedTilePrimitive.h"
#include "Runtime/Public/CesiumRasterOverlayBinding.h"

namespace {
// Do not construct Godot values at shared-library load time. GDExtension API
// function pointers are not initialized until the entry point is invoked.
constexpr const char* CREATE_MATERIAL_HOOK = "_create_material";
constexpr const char* CUSTOMIZE_MATERIAL_HOOK = "_customize_material";
constexpr const char* PRIMITIVE_LOADED_HOOK = "_on_tile_mesh_primitive_loaded";
constexpr const char* RASTER_ATTACHED_HOOK = "_on_raster_overlay_attached";
constexpr const char* RASTER_DETACHING_HOOK = "_on_raster_overlay_detaching";
constexpr const char* TILE_LOADED_HOOK = "_on_tile_loaded";
constexpr const char* VISIBILITY_CHANGED_HOOK = "_on_tile_visibility_changed";
constexpr const char* TILE_UNLOADING_HOOK = "_on_tile_unloading";
}

Ref<Material> Cesium3DTilesetLifecycleEventReceiver::create_material(
	const Ref<CesiumLoadedTilePrimitive>& tilePrimitive,
	const Ref<Material>& defaultMaterial
) {
	if (!this->has_method(CREATE_MATERIAL_HOOK)) {
		return defaultMaterial;
	}

	Ref<Material> selectedMaterial = this->call(CREATE_MATERIAL_HOOK, tilePrimitive, defaultMaterial);
	if (selectedMaterial.is_null()) {
		WARN_PRINT("Cesium lifecycle receiver _create_material returned a non-Material value; using the default material");
		return defaultMaterial;
	}
	return selectedMaterial;
}

void Cesium3DTilesetLifecycleEventReceiver::customize_material(
	const Ref<CesiumLoadedTilePrimitive>& tilePrimitive,
	const Ref<Material>& material
) {
	if (this->has_method(CUSTOMIZE_MATERIAL_HOOK)) {
		this->call(CUSTOMIZE_MATERIAL_HOOK, tilePrimitive, material);
	}
}

void Cesium3DTilesetLifecycleEventReceiver::on_tile_mesh_primitive_loaded(
	const Ref<CesiumLoadedTilePrimitive>& tilePrimitive
) {
	if (this->has_method(PRIMITIVE_LOADED_HOOK)) {
		this->call(PRIMITIVE_LOADED_HOOK, tilePrimitive);
	}
	this->emit_signal("tile_mesh_primitive_loaded", tilePrimitive);
}

void Cesium3DTilesetLifecycleEventReceiver::on_raster_overlay_attached(
	const Ref<CesiumRasterOverlayBinding>& binding
) {
	if (this->has_method(RASTER_ATTACHED_HOOK)) {
		this->call(RASTER_ATTACHED_HOOK, binding);
	}
	this->emit_signal("raster_overlay_attached", binding);
}

void Cesium3DTilesetLifecycleEventReceiver::on_raster_overlay_detaching(
	const Ref<CesiumRasterOverlayBinding>& binding
) {
	if (this->has_method(RASTER_DETACHING_HOOK)) {
		this->call(RASTER_DETACHING_HOOK, binding);
	}
	this->emit_signal("raster_overlay_detaching", binding);
}

void Cesium3DTilesetLifecycleEventReceiver::on_tile_loaded(Cesium3DTile* tile) {
	if (this->has_method(TILE_LOADED_HOOK)) {
		this->call(TILE_LOADED_HOOK, tile);
	}
	this->emit_signal("tile_loaded", tile);
}

void Cesium3DTilesetLifecycleEventReceiver::on_tile_visibility_changed(
	Cesium3DTile* tile,
	bool visible
) {
	if (this->has_method(VISIBILITY_CHANGED_HOOK)) {
		this->call(VISIBILITY_CHANGED_HOOK, tile, visible);
	}
	this->emit_signal("tile_visibility_changed", tile, visible);
}

void Cesium3DTilesetLifecycleEventReceiver::on_tile_unloading(Cesium3DTile* tile) {
	if (this->has_method(TILE_UNLOADING_HOOK)) {
		this->call(TILE_UNLOADING_HOOK, tile);
	}
	this->emit_signal("tile_unloading", tile);
}

void Cesium3DTilesetLifecycleEventReceiver::_bind_methods() {
	ADD_SIGNAL(MethodInfo(
		"tile_mesh_primitive_loaded",
		PropertyInfo(Variant::OBJECT, "tile_primitive", PROPERTY_HINT_RESOURCE_TYPE, "CesiumLoadedTilePrimitive")
	));
	ADD_SIGNAL(MethodInfo(
		"raster_overlay_attached",
		PropertyInfo(Variant::OBJECT, "binding", PROPERTY_HINT_RESOURCE_TYPE, "CesiumRasterOverlayBinding")
	));
	ADD_SIGNAL(MethodInfo(
		"raster_overlay_detaching",
		PropertyInfo(Variant::OBJECT, "binding", PROPERTY_HINT_RESOURCE_TYPE, "CesiumRasterOverlayBinding")
	));
	ADD_SIGNAL(MethodInfo(
		"tile_loaded",
		PropertyInfo(Variant::OBJECT, "tile", PROPERTY_HINT_NODE_TYPE, "Cesium3DTile")
	));
	ADD_SIGNAL(MethodInfo(
		"tile_visibility_changed",
		PropertyInfo(Variant::OBJECT, "tile", PROPERTY_HINT_NODE_TYPE, "Cesium3DTile"),
		PropertyInfo(Variant::BOOL, "visible")
	));
	ADD_SIGNAL(MethodInfo(
		"tile_unloading",
		PropertyInfo(Variant::OBJECT, "tile", PROPERTY_HINT_NODE_TYPE, "Cesium3DTile")
	));
}
