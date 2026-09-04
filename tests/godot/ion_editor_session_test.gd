extends SceneTree


func _initialize() -> void:
	_run.call_deferred()


func _run() -> void:
	if not ClassDB.class_exists("CesiumIonEditorSession"):
		_fail("CesiumIonEditorSession is not registered")
		return
	var session := ClassDB.instantiate("CesiumIonEditorSession") as Node
	if session == null:
		_fail("CesiumIonEditorSession could not be constructed")
		return
	root.add_child(session)
	if session.has_method("get_session_access_token"):
		_fail("Editor account bearer token is exposed through ClassDB")
		return
	for method_name in ["connect_to_ion", "cancel_connection", "sign_out", "request_assets", "request_tokens", "create_token"]:
		if not session.has_method(method_name):
			_fail("Missing editor-session method: %s" % method_name)
			return
	for signal_name in ["state_changed", "connected", "disconnected", "authorization_failed", "operation_completed"]:
		if not session.has_signal(signal_name):
			_fail("Missing editor-session signal: %s" % signal_name)
			return

	var completions: Array[Array] = []
	session.operation_completed.connect(func(operation, success, payload, error):
		completions.append([operation, success, payload, error])
	)
	session.request_assets()
	if completions.size() != 1 or completions[0][0] != "assets" or completions[0][1]:
		_fail("Disconnected account operation did not fail safely: %s" % [completions])
		return
	if str(completions[0][3]).find("Sign in") < 0:
		_fail("Disconnected account operation did not explain the failure")
		return

	var config := CesiumGDConfig.get_singleton(session)
	config.accessToken = "runtime-token-is-separate"
	if config.accessToken != "runtime-token-is-separate":
		_fail("Runtime asset-token configuration no longer works in memory")
		return
	config.accessToken = ""
	session.free()
	print("Cesium ion editor-session boundary test passed")
	quit(0)


func _fail(message: String) -> void:
	push_error(message)
	quit(1)
