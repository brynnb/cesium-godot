// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Adapted for Godot from Cesium for Unreal under the Apache-2.0 license.

#include "Runtime/Public/Networking/CesiumUrlUtility.h"

#include <CesiumUtility/Uri.h>

#if defined(CESIUM_GD_EXT)
#include "godot_cpp/classes/project_settings.hpp"
#elif defined(CESIUM_GD_MODULE)
#include "core/config/project_settings.h"
#endif

#include <cstddef>
#include <string>

namespace {
bool is_ascii_alpha(char32_t character) {
	return (character >= U'A' && character <= U'Z') ||
		(character >= U'a' && character <= U'z');
}

bool is_windows_drive_path(const String& path) {
	return path.length() >= 3 &&
		is_ascii_alpha(path.unicode_at(0)) &&
		path.unicode_at(1) == U':' &&
		(path.unicode_at(2) == U'/' || path.unicode_at(2) == U'\\');
}

bool is_absolute_local_path(const String& path) {
	return path.begins_with("/") || is_windows_drive_path(path);
}
} // namespace

String CesiumUrlUtility::local_path_to_file_url(const String& path) {
	String localPath = path.strip_edges().replace("\\", "/");
	if (localPath.is_empty()) return String();

	if (localPath.begins_with("res://") || localPath.begins_with("user://")) {
		localPath = ProjectSettings::get_singleton()->globalize_path(localPath);
	} else if (!is_absolute_local_path(localPath)) {
		localPath = ProjectSettings::get_singleton()->globalize_path(
			String("res://") + localPath
		);
	}
	localPath = localPath.replace("\\", "/").simplify_path();
	if (!is_absolute_local_path(localPath)) return String();

	const CharString utf8Path = localPath.utf8();
	const std::string uriPath = CesiumUtility::Uri::nativePathToUriPath(
		std::string(utf8Path.get_data(), static_cast<size_t>(utf8Path.length()))
	);
	if (uriPath.starts_with("//")) {
		return String::utf8((std::string("file:") + uriPath).c_str());
	}
	if (uriPath.empty() || uriPath.front() != '/') return String();
	return String::utf8((std::string("file://") + uriPath).c_str());
}

void CesiumUrlUtility::_bind_methods() {
	ClassDB::bind_static_method(
		"CesiumUrlUtility",
		D_METHOD("local_path_to_file_url", "path"),
		&CesiumUrlUtility::local_path_to_file_url
	);
}
