// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Adapted for Godot from Cesium for Unreal under the Apache-2.0 license.

#ifndef CESIUM_URL_UTILITY_H
#define CESIUM_URL_UTILITY_H

/**
 * Last upstream review: Cesium for Unreal v2.29.0.
 *
 * Converts Godot and native filesystem paths into canonical file URLs for
 * Cesium Native. Keeping this at the public runtime boundary avoids every
 * language and provider inventing its own platform-specific slash and URI
 * escaping rules.
 */

#if defined(CESIUM_GD_MODULE)
#include "core/object/object.h"
#include "core/string/ustring.h"
#elif defined(CESIUM_GD_EXT)
#include "godot_cpp/classes/object.hpp"
#include "godot_cpp/variant/string.hpp"
using namespace godot;
#endif

class CesiumUrlUtility : public Object {
	GDCLASS(CesiumUrlUtility, Object)

public:
	static String local_path_to_file_url(const String& path);

protected:
	static void _bind_methods();
};

#endif // CESIUM_URL_UTILITY_H
