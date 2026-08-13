// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Adapted for Godot from Cesium for Unreal under the Apache-2.0 license.

#include "Runtime/Public/Metadata/CesiumMetadataStyle.h"

#include "Runtime/Private/Metadata/CesiumFeatureStyleEncoding.h"
#include "Runtime/Public/CesiumLoadedTilePrimitive.h"
#include "Runtime/Public/Metadata/CesiumModelMetadata.h"
#include "Runtime/Public/Metadata/CesiumPropertyTable.h"

#if defined(CESIUM_GD_EXT)
#include "godot_cpp/classes/image.hpp"
#include "godot_cpp/classes/shader.hpp"
#include "godot_cpp/classes/shader_material.hpp"
#include "godot_cpp/core/error_macros.hpp"
#include "godot_cpp/variant/packed_byte_array.hpp"
#endif

#include <algorithm>
#include <cmath>
#include <limits>

namespace {
bool is_number(const Variant& value) {
	return value.get_type() == Variant::INT ||
		value.get_type() == Variant::FLOAT;
}

bool numeric_compare(
	const Variant& left,
	const Variant& right,
	const String& operation
) {
	if (!is_number(left) || !is_number(right)) {
		return false;
	}
	const double a = static_cast<double>(left);
	const double b = static_cast<double>(right);
	if (operation == "less" || operation == "<") {
		return a < b;
	}
	if (operation == "less_or_equal" || operation == "<=") {
		return a <= b;
	}
	if (operation == "greater" || operation == ">") {
		return a > b;
	}
	if (operation == "greater_or_equal" || operation == ">=") {
		return a >= b;
	}
	return false;
}

bool rule_matches(
	const Dictionary& rule,
	int64_t featureId,
	const Dictionary& metadata
) {
	if (!static_cast<bool>(rule.get("enabled", true))) {
		return false;
	}
	const String property = rule.get("property", "feature_id");
	Variant actual;
	if (property.is_empty() || property == "feature_id") {
		actual = featureId;
	} else if (metadata.has(property)) {
		actual = metadata[property];
	} else {
		return false;
	}

	const String operation = String(rule.get("operator", "equals")).to_lower();
	const Variant expected = rule.get("value", Variant());
	if (operation == "equals" || operation == "equal" || operation == "==") {
		if (is_number(actual) && is_number(expected)) {
			return static_cast<double>(actual) == static_cast<double>(expected);
		}
		return actual == expected;
	}
	if (
		operation == "not_equals" || operation == "not_equal" ||
		operation == "!="
	) {
		if (is_number(actual) && is_number(expected)) {
			return static_cast<double>(actual) != static_cast<double>(expected);
		}
		return actual != expected;
	}
	if (
		operation == "less" || operation == "<" ||
		operation == "less_or_equal" || operation == "<=" ||
		operation == "greater" || operation == ">" ||
		operation == "greater_or_equal" || operation == ">="
	) {
		return numeric_compare(actual, expected, operation);
	}
	if (operation == "between") {
		const Variant upper = rule.get("maximum", rule.get("second_value", Variant()));
		return is_number(actual) && is_number(expected) && is_number(upper) &&
			static_cast<double>(actual) >= static_cast<double>(expected) &&
			static_cast<double>(actual) <= static_cast<double>(upper);
	}
	if (operation == "contains") {
		if (actual.get_type() == Variant::STRING) {
			return String(actual).contains(String(expected));
		}
		if (actual.get_type() == Variant::ARRAY) {
			return Array(actual).has(expected);
		}
		return false;
	}
	if (operation == "in") {
		return expected.get_type() == Variant::ARRAY &&
			Array(expected).has(actual);
	}
	return false;
}

uint8_t color_byte(real_t value) {
	return static_cast<uint8_t>(Math::round(
		Math::clamp<real_t>(value, 0.0, 1.0) * 255.0
	));
}
}

void CesiumMetadataStyle::set_enabled(bool enabled) {
	if (this->m_enabled == enabled) {
		return;
	}
	this->m_enabled = enabled;
	this->changed(true);
}

bool CesiumMetadataStyle::get_enabled() const {
	return this->m_enabled;
}

void CesiumMetadataStyle::set_feature_source(int32_t source) {
	const FeatureSource bounded = source == InstanceFeatures
		? InstanceFeatures
		: MeshFeatures;
	if (this->m_featureSource == bounded) {
		return;
	}
	this->m_featureSource = bounded;
	this->changed(true);
}

int32_t CesiumMetadataStyle::get_feature_source() const {
	return static_cast<int32_t>(this->m_featureSource);
}

void CesiumMetadataStyle::set_feature_id_set_name(const String& name) {
	if (this->m_featureIdSetName == name) {
		return;
	}
	this->m_featureIdSetName = name;
	this->changed(true);
}

String CesiumMetadataStyle::get_feature_id_set_name() const {
	return this->m_featureIdSetName;
}

void CesiumMetadataStyle::set_feature_id_set_index(int32_t index) {
	const int32_t bounded = std::max<int32_t>(0, index);
	if (this->m_featureIdSetIndex == bounded) {
		return;
	}
	this->m_featureIdSetIndex = bounded;
	this->changed(true);
}

int32_t CesiumMetadataStyle::get_feature_id_set_index() const {
	return this->m_featureIdSetIndex;
}

void CesiumMetadataStyle::set_rules(const Array& rules) {
	this->m_rules = rules.duplicate(true);
	this->changed(false);
}

Array CesiumMetadataStyle::get_rules() const {
	return this->m_rules.duplicate(true);
}

void CesiumMetadataStyle::set_default_show(bool show) {
	if (this->m_defaultShow == show) {
		return;
	}
	this->m_defaultShow = show;
	this->changed(false);
}

bool CesiumMetadataStyle::get_default_show() const {
	return this->m_defaultShow;
}

void CesiumMetadataStyle::set_default_color(const Color& color) {
	if (this->m_defaultColor == color) {
		return;
	}
	this->m_defaultColor = color;
	this->changed(false);
}

Color CesiumMetadataStyle::get_default_color() const {
	return this->m_defaultColor;
}

void CesiumMetadataStyle::set_default_color_mix(real_t mix) {
	const real_t bounded = Math::clamp<real_t>(mix, 0.0, 1.0);
	if (Math::is_equal_approx(this->m_defaultColorMix, bounded)) {
		return;
	}
	this->m_defaultColorMix = bounded;
	this->changed(false);
}

real_t CesiumMetadataStyle::get_default_color_mix() const {
	return this->m_defaultColorMix;
}

void CesiumMetadataStyle::set_maximum_features(int64_t count) {
	const int64_t bounded = std::clamp<int64_t>(count, 1, 16 * 1024 * 1024);
	if (this->m_maximumFeatures == bounded) {
		return;
	}
	this->m_maximumFeatures = bounded;
	this->changed(false);
}

int64_t CesiumMetadataStyle::get_maximum_features() const {
	return this->m_maximumFeatures;
}

void CesiumMetadataStyle::set_maximum_cache_bytes(int64_t bytes) {
	const int64_t bounded = std::clamp<int64_t>(
		bytes,
		1024 * 1024,
		static_cast<int64_t>(8) * 1024 * 1024 * 1024
	);
	if (this->m_maximumCacheBytes == bounded) {
		return;
	}
	this->m_maximumCacheBytes = bounded;
	this->changed(false);
}

int64_t CesiumMetadataStyle::get_maximum_cache_bytes() const {
	return this->m_maximumCacheBytes;
}

int64_t CesiumMetadataStyle::get_lookup_cache_count() const {
	return static_cast<int64_t>(this->m_lookupCache.size());
}

int64_t CesiumMetadataStyle::get_lookup_cache_bytes() const {
	return this->m_lookupCacheBytes;
}

Dictionary CesiumMetadataStyle::evaluate_feature(
	int64_t featureId,
	const Dictionary& metadata
) const {
	Dictionary result;
	result["show"] = this->m_defaultShow;
	result["color"] = this->m_defaultColor;
	result["color_mix"] = this->m_defaultColorMix;
	result["matched"] = false;
	result["rule_index"] = -1;
	for (int64_t index = 0; index < this->m_rules.size(); ++index) {
		const Variant value = this->m_rules[index];
		if (value.get_type() != Variant::DICTIONARY) {
			continue;
		}
		const Dictionary rule = value;
		if (!rule_matches(rule, featureId, metadata)) {
			continue;
		}
		result["show"] = rule.get("show", result["show"]);
		const Variant color = rule.get("color", result["color"]);
		if (color.get_type() == Variant::COLOR) {
			result["color"] = color;
		}
		const Variant mix = rule.get("color_mix", 1.0);
		if (is_number(mix)) {
			result["color_mix"] = Math::clamp<double>(
				static_cast<double>(mix),
				0.0,
				1.0
			);
		}
		result["matched"] = true;
		result["rule_index"] = index;
		break;
	}
	return result;
}

const CesiumMetadataStyle::LookupTextures*
CesiumMetadataStyle::get_or_create_lookup(
	const Ref<CesiumModelMetadata>& metadata,
	int32_t propertyTableIndex,
	int64_t featureCount
) {
	const ObjectID metadataId = metadata.is_valid()
		? ObjectID(metadata->get_instance_id())
		: ObjectID();
	this->prune_lookup_cache(0);
	for (LookupTextures& lookup : this->m_lookupCache) {
		if (
			lookup.metadata == metadataId &&
			lookup.propertyTableIndex == propertyTableIndex &&
			lookup.featureCount == featureCount
		) {
			lookup.lastUse = ++this->m_lookupUseCounter;
			return &lookup;
		}
	}
	if (
		featureCount <= 0 || featureCount > this->m_maximumFeatures ||
		featureCount > static_cast<int64_t>(std::numeric_limits<int32_t>::max())
	) {
		return nullptr;
	}

	const int32_t dimension = static_cast<int32_t>(std::ceil(std::sqrt(
		static_cast<double>(featureCount)
	)));
	const int64_t texelCount = static_cast<int64_t>(dimension) * dimension;
	const int64_t retainedBytes = texelCount * 6;
	if (retainedBytes > this->m_maximumCacheBytes) {
		return nullptr;
	}
	this->prune_lookup_cache(retainedBytes);
	PackedByteArray colorBytes;
	PackedByteArray controlBytes;
	colorBytes.resize(texelCount * 4);
	controlBytes.resize(texelCount * 2);
	uint8_t* colors = colorBytes.ptrw();
	uint8_t* controls = controlBytes.ptrw();
	std::fill(colors, colors + colorBytes.size(), 255);
	for (int64_t index = 0; index < texelCount; ++index) {
		controls[index * 2] = 255;
		controls[index * 2 + 1] = 0;
	}

	Ref<CesiumPropertyTable> table = metadata.is_valid()
		? metadata->get_property_table(propertyTableIndex)
		: Ref<CesiumPropertyTable>();
	for (int64_t featureId = 0; featureId < featureCount; ++featureId) {
		const Dictionary values = table.is_valid()
			? table->get_metadata_values_for_feature(featureId)
			: Dictionary();
		const Dictionary style = this->evaluate_feature(featureId, values);
		const Color color = style.get("color", Color(1, 1, 1, 1));
		const real_t mix = style.get("color_mix", 0.0);
		const int64_t colorOffset = featureId * 4;
		colors[colorOffset] = color_byte(color.r);
		colors[colorOffset + 1] = color_byte(color.g);
		colors[colorOffset + 2] = color_byte(color.b);
		colors[colorOffset + 3] = color_byte(color.a);
		const int64_t controlOffset = featureId * 2;
		controls[controlOffset] = static_cast<bool>(style.get("show", true))
			? 255
			: 0;
		controls[controlOffset + 1] = color_byte(mix);
	}

	Ref<Image> colorImage = Image::create_from_data(
		dimension,
		dimension,
		false,
		Image::FORMAT_RGBA8,
		colorBytes
	);
	Ref<Image> controlImage = Image::create_from_data(
		dimension,
		dimension,
		false,
		Image::FORMAT_RG8,
		controlBytes
	);
	if (colorImage.is_null() || controlImage.is_null()) {
		return nullptr;
	}
	LookupTextures lookup;
	lookup.metadata = metadataId;
	lookup.propertyTableIndex = propertyTableIndex;
	lookup.featureCount = featureCount;
	lookup.colors = ImageTexture::create_from_image(colorImage);
	lookup.controls = ImageTexture::create_from_image(controlImage);
	lookup.dimension = dimension;
	lookup.retainedBytes = retainedBytes;
	lookup.lastUse = ++this->m_lookupUseCounter;
	if (lookup.colors.is_null() || lookup.controls.is_null()) {
		return nullptr;
	}
	this->m_lookupCacheBytes += retainedBytes;
	this->m_lookupCache.emplace_back(std::move(lookup));
	return &this->m_lookupCache.back();
}

void CesiumMetadataStyle::prune_lookup_cache(int64_t bytesNeeded) {
	for (auto it = this->m_lookupCache.begin(); it != this->m_lookupCache.end();) {
		if (
			it->metadata != ObjectID() &&
			ObjectDB::get_instance(it->metadata) == nullptr
		) {
			this->m_lookupCacheBytes -= it->retainedBytes;
			it = this->m_lookupCache.erase(it);
		} else {
			++it;
		}
	}
	while (
		!this->m_lookupCache.empty() &&
		this->m_lookupCacheBytes + bytesNeeded > this->m_maximumCacheBytes
	) {
		const auto oldest = std::min_element(
			this->m_lookupCache.begin(),
			this->m_lookupCache.end(),
			[](const LookupTextures& left, const LookupTextures& right) {
				return left.lastUse < right.lastUse;
			}
		);
		this->m_lookupCacheBytes -= oldest->retainedBytes;
		this->m_lookupCache.erase(oldest);
	}
}

bool CesiumMetadataStyle::apply_to_primitive(
	const Ref<CesiumLoadedTilePrimitive>& primitive
) {
	if (primitive.is_null()) {
		return false;
	}
	Ref<ShaderMaterial> material = primitive->get_active_material();
	if (material.is_null() || material->get_shader().is_null()) {
		return false;
	}
	if (!this->m_enabled) {
		this->clear_from_primitive(primitive);
		return true;
	}
	// Encoding belongs to the prepared primitive, not to whichever material a
	// lifecycle receiver later selects. The generated material is its stable
	// descriptor; a compatible custom material may consume the same contract.
	Ref<ShaderMaterial> encodingMaterial = primitive->get_default_material();
	if (encodingMaterial.is_null()) {
		encodingMaterial = material;
	}
	if (!static_cast<bool>(encodingMaterial->get_meta(
		"cesium_feature_style_encoded",
		false
	))) {
		material->set_meta(
			"cesium_metadata_style_error",
			"selected feature ID set was not encoded for this primitive"
		);
		return false;
	}
	const String source = encodingMaterial->get_meta(
		"cesium_feature_style_source",
		"none"
	);
	const String shaderCode = material->get_shader()->get_code();
	const bool hasCommonContract =
		shaderCode.contains("cesium_feature_style_enabled") &&
		shaderCode.contains("cesium_feature_style_colors") &&
		shaderCode.contains("cesium_feature_style_controls") &&
		shaderCode.contains("cesium_feature_style_dimension") &&
		shaderCode.contains("cesium_feature_style_feature_count") &&
		shaderCode.contains("cesium_feature_style_null_id");
	const bool hasSourceContract = source == "vertex"
		? shaderCode.contains("CUSTOM0")
		: source == "instance"
			? shaderCode.contains("INSTANCE_CUSTOM")
			: source == "texture"
				? shaderCode.contains("cesium_feature_id_texture")
				: false;
	if (!hasCommonContract || !hasSourceContract) {
		material->set_meta(
			"cesium_metadata_style_error",
			"active custom shader does not implement the Cesium metadata-style contract"
		);
		material->set_meta("cesium_metadata_style_applied", false);
		return false;
	}
	const int64_t featureCount = encodingMaterial->get_meta(
		"cesium_feature_style_feature_count",
		0
	);
	const int32_t propertyTableIndex = static_cast<int32_t>(
		static_cast<int64_t>(encodingMaterial->get_meta(
			"cesium_feature_style_property_table_index",
			-1
		))
	);
	const LookupTextures* lookup = this->get_or_create_lookup(
		primitive->get_model_metadata(),
		propertyTableIndex,
		featureCount
	);
	if (lookup == nullptr) {
		material->set_shader_parameter("cesium_feature_style_enabled", false);
		material->set_meta(
			"cesium_metadata_style_error",
			featureCount > this->m_maximumFeatures
				? "feature count exceeds metadata style maximum_features"
				: featureCount > 0 &&
					static_cast<int64_t>(std::ceil(std::sqrt(
						static_cast<double>(featureCount)
					))) *
					static_cast<int64_t>(std::ceil(std::sqrt(
						static_cast<double>(featureCount)
					))) * 6 > this->m_maximumCacheBytes
					? "lookup texture exceeds metadata style maximum_cache_bytes"
					: "could not create metadata style lookup textures"
		);
		return false;
	}
	material->set_shader_parameter(
		"cesium_feature_style_colors",
		lookup->colors
	);
	material->set_shader_parameter(
		"cesium_feature_style_controls",
		lookup->controls
	);
	material->set_shader_parameter(
		"cesium_feature_style_dimension",
		lookup->dimension
	);
	material->set_shader_parameter(
		"cesium_feature_style_feature_count",
		static_cast<double>(featureCount)
	);
	material->set_shader_parameter(
		"cesium_feature_style_null_id",
		static_cast<double>(encodingMaterial->get_meta(
			"cesium_feature_style_null_id",
			-1
		))
	);
	if (source == "texture" && encodingMaterial != material) {
		for (const char* parameter : {
			"cesium_feature_id_texture",
			"cesium_feature_id_uv_set",
			"cesium_feature_id_texture_transform",
			"cesium_feature_id_texture_rotation",
			"cesium_feature_id_texture_wrap",
			"cesium_feature_id_channel_weights",
		}) {
			material->set_shader_parameter(
				parameter,
				encodingMaterial->get_shader_parameter(parameter)
			);
		}
	}
	material->set_shader_parameter("cesium_feature_style_enabled", true);
	material->set_meta("cesium_metadata_style_error", "");
	material->set_meta("cesium_metadata_style_applied", true);
	return true;
}

void CesiumMetadataStyle::clear_from_primitive(
	const Ref<CesiumLoadedTilePrimitive>& primitive
) const {
	if (primitive.is_null()) {
		return;
	}
	Ref<ShaderMaterial> material = primitive->get_active_material();
	if (material.is_null()) {
		return;
	}
	material->set_shader_parameter("cesium_feature_style_enabled", false);
	material->set_meta("cesium_metadata_style_applied", false);
}

CesiumFeatureStyleEncodingDescription
CesiumMetadataStyle::get_encoding_description() const {
	CesiumFeatureStyleEncodingDescription result;
	result.enabled = this->m_enabled;
	result.useInstanceFeatures =
		this->m_featureSource == InstanceFeatures;
	result.featureIdSetIndex = this->m_featureIdSetIndex;
	const CharString utf8 = this->m_featureIdSetName.utf8();
	result.featureIdSetName.assign(utf8.get_data(), utf8.length());
	return result;
}

uint64_t CesiumMetadataStyle::get_encoding_revision() const {
	return this->m_encodingRevision;
}

void CesiumMetadataStyle::clear_lookup_cache() {
	this->m_lookupCache.clear();
	this->m_lookupCacheBytes = 0;
}

void CesiumMetadataStyle::changed(bool encodingChanged) {
	this->clear_lookup_cache();
	if (encodingChanged) {
		++this->m_encodingRevision;
		this->emit_signal("encoding_changed");
	}
	this->emit_changed();
}

void CesiumMetadataStyle::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_enabled", "enabled"), &CesiumMetadataStyle::set_enabled);
	ClassDB::bind_method(D_METHOD("get_enabled"), &CesiumMetadataStyle::get_enabled);
	ClassDB::bind_method(D_METHOD("set_feature_source", "source"), &CesiumMetadataStyle::set_feature_source);
	ClassDB::bind_method(D_METHOD("get_feature_source"), &CesiumMetadataStyle::get_feature_source);
	ClassDB::bind_method(D_METHOD("set_feature_id_set_name", "name"), &CesiumMetadataStyle::set_feature_id_set_name);
	ClassDB::bind_method(D_METHOD("get_feature_id_set_name"), &CesiumMetadataStyle::get_feature_id_set_name);
	ClassDB::bind_method(D_METHOD("set_feature_id_set_index", "index"), &CesiumMetadataStyle::set_feature_id_set_index);
	ClassDB::bind_method(D_METHOD("get_feature_id_set_index"), &CesiumMetadataStyle::get_feature_id_set_index);
	ClassDB::bind_method(D_METHOD("set_rules", "rules"), &CesiumMetadataStyle::set_rules);
	ClassDB::bind_method(D_METHOD("get_rules"), &CesiumMetadataStyle::get_rules);
	ClassDB::bind_method(D_METHOD("set_default_show", "show"), &CesiumMetadataStyle::set_default_show);
	ClassDB::bind_method(D_METHOD("get_default_show"), &CesiumMetadataStyle::get_default_show);
	ClassDB::bind_method(D_METHOD("set_default_color", "color"), &CesiumMetadataStyle::set_default_color);
	ClassDB::bind_method(D_METHOD("get_default_color"), &CesiumMetadataStyle::get_default_color);
	ClassDB::bind_method(D_METHOD("set_default_color_mix", "mix"), &CesiumMetadataStyle::set_default_color_mix);
	ClassDB::bind_method(D_METHOD("get_default_color_mix"), &CesiumMetadataStyle::get_default_color_mix);
	ClassDB::bind_method(D_METHOD("set_maximum_features", "count"), &CesiumMetadataStyle::set_maximum_features);
	ClassDB::bind_method(D_METHOD("get_maximum_features"), &CesiumMetadataStyle::get_maximum_features);
	ClassDB::bind_method(D_METHOD("set_maximum_cache_bytes", "bytes"), &CesiumMetadataStyle::set_maximum_cache_bytes);
	ClassDB::bind_method(D_METHOD("get_maximum_cache_bytes"), &CesiumMetadataStyle::get_maximum_cache_bytes);
	ClassDB::bind_method(D_METHOD("get_lookup_cache_count"), &CesiumMetadataStyle::get_lookup_cache_count);
	ClassDB::bind_method(D_METHOD("get_lookup_cache_bytes"), &CesiumMetadataStyle::get_lookup_cache_bytes);
	ClassDB::bind_method(D_METHOD("evaluate_feature", "feature_id", "metadata"), &CesiumMetadataStyle::evaluate_feature);
	ClassDB::bind_method(D_METHOD("apply_to_primitive", "primitive"), &CesiumMetadataStyle::apply_to_primitive);
	ClassDB::bind_method(D_METHOD("clear_from_primitive", "primitive"), &CesiumMetadataStyle::clear_from_primitive);
	ClassDB::bind_method(D_METHOD("clear_lookup_cache"), &CesiumMetadataStyle::clear_lookup_cache);

	BIND_ENUM_CONSTANT(MeshFeatures);
	BIND_ENUM_CONSTANT(InstanceFeatures);
	ADD_SIGNAL(MethodInfo("encoding_changed"));

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "enabled"), "set_enabled", "get_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "feature_source", PROPERTY_HINT_ENUM, "Mesh features,Instance features"), "set_feature_source", "get_feature_source");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "feature_id_set_name"), "set_feature_id_set_name", "get_feature_id_set_name");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "feature_id_set_index", PROPERTY_HINT_RANGE, "0,2147483647,1"), "set_feature_id_set_index", "get_feature_id_set_index");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "rules", PROPERTY_HINT_ARRAY_TYPE, "Dictionary"), "set_rules", "get_rules");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "default_show"), "set_default_show", "get_default_show");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "default_color"), "set_default_color", "get_default_color");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "default_color_mix", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_default_color_mix", "get_default_color_mix");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "maximum_features", PROPERTY_HINT_RANGE, "1,16777216,1,or_greater"), "set_maximum_features", "get_maximum_features");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "maximum_cache_bytes", PROPERTY_HINT_RANGE, "1048576,8589934592,1048576,or_greater,suffix:B"), "set_maximum_cache_bytes", "get_maximum_cache_bytes");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "lookup_cache_count", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_lookup_cache_count");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "lookup_cache_bytes", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_lookup_cache_bytes");
}
