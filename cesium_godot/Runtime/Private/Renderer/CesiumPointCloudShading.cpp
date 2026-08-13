// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Adapted for Godot from Cesium for Unreal under the Apache-2.0 license.

#include "Runtime/Public/Renderer/CesiumPointCloudShading.h"

#if defined(CESIUM_GD_MODULE)
#include "scene/resources/shader.h"
#elif defined(CESIUM_GD_EXT)
#include "godot_cpp/classes/shader.hpp"
#include "godot_cpp/classes/shader_material.hpp"
#include "godot_cpp/core/math.hpp"
#endif

#include <algorithm>
#include <cmath>

namespace {
real_t finite_non_negative(real_t value) {
	return std::isfinite(static_cast<double>(value))
		? std::max<real_t>(0.0, value)
		: 0.0;
}

bool has_point_cloud_shader_contract(const Ref<ShaderMaterial>& material) {
	if (material.is_null()) {
		return false;
	}
	if (
		static_cast<bool>(material->get_meta(
			"cesium_point_cloud_supported",
			false
		))
	) {
		return true;
	}
	const Ref<Shader> shader = material->get_shader();
	if (shader.is_null()) {
		return false;
	}
	bool hasAttenuation = false;
	bool hasGeometricError = false;
	bool hasMaximumSize = false;
	bool hasDiameter = false;
	const Array uniforms = shader->get_shader_uniform_list(false);
	for (int64_t index = 0; index < uniforms.size(); ++index) {
		const Dictionary uniform = uniforms[index];
		const StringName name = uniform.get("name", StringName());
		hasAttenuation = hasAttenuation ||
			name == StringName("cesium_point_attenuation_enabled");
		hasGeometricError = hasGeometricError ||
			name == StringName("cesium_point_geometric_error");
		hasMaximumSize = hasMaximumSize ||
			name == StringName("cesium_point_maximum_size");
		hasDiameter = hasDiameter ||
			name == StringName("cesium_point_diameter");
	}
	return hasAttenuation && hasGeometricError && hasMaximumSize && hasDiameter;
}
}

void CesiumPointCloudShading::set_attenuation(bool enabled) {
	if (this->m_attenuation == enabled) {
		return;
	}
	this->m_attenuation = enabled;
	this->emit_changed();
}

bool CesiumPointCloudShading::get_attenuation() const {
	return this->m_attenuation;
}

void CesiumPointCloudShading::set_geometric_error_scale(real_t scale) {
	const real_t bounded = finite_non_negative(scale);
	if (Math::is_equal_approx(this->m_geometricErrorScale, bounded)) {
		return;
	}
	this->m_geometricErrorScale = bounded;
	this->emit_changed();
}

real_t CesiumPointCloudShading::get_geometric_error_scale() const {
	return this->m_geometricErrorScale;
}

void CesiumPointCloudShading::set_maximum_attenuation(real_t pixels) {
	const real_t bounded = finite_non_negative(pixels);
	if (Math::is_equal_approx(this->m_maximumAttenuation, bounded)) {
		return;
	}
	this->m_maximumAttenuation = bounded;
	this->emit_changed();
}

real_t CesiumPointCloudShading::get_maximum_attenuation() const {
	return this->m_maximumAttenuation;
}

void CesiumPointCloudShading::set_base_resolution(real_t meters) {
	const real_t bounded = finite_non_negative(meters);
	if (Math::is_equal_approx(this->m_baseResolution, bounded)) {
		return;
	}
	this->m_baseResolution = bounded;
	this->emit_changed();
}

real_t CesiumPointCloudShading::get_base_resolution() const {
	return this->m_baseResolution;
}

double CesiumPointCloudShading::resolve_geometric_error(
	double tileGeometricError,
	const Vector3& dimensions,
	int64_t pointCount
) const {
	if (std::isfinite(tileGeometricError) && tileGeometricError > 0.0) {
		return tileGeometricError;
	}
	if (this->m_baseResolution > 0.0) {
		return static_cast<double>(this->m_baseResolution);
	}
	if (pointCount <= 0) {
		return 0.0;
	}
	const double volume =
		std::max(0.0, static_cast<double>(dimensions.x)) *
		std::max(0.0, static_cast<double>(dimensions.y)) *
		std::max(0.0, static_cast<double>(dimensions.z));
	return volume > 0.0
		? std::cbrt(volume / static_cast<double>(pointCount))
		: 0.0;
}

Dictionary CesiumPointCloudShading::resolve_parameters(
	bool usesAdditiveRefinement,
	double tileGeometricError,
	const Vector3& dimensions,
	int64_t pointCount,
	int64_t diameter,
	double maximumScreenSpaceError
) const {
	const double unscaledError = this->resolve_geometric_error(
		tileGeometricError,
		dimensions,
		pointCount
	);
	double maximumPointSize = 1.0;
	if (this->m_attenuation) {
		maximumPointSize = usesAdditiveRefinement
			? 5.0
			: std::max(0.0, maximumScreenSpaceError);
		if (this->m_maximumAttenuation > 0.0) {
			maximumPointSize = this->m_maximumAttenuation;
		}
	}
	Dictionary result;
	result["attenuation"] = this->m_attenuation;
	result["uses_additive_refinement"] = usesAdditiveRefinement;
	result["tile_geometric_error"] = tileGeometricError;
	result["unscaled_geometric_error"] = unscaledError;
	result["geometric_error"] =
		unscaledError * static_cast<double>(this->m_geometricErrorScale);
	result["geometric_error_scale"] = this->m_geometricErrorScale;
	result["maximum_point_size"] = maximumPointSize;
	result["maximum_attenuation"] = this->m_maximumAttenuation;
	result["base_resolution"] = this->m_baseResolution;
	result["diameter"] = std::max<int64_t>(0, diameter);
	result["point_count"] = std::max<int64_t>(0, pointCount);
	result["dimensions"] = dimensions;
	result["uses_bentley_point_style"] = diameter >= 1;
	return result;
}

bool CesiumPointCloudShading::apply_to_material(
	const Ref<Material>& material,
	bool usesAdditiveRefinement,
	double tileGeometricError,
	const Vector3& dimensions,
	int64_t pointCount,
	int64_t diameter,
	double maximumScreenSpaceError
) const {
	Ref<ShaderMaterial> shaderMaterial = material;
	if (!has_point_cloud_shader_contract(shaderMaterial)) {
		return false;
	}
	const Dictionary parameters = this->resolve_parameters(
		usesAdditiveRefinement,
		tileGeometricError,
		dimensions,
		pointCount,
		diameter,
		maximumScreenSpaceError
	);
	shaderMaterial->set_shader_parameter(
		"cesium_point_attenuation_enabled",
		parameters["attenuation"]
	);
	shaderMaterial->set_shader_parameter(
		"cesium_point_geometric_error",
		parameters["geometric_error"]
	);
	shaderMaterial->set_shader_parameter(
		"cesium_point_maximum_size",
		parameters["maximum_point_size"]
	);
	shaderMaterial->set_shader_parameter(
		"cesium_point_diameter",
		static_cast<double>(parameters["diameter"])
	);
	shaderMaterial->set_meta("cesium_point_cloud_parameters", parameters);
	return true;
}

void CesiumPointCloudShading::_bind_methods() {
	ClassDB::bind_method(
		D_METHOD("set_attenuation", "enabled"),
		&CesiumPointCloudShading::set_attenuation
	);
	ClassDB::bind_method(
		D_METHOD("get_attenuation"),
		&CesiumPointCloudShading::get_attenuation
	);
	ClassDB::bind_method(
		D_METHOD("set_geometric_error_scale", "scale"),
		&CesiumPointCloudShading::set_geometric_error_scale
	);
	ClassDB::bind_method(
		D_METHOD("get_geometric_error_scale"),
		&CesiumPointCloudShading::get_geometric_error_scale
	);
	ClassDB::bind_method(
		D_METHOD("set_maximum_attenuation", "pixels"),
		&CesiumPointCloudShading::set_maximum_attenuation
	);
	ClassDB::bind_method(
		D_METHOD("get_maximum_attenuation"),
		&CesiumPointCloudShading::get_maximum_attenuation
	);
	ClassDB::bind_method(
		D_METHOD("set_base_resolution", "meters"),
		&CesiumPointCloudShading::set_base_resolution
	);
	ClassDB::bind_method(
		D_METHOD("get_base_resolution"),
		&CesiumPointCloudShading::get_base_resolution
	);
	ClassDB::bind_method(
		D_METHOD(
			"resolve_geometric_error",
			"tile_geometric_error",
			"dimensions",
			"point_count"
		),
		&CesiumPointCloudShading::resolve_geometric_error
	);
	ClassDB::bind_method(
		D_METHOD(
			"resolve_parameters",
			"uses_additive_refinement",
			"tile_geometric_error",
			"dimensions",
			"point_count",
			"diameter",
			"maximum_screen_space_error"
		),
		&CesiumPointCloudShading::resolve_parameters
	);
	ClassDB::bind_method(
		D_METHOD(
			"apply_to_material",
			"material",
			"uses_additive_refinement",
			"tile_geometric_error",
			"dimensions",
			"point_count",
			"diameter",
			"maximum_screen_space_error"
		),
		&CesiumPointCloudShading::apply_to_material
	);

	ADD_PROPERTY(
		PropertyInfo(Variant::BOOL, "attenuation"),
		"set_attenuation",
		"get_attenuation"
	);
	ADD_PROPERTY(
		PropertyInfo(
			Variant::FLOAT,
			"geometric_error_scale",
			PROPERTY_HINT_RANGE,
			"0,100,0.01,or_greater"
		),
		"set_geometric_error_scale",
		"get_geometric_error_scale"
	);
	ADD_PROPERTY(
		PropertyInfo(
			Variant::FLOAT,
			"maximum_attenuation",
			PROPERTY_HINT_RANGE,
			"0,256,0.1,or_greater,suffix:px"
		),
		"set_maximum_attenuation",
		"get_maximum_attenuation"
	);
	ADD_PROPERTY(
		PropertyInfo(
			Variant::FLOAT,
			"base_resolution",
			PROPERTY_HINT_RANGE,
			"0,1000,0.001,or_greater,suffix:m"
		),
		"set_base_resolution",
		"get_base_resolution"
	);
}
