#ifndef CESIUM_RASTER_OVERLAY_BINDING_H
#define CESIUM_RASTER_OVERLAY_BINDING_H

#if defined(CESIUM_GD_MODULE)
#include "core/object/ref_counted.h"
#include "scene/resources/material.h"
#include "scene/resources/texture.h"
#elif defined(CESIUM_GD_EXT)
#include "godot_cpp/classes/material.hpp"
#include "godot_cpp/classes/ref.hpp"
#include "godot_cpp/classes/ref_counted.hpp"
#include "godot_cpp/classes/texture2d.hpp"
#include "godot_cpp/variant/string.hpp"
#include "godot_cpp/variant/vector2.hpp"
using namespace godot;
#endif

class CesiumLoadedTilePrimitive;

/**
 * Describes one Cesium raster tile attached to one loaded glTF primitive.
 *
 * Godot counterpart of Cesium for Unreal v2.29.0:
 * Source/CesiumRuntime/Private/CesiumGltfComponent.h (FRasterOverlayTile)
 * Source/CesiumRuntime/Private/CesiumGltfComponent.cpp
 * (AttachRasterTile / DetachRasterTile)
 *
 * The binding owns Godot references only. It never retains cesium-native tile
 * pointers, so scripts may safely retain it after a geometry tile unloads.
 */
class CesiumRasterOverlayBinding : public RefCounted {
	GDCLASS(CesiumRasterOverlayBinding, RefCounted)

public:
	Ref<CesiumLoadedTilePrimitive> get_tile_primitive() const;
	String get_overlay_key() const;
	Ref<Texture2D> get_texture() const;
	Ref<Material> get_material() const;
	int32_t get_texture_coordinate_id() const;
	int32_t get_texture_coordinate_index() const;
	Vector2 get_translation() const;
	Vector2 get_scale() const;
	Vector2 transform_texture_coordinate(const Vector2& textureCoordinate) const;
	bool is_attached() const;
	String get_shader_parameter_prefix() const;

	/**
	 * Applies the documented Cesium overlay parameters to a ShaderMaterial.
	 * StandardMaterial3D fallback application is owned by Cesium3DTile because
	 * it must arbitrate multiple overlays and restore the original material.
	 */
	bool apply_to_material(const Ref<Material>& material) const;
	bool clear_from_material(const Ref<Material>& material) const;

	// Renderer integration only.
	void initialize(
		const Ref<CesiumLoadedTilePrimitive>& tilePrimitive,
		const String& overlayKey,
		const Ref<Texture2D>& texture,
		int32_t textureCoordinateId,
		int32_t textureCoordinateIndex,
		const Vector2& translation,
		const Vector2& scale
	);
	void mark_detached();
	void set_material(const Ref<Material>& material);

protected:
	static void _bind_methods();

private:
	String make_parameter_name(const String& suffix) const;

	Ref<CesiumLoadedTilePrimitive> m_tilePrimitive;
	String m_overlayKey;
	Ref<Texture2D> m_texture;
	Ref<Material> m_material;
	int32_t m_textureCoordinateId = -1;
	int32_t m_textureCoordinateIndex = -1;
	Vector2 m_translation;
	Vector2 m_scale = Vector2(1.0, 1.0);
	bool m_attached = false;
};

#endif // CESIUM_RASTER_OVERLAY_BINDING_H
