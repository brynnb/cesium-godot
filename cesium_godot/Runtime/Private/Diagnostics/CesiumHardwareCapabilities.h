// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Godot-native hardware capability adaptation for bounded streaming defaults.
// Last upstream review: Cesium for Unreal v2.29.0.

#ifndef CESIUM_HARDWARE_CAPABILITIES_H
#define CESIUM_HARDWARE_CAPABILITIES_H

#if defined(CESIUM_GD_MODULE)
#include "core/variant/dictionary.h"
#elif defined(CESIUM_GD_EXT)
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>
using namespace godot;
#endif

#include <cstdint>

enum class CesiumHardwareBudgetProfile : int32_t {
	Conservative = 0,
	Balanced = 1,
	Aggressive = 2
};

struct CesiumHardwareCapabilities final {
	int32_t logicalProcessorCount = 1;
	int64_t physicalMemoryBytes = -1;
	int64_t availableMemoryBytes = -1;
	bool renderingDeviceAvailable = false;
	String renderingMethod;
	String renderingDriver;
	String videoAdapterName;
	String videoAdapterVendor;
	int32_t videoAdapterType = 0;
	int64_t deviceTotalMemoryBytes = -1;
	int64_t deviceUsedMemoryBytes = -1;

	static CesiumHardwareCapabilities capture();

	int64_t recommend_total_cache_bytes(
		CesiumHardwareBudgetProfile profile
	) const;

	Dictionary to_dictionary() const;
};

#endif // CESIUM_HARDWARE_CAPABILITIES_H
