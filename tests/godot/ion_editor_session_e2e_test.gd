extends SceneTree

const TIMEOUT_MSEC := 15000

var completions: Array[Array] = []


func _initialize() -> void:
	_run.call_deferred()


func _run() -> void:
	var fixture_url := OS.get_environment("CESIUM_ION_FIXTURE_URL")
	if fixture_url.is_empty():
		_fail("CESIUM_ION_FIXTURE_URL is missing")
		return
	var session := CesiumIonEditorSession.new()
	session.api_url = fixture_url
	session.server_url = fixture_url
	session.operation_completed.connect(func(operation, success, payload, error):
		completions.append([operation, success, payload, error])
	)
	root.add_child(session)
	session.connect_to_ion()
	if not await _wait_for(func(): return session.is_connected):
		_fail("OAuth connection timed out: %s" % session.error_message)
		return
	if session.profile_name != "fixture-user":
		_fail("Profile validation returned the wrong user: %s" % session.profile_name)
		return

	session.request_assets()
	var assets := await _completion("assets")
	if not assets[1] or assets[2].size() != 1 or assets[2][0]["id"] != 101:
		_fail("Asset request did not cross the authenticated boundary: %s" % [assets])
		return
	session.request_tokens()
	var tokens := await _completion("tokens")
	if not tokens[1] or tokens[2].size() != 1 or tokens[2][0]["id"] != "fixture-existing":
		_fail("Token request did not cross the authenticated boundary: %s" % [tokens])
		return
	session.create_token("Created by E2E", PackedStringArray(["assets:read"]))
	var created := await _completion("create_token")
	if not created[1] or created[2]["id"] != "fixture-created":
		_fail("Token creation did not cross the authenticated boundary: %s" % [created])
		return

	var secure_storage_available: bool = session.secure_storage_available
	session.free()
	if secure_storage_available:
		var resumed := CesiumIonEditorSession.new()
		resumed.api_url = fixture_url
		resumed.server_url = fixture_url
		root.add_child(resumed)
		if not await _wait_for(func(): return resumed.is_connected):
			_fail(
				"The OS-vault session did not resume: state=%s available=%s error=%s storage_error=%s"
				% [resumed.state, resumed.secure_storage_available, resumed.error_message, resumed.secure_storage_error]
			)
			return
		if resumed.profile_name != "fixture-user":
			_fail("The resumed OS-vault session returned the wrong profile")
			return
		resumed.sign_out()
		if resumed.is_connected or resumed.state != CesiumIonEditorSession.Disconnected:
			_fail("Sign-out did not clear the resumed editor session")
			return
		resumed.free()
	else:
		print("Secure credential vault unavailable; persistence check skipped")
	print("Cesium ion editor authorization E2E test passed")
	quit(0)


func _completion(operation: String) -> Array:
	var deadline := Time.get_ticks_msec() + TIMEOUT_MSEC
	while Time.get_ticks_msec() < deadline:
		for index in completions.size():
			if completions[index][0] == operation:
				return completions.pop_at(index)
		await process_frame
	return [operation, false, null, "timed out"]


func _wait_for(predicate: Callable) -> bool:
	var deadline := Time.get_ticks_msec() + TIMEOUT_MSEC
	while Time.get_ticks_msec() < deadline:
		if predicate.call():
			return true
		await process_frame
	return false


func _fail(message: String) -> void:
	push_error(message)
	quit(1)
