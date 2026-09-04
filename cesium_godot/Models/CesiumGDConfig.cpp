#include "CesiumGDConfig.h"
#include "Utils/AssetManipulation.h"
#include "godot_cpp/core/class_db.hpp"

void CesiumGDConfig::set_access_token(const String& accessToken)
{
	this->m_accessToken = accessToken;
}

const String& CesiumGDConfig::get_access_token() const
{
	return this->m_accessToken;
}

void CesiumGDConfig::clear_session() {
	if (s_instance != nullptr) s_instance->m_accessToken = String();
}

CesiumGDConfig* CesiumGDConfig::get_singleton(Node* baseNode) {
	if (s_instance != nullptr) {
		return s_instance;
	}
	s_instance = CesiumGodot::AssetManipulation::find_or_create_config_node(baseNode);
	return s_instance;
}

void CesiumGDConfig::_enter_tree() {
}

void CesiumGDConfig::_bind_methods()
{

	ClassDB::bind_method(D_METHOD("get_access_token"), &CesiumGDConfig::get_access_token);
	ClassDB::bind_method(D_METHOD("set_access_token", "accessToken"), &CesiumGDConfig::set_access_token);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "accessToken"), "set_access_token", "get_access_token");
	
	ClassDB::bind_static_method("CesiumGDConfig", D_METHOD("get_singleton", "baseNode"), CesiumGDConfig::get_singleton);
	
}
