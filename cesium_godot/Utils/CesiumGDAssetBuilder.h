
#ifndef CESIUM_GD_ASSET_BUILDER
#define CESIUM_GD_ASSET_BUILDER

#include "Godot/Nodes/CesiumGeoreference.h"
#include <cstdint>
#if defined(CESIUM_GD_EXT)
#include <godot_cpp/classes/node3d.hpp>
using namespace godot;
#elif defined(CESIUM_GD_MODULE)

#endif

class CesiumGeoreference;

/// @brief Simple front-facing UI for the AssetManipulation namespace
class CesiumGDAssetBuilder : public Node3D {
  GDCLASS(CesiumGDAssetBuilder, Node3D)
public:

  void instantiate_tileset(int32_t assetId = 0, const String& assetType = "", const String& assetName = "Blank");

  void instantiate_dynamic_cam();

  void instantiate_orbit_cam();

  Variant get_georeference_camera_script() const;

  void set_georeference_camera_script(Variant cameraScript);

  CesiumGeoreference* find_or_create_globe();
  
  Array find_all_tilesets();

private:
  Variant m_georeferenceCameraScript;
  Variant m_normalCameraScript;

protected:
  static void _bind_methods();
};

#endif
