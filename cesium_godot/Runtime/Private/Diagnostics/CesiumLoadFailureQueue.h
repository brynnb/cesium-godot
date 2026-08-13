#ifndef CESIUM_LOAD_FAILURE_QUEUE_H
#define CESIUM_LOAD_FAILURE_QUEUE_H

#include "Runtime/Public/Diagnostics/CesiumLoadFailure.h"

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

/** Removes common credential query values before a URL reaches diagnostics. */
std::string redact_cesium_diagnostic_url(const std::string& url);

/** Plain-data failure payload safe to create on Cesium worker threads. */
struct CesiumLoadFailureRecord {
	uint64_t sourceInstanceId = 0;
	CesiumLoadFailure::Category category = CesiumLoadFailure::Category::Unknown;
	CesiumLoadFailure::Stage stage = CesiumLoadFailure::Stage::StageUnknown;
	std::string message;
	std::string url;
	std::string tileId;
	std::string overlayKey;
	int32_t httpStatusCode = 0;
	bool terminal = true;
	bool retryable = false;
	bool retryScheduled = false;
	int32_t attempt = 0;
	int32_t maximumAttempts = 0;
	double retryDelaySeconds = 0.0;
};

/**
 * Multi-producer/main-thread-consumer bridge. No Godot Object or Resource is
 * created while the mutex is held or from a worker thread.
 */
class CesiumLoadFailureQueue final {
public:
	explicit CesiumLoadFailureQueue(size_t maximumRecords = 1024)
		: m_maximumRecords(maximumRecords) {}
	void push(CesiumLoadFailureRecord&& record);
	std::vector<CesiumLoadFailureRecord> drain();
	void clear();
	size_t size() const;
	uint64_t get_dropped_count() const;

private:
	mutable std::mutex m_mutex;
	std::vector<CesiumLoadFailureRecord> m_records;
	size_t m_maximumRecords;
	uint64_t m_droppedCount = 0;
};

#endif // CESIUM_LOAD_FAILURE_QUEUE_H
