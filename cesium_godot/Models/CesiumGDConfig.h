#ifndef CESIUM_GD_CONFIG
#define CESIUM_GD_CONFIG

#include "godot_cpp/classes/node3d.hpp"
#if defined(CESIUM_GD_EXT)
#include <godot_cpp/classes/resource.hpp>
using namespace godot;
#elif defined(CESIUM_GD_MODULE)
#include "core/io/resource.h"
#endif

#include <string_view>

/// @brief Configuration resource for Cesium's server components
class CesiumGDConfig : public Node3D {
	GDCLASS(CesiumGDConfig, Node3D)
public:
	static constexpr std::string_view DEFAULT_ION_API_URL = "https://api.cesium.com/";
	static constexpr std::string_view DEFAULT_SERVER_URL = "https://ion.cesium.com";
	static constexpr int32_t DEFAULT_APPLICATION_ID = 891;

	CesiumGDConfig() = default;

	void set_access_token(const String& accessToken);

	const String& get_access_token() const;

	static CesiumGDConfig* get_singleton(Node* baseNode);

	static void clear_session();
		
	void _enter_tree() override;
	
private:
	Error create_cache_session_file();
	
	String m_accessToken = "";
	static inline CesiumGDConfig* s_instance = nullptr;
	
protected:
	static void _bind_methods();
};

#endif // CESIUM_GD_CONFIG
