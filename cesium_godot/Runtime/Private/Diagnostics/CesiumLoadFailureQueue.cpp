#include "Runtime/Private/Diagnostics/CesiumLoadFailureQueue.h"

#include <algorithm>
#include <cctype>

std::string redact_cesium_diagnostic_url(const std::string& url) {
	const size_t queryStart = url.find('?');
	if (queryStart == std::string::npos) {
		return url;
	}
	std::string result = url.substr(0, queryStart + 1);
	size_t cursor = queryStart + 1;
	while (cursor <= url.size()) {
		const size_t separator = url.find('&', cursor);
		const size_t end = separator == std::string::npos
			? url.size()
			: separator;
		const std::string field = url.substr(cursor, end - cursor);
		const size_t equals = field.find('=');
		std::string key = equals == std::string::npos
			? field
			: field.substr(0, equals);
		std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) {
			return static_cast<char>(std::tolower(c));
		});
		const bool sensitive = key == "token" || key == "access_token" ||
			key == "key" || key == "api_key" || key == "signature" ||
			key == "sig";
		result += sensitive && equals != std::string::npos
			? field.substr(0, equals + 1) + "<redacted>"
			: field;
		if (separator == std::string::npos) {
			break;
		}
		result.push_back('&');
		cursor = separator + 1;
	}
	return result;
}

void CesiumLoadFailureQueue::push(CesiumLoadFailureRecord&& record) {
	std::lock_guard<std::mutex> lock(this->m_mutex);
	if (this->m_maximumRecords == 0) {
		++this->m_droppedCount;
		return;
	}
	if (this->m_records.size() >= this->m_maximumRecords) {
		this->m_records.erase(this->m_records.begin());
		++this->m_droppedCount;
	}
	this->m_records.emplace_back(std::move(record));
}

std::vector<CesiumLoadFailureRecord> CesiumLoadFailureQueue::drain() {
	std::lock_guard<std::mutex> lock(this->m_mutex);
	std::vector<CesiumLoadFailureRecord> result;
	result.swap(this->m_records);
	return result;
}

void CesiumLoadFailureQueue::clear() {
	std::lock_guard<std::mutex> lock(this->m_mutex);
	this->m_records.clear();
}

size_t CesiumLoadFailureQueue::size() const {
	std::lock_guard<std::mutex> lock(this->m_mutex);
	return this->m_records.size();
}

uint64_t CesiumLoadFailureQueue::get_dropped_count() const {
	std::lock_guard<std::mutex> lock(this->m_mutex);
	return this->m_droppedCount;
}
