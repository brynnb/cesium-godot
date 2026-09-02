// The lifecycle shape and callback ordering in this file are adapted from
// Cesium for Unreal's ICesium3DTilesetLifecycleEventReceiver (Apache-2.0).
// Copyright 2020-2026 CesiumGS, Inc. and Contributors.

#include "Runtime/Public/Cesium3DTilesetLifecycleEventReceiver.h"

#include "Models/Cesium3DTile.h"
#include "Runtime/Public/CesiumLoadedTilePrimitive.h"
#include "Runtime/Public/CesiumRasterOverlayBinding.h"

void Cesium3DTilesetLifecycleEventReceiver::set_material_selector(
	const Callable& selector
) {
	this->m_materialSelector = selector;
}

Callable Cesium3DTilesetLifecycleEventReceiver::get_material_selector() const {
	return this->m_materialSelector;
}

Ref<Material> Cesium3DTilesetLifecycleEventReceiver::create_material(
	const Ref<CesiumLoadedTilePrimitive>& tilePrimitive,
	const Ref<Material>& defaultMaterial
) {
	if (!this->m_materialSelector.is_valid()) {
		return defaultMaterial;
	}

	Array arguments;
	arguments.push_back(tilePrimitive);
	arguments.push_back(defaultMaterial);
	const Variant result = this->m_materialSelector.callv(arguments);
	Ref<Material> selectedMaterial;
	if (result.get_type() == Variant::OBJECT) {
		selectedMaterial = Ref<Material>(
			Object::cast_to<Material>(static_cast<Object*>(result))
		);
	}
	if (selectedMaterial.is_null()) {
		WARN_PRINT("Cesium lifecycle material_selector returned a non-Material value; using the default material");
		return defaultMaterial;
	}
	return selectedMaterial;
}

void Cesium3DTilesetLifecycleEventReceiver::customize_material(
	const Ref<CesiumLoadedTilePrimitive>& tilePrimitive,
	const Ref<Material>& material
) {
	this->emit_signal("material_customizing", tilePrimitive, material);
}

void Cesium3DTilesetLifecycleEventReceiver::on_tile_mesh_primitive_loaded(
	const Ref<CesiumLoadedTilePrimitive>& tilePrimitive
) {
	this->emit_signal("tile_mesh_primitive_loaded", tilePrimitive);
}

void Cesium3DTilesetLifecycleEventReceiver::on_raster_overlay_attached(
	const Ref<CesiumRasterOverlayBinding>& binding
) {
	this->emit_signal("raster_overlay_attached", binding);
}

void Cesium3DTilesetLifecycleEventReceiver::on_raster_overlay_detaching(
	const Ref<CesiumRasterOverlayBinding>& binding
) {
	this->emit_signal("raster_overlay_detaching", binding);
}

void Cesium3DTilesetLifecycleEventReceiver::on_tile_loaded(Cesium3DTile* tile) {
	this->emit_signal("tile_loaded", tile);
}

void Cesium3DTilesetLifecycleEventReceiver::on_tile_visibility_changed(
	Cesium3DTile* tile,
	bool visible
) {
	this->emit_signal("tile_visibility_changed", tile, visible);
}

void Cesium3DTilesetLifecycleEventReceiver::on_tile_unloading(Cesium3DTile* tile) {
	this->emit_signal("tile_unloading", tile);
}

void Cesium3DTilesetLifecycleEventReceiver::_bind_methods() {
	ClassDB::bind_method(
		D_METHOD("set_material_selector", "selector"),
		&Cesium3DTilesetLifecycleEventReceiver::set_material_selector
	);
	ClassDB::bind_method(
		D_METHOD("get_material_selector"),
		&Cesium3DTilesetLifecycleEventReceiver::get_material_selector
	);
	ADD_PROPERTY(
		PropertyInfo(Variant::CALLABLE, "material_selector"),
		"set_material_selector",
		"get_material_selector"
	);
	ADD_SIGNAL(MethodInfo(
		"material_customizing",
		PropertyInfo(Variant::OBJECT, "tile_primitive", PROPERTY_HINT_RESOURCE_TYPE, "CesiumLoadedTilePrimitive"),
		PropertyInfo(
			Variant::OBJECT,
			"material",
			PROPERTY_HINT_RESOURCE_TYPE,
			"Material"
		)
	));
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
