// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Adapted for Godot from Cesium for Unreal under the Apache-2.0 license.

#include "Godot/Nodes/CesiumTileExcluder.h"

#include "Godot/Nodes/CesiumGDTileset.h"
#include "Runtime/Private/TileSelection/CesiumTileExcluderAdapter.h"
#include "Runtime/Public/TileSelection/CesiumTileExclusionContext.h"

CesiumTileExcluder::CesiumTileExcluder() = default;

CesiumTileExcluder::~CesiumTileExcluder() {
	this->remove_from_tileset();
}

void CesiumTileExcluder::set_enabled(bool enabled) {
	if (this->m_enabled == enabled) {
		return;
	}
	this->m_enabled = enabled;
	if (enabled) {
		this->add_to_tileset();
	} else {
		this->remove_from_tileset();
	}
}

bool CesiumTileExcluder::get_enabled() const {
	return this->m_enabled;
}

void CesiumTileExcluder::set_predicate(const Callable& predicate) {
	this->m_predicate = predicate;
}

Callable CesiumTileExcluder::get_predicate() const {
	return this->m_predicate;
}

Error CesiumTileExcluder::add_to_tileset(Cesium3DTileset* tileset) {
	if (!this->m_enabled) {
		return Error::ERR_UNAVAILABLE;
	}
	if (tileset == nullptr) {
		tileset = Object::cast_to<Cesium3DTileset>(this->get_parent());
	}
	if (tileset == nullptr) {
		return Error::ERR_INVALID_PARAMETER;
	}
	if (this->m_nativeAdapter != nullptr) {
		return tileset == this->resolve_tileset()
			? Error::OK
			: Error::ERR_ALREADY_IN_USE;
	}
	this->m_nativeAdapter = std::make_shared<CesiumTileExcluderAdapter>(
		ObjectID(this->get_instance_id())
	);
	this->m_tileset = tileset->get_instance_id();
	tileset->add_tile_excluder(this->m_nativeAdapter);
	return Error::OK;
}

void CesiumTileExcluder::remove_from_tileset(Cesium3DTileset* tileset) {
	if (this->m_nativeAdapter == nullptr) {
		this->m_tileset = ObjectID();
		return;
	}
	if (tileset == nullptr) {
		tileset = this->resolve_tileset();
	}
	if (tileset != nullptr) {
		tileset->remove_tile_excluder(this->m_nativeAdapter);
	}
	this->m_nativeAdapter.reset();
	this->m_tileset = ObjectID();
}

void CesiumTileExcluder::refresh() {
	Cesium3DTileset* tileset = this->resolve_tileset();
	this->remove_from_tileset(tileset);
	if (this->m_enabled && tileset != nullptr) {
		this->add_to_tileset(tileset);
	}
}

bool CesiumTileExcluder::is_added_to_tileset() const {
	return this->m_nativeAdapter != nullptr && this->resolve_tileset() != nullptr;
}

int64_t CesiumTileExcluder::get_frame_count() const {
	return static_cast<int64_t>(this->m_frameCount);
}

int64_t CesiumTileExcluder::get_evaluation_count() const {
	return static_cast<int64_t>(this->m_evaluationCount);
}

int64_t CesiumTileExcluder::get_excluded_count() const {
	return static_cast<int64_t>(this->m_excludedCount);
}

int64_t CesiumTileExcluder::get_invalid_result_count() const {
	return static_cast<int64_t>(this->m_invalidResultCount);
}

int64_t CesiumTileExcluder::get_last_frame_evaluation_count() const {
	return static_cast<int64_t>(this->m_lastFrameEvaluationCount);
}

int64_t CesiumTileExcluder::get_last_frame_excluded_count() const {
	return static_cast<int64_t>(this->m_lastFrameExcludedCount);
}

void CesiumTileExcluder::reset_statistics() {
	this->m_frameCount = 0;
	this->m_evaluationCount = 0;
	this->m_excludedCount = 0;
	this->m_invalidResultCount = 0;
	this->m_currentFrameEvaluationCount = 0;
	this->m_currentFrameExcludedCount = 0;
	this->m_lastFrameEvaluationCount = 0;
	this->m_lastFrameExcludedCount = 0;
}

void CesiumTileExcluder::begin_native_frame() {
	this->m_lastFrameEvaluationCount = this->m_currentFrameEvaluationCount;
	this->m_lastFrameExcludedCount = this->m_currentFrameExcludedCount;
	this->m_currentFrameEvaluationCount = 0;
	this->m_currentFrameExcludedCount = 0;
	++this->m_frameCount;
}

bool CesiumTileExcluder::evaluate_native_context(
	const Ref<CesiumTileExclusionContext>& context
) {
	++this->m_evaluationCount;
	++this->m_currentFrameEvaluationCount;

	const Callable& callback = this->m_predicate;
	if (!callback.is_valid()) {
		return false;
	}

	Array arguments;
	arguments.push_back(context);
	const Variant result = callback.callv(arguments);
	if (result.get_type() != Variant::BOOL) {
		++this->m_invalidResultCount;
		return false;
	}
	const bool excluded = static_cast<bool>(result);
	if (excluded) {
		++this->m_excludedCount;
		++this->m_currentFrameExcludedCount;
	}
	return excluded;
}

void CesiumTileExcluder::_ready() {
	if (this->m_enabled) {
		this->add_to_tileset();
	}
}

void CesiumTileExcluder::_exit_tree() {
	this->remove_from_tileset();
}

Cesium3DTileset* CesiumTileExcluder::resolve_tileset() const {
	if (!this->m_tileset.is_null()) {
		return Object::cast_to<Cesium3DTileset>(
			ObjectDB::get_instance(this->m_tileset)
		);
	}
	return Object::cast_to<Cesium3DTileset>(this->get_parent());
}

void CesiumTileExcluder::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_enabled", "enabled"), &CesiumTileExcluder::set_enabled);
	ClassDB::bind_method(D_METHOD("get_enabled"), &CesiumTileExcluder::get_enabled);
	ClassDB::bind_method(D_METHOD("set_predicate", "predicate"), &CesiumTileExcluder::set_predicate);
	ClassDB::bind_method(D_METHOD("get_predicate"), &CesiumTileExcluder::get_predicate);
	ClassDB::bind_method(D_METHOD("add_to_tileset", "tileset"), &CesiumTileExcluder::add_to_tileset, DEFVAL(nullptr));
	ClassDB::bind_method(D_METHOD("remove_from_tileset", "tileset"), &CesiumTileExcluder::remove_from_tileset, DEFVAL(nullptr));
	ClassDB::bind_method(D_METHOD("refresh"), &CesiumTileExcluder::refresh);
	ClassDB::bind_method(D_METHOD("is_added_to_tileset"), &CesiumTileExcluder::is_added_to_tileset);
	ClassDB::bind_method(D_METHOD("get_frame_count"), &CesiumTileExcluder::get_frame_count);
	ClassDB::bind_method(D_METHOD("get_evaluation_count"), &CesiumTileExcluder::get_evaluation_count);
	ClassDB::bind_method(D_METHOD("get_excluded_count"), &CesiumTileExcluder::get_excluded_count);
	ClassDB::bind_method(D_METHOD("get_invalid_result_count"), &CesiumTileExcluder::get_invalid_result_count);
	ClassDB::bind_method(D_METHOD("get_last_frame_evaluation_count"), &CesiumTileExcluder::get_last_frame_evaluation_count);
	ClassDB::bind_method(D_METHOD("get_last_frame_excluded_count"), &CesiumTileExcluder::get_last_frame_excluded_count);
	ClassDB::bind_method(D_METHOD("reset_statistics"), &CesiumTileExcluder::reset_statistics);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "enabled"), "set_enabled", "get_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::CALLABLE, "predicate"), "set_predicate", "get_predicate");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "added_to_tileset", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "is_added_to_tileset");
#define CESIUM_READ_ONLY_PROPERTY(type, name) \
	ADD_PROPERTY(PropertyInfo(type, #name, PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_" #name)
	CESIUM_READ_ONLY_PROPERTY(Variant::INT, frame_count);
	CESIUM_READ_ONLY_PROPERTY(Variant::INT, evaluation_count);
	CESIUM_READ_ONLY_PROPERTY(Variant::INT, excluded_count);
	CESIUM_READ_ONLY_PROPERTY(Variant::INT, invalid_result_count);
	CESIUM_READ_ONLY_PROPERTY(Variant::INT, last_frame_evaluation_count);
	CESIUM_READ_ONLY_PROPERTY(Variant::INT, last_frame_excluded_count);
#undef CESIUM_READ_ONLY_PROPERTY
}
