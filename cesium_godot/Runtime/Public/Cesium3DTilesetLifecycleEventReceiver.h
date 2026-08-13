#ifndef CESIUM_3D_TILESET_LIFECYCLE_EVENT_RECEIVER_H
#define CESIUM_3D_TILESET_LIFECYCLE_EVENT_RECEIVER_H

#if defined(CESIUM_GD_MODULE)
#include "scene/main/node.h"
#include "scene/resources/material.h"
#elif defined(CESIUM_GD_EXT)
#include "godot_cpp/classes/material.hpp"
#include "godot_cpp/classes/node.hpp"
#include "godot_cpp/classes/ref.hpp"
using namespace godot;
#endif

class Cesium3DTile;
class CesiumLoadedTilePrimitive;
class CesiumRasterOverlayBinding;

/**
 * Receives main-thread events for Cesium tile resource lifecycles.
 *
 * Godot counterpart of Cesium for Unreal v2.29.0:
 * Source/CesiumRuntime/Public/Cesium3DTilesetLifecycleEventReceiver.h
 *
 * A GDScript subclass may implement any of the optional underscored hooks
 * documented in docs/TILE_LIFECYCLE.md.
 */
class Cesium3DTilesetLifecycleEventReceiver : public Node {
	GDCLASS(Cesium3DTilesetLifecycleEventReceiver, Node)

public:
	virtual Ref<Material> create_material(
		const Ref<CesiumLoadedTilePrimitive>& tilePrimitive,
		const Ref<Material>& defaultMaterial
	);

	virtual void customize_material(
		const Ref<CesiumLoadedTilePrimitive>& tilePrimitive,
		const Ref<Material>& material
	);

	virtual void on_tile_mesh_primitive_loaded(
		const Ref<CesiumLoadedTilePrimitive>& tilePrimitive
	);
	virtual void on_raster_overlay_attached(
		const Ref<CesiumRasterOverlayBinding>& binding
	);
	virtual void on_raster_overlay_detaching(
		const Ref<CesiumRasterOverlayBinding>& binding
	);

	virtual void on_tile_loaded(Cesium3DTile* tile);
	virtual void on_tile_visibility_changed(Cesium3DTile* tile, bool visible);
	virtual void on_tile_unloading(Cesium3DTile* tile);

protected:
	static void _bind_methods();
};

#endif // CESIUM_3D_TILESET_LIFECYCLE_EVENT_RECEIVER_H
