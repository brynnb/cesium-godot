extends SceneTree

const EARTH_RADIUS := 6_378_137.0
const LOAD_TIMEOUT_FRAMES := 600
const TRANSITION_TIMEOUT_FRAMES := 180
const EXPECTED_CREDIT_COUNT := 3
const FIXTURE_LOGO := "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mNk+A8AAQUBAScY42YAAAAASUVORK5CYII="

var test_root: Node3D
var camera: Camera3D
var georeference: CesiumGeoreference
var active_tilesets: Array[Cesium3DTileset] = []
var changed_sizes: Array[int] = []


func _initialize() -> void:
	_run.call_deferred()


func _run() -> void:
	test_root = Node3D.new()
	test_root.name = "CreditsAttributionTest"
	root.add_child(test_root)

	camera = Camera3D.new()
	camera.name = "TestCamera"
	camera.current = true
	camera.position = Vector3(EARTH_RADIUS + 50.0, 100.0, 300.0)
	test_root.add_child(camera)
	camera.look_at(Vector3(EARTH_RADIUS + 50.0, 0.0, 50.0), Vector3.UP)

	georeference = CesiumGeoreference.new()
	georeference.name = "LocalCoordinates"
	georeference.origin_type = CesiumGeoreference.TrueOrigin
	test_root.add_child(georeference)

	var credits := CesiumGDCreditSystem.new()
	credits.name = "PrimaryCredits"
	credits.open_links_externally = false
	credits.credits_changed.connect(_on_credits_changed)
	test_root.add_child(credits)

	var first := _make_tileset("CreditFixtureA", credits)
	var second := _make_tileset("CreditFixtureB", credits)
	if not await _pump_until_count(credits, EXPECTED_CREDIT_COUNT, LOAD_TIMEOUT_FRAMES):
		_fail(
			"Timed out waiting for shared fixture credits: count=%d html=%s first=%s second=%s"
			% [credits.current_credit_count, credits.current_html, first.get_streaming_statistics(), second.get_streaming_statistics()]
		)
		return
	if not _check_credit_snapshot(credits, false):
		return
	if first.resolved_credit_system != credits or second.resolved_credit_system != credits:
		_fail("Tilesets did not retain their explicitly configured credit system")
		return
	if changed_sizes.count(EXPECTED_CREDIT_COUNT) != 1:
		_fail("Duplicate tilesets emitted duplicate initial credit lists: %s" % [changed_sizes])
		return

	# Recreate one Native tileset. Because both tilesets feed one collector, its
	# temporary reference release must not remove or duplicate shared credits.
	first.invalidate_resolved_credit_system()
	if not await _pump_until_count(credits, EXPECTED_CREDIT_COUNT, TRANSITION_TIMEOUT_FRAMES):
		_fail("Source recreation lost shared credits")
		return
	if first.resolved_credit_system != credits:
		_fail("Source recreation resolved a different credit system")
		return

	# Native captures this policy when glTF content credits are created. Toggling
	# it recreates loaded content so every current credit changes atomically.
	first.show_credits_on_screen = true
	second.show_credits_on_screen = true
	if not await _pump_until_presentation(credits, true, TRANSITION_TIMEOUT_FRAMES):
		_fail("Credits did not move to the direct on-screen presentation")
		return
	if not _check_credit_snapshot(credits, true):
		return
	first.show_credits_on_screen = false
	second.show_credits_on_screen = false
	if not await _pump_until_presentation(credits, false, TRANSITION_TIMEOUT_FRAMES):
		_fail("Credits did not return to the attribution popup")
		return

	var retained_credit := credits.current_credits[0] as CesiumCredit
	active_tilesets.erase(first)
	first.free()
	if not await _pump_until_count(credits, EXPECTED_CREDIT_COUNT, TRANSITION_TIMEOUT_FRAMES):
		_fail("Removing one of two sources removed shared credits")
		return
	active_tilesets.erase(second)
	second.free()
	if not await _pump_until_count(credits, 0, TRANSITION_TIMEOUT_FRAMES):
		_fail("Credits remained after the last source was destroyed")
		return
	if retained_credit == null or retained_credit.html.is_empty():
		_fail("A retained credit snapshot depended on its destroyed source")
		return

	# An explicitly selected second system is isolated from the first one.
	var isolated := CesiumGDCreditSystem.new()
	isolated.name = "IsolatedCredits"
	isolated.open_links_externally = false
	test_root.add_child(isolated)
	var isolated_tileset := _make_tileset("IsolatedFixture", isolated)
	if not await _pump_until_count(isolated, EXPECTED_CREDIT_COUNT, LOAD_TIMEOUT_FRAMES):
		_fail("Explicit isolated credit system did not receive its source")
		return
	if credits.current_credit_count != 0:
		_fail("Explicit isolated credits leaked into another collector")
		return
	active_tilesets.erase(isolated_tileset)
	isolated_tileset.free()
	if not await _pump_until_count(isolated, 0, TRANSITION_TIMEOUT_FRAMES):
		_fail("Isolated credits remained after source teardown")
		return

	# Remote logos use bounded asynchronous HTTP requests. The full test runner
	# supplies a deterministic localhost server; direct single-test runs still
	# cover the complete inline-image path without requiring a network.
	var server_url := OS.get_environment("CESIUM_TEST_SERVER_URL")
	if not server_url.is_empty():
		var remote := CesiumGDCreditSystem.new()
		remote.name = "RemoteCredits"
		remote.open_links_externally = false
		test_root.add_child(remote)
		var remote_html := (
			"<a href=\"https://example.invalid/remote-logo\"><img src=\"%s/credit-logo.png\" "
			+ "alt=\"Remote Fixture Logo\"></a>"
		) % server_url
		var remote_tileset := _make_tileset(
			"RemoteCreditFixture",
			remote,
			remote_html
		)
		if not await _pump_until_count(remote, EXPECTED_CREDIT_COUNT, LOAD_TIMEOUT_FRAMES):
			_fail("Remote-credit source did not publish its attribution")
			return
		if not await _pump_until_images(remote, 1, LOAD_TIMEOUT_FRAMES):
			_fail(
				"Remote credit logo did not load: pending=%d failed=%d"
				% [remote.pending_credit_image_count, remote.failed_credit_image_count]
			)
			return
		if remote.failed_credit_image_count != 0:
			_fail("Remote credit logo reported a failure after loading")
			return
		active_tilesets.erase(remote_tileset)
		remote_tileset.free()
		if not await _pump_until_count(remote, 0, TRANSITION_TIMEOUT_FRAMES):
			_fail("Remote attribution remained after source teardown")
			return
		remote.free()

	credits.free()
	isolated.free()
	await process_frame

	# Default resolution must not depend on a stale static raw pointer. Deleting
	# the resolved node and invalidating the tileset creates a fresh scene system.
	var default_tileset := _make_tileset("DefaultCreditFixture", null)
	var default_credits: CesiumGDCreditSystem = null
	for _frame in range(LOAD_TIMEOUT_FRAMES):
		await _pump_one_frame()
		default_credits = default_tileset.resolved_credit_system
		if (
			default_credits != null and
			default_credits.current_credit_count == EXPECTED_CREDIT_COUNT
		):
			break
	if default_credits == null:
		_fail("A default scene credit system was not created")
		return
	var old_default_id := default_credits.get_instance_id()
	default_credits.free()
	await process_frame
	default_tileset.invalidate_resolved_credit_system()
	var replacement: CesiumGDCreditSystem = null
	for _frame in range(LOAD_TIMEOUT_FRAMES):
		await _pump_one_frame()
		replacement = default_tileset.resolved_credit_system
		if (
			replacement != null and
			replacement.get_instance_id() != old_default_id and
			replacement.current_credit_count == EXPECTED_CREDIT_COUNT
		):
			break
	if replacement == null or replacement.get_instance_id() == old_default_id:
		_fail("Default credit resolution reused a freed singleton")
		return

	active_tilesets.erase(default_tileset)
	default_tileset.free()
	await _pump_until_count(replacement, 0, TRANSITION_TIMEOUT_FRAMES)
	test_root.free()
	print("Cesium credits/attribution test passed")
	quit(0)


func _make_tileset(
	tileset_name: String,
	credits: CesiumGDCreditSystem,
	custom_credit := ""
) -> Cesium3DTileset:
	var result := Cesium3DTileset.new()
	result.name = tileset_name
	result.data_source = 1
	result.url = "file://" + ProjectSettings.globalize_path(
		"res://fixtures/credits/tileset.json"
	)
	result.credit = custom_credit if not custom_credit.is_empty() else (
		"<a href=\"https://example.invalid/logo?x=1&amp;y=2\"><img src=\"%s\" "
		+ "alt=\"Fixture Logo &amp; Mark\"></a>"
	) % FIXTURE_LOGO
	result.create_physics_meshes = false
	result.maximum_screen_space_error = 1.0
	result.maximum_simultaneous_tile_loads = 2
	result.preload_ancestors = false
	result.preload_siblings = false
	result.http_cache_enabled = false
	if credits != null:
		result.credit_system = credits
	georeference.add_child(result)
	active_tilesets.push_back(result)
	return result


func _check_credit_snapshot(
	credits: CesiumGDCreditSystem,
	expect_on_screen: bool
) -> bool:
	if credits.current_credit_count != EXPECTED_CREDIT_COUNT:
		_fail("Unexpected credit count: %d" % credits.current_credit_count)
		return false
	if credits.current_html.is_empty() or credits.current_plain_text.is_empty():
		_fail("Aggregate HTML/plain-text attribution was empty")
		return false
	if (
		credits.loaded_credit_image_count != 1 or
		credits.pending_credit_image_count != 0 or
		credits.failed_credit_image_count != 0 or
		credits.get_credit_image(FIXTURE_LOGO) == null
	):
		_fail("Inline credit logo was not decoded and retained")
		return false
	if expect_on_screen:
		if credits.has_hidden_credits or credits.on_screen_text.is_empty():
			_fail("Direct credits were still hidden behind the popup")
			return false
	else:
		if not credits.has_hidden_credits or credits.popup_text.is_empty():
			_fail("Hidden credits were not exposed through the popup model")
			return false

	var found_link := false
	var found_image := false
	var found_plain := false
	for value in credits.current_credits:
		var credit := value as CesiumCredit
		if credit == null or credit.show_on_screen != expect_on_screen:
			_fail("Credit snapshot type or presentation flag was wrong")
			return false
		if credit.plain_text == "Fixture Maps & Survey":
			found_link = (
				credit.links.size() == 1 and
				credit.links[0].url == "https://example.invalid/maps?x=1%26y=2"
			)
		elif credit.plain_text == "Fixture Logo & Mark":
			found_image = (
				credit.image_urls == PackedStringArray([FIXTURE_LOGO]) and
				credit.accessible_text == "Fixture Logo & Mark" and
				credit.links.size() == 1 and
				credit.links[0].url == "https://example.invalid/logo?x=1&y=2"
			)
		elif credit.plain_text == "Plain © Data":
			found_plain = true
	if not found_link or not found_image or not found_plain:
		_fail(
			"Structured HTML attribution was incomplete: link=%s image=%s plain=%s"
			% [found_link, found_image, found_plain]
		)
		return false
	return true


func _pump_until_count(
	credits: CesiumGDCreditSystem,
	expected: int,
	frame_limit: int
) -> bool:
	for _frame in range(frame_limit):
		await _pump_one_frame()
		if is_instance_valid(credits) and credits.current_credit_count == expected:
			return true
	return false


func _pump_until_presentation(
	credits: CesiumGDCreditSystem,
	expect_on_screen: bool,
	frame_limit: int
) -> bool:
	for _frame in range(frame_limit):
		await _pump_one_frame()
		if credits.current_credit_count != EXPECTED_CREDIT_COUNT:
			continue
		var all_match := true
		for value in credits.current_credits:
			if (value as CesiumCredit).show_on_screen != expect_on_screen:
				all_match = false
				break
		if all_match:
			return true
	return false


func _pump_until_images(
	credits: CesiumGDCreditSystem,
	expected: int,
	frame_limit: int
) -> bool:
	for _frame in range(frame_limit):
		await _pump_one_frame()
		if (
			is_instance_valid(credits) and
			credits.loaded_credit_image_count == expected and
			credits.pending_credit_image_count == 0
		):
			return true
	return false


func _pump_one_frame() -> void:
	for current in active_tilesets.duplicate():
		if is_instance_valid(current):
			current.update_tileset(camera.global_transform)
	await process_frame


func _on_credits_changed(current_credits: Array) -> void:
	changed_sizes.push_back(current_credits.size())


func _fail(message: String) -> void:
	push_error(message)
	if is_instance_valid(test_root):
		test_root.free()
	quit(1)
