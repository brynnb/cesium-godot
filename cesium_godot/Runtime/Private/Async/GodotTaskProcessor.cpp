// Task scheduling in this file is adapted from Cesium for Unreal v2.29.0's
// UnrealTaskProcessor (Apache-2.0).
// Copyright 2020-2026 CesiumGS, Inc. and Contributors.

#include "Runtime/Private/Async/GodotTaskProcessor.h"

#include <algorithm>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <utility>

struct GodotTaskProcessor::State {
	std::queue<std::function<void()>> tasks;
	std::mutex mutex;
	std::condition_variable condition;
	bool stopping = false;
};

GodotTaskProcessor::GodotTaskProcessor(size_t workerCount) {
	workerCount = std::max<size_t>(1, workerCount);
	this->m_state = std::make_shared<State>();
	this->m_workers.reserve(workerCount);
	for (size_t index = 0; index < workerCount; ++index) {
		const std::shared_ptr<State> state = this->m_state;
		this->m_workers.emplace_back([state]() {
			GodotTaskProcessor::run_worker(state);
		});
	}
}

GodotTaskProcessor::~GodotTaskProcessor() {
	this->shutdown();
}

void GodotTaskProcessor::shutdown() {
	const std::shared_ptr<State> state = this->m_state;
	if (state == nullptr) return;
	{
		std::lock_guard<std::mutex> lock(state->mutex);
		state->stopping = true;
	}
	state->condition.notify_all();
	const std::thread::id currentThread = std::this_thread::get_id();
	for (std::thread& worker : this->m_workers) {
		if (!worker.joinable()) continue;
		if (worker.get_id() == currentThread) worker.detach();
		else worker.join();
	}
}

size_t GodotTaskProcessor::get_worker_count() const {
	return this->m_workers.size();
}

void GodotTaskProcessor::startTask(std::function<void()> task) {
	const std::shared_ptr<State> state = this->m_state;
	bool runSynchronously = false;
	{
		std::lock_guard<std::mutex> lock(state->mutex);
		// Cesium's AsyncSystem retains the processor for the lifetime in which it
		// can submit work. Reaching this branch would indicate a contract bug;
		// execute synchronously so the associated promise is still completed.
		if (state->stopping) {
			runSynchronously = true;
		} else {
			state->tasks.emplace(std::move(task));
		}
	}
	if (runSynchronously) {
		task();
		return;
	}
	state->condition.notify_one();
}

void GodotTaskProcessor::run_worker(const std::shared_ptr<State>& state) {
	for (;;) {
		std::function<void()> task;
		{
			std::unique_lock<std::mutex> lock(state->mutex);
			state->condition.wait(lock, [state]() {
				return state->stopping || !state->tasks.empty();
			});
			if (state->stopping && state->tasks.empty()) {
				return;
			}
			task = std::move(state->tasks.front());
			state->tasks.pop();
		}
		task();
	}
}
