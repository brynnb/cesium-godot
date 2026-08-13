// Copyright 2020-2026 CesiumGS, Inc. and Contributors

#include "Runtime/Private/Diagnostics/CesiumHardwareCapabilities.h"

#if defined(CESIUM_GD_MODULE)
#include "core/os/os.h"
#include "servers/rendering/rendering_device.h"
#include "servers/rendering_server.h"
#elif defined(CESIUM_GD_EXT)
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/rendering_device.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#endif

#include <algorithm>
#include <cmath>
#include <limits>

namespace {
constexpr int64_t MEBIBYTE = 1024LL * 1024;
constexpr int64_t GIBIBYTE = 1024LL * 1024 * 1024;

int64_t valid_capacity(const Variant& value) {
	if (value.get_type() != Variant::INT) {
		return -1;
	}
	const int64_t capacity = static_cast<int64_t>(value);
	return capacity > 0 ? capacity : -1;
}

int64_t valid_capacity(uint64_t capacity) {
	if (
		capacity == 0 ||
		capacity > static_cast<uint64_t>(std::numeric_limits<int64_t>::max())
	) {
		return -1;
	}
	return static_cast<int64_t>(capacity);
}

int64_t scaled_capacity(int64_t capacity, double fraction) {
	if (capacity <= 0) {
		return -1;
	}
	return static_cast<int64_t>(
		std::floor(static_cast<double>(capacity) * fraction)
	);
}

void include_positive_minimum(int64_t candidate, int64_t& result) {
	if (candidate <= 0) {
		return;
	}
	result = result > 0 ? std::min(result, candidate) : candidate;
}
} // namespace

CesiumHardwareCapabilities CesiumHardwareCapabilities::capture() {
	CesiumHardwareCapabilities result;
	OS* os = OS::get_singleton();
	if (os != nullptr) {
		result.logicalProcessorCount = std::max(1, os->get_processor_count());
		const Dictionary memory = os->get_memory_info();
		result.physicalMemoryBytes = valid_capacity(memory.get("physical", -1));
		result.availableMemoryBytes = valid_capacity(memory.get("available", -1));
	}

	RenderingServer* renderingServer = RenderingServer::get_singleton();
	if (renderingServer == nullptr) {
		return result;
	}
	result.renderingMethod = renderingServer->get_current_rendering_method();
	result.renderingDriver =
		renderingServer->get_current_rendering_driver_name();
	result.videoAdapterName = renderingServer->get_video_adapter_name();
	result.videoAdapterVendor = renderingServer->get_video_adapter_vendor();
	result.videoAdapterType = static_cast<int32_t>(
		renderingServer->get_video_adapter_type()
	);
	RenderingDevice* renderingDevice = renderingServer->get_rendering_device();
	result.renderingDeviceAvailable = renderingDevice != nullptr;
	if (renderingDevice == nullptr) {
		return result;
	}
	result.deviceUsedMemoryBytes = valid_capacity(
		renderingDevice->get_memory_usage(RenderingDevice::MEMORY_TOTAL)
	);
	result.deviceTotalMemoryBytes = valid_capacity(
		renderingDevice->get_device_total_memory()
	);
	return result;
}

int64_t CesiumHardwareCapabilities::recommend_total_cache_bytes(
	CesiumHardwareBudgetProfile profile
) const {
	double physicalFraction = 0.10;
	double availableFraction = 0.25;
	double deviceFraction = 0.25;
	int64_t maximum = 2 * GIBIBYTE;
	if (profile == CesiumHardwareBudgetProfile::Balanced) {
		physicalFraction = 0.20;
		availableFraction = 0.50;
		deviceFraction = 0.50;
		maximum = 8 * GIBIBYTE;
	} else if (profile == CesiumHardwareBudgetProfile::Aggressive) {
		physicalFraction = 0.30;
		availableFraction = 0.70;
		deviceFraction = 0.70;
		maximum = 16 * GIBIBYTE;
	}

	int64_t recommendation = -1;
	include_positive_minimum(
		scaled_capacity(this->physicalMemoryBytes, physicalFraction),
		recommendation
	);
	include_positive_minimum(
		scaled_capacity(this->availableMemoryBytes, availableFraction),
		recommendation
	);
	include_positive_minimum(
		scaled_capacity(this->deviceTotalMemoryBytes, deviceFraction),
		recommendation
	);
	if (recommendation <= 0) {
		// Preserve Cesium Native's established default when every capacity is
		// unknown (for example a restricted Web or headless environment).
		return 512 * MEBIBYTE;
	}
	// A profile's upper bound limits runaway caches on very large machines.
	// Do not impose a lower bound: on a memory-constrained machine that could
	// increase the recommendation beyond the safe fraction of available RAM.
	return std::min(recommendation, maximum);
}

Dictionary CesiumHardwareCapabilities::to_dictionary() const {
	Dictionary result;
	result["logical_processor_count"] = this->logicalProcessorCount;
	result["physical_memory_bytes"] = this->physicalMemoryBytes;
	result["available_memory_bytes"] = this->availableMemoryBytes;
	result["rendering_device_available"] = this->renderingDeviceAvailable;
	result["rendering_method"] = this->renderingMethod;
	result["rendering_driver"] = this->renderingDriver;
	result["video_adapter_name"] = this->videoAdapterName;
	result["video_adapter_vendor"] = this->videoAdapterVendor;
	result["video_adapter_type"] = this->videoAdapterType;
	result["device_total_memory_bytes"] = this->deviceTotalMemoryBytes;
	result["device_used_memory_bytes"] = this->deviceUsedMemoryBytes;
	result["device_total_memory_available"] =
		this->deviceTotalMemoryBytes > 0;
	result["recommended_cache_bytes_conservative"] =
		this->recommend_total_cache_bytes(
			CesiumHardwareBudgetProfile::Conservative
		);
	result["recommended_cache_bytes_balanced"] =
		this->recommend_total_cache_bytes(
			CesiumHardwareBudgetProfile::Balanced
		);
	result["recommended_cache_bytes_aggressive"] =
		this->recommend_total_cache_bytes(
			CesiumHardwareBudgetProfile::Aggressive
		);
	return result;
}
