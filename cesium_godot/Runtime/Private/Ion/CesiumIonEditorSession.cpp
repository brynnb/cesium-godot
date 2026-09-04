#include "Runtime/Public/Ion/CesiumIonEditorSession.h"

#include "Runtime/Private/Async/GodotTaskProcessor.h"
#include "Runtime/Private/Ion/CesiumEditorCredentialStore.h"
#include "Runtime/Private/Networking/NetworkAssetAccessor.h"

#include <CesiumAsync/AsyncSystem.h>
#include <CesiumAsync/Future.h>
#include <CesiumAsync/GunzipAssetAccessor.h>
#include <CesiumIonClient/ApplicationData.h>
#include <CesiumIonClient/Assets.h>
#include <CesiumIonClient/Connection.h>
#include <CesiumIonClient/LoginToken.h>
#include <CesiumIonClient/Profile.h>
#include <CesiumIonClient/Response.h>
#include <CesiumIonClient/Token.h>
#include <CesiumIonClient/TokenList.h>
#include <CesiumUtility/Result.h>

#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/object.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_int64_array.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>

#include <algorithm>
#include <exception>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {
constexpr const char* AccessAccount = "oauth-access-token";
constexpr const char* RefreshAccount = "oauth-refresh-token";

String normalize_url(const String& value, bool trailingSlash) {
	String result = value.strip_edges();
	while (result.ends_with("/")) result = result.left(-1);
	if (trailingSlash && !result.is_empty()) result += "/";
	return result;
}

String native_errors(const CesiumUtility::ErrorList& errors) {
	String result;
	for (const std::string& error : errors.errors) {
		if (!result.is_empty()) result += "\n";
		result += error.c_str();
	}
	return result.is_empty() ? String("Cesium ion authorization failed") : result;
}

String response_error(
	const String& operation,
	uint16_t status,
	const std::string& code,
	const std::string& message
) {
	String result = operation + String(" failed");
	if (status != 0) result += " (HTTP " + String::num_int64(status) + ")";
	if (!code.empty()) result += ": " + String(code.c_str());
	if (!message.empty()) {
		result += code.empty() ? ": " : " - ";
		result += message.c_str();
	}
	return result;
}

PackedStringArray to_packed_strings(const std::vector<std::string>& values) {
	PackedStringArray result;
	for (const std::string& value : values) result.push_back(value.c_str());
	return result;
}

Dictionary asset_to_dictionary(const CesiumIonClient::Asset& asset) {
	Dictionary result;
	result["id"] = asset.id;
	result["name"] = asset.name.c_str();
	result["description"] = asset.description.c_str();
	result["attribution"] = asset.attribution.c_str();
	result["type"] = asset.type.c_str();
	result["bytes"] = asset.bytes;
	result["date_added"] = asset.dateAdded.c_str();
	result["status"] = asset.status.c_str();
	result["percent_complete"] = static_cast<int32_t>(asset.percentComplete);
	return result;
}

Dictionary token_to_dictionary(const CesiumIonClient::Token& token) {
	Dictionary result;
	result["id"] = token.id.c_str();
	result["name"] = token.name.c_str();
	result["token"] = token.token.c_str();
	result["date_added"] = token.dateAdded.c_str();
	result["date_modified"] = token.dateModified.c_str();
	result["date_last_used"] = token.dateLastUsed.c_str();
	result["is_default"] = token.isDefault;
	result["scopes"] = to_packed_strings(token.scopes);
	if (token.assetIds) {
		PackedInt64Array ids;
		for (int64_t id : *token.assetIds) ids.push_back(id);
		result["asset_ids"] = ids;
	} else {
		result["asset_ids"] = Variant();
	}
	result["allowed_urls"] = token.allowedUrls
		? Variant(to_packed_strings(*token.allowedUrls))
		: Variant();
	return result;
}
} // namespace

struct CesiumIonEditorSession::Runtime {
	Runtime()
		: taskProcessor(std::make_shared<GodotTaskProcessor>(2)),
		asyncSystem(taskProcessor),
		networkAccessor(std::make_shared<NetworkAssetAccessor>(
			std::shared_ptr<CesiumTilesetRuntimeStatistics>(),
			std::shared_ptr<CesiumLoadFailureQueue>(),
			0,
			CesiumNetworkRetryOptions{}
		)),
		assetAccessor(std::make_shared<CesiumAsync::GunzipAssetAccessor>(
			networkAccessor
		)) {}

	std::shared_ptr<GodotTaskProcessor> taskProcessor;
	CesiumAsync::AsyncSystem asyncSystem;
	std::shared_ptr<NetworkAssetAccessor> networkAccessor;
	std::shared_ptr<CesiumAsync::IAssetAccessor> assetAccessor;
	std::optional<CesiumIonClient::ApplicationData> applicationData;
	std::optional<CesiumIonClient::Connection> connection;
	std::optional<CesiumIonClient::ConnectionAuthorization> authorization;
	std::vector<CesiumAsync::Future<void>> continuations;
	uint64_t generation = 0;
	bool resumeRequested = false;
	std::string storedAccessToken;
	std::string storedRefreshToken;
	bool assetsRequestPending = false;
	bool tokensRequestPending = false;
	bool createTokenRequestPending = false;
};

CesiumIonEditorSession::CesiumIonEditorSession() {
	this->set_process(false);
}

CesiumIonEditorSession::~CesiumIonEditorSession() {
	this->shutdown_runtime();
}

void CesiumIonEditorSession::_enter_tree() {
	if (!Engine::get_singleton()->is_editor_hint()) {
		this->set_process(false);
		return;
	}
	this->m_secureStorageBackend =
		CesiumGodot::EditorCredentials::backend_name();
	std::string storageError;
	this->m_secureStorageAvailable =
		CesiumGodot::EditorCredentials::is_available(storageError);
	this->m_secureStorageError = storageError.c_str();
	this->remove_legacy_plaintext_session();
	this->set_process(true);
	this->resume_session();
}

void CesiumIonEditorSession::_exit_tree() {
	this->set_process(false);
	this->shutdown_runtime();
}

void CesiumIonEditorSession::_process(double) {
	if (this->m_runtime == nullptr) return;
	this->m_runtime->networkAccessor->tick();
	this->m_runtime->asyncSystem.dispatchMainThreadTasks();
	this->prune_continuations();
}

void CesiumIonEditorSession::ensure_runtime() {
	if (this->m_runtime == nullptr) this->m_runtime = std::make_unique<Runtime>();
}

void CesiumIonEditorSession::connect_to_ion() {
	if (!Engine::get_singleton()->is_editor_hint()) {
		this->set_state(Error, "Cesium ion user login is available only in the editor");
		return;
	}
	this->cancel_connection();
	this->ensure_runtime();
	this->m_runtime->resumeRequested = false;
	this->set_state(Connecting);
	this->begin_app_data(false);
}

void CesiumIonEditorSession::resume_session() {
	if (this->m_resumeAttempted || !Engine::get_singleton()->is_editor_hint()) return;
	this->m_resumeAttempted = true;
	if (!this->m_secureStorageAvailable) return;
	const std::string service = this->credential_service().utf8().get_data();
	auto access = CesiumGodot::EditorCredentials::read(service, AccessAccount);
	auto refresh = CesiumGodot::EditorCredentials::read(service, RefreshAccount);
	if (!access.success || !refresh.success) {
		this->m_secureStorageError = (!access.success ? access.error : refresh.error).c_str();
		this->m_secureStorageAvailable = false;
		return;
	}
	if (!access.found || access.value.empty()) return;
	this->ensure_runtime();
	this->m_runtime->storedAccessToken = std::move(access.value);
	this->m_runtime->storedRefreshToken = refresh.found
		? std::move(refresh.value)
		: std::string();
	this->m_runtime->resumeRequested = true;
	this->set_state(Resuming);
	this->begin_app_data(true);
}

void CesiumIonEditorSession::begin_app_data(bool resume) {
	if (this->m_runtime == nullptr) return;
	const uint64_t generation = ++this->m_runtime->generation;
	const ObjectID selfId(this->get_instance_id());
	const std::string apiUrl = this->m_apiUrl.utf8().get_data();
	auto continuation = CesiumIonClient::Connection::appData(
		this->m_runtime->asyncSystem,
		this->m_runtime->assetAccessor,
		apiUrl
	).thenInMainThread([selfId, generation, resume](
		CesiumIonClient::Response<CesiumIonClient::ApplicationData>&& response
	) {
		CesiumIonEditorSession* self = Object::cast_to<CesiumIonEditorSession>(
			ObjectDB::get_instance(selfId)
		);
		if (self == nullptr || self->m_runtime == nullptr ||
			self->m_runtime->generation != generation) return;
		if (!response.value) {
			self->set_state(Error, String("Could not connect to Cesium ion: ") +
				response.errorMessage.c_str());
			return;
		}
		self->m_runtime->applicationData = std::move(*response.value);
		if (!self->m_runtime->applicationData->needsOauthAuthentication()) {
			self->m_runtime->connection.emplace(
				self->m_runtime->asyncSystem,
				self->m_runtime->assetAccessor,
				*self->m_runtime->applicationData,
				self->m_apiUrl.utf8().get_data()
			);
			self->finish_connected("Single-user ion server");
			return;
		}
		if (resume) {
			auto token = CesiumIonClient::LoginToken::parse(
				self->m_runtime->storedAccessToken
			);
			if (!token.value) {
				self->sign_out();
				self->set_state(Disconnected);
				return;
			}
			self->m_runtime->connection.emplace(
				self->m_runtime->asyncSystem,
				self->m_runtime->assetAccessor,
				*token.value,
				self->m_runtime->storedRefreshToken,
				self->m_oauthApplicationId,
				self->m_oauthRedirectPath.utf8().get_data(),
				*self->m_runtime->applicationData,
				self->m_apiUrl.utf8().get_data()
			);
			self->m_runtime->storedAccessToken.clear();
			self->m_runtime->storedRefreshToken.clear();
			self->validate_connection();
		} else {
			self->begin_authorization();
		}
	}).catchInMainThread([selfId, generation](std::exception&& exception) {
		CesiumIonEditorSession* self = Object::cast_to<CesiumIonEditorSession>(
			ObjectDB::get_instance(selfId)
		);
		if (self != nullptr && self->m_runtime != nullptr &&
			self->m_runtime->generation == generation) {
			self->set_state(Error, String("Could not connect to Cesium ion: ") + exception.what());
		}
	});
	this->m_runtime->continuations.emplace_back(std::move(continuation));
}

void CesiumIonEditorSession::begin_authorization() {
	if (this->m_runtime == nullptr || !this->m_runtime->applicationData) return;
	const uint64_t generation = this->m_runtime->generation;
	const ObjectID selfId(this->get_instance_id());
	const std::string authorizeUrl = normalize_url(this->m_serverUrl, false)
		.utf8().get_data() + std::string("/oauth");
	this->m_runtime->authorization = CesiumIonClient::Connection::authorizeCancelable(
		this->m_runtime->asyncSystem,
		this->m_runtime->assetAccessor,
		"Cesium for Godot",
		this->m_oauthApplicationId,
		this->m_oauthRedirectPath.utf8().get_data(),
		{"assets:list", "assets:read", "profile:read", "tokens:read", "tokens:write", "geocode"},
		[selfId](const std::string& url) {
			CesiumIonEditorSession* self = Object::cast_to<CesiumIonEditorSession>(
				ObjectDB::get_instance(selfId)
			);
			if (self != nullptr && self->m_authorizationUrlHandler.is_valid()) {
				self->m_authorizationUrlHandler.call(String(url.c_str()));
			} else {
				OS::get_singleton()->shell_open(url.c_str());
			}
		},
		*this->m_runtime->applicationData,
		this->m_apiUrl.utf8().get_data(),
		authorizeUrl
	);
	auto continuation = std::move(this->m_runtime->authorization->future)
		.thenInMainThread([selfId, generation](
			CesiumUtility::Result<CesiumIonClient::Connection>&& result
		) {
			CesiumIonEditorSession* self = Object::cast_to<CesiumIonEditorSession>(
				ObjectDB::get_instance(selfId)
			);
			if (self == nullptr || self->m_runtime == nullptr ||
				self->m_runtime->generation != generation) return;
			self->m_runtime->authorization.reset();
			if (!result.value) {
				const String message = native_errors(result.errors);
				if (message.to_lower().contains("canceled")) self->set_state(Disconnected);
				else self->set_state(Error, message);
				return;
			}
			self->m_runtime->connection = std::move(*result.value);
			self->validate_connection();
		}).catchInMainThread([selfId, generation](std::exception&& exception) {
			CesiumIonEditorSession* self = Object::cast_to<CesiumIonEditorSession>(
				ObjectDB::get_instance(selfId)
			);
			if (self != nullptr && self->m_runtime != nullptr &&
				self->m_runtime->generation == generation) {
				self->m_runtime->authorization.reset();
				self->set_state(Error, String("Cesium ion authorization failed: ") + exception.what());
			}
		});
	this->m_runtime->continuations.emplace_back(std::move(continuation));
}

void CesiumIonEditorSession::validate_connection() {
	if (this->m_runtime == nullptr || !this->m_runtime->connection) return;
	const uint64_t generation = this->m_runtime->generation;
	const ObjectID selfId(this->get_instance_id());
	auto continuation = this->m_runtime->connection->me()
		.thenInMainThread([selfId, generation](
			CesiumIonClient::Response<CesiumIonClient::Profile>&& response
		) {
			CesiumIonEditorSession* self = Object::cast_to<CesiumIonEditorSession>(
				ObjectDB::get_instance(selfId)
			);
			if (self == nullptr || self->m_runtime == nullptr ||
				self->m_runtime->generation != generation) return;
			if (!response.value) {
				self->sign_out();
				self->set_state(Error, String("Cesium ion sign-in could not be resumed: ") + response.errorMessage.c_str());
				return;
			}
			const std::string& username = response.value->username;
			self->finish_connected(
				username.empty() ? response.value->email.c_str() : username.c_str()
			);
		}).catchInMainThread([selfId, generation](std::exception&& exception) {
			CesiumIonEditorSession* self = Object::cast_to<CesiumIonEditorSession>(
				ObjectDB::get_instance(selfId)
			);
			if (self != nullptr && self->m_runtime != nullptr &&
				self->m_runtime->generation == generation) {
				self->set_state(Error, String("Cesium ion sign-in validation failed: ") + exception.what());
			}
		});
	this->m_runtime->continuations.emplace_back(std::move(continuation));
}

void CesiumIonEditorSession::finish_connected(const String& profileName) {
	this->m_profileName = profileName;
	this->persist_session();
	this->set_state(Connected);
}

void CesiumIonEditorSession::persist_session() {
	if (!this->m_secureStorageAvailable || this->m_runtime == nullptr ||
		!this->m_runtime->connection) return;
	const std::string service = this->credential_service().utf8().get_data();
	auto access = CesiumGodot::EditorCredentials::write(
		service, AccessAccount, this->m_runtime->connection->getAccessToken()
	);
	auto refresh = CesiumGodot::EditorCredentials::write(
		service, RefreshAccount, this->m_runtime->connection->getRefreshToken()
	);
	if (!access.success || !refresh.success) {
		// Never leave a half-written resumable session behind.
		CesiumGodot::EditorCredentials::remove(service, AccessAccount);
		CesiumGodot::EditorCredentials::remove(service, RefreshAccount);
		this->m_secureStorageError = (!access.success ? access.error : refresh.error).c_str();
		this->m_secureStorageAvailable = false;
	}
}

void CesiumIonEditorSession::cancel_connection() {
	if (this->m_runtime == nullptr) return;
	++this->m_runtime->generation;
	if (this->m_runtime->assetsRequestPending) {
		this->emit_signal("operation_completed", "assets", false, Variant(), "Cesium ion asset request was canceled");
	}
	if (this->m_runtime->tokensRequestPending) {
		this->emit_signal("operation_completed", "tokens", false, Variant(), "Cesium ion token request was canceled");
	}
	if (this->m_runtime->createTokenRequestPending) {
		this->emit_signal("operation_completed", "create_token", false, Variant(), "Cesium ion token creation was canceled");
	}
	this->m_runtime->assetsRequestPending = false;
	this->m_runtime->tokensRequestPending = false;
	this->m_runtime->createTokenRequestPending = false;
	if (this->m_runtime->authorization && this->m_runtime->authorization->cancel) {
		this->m_runtime->authorization->cancel();
	}
	this->m_runtime->authorization.reset();
	if (this->m_state == Connecting || this->m_state == Resuming) {
		this->set_state(Disconnected);
	}
}

void CesiumIonEditorSession::sign_out() {
	this->cancel_connection();
	if (this->m_runtime != nullptr) {
		this->m_runtime->connection.reset();
		this->m_runtime->storedAccessToken.clear();
		this->m_runtime->storedRefreshToken.clear();
	}
	this->m_profileName = String();
	const std::string service = this->credential_service().utf8().get_data();
	CesiumGodot::EditorCredentials::remove(service, AccessAccount);
	CesiumGodot::EditorCredentials::remove(service, RefreshAccount);
	this->set_state(Disconnected);
}

void CesiumIonEditorSession::request_assets() {
	if (this->m_runtime == nullptr || !this->m_runtime->connection || this->m_state != Connected) {
		this->emit_signal("request_failed", "assets", "Sign in to Cesium ion before listing assets");
		this->emit_signal("operation_completed", "assets", false, Variant(), "Sign in to Cesium ion before listing assets");
		return;
	}
	if (this->m_runtime->assetsRequestPending) {
		this->emit_signal("request_failed", "assets", "A Cesium ion asset request is already in progress");
		return;
	}
	this->m_runtime->assetsRequestPending = true;
	const uint64_t generation = this->m_runtime->generation;
	const ObjectID selfId(this->get_instance_id());
	auto continuation = this->m_runtime->connection->assets()
		.thenInMainThread([selfId, generation](CesiumIonClient::Response<CesiumIonClient::Assets>&& response) {
			CesiumIonEditorSession* self = Object::cast_to<CesiumIonEditorSession>(ObjectDB::get_instance(selfId));
			if (self == nullptr || self->m_runtime == nullptr || self->m_runtime->generation != generation) return;
			self->m_runtime->assetsRequestPending = false;
			self->persist_session();
			if (!response.value) {
				const String error = response_error("Cesium ion asset request", response.httpStatusCode, response.errorCode, response.errorMessage);
				self->emit_signal("request_failed", "assets", error);
				self->emit_signal("operation_completed", "assets", false, Variant(), error);
				return;
			}
			Array assets;
			for (const CesiumIonClient::Asset& asset : response.value->items) assets.push_back(asset_to_dictionary(asset));
			self->emit_signal("assets_received", assets);
			self->emit_signal("operation_completed", "assets", true, assets, String());
		}).catchInMainThread([selfId, generation](std::exception&& exception) {
			CesiumIonEditorSession* self = Object::cast_to<CesiumIonEditorSession>(ObjectDB::get_instance(selfId));
			if (self != nullptr && self->m_runtime != nullptr && self->m_runtime->generation == generation) {
				self->m_runtime->assetsRequestPending = false;
				const String error = String("Cesium ion asset request failed: ") + exception.what();
				self->emit_signal("request_failed", "assets", error);
				self->emit_signal("operation_completed", "assets", false, Variant(), error);
			}
		});
	this->m_runtime->continuations.emplace_back(std::move(continuation));
}

void CesiumIonEditorSession::request_tokens() {
	if (this->m_runtime == nullptr || !this->m_runtime->connection || this->m_state != Connected) {
		this->emit_signal("request_failed", "tokens", "Sign in to Cesium ion before listing account tokens");
		this->emit_signal("operation_completed", "tokens", false, Variant(), "Sign in to Cesium ion before listing account tokens");
		return;
	}
	if (this->m_runtime->tokensRequestPending) {
		this->emit_signal("request_failed", "tokens", "A Cesium ion token request is already in progress");
		return;
	}
	this->m_runtime->tokensRequestPending = true;
	const uint64_t generation = this->m_runtime->generation;
	const ObjectID selfId(this->get_instance_id());
	auto continuation = this->m_runtime->connection->tokens()
		.thenInMainThread([selfId, generation](CesiumIonClient::Response<CesiumIonClient::TokenList>&& response) {
			CesiumIonEditorSession* self = Object::cast_to<CesiumIonEditorSession>(ObjectDB::get_instance(selfId));
			if (self == nullptr || self->m_runtime == nullptr || self->m_runtime->generation != generation) return;
			self->m_runtime->tokensRequestPending = false;
			self->persist_session();
			if (!response.value) {
				const String error = response_error("Cesium ion token request", response.httpStatusCode, response.errorCode, response.errorMessage);
				self->emit_signal("request_failed", "tokens", error);
				self->emit_signal("operation_completed", "tokens", false, Variant(), error);
				return;
			}
			Array tokens;
			for (const CesiumIonClient::Token& token : response.value->items) tokens.push_back(token_to_dictionary(token));
			self->emit_signal("tokens_received", tokens);
			self->emit_signal("operation_completed", "tokens", true, tokens, String());
		}).catchInMainThread([selfId, generation](std::exception&& exception) {
			CesiumIonEditorSession* self = Object::cast_to<CesiumIonEditorSession>(ObjectDB::get_instance(selfId));
			if (self != nullptr && self->m_runtime != nullptr && self->m_runtime->generation == generation) {
				self->m_runtime->tokensRequestPending = false;
				const String error = String("Cesium ion token request failed: ") + exception.what();
				self->emit_signal("request_failed", "tokens", error);
				self->emit_signal("operation_completed", "tokens", false, Variant(), error);
			}
		});
	this->m_runtime->continuations.emplace_back(std::move(continuation));
}

void CesiumIonEditorSession::create_token(const String& name, const PackedStringArray& scopes) {
	if (this->m_runtime == nullptr || !this->m_runtime->connection || this->m_state != Connected) {
		this->emit_signal("request_failed", "create_token", "Sign in to Cesium ion before creating an account token");
		this->emit_signal("operation_completed", "create_token", false, Variant(), "Sign in to Cesium ion before creating an account token");
		return;
	}
	if (this->m_runtime->createTokenRequestPending) {
		this->emit_signal("request_failed", "create_token", "Cesium ion token creation is already in progress");
		return;
	}
	this->m_runtime->createTokenRequestPending = true;
	std::vector<std::string> nativeScopes;
	nativeScopes.reserve(scopes.size());
	for (int64_t i = 0; i < scopes.size(); ++i) nativeScopes.emplace_back(scopes[i].utf8().get_data());
	const uint64_t generation = this->m_runtime->generation;
	const ObjectID selfId(this->get_instance_id());
	auto continuation = this->m_runtime->connection->createToken(name.utf8().get_data(), nativeScopes)
		.thenInMainThread([selfId, generation](CesiumIonClient::Response<CesiumIonClient::Token>&& response) {
			CesiumIonEditorSession* self = Object::cast_to<CesiumIonEditorSession>(ObjectDB::get_instance(selfId));
			if (self == nullptr || self->m_runtime == nullptr || self->m_runtime->generation != generation) return;
			self->m_runtime->createTokenRequestPending = false;
			self->persist_session();
			if (!response.value) {
				const String error = response_error("Cesium ion token creation", response.httpStatusCode, response.errorCode, response.errorMessage);
				self->emit_signal("request_failed", "create_token", error);
				self->emit_signal("operation_completed", "create_token", false, Variant(), error);
				return;
			}
			const Dictionary token = token_to_dictionary(*response.value);
			self->emit_signal("token_created", token);
			self->emit_signal("operation_completed", "create_token", true, token, String());
		}).catchInMainThread([selfId, generation](std::exception&& exception) {
			CesiumIonEditorSession* self = Object::cast_to<CesiumIonEditorSession>(ObjectDB::get_instance(selfId));
			if (self != nullptr && self->m_runtime != nullptr && self->m_runtime->generation == generation) {
				self->m_runtime->createTokenRequestPending = false;
				const String error = String("Cesium ion token creation failed: ") + exception.what();
				self->emit_signal("request_failed", "create_token", error);
				self->emit_signal("operation_completed", "create_token", false, Variant(), error);
			}
		});
	this->m_runtime->continuations.emplace_back(std::move(continuation));
}

void CesiumIonEditorSession::set_state(State state, const String& error) {
	const bool changed = this->m_state != state || this->m_errorMessage != error;
	this->m_state = state;
	this->m_errorMessage = error;
	if (changed) this->emit_signal("state_changed", static_cast<int32_t>(state));
	if (state == Error && !error.is_empty()) this->emit_signal("authorization_failed", error);
	if (state == Connected) this->emit_signal("connected", this->m_profileName);
	if (state == Disconnected) this->emit_signal("disconnected");
}

int32_t CesiumIonEditorSession::get_state() const { return this->m_state; }
bool CesiumIonEditorSession::get_is_connected() const { return this->m_state == Connected; }
bool CesiumIonEditorSession::get_is_connecting() const { return this->m_state == Connecting || this->m_state == Resuming; }
const String& CesiumIonEditorSession::get_error_message() const { return this->m_errorMessage; }
const String& CesiumIonEditorSession::get_profile_name() const { return this->m_profileName; }
const String& CesiumIonEditorSession::get_api_url() const { return this->m_apiUrl; }
const String& CesiumIonEditorSession::get_server_url() const { return this->m_serverUrl; }
int64_t CesiumIonEditorSession::get_oauth_application_id() const { return this->m_oauthApplicationId; }
const String& CesiumIonEditorSession::get_oauth_redirect_path() const { return this->m_oauthRedirectPath; }
Callable CesiumIonEditorSession::get_authorization_url_handler() const { return this->m_authorizationUrlHandler; }
bool CesiumIonEditorSession::get_secure_storage_available() const { return this->m_secureStorageAvailable; }
const String& CesiumIonEditorSession::get_secure_storage_backend() const { return this->m_secureStorageBackend; }
const String& CesiumIonEditorSession::get_secure_storage_error() const { return this->m_secureStorageError; }

void CesiumIonEditorSession::set_api_url(const String& value) {
	const String normalized = normalize_url(value, true);
	if (normalized == this->m_apiUrl) return;
	// Exported properties are commonly configured before the Node enters the
	// tree. That must not mutate an existing credential-vault session.
	if (this->is_inside_tree()) this->sign_out();
	this->m_apiUrl = normalized;
	this->m_resumeAttempted = false;
}

void CesiumIonEditorSession::set_server_url(const String& value) {
	const String normalized = normalize_url(value, false);
	if (normalized == this->m_serverUrl) return;
	if (this->is_inside_tree()) this->sign_out();
	this->m_serverUrl = normalized;
	this->m_resumeAttempted = false;
}

void CesiumIonEditorSession::set_oauth_application_id(int64_t value) {
	this->m_oauthApplicationId = std::max<int64_t>(1, value);
}

void CesiumIonEditorSession::set_oauth_redirect_path(const String& value) {
	String normalized = value.strip_edges();
	if (!normalized.begins_with("/")) normalized = "/" + normalized;
	this->m_oauthRedirectPath = normalized;
}

void CesiumIonEditorSession::set_authorization_url_handler(const Callable& value) {
	this->m_authorizationUrlHandler = value;
}

String CesiumIonEditorSession::credential_service() const {
	return normalize_url(this->m_apiUrl, false).uri_encode();
}

void CesiumIonEditorSession::remove_legacy_plaintext_session() {
	const String path = ProjectSettings::get_singleton()->globalize_path(
		"user://cache/ion_session.dat"
	);
	if (FileAccess::file_exists(path)) DirAccess::remove_absolute(path);
}

void CesiumIonEditorSession::prune_continuations() {
	if (this->m_runtime == nullptr) return;
	auto iterator = this->m_runtime->continuations.begin();
	while (iterator != this->m_runtime->continuations.end()) {
		if (!iterator->isReady()) { ++iterator; continue; }
		iterator->wait();
		iterator = this->m_runtime->continuations.erase(iterator);
	}
}

void CesiumIonEditorSession::shutdown_runtime() {
	if (this->m_runtime == nullptr) return;
	this->cancel_connection();
	this->m_runtime->networkAccessor->cancel_all();
	this->m_runtime->networkAccessor->tick();
	std::vector<CesiumAsync::Future<void>> continuations =
		std::move(this->m_runtime->continuations);
	for (auto& continuation : continuations) {
		this->m_runtime->networkAccessor->tick();
		continuation.waitInMainThread();
	}
	this->m_runtime->networkAccessor->tick();
	this->m_runtime->asyncSystem.dispatchMainThreadTasks();
	this->m_runtime->taskProcessor->shutdown();
	this->m_runtime.reset();
}

void CesiumIonEditorSession::_bind_methods() {
	ClassDB::bind_method(D_METHOD("connect_to_ion"), &CesiumIonEditorSession::connect_to_ion);
	ClassDB::bind_method(D_METHOD("resume_session"), &CesiumIonEditorSession::resume_session);
	ClassDB::bind_method(D_METHOD("cancel_connection"), &CesiumIonEditorSession::cancel_connection);
	ClassDB::bind_method(D_METHOD("sign_out"), &CesiumIonEditorSession::sign_out);
	ClassDB::bind_method(D_METHOD("request_assets"), &CesiumIonEditorSession::request_assets);
	ClassDB::bind_method(D_METHOD("request_tokens"), &CesiumIonEditorSession::request_tokens);
	ClassDB::bind_method(D_METHOD("create_token", "name", "scopes"), &CesiumIonEditorSession::create_token);
	ClassDB::bind_method(D_METHOD("get_state"), &CesiumIonEditorSession::get_state);
	ClassDB::bind_method(D_METHOD("get_is_connected"), &CesiumIonEditorSession::get_is_connected);
	ClassDB::bind_method(D_METHOD("get_is_connecting"), &CesiumIonEditorSession::get_is_connecting);
	ClassDB::bind_method(D_METHOD("get_error_message"), &CesiumIonEditorSession::get_error_message);
	ClassDB::bind_method(D_METHOD("get_profile_name"), &CesiumIonEditorSession::get_profile_name);
	ClassDB::bind_method(D_METHOD("get_api_url"), &CesiumIonEditorSession::get_api_url);
	ClassDB::bind_method(D_METHOD("set_api_url", "value"), &CesiumIonEditorSession::set_api_url);
	ClassDB::bind_method(D_METHOD("get_server_url"), &CesiumIonEditorSession::get_server_url);
	ClassDB::bind_method(D_METHOD("set_server_url", "value"), &CesiumIonEditorSession::set_server_url);
	ClassDB::bind_method(D_METHOD("get_oauth_application_id"), &CesiumIonEditorSession::get_oauth_application_id);
	ClassDB::bind_method(D_METHOD("set_oauth_application_id", "value"), &CesiumIonEditorSession::set_oauth_application_id);
	ClassDB::bind_method(D_METHOD("get_oauth_redirect_path"), &CesiumIonEditorSession::get_oauth_redirect_path);
	ClassDB::bind_method(D_METHOD("set_oauth_redirect_path", "value"), &CesiumIonEditorSession::set_oauth_redirect_path);
	ClassDB::bind_method(D_METHOD("get_authorization_url_handler"), &CesiumIonEditorSession::get_authorization_url_handler);
	ClassDB::bind_method(D_METHOD("set_authorization_url_handler", "value"), &CesiumIonEditorSession::set_authorization_url_handler);
	ClassDB::bind_method(D_METHOD("get_secure_storage_available"), &CesiumIonEditorSession::get_secure_storage_available);
	ClassDB::bind_method(D_METHOD("get_secure_storage_backend"), &CesiumIonEditorSession::get_secure_storage_backend);
	ClassDB::bind_method(D_METHOD("get_secure_storage_error"), &CesiumIonEditorSession::get_secure_storage_error);

	BIND_ENUM_CONSTANT(Disconnected);
	BIND_ENUM_CONSTANT(Resuming);
	BIND_ENUM_CONSTANT(Connecting);
	BIND_ENUM_CONSTANT(Connected);
	BIND_ENUM_CONSTANT(Error);
	ADD_SIGNAL(MethodInfo("state_changed", PropertyInfo(Variant::INT, "state")));
	ADD_SIGNAL(MethodInfo("connected", PropertyInfo(Variant::STRING, "profile_name")));
	ADD_SIGNAL(MethodInfo("disconnected"));
	ADD_SIGNAL(MethodInfo("authorization_failed", PropertyInfo(Variant::STRING, "message")));
	ADD_SIGNAL(MethodInfo("assets_received", PropertyInfo(Variant::ARRAY, "assets")));
	ADD_SIGNAL(MethodInfo("tokens_received", PropertyInfo(Variant::ARRAY, "tokens")));
	ADD_SIGNAL(MethodInfo("token_created", PropertyInfo(Variant::DICTIONARY, "token")));
	ADD_SIGNAL(MethodInfo("request_failed", PropertyInfo(Variant::STRING, "operation"), PropertyInfo(Variant::STRING, "message")));
	ADD_SIGNAL(MethodInfo("operation_completed", PropertyInfo(Variant::STRING, "operation"), PropertyInfo(Variant::BOOL, "success"), PropertyInfo(Variant::NIL, "payload"), PropertyInfo(Variant::STRING, "error")));
	ADD_PROPERTY(PropertyInfo(Variant::INT, "state", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_state");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "is_connected", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_is_connected");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "is_connecting", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_is_connecting");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "error_message", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_error_message");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "profile_name", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_profile_name");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "api_url"), "set_api_url", "get_api_url");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "server_url"), "set_server_url", "get_server_url");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "oauth_application_id"), "set_oauth_application_id", "get_oauth_application_id");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "oauth_redirect_path"), "set_oauth_redirect_path", "get_oauth_redirect_path");
	ADD_PROPERTY(PropertyInfo(Variant::CALLABLE, "authorization_url_handler"), "set_authorization_url_handler", "get_authorization_url_handler");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "secure_storage_available", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_secure_storage_available");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "secure_storage_backend", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_secure_storage_backend");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "secure_storage_error", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_secure_storage_error");
}
