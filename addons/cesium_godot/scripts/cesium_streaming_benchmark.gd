class_name CesiumStreamingBenchmark
extends RefCounted

## Runs a repeatable camera route against any Cesium3DTileset and returns a
## JSON-safe metrics Dictionary. Route entries accept position, target,
## travel_frames, and settle_frames. The caller owns normal tileset updates and
## must pause them while this coroutine runs.


func run(
	tileset: Cesium3DTileset,
	camera: Camera3D,
	route: Array[Dictionary]
) -> Dictionary:
	if tileset == null or camera == null or route.is_empty():
		return {"error": "tileset, camera, and at least one route point are required"}

	var frame_times_ms: Array[float] = []
	var update_times_ms: Array[float] = []
	var samples: Array[Dictionary] = []
	var transition_frames := 0
	var empty_visible_frames := 0
	var previous_visual_state := ""
	var start_ticks := Time.get_ticks_usec()
	var previous_frame_ticks := start_ticks
	var frame_index := 0
	var maximum_worker_queue := 0
	var maximum_main_queue := 0
	var maximum_loaded_data_bytes := 0
	var maximum_cached_bytes := 0
	var maximum_shared_texture_bytes := 0
	var maximum_shared_model_bytes := 0
	var maximum_process_memory_bytes := 0

	for route_index in range(route.size()):
		var point: Dictionary = route[route_index]
		var destination := point.get("position", camera.global_position) as Vector3
		var target := point.get(
			"target",
			destination + -camera.global_basis.z
		) as Vector3
		var travel_frames := maxi(1, int(point.get("travel_frames", 1)))
		var settle_frames := maxi(0, int(point.get("settle_frames", 0)))
		var start_position := camera.global_position
		for local_frame in range(travel_frames + settle_frames):
			if local_frame < travel_frames:
				var weight := float(local_frame + 1) / float(travel_frames)
				camera.global_position = start_position.lerp(destination, weight)
			else:
				camera.global_position = destination
			camera.look_at(target, Vector3.UP)

			var update_start := Time.get_ticks_usec()
			tileset.update_tileset(camera.global_transform)
			var update_end := Time.get_ticks_usec()
			await camera.get_tree().process_frame
			var frame_end := Time.get_ticks_usec()
			update_times_ms.append(float(update_end - update_start) / 1000.0)
			frame_times_ms.append(float(frame_end - previous_frame_ticks) / 1000.0)
			previous_frame_ticks = frame_end

			var statistics := tileset.get_streaming_statistics()
			var visual_state := "%d/%d/%d/%d" % [
				int(statistics.get("selected", 0)),
				int(statistics.get("fading_out", 0)),
				int(statistics.get("visible", 0)),
				int(statistics.get("hidden", 0)),
			]
			if not previous_visual_state.is_empty() and visual_state != previous_visual_state:
				transition_frames += 1
			previous_visual_state = visual_state
			if int(statistics.get("visible", 0)) == 0:
				empty_visible_frames += 1
			maximum_worker_queue = maxi(
				maximum_worker_queue,
				int(statistics.get("worker_queue", 0))
			)
			maximum_main_queue = maxi(
				maximum_main_queue,
				int(statistics.get("main_thread_queue", 0))
			)
			maximum_loaded_data_bytes = maxi(
				maximum_loaded_data_bytes,
				int(statistics.get("loaded_data_bytes", 0))
			)
			maximum_cached_bytes = maxi(
				maximum_cached_bytes,
				int(statistics.get("cached_bytes", 0))
			)
			maximum_shared_texture_bytes = maxi(
				maximum_shared_texture_bytes,
				int(statistics.get("shared_texture_live_bytes", 0))
			)
			maximum_shared_model_bytes = maxi(
				maximum_shared_model_bytes,
				int(statistics.get("shared_model_live_geometry_bytes", 0)) +
				int(statistics.get("shared_model_live_texture_bytes", 0))
			)
			maximum_process_memory_bytes = maxi(
				maximum_process_memory_bytes,
				int(Performance.get_monitor(Performance.MEMORY_STATIC))
			)
			# Keep route output bounded while preserving each waypoint boundary and
			# one sample per ten frames for queue/memory trend inspection.
			if (
				local_frame == 0 or
				local_frame == travel_frames - 1 or
				local_frame == travel_frames + settle_frames - 1 or
				frame_index % 10 == 0
			):
				samples.append({
					"frame": frame_index,
					"route_index": route_index,
					"position": [
						camera.global_position.x,
						camera.global_position.y,
						camera.global_position.z,
					],
					"worker_queue": int(statistics.get("worker_queue", 0)),
					"main_thread_queue": int(
						statistics.get("main_thread_queue", 0)
					),
					"selected": int(statistics.get("selected", 0)),
					"fading_out": int(statistics.get("fading_out", 0)),
					"visible": int(statistics.get("visible", 0)),
					"loaded_data_bytes": int(
						statistics.get("loaded_data_bytes", 0)
					),
				})
			frame_index += 1

	var final_statistics := tileset.get_streaming_statistics()
	var elapsed_ms := float(Time.get_ticks_usec() - start_ticks) / 1000.0
	return {
		"schema": "cesium-godot-benchmark-v1",
		"route_points": route.size(),
		"route_frames": frame_index,
		"elapsed_ms": elapsed_ms,
		"frame_ms": _summarize(frame_times_ms),
		"tileset_update_ms": _summarize(update_times_ms),
		"visual_transition_frames": transition_frames,
		"empty_visible_frames": empty_visible_frames,
		"maximum_worker_queue": maximum_worker_queue,
		"maximum_main_thread_queue": maximum_main_queue,
		"maximum_loaded_data_bytes": maximum_loaded_data_bytes,
		"maximum_cached_bytes": maximum_cached_bytes,
		"maximum_shared_texture_bytes": maximum_shared_texture_bytes,
		"maximum_shared_model_bytes": maximum_shared_model_bytes,
		"maximum_process_memory_bytes": maximum_process_memory_bytes,
		"terminal_failures": int(final_statistics.get("failed", 0)),
		"final_worker_queue": int(final_statistics.get("worker_queue", 0)),
		"final_main_thread_queue": int(
			final_statistics.get("main_thread_queue", 0)
		),
		"samples": samples,
	}


func _summarize(values: Array[float]) -> Dictionary:
	if values.is_empty():
		return {"count": 0, "mean": 0.0, "p50": 0.0, "p95": 0.0, "max": 0.0}
	var ordered := values.duplicate()
	ordered.sort()
	var total := 0.0
	for value in ordered:
		total += value
	return {
		"count": ordered.size(),
		"mean": total / float(ordered.size()),
		"p50": _percentile(ordered, 0.50),
		"p95": _percentile(ordered, 0.95),
		"max": ordered[-1],
	}


func _percentile(ordered: Array[float], percentile: float) -> float:
	var index := clampi(
		int(ceil(percentile * float(ordered.size()))) - 1,
		0,
		ordered.size() - 1
	)
	return ordered[index]
