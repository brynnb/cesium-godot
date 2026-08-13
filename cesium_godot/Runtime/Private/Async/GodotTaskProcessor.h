#ifndef CESIUM_GODOT_TASK_PROCESSOR_H
#define CESIUM_GODOT_TASK_PROCESSOR_H

#include "CesiumAsync/ITaskProcessor.h"

#include <cstddef>
#include <functional>
#include <memory>
#include <thread>
#include <vector>

/**
 * Bounded worker executor for Cesium Native CPU tasks.
 *
 * Cesium for Unreal counterparts:
 * - Source/CesiumRuntime/Public/UnrealTaskProcessor.h
 * - Source/CesiumRuntime/Private/UnrealTaskProcessor.cpp
 *
 * Last upstream review: Cesium for Unreal v2.29.0.
 *
 * This is the Godot counterpart of Cesium for Unreal's UnrealTaskProcessor.
 * Workers drain already-accepted work before shutdown, so Cesium promises are
 * never abandoned while their owning Tileset is being destroyed.
 */
class GodotTaskProcessor final : public CesiumAsync::ITaskProcessor {
public:
	explicit GodotTaskProcessor(size_t workerCount);
	~GodotTaskProcessor() override;

	GodotTaskProcessor(const GodotTaskProcessor&) = delete;
	GodotTaskProcessor& operator=(const GodotTaskProcessor&) = delete;

	size_t get_worker_count() const;
	/**
	 * Stops accepting queued work and joins every worker on the calling thread.
	 *
	 * Runtime owners should call this after draining their Native futures and
	 * before releasing their final explicit processor reference. It is
	 * idempotent; the destructor calls it as a fallback.
	 */
	void shutdown();

private:
	struct State;
	void startTask(std::function<void()> task) override;
	static void run_worker(const std::shared_ptr<State>& state);

	std::shared_ptr<State> m_state;
	std::vector<std::thread> m_workers;
};

#endif // CESIUM_GODOT_TASK_PROCESSOR_H
