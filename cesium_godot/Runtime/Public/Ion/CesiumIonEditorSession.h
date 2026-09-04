#ifndef CESIUM_ION_EDITOR_SESSION_H
#define CESIUM_ION_EDITOR_SESSION_H

#if defined(CESIUM_GD_MODULE)
#include "scene/main/node.h"
#include "core/variant/array.h"
#include "core/variant/callable.h"
#include "core/variant/packed_string_array.h"
#elif defined(CESIUM_GD_EXT)
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/callable.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
using namespace godot;
#endif

#include <cstdint>
#include <memory>

/**
 * Editor-only Cesium ion login session.
 *
 * Authorization is delegated to Cesium Native's PKCE/state-validated flow.
 * Login and refresh tokens live in the operating-system credential vault, not
 * in the project, EditorSettings, or user:// files. On platforms without an
 * available vault, the session remains usable in memory for the current editor
 * process but is deliberately not persisted.
 */
class CesiumIonEditorSession : public Node {
	GDCLASS(CesiumIonEditorSession, Node)

public:
	enum State {
		Disconnected = 0,
		Resuming = 1,
		Connecting = 2,
		Connected = 3,
		Error = 4,
	};

	CesiumIonEditorSession();
	~CesiumIonEditorSession() override;

	void _enter_tree() override;
	void _exit_tree() override;
	void _process(double delta) override;

	void connect_to_ion();
	void resume_session();
	void cancel_connection();
	void sign_out();
	void request_assets();
	void request_tokens();
	void create_token(const String& name, const PackedStringArray& scopes);

	int32_t get_state() const;
	bool get_is_connected() const;
	bool get_is_connecting() const;
	const String& get_error_message() const;
	const String& get_profile_name() const;
	const String& get_api_url() const;
	void set_api_url(const String& value);
	const String& get_server_url() const;
	void set_server_url(const String& value);
	int64_t get_oauth_application_id() const;
	void set_oauth_application_id(int64_t value);
	const String& get_oauth_redirect_path() const;
	void set_oauth_redirect_path(const String& value);
	Callable get_authorization_url_handler() const;
	void set_authorization_url_handler(const Callable& value);
	bool get_secure_storage_available() const;
	const String& get_secure_storage_backend() const;
	const String& get_secure_storage_error() const;

protected:
	static void _bind_methods();

private:
	struct Runtime;
	std::unique_ptr<Runtime> m_runtime;
	State m_state = Disconnected;
	String m_errorMessage;
	String m_profileName;
	String m_apiUrl = "https://api.cesium.com/";
	String m_serverUrl = "https://ion.cesium.com";
	int64_t m_oauthApplicationId = 891;
	String m_oauthRedirectPath = "/cesium-for-godot/oauth2/callback";
	Callable m_authorizationUrlHandler;
	bool m_secureStorageAvailable = false;
	String m_secureStorageBackend;
	String m_secureStorageError;
	bool m_resumeAttempted = false;

	void ensure_runtime();
	void shutdown_runtime();
	void begin_app_data(bool resume);
	void begin_authorization();
	void validate_connection();
	void set_state(State state, const String& error = String());
	void finish_connected(const String& profileName);
	void persist_session();
	void remove_legacy_plaintext_session();
	String credential_service() const;
	void prune_continuations();
};

VARIANT_ENUM_CAST(CesiumIonEditorSession::State);

#endif
