#include "CesiumGDPanel.h"

#if defined(CESIUM_GD_EXT)
#include "godot_cpp/classes/os.hpp"
#elif defined(CESIUM_GD_MODULE)
#include "core/os/os.h"
#endif

void CesiumGDPanel::open_learn_page()
{
	OS::get_singleton()->shell_open("https://cesium.com/learn/");
}

void CesiumGDPanel::open_help_page()
{
	OS::get_singleton()->shell_open("https://community.cesium.com/");
}

void CesiumGDPanel::_bind_methods()
{
	ClassDB::bind_static_method("CesiumGDPanel", D_METHOD("open_learn_page"), &CesiumGDPanel::open_learn_page);
	ClassDB::bind_static_method("CesiumGDPanel", D_METHOD("open_help_page"), &CesiumGDPanel::open_help_page);
}
