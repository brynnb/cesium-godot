// Credentials are editor-user data, never project data. This file talks to
// each desktop OS vault directly. Linux resolves libsecret dynamically so a
// runtime-only game does not gain a hard libsecret dependency.

#include "Runtime/Private/Ion/CesiumEditorCredentialStore.h"

#include <cstring>
#include <memory>
#include <string>

#if defined(WINDOWS_ENABLED)
#define UNICODE
#include <windows.h>
#include <wincred.h>
#elif defined(MACOS_ENABLED)
#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>
#elif defined(LINUX_ENABLED) && !defined(ANDROID_ENABLED)
#include <dlfcn.h>
#endif

namespace CesiumGodot::EditorCredentials {
namespace {
constexpr const char* Package = "org.cesium.cesium-for-godot";

#if defined(WINDOWS_ENABLED)
std::wstring wide(const std::string& value) {
	if (value.empty()) return {};
	const int count = MultiByteToWideChar(
		CP_UTF8, 0, value.c_str(), -1, nullptr, 0
	);
	if (count <= 0) return {};
	std::wstring result(static_cast<size_t>(count), L'\0');
	MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, result.data(), count);
	result.resize(static_cast<size_t>(count - 1));
	return result;
}

std::wstring target(const std::string& service, const std::string& account) {
	return wide(std::string(Package) + "." + service + "/" + account);
}

std::string windows_error(DWORD code) {
	return "Windows Credential Manager error " + std::to_string(code);
}
#elif defined(MACOS_ENABLED)
class ScopedCF {
public:
	explicit ScopedCF(CFTypeRef value = nullptr) : value(value) {}
	~ScopedCF() { if (this->value != nullptr) CFRelease(this->value); }
	ScopedCF(const ScopedCF&) = delete;
	ScopedCF& operator=(const ScopedCF&) = delete;
	CFTypeRef get() const { return this->value; }
private:
	CFTypeRef value;
};

CFStringRef cf_string(const std::string& value) {
	return CFStringCreateWithCString(
		kCFAllocatorDefault, value.c_str(), kCFStringEncodingUTF8
	);
}

CFMutableDictionaryRef query(
	const std::string& service,
	const std::string& account
) {
	CFMutableDictionaryRef result = CFDictionaryCreateMutable(
		kCFAllocatorDefault, 0,
		&kCFTypeDictionaryKeyCallBacks,
		&kCFTypeDictionaryValueCallBacks
	);
	if (result == nullptr) return nullptr;
	ScopedCF serviceValue(cf_string(std::string(Package) + "." + service));
	ScopedCF accountValue(cf_string(account));
	if (serviceValue.get() == nullptr || accountValue.get() == nullptr) {
		CFRelease(result);
		return nullptr;
	}
	CFDictionaryAddValue(result, kSecClass, kSecClassGenericPassword);
	CFDictionaryAddValue(result, kSecAttrService, serviceValue.get());
	CFDictionaryAddValue(result, kSecAttrAccount, accountValue.get());
	return result;
}

std::string mac_error(OSStatus status) {
	return "macOS Keychain error " + std::to_string(status);
}
#elif defined(LINUX_ENABLED) && !defined(ANDROID_ENABLED)
using gboolean = int;
using gint = int;
using GQuark = unsigned int;
using gpointer = void*;
struct GError { GQuark domain; gint code; char* message; };
struct SecretSchemaAttribute { const char* name; int type; };
struct SecretSchema {
	const char* name;
	int flags;
	SecretSchemaAttribute attributes[32];
	gint reserved;
	gpointer reserved1;
	gpointer reserved2;
	gpointer reserved3;
	gpointer reserved4;
	gpointer reserved5;
	gpointer reserved6;
	gpointer reserved7;
};

struct SecretApi {
	using Store = gboolean (*)(
		const SecretSchema*, const char*, const char*, const char*,
		void*, GError**, ...
	);
	using Lookup = char* (*)(const SecretSchema*, void*, GError**, ...);
	using Clear = gboolean (*)(const SecretSchema*, void*, GError**, ...);
	using FreePassword = void (*)(char*);
	using FreeError = void (*)(GError*);

	void* secret = nullptr;
	void* glib = nullptr;
	Store store = nullptr;
	Lookup lookup = nullptr;
	Clear clear = nullptr;
	FreePassword freePassword = nullptr;
	FreeError freeError = nullptr;
	std::string error;

	SecretApi() {
		this->secret = dlopen("libsecret-1.so.0", RTLD_NOW | RTLD_LOCAL);
		this->glib = dlopen("libglib-2.0.so.0", RTLD_NOW | RTLD_LOCAL);
		if (this->secret == nullptr || this->glib == nullptr) {
			this->error = "Secret Service support is unavailable (libsecret is not installed)";
			return;
		}
		this->store = reinterpret_cast<Store>(dlsym(this->secret, "secret_password_store_sync"));
		this->lookup = reinterpret_cast<Lookup>(dlsym(this->secret, "secret_password_lookup_sync"));
		this->clear = reinterpret_cast<Clear>(dlsym(this->secret, "secret_password_clear_sync"));
		this->freePassword = reinterpret_cast<FreePassword>(dlsym(this->secret, "secret_password_free"));
		this->freeError = reinterpret_cast<FreeError>(dlsym(this->glib, "g_error_free"));
		if (!this->store || !this->lookup || !this->clear ||
			!this->freePassword || !this->freeError) {
			this->error = "Secret Service support is incomplete";
		}
	}

	~SecretApi() {
		if (this->glib != nullptr) dlclose(this->glib);
		if (this->secret != nullptr) dlclose(this->secret);
	}

	bool ready() const { return this->error.empty(); }
};

SecretApi& secret_api() {
	static SecretApi api;
	return api;
}

SecretSchema schema() {
	SecretSchema result{};
	result.name = Package;
	result.attributes[0] = {"service", 0};
	result.attributes[1] = {"account", 0};
	return result;
}

std::string consume_error(SecretApi& api, GError* error) {
	if (error == nullptr) return {};
	const std::string message = error->message != nullptr
		? error->message
		: "Secret Service operation failed";
	api.freeError(error);
	return message;
}
#endif
} // namespace

const char* backend_name() noexcept {
#if defined(WINDOWS_ENABLED)
	return "Windows Credential Manager";
#elif defined(MACOS_ENABLED)
	return "macOS Keychain";
#elif defined(LINUX_ENABLED) && !defined(ANDROID_ENABLED)
	return "Secret Service";
#else
	return "memory only";
#endif
}

bool is_available(std::string& error) noexcept {
#if defined(WINDOWS_ENABLED) || defined(MACOS_ENABLED)
	error.clear();
	return true;
#elif defined(LINUX_ENABLED) && !defined(ANDROID_ENABLED)
	SecretApi& api = secret_api();
	error = api.error;
	return api.ready();
#else
	error = "No secure credential vault is available on this platform";
	return false;
#endif
}

Result read(const std::string& service, const std::string& account) noexcept {
#if defined(WINDOWS_ENABLED)
	const std::wstring key = target(service, account);
	PCREDENTIALW credential = nullptr;
	if (!CredReadW(key.c_str(), CRED_TYPE_GENERIC, 0, &credential)) {
		const DWORD code = GetLastError();
		return {code == ERROR_NOT_FOUND, false, {}, code == ERROR_NOT_FOUND ? "" : windows_error(code)};
	}
	std::string value(
		reinterpret_cast<const char*>(credential->CredentialBlob),
		credential->CredentialBlobSize
	);
	CredFree(credential);
	return {true, true, std::move(value), {}};
#elif defined(MACOS_ENABLED)
	ScopedCF q(query(service, account));
	if (q.get() == nullptr) return {false, false, {}, "Could not create Keychain query"};
	CFDictionaryAddValue(
		static_cast<CFMutableDictionaryRef>(const_cast<void*>(q.get())),
		kSecReturnData, kCFBooleanTrue
	);
	CFTypeRef raw = nullptr;
	const OSStatus status = SecItemCopyMatching(
		static_cast<CFDictionaryRef>(q.get()), &raw
	);
	if (status == errSecItemNotFound) return {true, false, {}, {}};
	if (status != errSecSuccess) return {false, false, {}, mac_error(status)};
	ScopedCF data(raw);
	const CFDataRef bytes = static_cast<CFDataRef>(raw);
	return {true, true, std::string(
		reinterpret_cast<const char*>(CFDataGetBytePtr(bytes)),
		static_cast<size_t>(CFDataGetLength(bytes))
	), {}};
#elif defined(LINUX_ENABLED) && !defined(ANDROID_ENABLED)
	SecretApi& api = secret_api();
	if (!api.ready()) return {false, false, {}, api.error};
	SecretSchema keySchema = schema();
	GError* error = nullptr;
	char* raw = api.lookup(
		&keySchema, nullptr, &error,
		"service", service.c_str(), "account", account.c_str(), nullptr
	);
	const std::string message = consume_error(api, error);
	if (!message.empty()) return {false, false, {}, message};
	if (raw == nullptr) return {true, false, {}, {}};
	std::string value(raw);
	api.freePassword(raw);
	return {true, true, std::move(value), {}};
#else
	return {false, false, {}, "Secure credential storage is unavailable"};
#endif
}

Result write(
	const std::string& service,
	const std::string& account,
	const std::string& value
) noexcept {
#if defined(WINDOWS_ENABLED)
	if (value.size() > CRED_MAX_CREDENTIAL_BLOB_SIZE) {
		return {false, false, {}, "Credential is too large for Windows Credential Manager"};
	}
	const std::wstring key = target(service, account);
	const std::wstring user = wide(account);
	CREDENTIALW credential{};
	credential.Type = CRED_TYPE_GENERIC;
	credential.TargetName = const_cast<wchar_t*>(key.c_str());
	credential.UserName = const_cast<wchar_t*>(user.c_str());
	credential.CredentialBlobSize = static_cast<DWORD>(value.size());
	credential.CredentialBlob = reinterpret_cast<LPBYTE>(
		const_cast<char*>(value.data())
	);
	credential.Persist = CRED_PERSIST_LOCAL_MACHINE;
	if (!CredWriteW(&credential, 0)) {
		return {false, false, {}, windows_error(GetLastError())};
	}
	return {true, true, {}, {}};
#elif defined(MACOS_ENABLED)
	ScopedCF q(query(service, account));
	if (q.get() == nullptr) return {false, false, {}, "Could not create Keychain query"};
	ScopedCF data(CFDataCreate(
		kCFAllocatorDefault,
		reinterpret_cast<const UInt8*>(value.data()),
		static_cast<CFIndex>(value.size())
	));
	CFDictionaryAddValue(
		static_cast<CFMutableDictionaryRef>(const_cast<void*>(q.get())),
		kSecValueData, data.get()
	);
	OSStatus status = SecItemAdd(static_cast<CFDictionaryRef>(q.get()), nullptr);
	if (status == errSecDuplicateItem) {
		CFDictionaryRemoveValue(
			static_cast<CFMutableDictionaryRef>(const_cast<void*>(q.get())),
			kSecValueData
		);
		CFMutableDictionaryRef update = CFDictionaryCreateMutable(
			kCFAllocatorDefault, 0,
			&kCFTypeDictionaryKeyCallBacks,
			&kCFTypeDictionaryValueCallBacks
		);
		ScopedCF updateOwner(update);
		CFDictionaryAddValue(update, kSecValueData, data.get());
		status = SecItemUpdate(static_cast<CFDictionaryRef>(q.get()), update);
	}
	return status == errSecSuccess
		? Result{true, true, {}, {}}
		: Result{false, false, {}, mac_error(status)};
#elif defined(LINUX_ENABLED) && !defined(ANDROID_ENABLED)
	SecretApi& api = secret_api();
	if (!api.ready()) return {false, false, {}, api.error};
	SecretSchema keySchema = schema();
	GError* error = nullptr;
	const gboolean ok = api.store(
		&keySchema, nullptr, "Cesium for Godot editor sign-in", value.c_str(),
		nullptr, &error,
		"service", service.c_str(), "account", account.c_str(), nullptr
	);
	const std::string message = consume_error(api, error);
	return ok && message.empty()
		? Result{true, true, {}, {}}
		: Result{false, false, {}, message.empty() ? "Secret Service write failed" : message};
#else
	return {false, false, {}, "Secure credential storage is unavailable"};
#endif
}

Result remove(const std::string& service, const std::string& account) noexcept {
#if defined(WINDOWS_ENABLED)
	const std::wstring key = target(service, account);
	if (!CredDeleteW(key.c_str(), CRED_TYPE_GENERIC, 0)) {
		const DWORD code = GetLastError();
		if (code != ERROR_NOT_FOUND) return {false, false, {}, windows_error(code)};
	}
	return {true, false, {}, {}};
#elif defined(MACOS_ENABLED)
	ScopedCF q(query(service, account));
	if (q.get() == nullptr) return {false, false, {}, "Could not create Keychain query"};
	const OSStatus status = SecItemDelete(static_cast<CFDictionaryRef>(q.get()));
	return status == errSecSuccess || status == errSecItemNotFound
		? Result{true, false, {}, {}}
		: Result{false, false, {}, mac_error(status)};
#elif defined(LINUX_ENABLED) && !defined(ANDROID_ENABLED)
	SecretApi& api = secret_api();
	if (!api.ready()) return {false, false, {}, api.error};
	SecretSchema keySchema = schema();
	GError* error = nullptr;
	api.clear(
		&keySchema, nullptr, &error,
		"service", service.c_str(), "account", account.c_str(), nullptr
	);
	const std::string message = consume_error(api, error);
	return message.empty()
		? Result{true, false, {}, {}}
		: Result{false, false, {}, message};
#else
	return {true, false, {}, {}};
#endif
}

} // namespace CesiumGodot::EditorCredentials
