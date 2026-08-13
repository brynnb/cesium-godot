// The binding data and shader-parameter lifecycle in this file are adapted
// from Cesium for Unreal's FRasterOverlayTile and AttachRasterTile /
// DetachRasterTile implementation (Apache-2.0).
// Copyright 2020-2026 CesiumGS, Inc. and Contributors.

#include "Runtime/Public/CesiumRasterOverlayBinding.h"

#include "Runtime/Public/CesiumLoadedTilePrimitive.h"

#if defined(CESIUM_GD_MODULE)
#include "scene/resources/material.h"
#elif defined(CESIUM_GD_EXT)
#include "godot_cpp/classes/shader_material.hpp"
#include "godot_cpp/variant/vector4.hpp"
#endif

namespace {
String sanitize_parameter_component(const String& source) {
	String result;
	for (int64_t index = 0; index < source.length(); ++index) {
		const char32_t character = source[index];
		const bool isAsciiLetter =
			(character >= U'a' && character <= U'z') ||
			(character >= U'A' && character <= U'Z');
		const bool isDigit = character >= U'0' && character <= U'9';
		if (isAsciiLetter || isDigit || character == U'_') {
			result += String::chr(character).to_lower();
		} else {
			result += "_";
		}
	}
	if (result.is_empty()) {
		return "overlay";
	}
	if (result[0] >= U'0' && result[0] <= U'9') {
		result = "overlay_" + result;
	}
	return result;
}
}

Ref<CesiumLoadedTilePrimitive> CesiumRasterOverlayBinding::get_tile_primitive() const {
	return this->m_tilePrimitive;
}

String CesiumRasterOverlayBinding::get_overlay_key() const {
	return this->m_overlayKey;
}

Ref<Texture2D> CesiumRasterOverlayBinding::get_texture() const {
	return this->m_texture;
}

Ref<Material> CesiumRasterOverlayBinding::get_material() const {
	return this->m_material;
}

int32_t CesiumRasterOverlayBinding::get_texture_coordinate_id() const {
	return this->m_textureCoordinateId;
}

int32_t CesiumRasterOverlayBinding::get_texture_coordinate_index() const {
	return this->m_textureCoordinateIndex;
}

Vector2 CesiumRasterOverlayBinding::get_translation() const {
	return this->m_translation;
}

Vector2 CesiumRasterOverlayBinding::get_scale() const {
	return this->m_scale;
}

Vector2 CesiumRasterOverlayBinding::transform_texture_coordinate(
	const Vector2& textureCoordinate
) const {
	return textureCoordinate * this->m_scale + this->m_translation;
}

bool CesiumRasterOverlayBinding::is_attached() const {
	return this->m_attached;
}

String CesiumRasterOverlayBinding::get_shader_parameter_prefix() const {
	return sanitize_parameter_component(this->m_overlayKey);
}

String CesiumRasterOverlayBinding::make_parameter_name(const String& suffix) const {
	return this->get_shader_parameter_prefix() + suffix;
}

bool CesiumRasterOverlayBinding::apply_to_material(const Ref<Material>& material) const {
	Ref<ShaderMaterial> shaderMaterial = material;
	if (shaderMaterial.is_null()) {
		return false;
	}

	shaderMaterial->set_shader_parameter(
		this->make_parameter_name("_texture"),
		this->m_texture
	);
	shaderMaterial->set_shader_parameter(
		this->make_parameter_name("_translation_scale"),
		Vector4(
			this->m_translation.x,
			this->m_translation.y,
			this->m_scale.x,
			this->m_scale.y
		)
	);
	shaderMaterial->set_shader_parameter(
		this->make_parameter_name("_texture_coordinate_index"),
		this->m_textureCoordinateIndex
	);
	if (this->get_shader_parameter_prefix() == "clipping") {
		shaderMaterial->set_shader_parameter("clipping_enabled", true);
	}
	return true;
}

bool CesiumRasterOverlayBinding::clear_from_material(const Ref<Material>& material) const {
	Ref<ShaderMaterial> shaderMaterial = material;
	if (shaderMaterial.is_null()) {
		return false;
	}

	shaderMaterial->set_shader_parameter(
		this->make_parameter_name("_texture"),
		Ref<Texture2D>()
	);
	shaderMaterial->set_shader_parameter(
		this->make_parameter_name("_translation_scale"),
		Vector4(0.0, 0.0, 1.0, 1.0)
	);
	shaderMaterial->set_shader_parameter(
		this->make_parameter_name("_texture_coordinate_index"),
		-1
	);
	if (this->get_shader_parameter_prefix() == "clipping") {
		shaderMaterial->set_shader_parameter("clipping_enabled", false);
	}
	return true;
}

void CesiumRasterOverlayBinding::initialize(
	const Ref<CesiumLoadedTilePrimitive>& tilePrimitive,
	const String& overlayKey,
	const Ref<Texture2D>& texture,
	int32_t textureCoordinateId,
	int32_t textureCoordinateIndex,
	const Vector2& translation,
	const Vector2& scale
) {
	this->m_tilePrimitive = tilePrimitive;
	this->m_overlayKey = overlayKey;
	this->m_texture = texture;
	this->m_textureCoordinateId = textureCoordinateId;
	this->m_textureCoordinateIndex = textureCoordinateIndex;
	this->m_translation = translation;
	this->m_scale = scale;
	this->m_attached = true;
}

void CesiumRasterOverlayBinding::mark_detached() {
	this->m_attached = false;
	this->m_texture.unref();
	this->m_material.unref();
}

void CesiumRasterOverlayBinding::set_material(const Ref<Material>& material) {
	this->m_material = material;
}

void CesiumRasterOverlayBinding::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_tile_primitive"), &CesiumRasterOverlayBinding::get_tile_primitive);
	ClassDB::bind_method(D_METHOD("get_overlay_key"), &CesiumRasterOverlayBinding::get_overlay_key);
	ClassDB::bind_method(D_METHOD("get_texture"), &CesiumRasterOverlayBinding::get_texture);
	ClassDB::bind_method(D_METHOD("get_material"), &CesiumRasterOverlayBinding::get_material);
	ClassDB::bind_method(D_METHOD("get_texture_coordinate_id"), &CesiumRasterOverlayBinding::get_texture_coordinate_id);
	ClassDB::bind_method(D_METHOD("get_texture_coordinate_index"), &CesiumRasterOverlayBinding::get_texture_coordinate_index);
	ClassDB::bind_method(D_METHOD("get_translation"), &CesiumRasterOverlayBinding::get_translation);
	ClassDB::bind_method(D_METHOD("get_scale"), &CesiumRasterOverlayBinding::get_scale);
	ClassDB::bind_method(D_METHOD("transform_texture_coordinate", "texture_coordinate"), &CesiumRasterOverlayBinding::transform_texture_coordinate);
	ClassDB::bind_method(D_METHOD("is_attached"), &CesiumRasterOverlayBinding::is_attached);
	ClassDB::bind_method(D_METHOD("get_shader_parameter_prefix"), &CesiumRasterOverlayBinding::get_shader_parameter_prefix);
	ClassDB::bind_method(D_METHOD("apply_to_material", "material"), &CesiumRasterOverlayBinding::apply_to_material);
	ClassDB::bind_method(D_METHOD("clear_from_material", "material"), &CesiumRasterOverlayBinding::clear_from_material);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "tile_primitive", PROPERTY_HINT_RESOURCE_TYPE, "CesiumLoadedTilePrimitive", PROPERTY_USAGE_NONE), "", "get_tile_primitive");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "overlay_key", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_overlay_key");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "texture", PROPERTY_HINT_RESOURCE_TYPE, "Texture2D", PROPERTY_USAGE_NONE), "", "get_texture");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "material", PROPERTY_HINT_RESOURCE_TYPE, "Material", PROPERTY_USAGE_NONE), "", "get_material");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "texture_coordinate_id", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_texture_coordinate_id");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "texture_coordinate_index", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_texture_coordinate_index");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "translation", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_translation");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "scale", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_scale");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "attached", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "is_attached");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "shader_parameter_prefix", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_shader_parameter_prefix");
}
