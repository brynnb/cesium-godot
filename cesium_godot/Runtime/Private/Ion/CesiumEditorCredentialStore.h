#ifndef CESIUM_EDITOR_CREDENTIAL_STORE_H
#define CESIUM_EDITOR_CREDENTIAL_STORE_H

#include <string>

namespace CesiumGodot::EditorCredentials {

struct Result {
	bool success = false;
	bool found = false;
	std::string value;
	std::string error;
};

const char* backend_name() noexcept;
bool is_available(std::string& error) noexcept;
Result read(const std::string& service, const std::string& account) noexcept;
Result write(
	const std::string& service,
	const std::string& account,
	const std::string& value
) noexcept;
Result remove(const std::string& service, const std::string& account) noexcept;

} // namespace CesiumGodot::EditorCredentials

#endif
