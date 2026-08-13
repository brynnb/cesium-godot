#include "Runtime/Private/Metadata/CesiumGodotMetadataConversions.h"

#if defined(CESIUM_GD_MODULE)
#include "core/variant/array.h"
#elif defined(CESIUM_GD_EXT)
#include "godot_cpp/variant/array.hpp"
#include "godot_cpp/variant/string.hpp"
#endif

#include <cstdint>
#include <limits>
#include <string>

namespace CesiumGodotMetadataConversions {

Dictionary json_object_to_dictionary(
	const CesiumUtility::JsonValue::Object& object
) {
	Dictionary result;
	for (const auto& [key, value] : object) {
		result[String(key.c_str())] = json_value_to_variant(value);
	}
	return result;
}

Variant json_value_to_variant(const CesiumUtility::JsonValue& value) {
	if (value.isNull()) {
		return Variant();
	}
	if (value.isBool()) {
		return value.getBool();
	}
	if (value.isString()) {
		return String(value.getStringOrDefault("").c_str());
	}
	if (value.isDouble()) {
		return value.getDouble();
	}
	if (value.isInt64()) {
		return value.getInt64();
	}
	if (value.isUint64()) {
		const uint64_t unsignedValue = value.getUint64();
		if (
			unsignedValue <=
			static_cast<uint64_t>(std::numeric_limits<int64_t>::max())
		) {
			return static_cast<int64_t>(unsignedValue);
		}
		// Godot integers are signed. Preserve out-of-range JSON losslessly.
		return String(std::to_string(unsignedValue).c_str());
	}
	if (value.isObject()) {
		return json_object_to_dictionary(value.getObject());
	}
	if (value.isArray()) {
		Array result;
		for (
			const CesiumUtility::JsonValue& item :
			std::get<CesiumUtility::JsonValue::Array>(value.value)
		) {
			result.push_back(json_value_to_variant(item));
		}
		return result;
	}
	return Variant();
}

} // namespace CesiumGodotMetadataConversions
