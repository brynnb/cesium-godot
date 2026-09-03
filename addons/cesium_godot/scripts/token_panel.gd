class_name TokenPanelData

enum TokenUsageType { New, Existing, Specific }

var existing_token_check: CheckBox
var existing_tokens_list: OptionButton
var specific_token_check: CheckBox
var config_picker: EditorResourcePicker
var new_token_check: CheckBox
var create_or_use_token_button: Button
var test_token_button: Button
var specific_token_name: TextEdit
var asset_list_group: BoxContainer
var asset_list_items: OptionButton
var asset_list_id: TextEdit
var new_token_name: TextEdit

var last_existing_tokens: Dictionary

var token_troubleshooting: TokenTroubleshooting

var request_node: HTTPRequest


var current_token_usage: TokenUsageType = TokenUsageType.Specific

var default_config: CesiumGDConfig = null

func initialize_fields(token_panel: Popup) -> void:
	token_panel.find_child("CloseButton").pressed.connect(func (): token_panel.hide())
	
	self.default_config = CesiumGDConfig.get_singleton(token_panel)
	self.token_troubleshooting = TokenTroubleshooting.new()
	token_panel.add_child(self.token_troubleshooting)
	self.token_troubleshooting.owner = token_panel
	self.existing_token_check = token_panel.find_child("ExistingTokenCheck") as CheckBox
	self.specific_token_check = token_panel.find_child("SpecificTokenCheck") as CheckBox
	self.new_token_check = token_panel.find_child("NewTokenCheck") as CheckBox
	self.create_or_use_token_button = token_panel.find_child("CreateOrUseToken") as Button
	self.test_token_button = token_panel.find_child("TestToken") as Button
	self.asset_list_group = token_panel.find_child("AssetListGroup") as BoxContainer
	self.asset_list_items = token_panel.find_child("AssetList") as OptionButton
	self.existing_tokens_list = token_panel.find_child("ExistingToken") as OptionButton
	# Then add the ResourcePicker
	var configuration_container = token_panel.find_child("ConfigurationContainer")

	self.new_token_name = token_panel.find_child("TokenName") as TextEdit
	
	self.specific_token_name = self.find_sibling(self.specific_token_check, "TokenName") as TextEdit
	self.specific_token_name.text = self.default_config.accessToken
	
	self.existing_token_check.toggled.connect(on_existing_token_check)
	self.specific_token_check.toggled.connect(on_specific_token_check)
	self.new_token_check.toggled.connect(on_new_token_check)
	token_panel.about_to_popup.connect(initialize_panel_buttons_from_state)
	self.create_or_use_token_button.pressed.connect(apply_or_create_token)
	self.test_token_button.pressed.connect(on_test_button_pressed)	
	self.request_node = HTTPRequest.new()
	token_panel.add_child(self.request_node)
	self.token_troubleshooting.set_data(self)


func initialize_panel_buttons_from_state() -> void:
	var usage = self.current_token_usage
	# Manually call the events
	if (usage == TokenPanelData.TokenUsageType.New):
		self.on_new_token_check(true)
	if (usage == TokenPanelData.TokenUsageType.Specific):
		self.on_specific_token_check(true)
	if (usage == TokenPanelData.TokenUsageType.Existing):
		self.on_existing_token_check(true)

func on_new_token_check(checked: bool) -> void:
	if (!checked):
		return
	self.specific_token_check.button_pressed = false
	self.existing_token_check.button_pressed = false
	self.current_token_usage = TokenPanelData.TokenUsageType.New
			

func on_specific_token_check(checked: bool) -> void:
	if (!checked):
		return
	self.new_token_check.button_pressed = false
	self.existing_token_check.button_pressed = false
	self.current_token_usage = TokenPanelData.TokenUsageType.Specific
	pass
	
func on_existing_token_check(checked: bool) -> void:
	if (!checked):
		return
	self.new_token_check.button_pressed = false
	self.specific_token_check.button_pressed = false
	self.current_token_usage = TokenPanelData.TokenUsageType.Existing
	# Send a request to get the token we want to use
	const url := "https://api.cesium.com/v2/tokens"
	var token : String = self.default_config.accessToken;
	var headers: PackedStringArray = ["Authorization: Bearer " + token]
	var err : int = self.request_node.request(url, headers)
	if (err != OK):
		push_error("Error getting tokens from Cesium Ion, try connecting manually!")
		return
	
	var response = await self.request_node.request_completed
	var status = response[1]
	var bodyBytes := response[3] as PackedByteArray
	var body := JSON.parse_string(bodyBytes.get_string_from_utf8()) as Dictionary

	if (status >= HTTPClient.ResponseCode.RESPONSE_BAD_REQUEST):
		push_error("Error connecting to the Cesium API for tokens, try signing in manually, server responded with: " + str(status) + "\nBody: " + str(body))
		return
	var items := body.get("items") as Array
	if (items == null):
		push_error("Failed to parse request, in body expected \"items\", found: " + str(body))
		return
	for item in items:
		item = item as Dictionary
		var name = item.get("name")
		var tokenContent = item.get("token")
		self.last_existing_tokens[name] = tokenContent
		self.existing_tokens_list.add_item(name)


func on_test_button_pressed() -> void:
	self.token_troubleshooting.is_valid_token(self.default_config.accessToken)

func apply_or_create_token() -> void:
	var usage := self.current_token_usage
	if (usage == TokenUsageType.Specific):
		self.default_config.accessToken = self.specific_token_name.text
	if (usage == TokenUsageType.Existing):
		var tkName = self.existing_tokens_list.get_item_text(self.existing_tokens_list.selected)
		var currToken: String = self.last_existing_tokens.get(tkName)
		self.default_config.accessToken = currToken
	if (usage == TokenUsageType.New):
		var createdToken: String = await self.create_new_token()
		if (createdToken == ""):
			OS.alert("Error creating the new token, please try to sign in manually to Cesium Ion", "Error!")
			return
		self.default_config.accessToken = createdToken
	OS.alert("Token changed in configuration, you can now close this window...", "Changes applied!")


func create_new_token() -> String:
	const url := "https://api.cesium.com/v2/tokens"
	# Make a POST and add the name, we'll default to all assets
	var token : String = self.default_config.accessToken;
	var headers: PackedStringArray = ["Authorization: Bearer " + token, "Content-Type: application/json"]

	var scopes := [
		"assets:list",
		"assets:read",
		"geocode",
		"tokens:read",
	]
	var reqBody: Dictionary = { "name": self.new_token_name.text, "scopes": scopes }
	var reqBodyStr: String = JSON.stringify(reqBody)
	var err : int = self.request_node.request(url, headers, HTTPClient.METHOD_POST, reqBodyStr)
	const errMsg := "Error creating a Cesium Ion token, try connecting manually!"
	if (err != OK):
		push_error(errMsg)
		return ""
	
	var response = await self.request_node.request_completed
	var status = response[1]

	var bodyBytes := response[3] as PackedByteArray
	var body := JSON.parse_string(bodyBytes.get_string_from_utf8()) as Dictionary

	if (status >= HTTPClient.ResponseCode.RESPONSE_BAD_REQUEST):		
		push_error(errMsg + "\nResponse body: " + str(body))
		return ""
	
	var resultToken: String = body.get("token")
	if (resultToken == null):
		push_error(errMsg + "\nResponse body: " + str(body))
		return ""
	return resultToken

# Finds a sibling node by name or predicate function.
# Returns `null` if no matching sibling is found.
func find_sibling(node: Node,
	target_name: String = "", 
	predicate: Callable = func(node: Node): return node.name == target_name) -> Node:
	var parent = node.get_parent()
	if not parent:
		return null

	for child in parent.get_children():
		if child != self and predicate.call(child):
			return child
	return null
