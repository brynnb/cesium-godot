extends SceneTree


func _initialize() -> void:
	var tileset := Cesium3DTileset.new()
	assert(not tileset.occlusion_culling_enabled)
	assert(tileset.occlusion_pool_size == 1000)
	assert(not tileset.delay_refinement_for_occlusion)

	var api_available := RenderingServer.has_method("viewport_query_occlusion")
	var statistics: Dictionary = tileset.get_streaming_statistics()
	assert(statistics.occlusion_culling_available == api_available)
	assert(not statistics.occlusion_culling_enabled)

	tileset.occlusion_culling_enabled = true
	tileset.occlusion_pool_size = 2048
	tileset.delay_refinement_for_occlusion = true
	assert(tileset.occlusion_culling_enabled)
	assert(tileset.occlusion_pool_size == 2048)
	assert(tileset.delay_refinement_for_occlusion)

	if api_available:
		var viewport_rid := RenderingServer.viewport_create()
		var results: PackedByteArray = RenderingServer.call(
			"viewport_query_occlusion",
			viewport_rid,
			[AABB(Vector3.ZERO, Vector3.ONE), "invalid"]
		)
		assert(results == PackedByteArray([0, 0]))
		RenderingServer.free_rid(viewport_rid)

	tileset.free()
	print("Occlusion bridge test passed (engine_api=%s)" % api_available)
	quit(0)
