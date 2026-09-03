@tool
extends RefCounted
class_name CesiumEditorAssetCatalog

## Credential-free, transport-independent catalog model used by editor tooling.
##
## This class deliberately does not perform network requests or directory
## discovery. A caller supplies already-read JSON or a Dictionary and may then
## search the normalized entries. Authentication headers, tokens, and cookies
## belong to the caller's request layer and are never accepted by this model.

const SCHEMA := "cesium-godot-asset-catalog-v1"
const TYPE_BLANK := "blank"
const TYPE_3D_TILES := "3d_tiles"
const SUPPORTED_TYPES := [TYPE_BLANK, TYPE_3D_TILES]

const _SENSITIVE_KEYS := [
	"authorization",
	"authtoken",
	"accesstoken",
	"bearertoken",
	"token",
	"apikey",
	"password",
	"passwd",
	"secret",
	"clientsecret",
	"credential",
	"credentials",
	"signature",
	"sig",
	"xamzcredential",
	"xamzsignature",
	"xgoogcredential",
	"xgoogsignature",
]

var _entries: Array[Dictionary] = []
var _errors := PackedStringArray()


## Replaces this model with a validated v1 catalog. The update is atomic: if
## any entry is invalid, no entries from the document are retained.
func parse_dictionary(document: Dictionary, catalog_location: String = "") -> bool:
	var errors := PackedStringArray()
	var normalized_entries: Array[Dictionary] = []

	if _has_sensitive_key(document):
		errors.append(
			"Catalog v1 does not accept credential, token, signature, or authorization fields"
		)
	if str(document.get("schema", "")) != SCHEMA:
		errors.append("Catalog schema must be '%s'" % SCHEMA)
	var raw_assets: Variant = document.get("assets", null)
	if not raw_assets is Array:
		errors.append("Catalog 'assets' must be an array")
	else:
		for index in range(raw_assets.size()):
			var raw_entry: Variant = raw_assets[index]
			if not raw_entry is Dictionary:
				errors.append("Asset %d must be an object" % (index + 1))
				continue
			var normalized := _normalize_entry(
				raw_entry,
				catalog_location,
				index,
				errors,
				false
			)
			if not normalized.is_empty():
				normalized_entries.append(normalized)

	_errors = errors
	if not errors.is_empty():
		_entries.clear()
		return false
	_entries = normalized_entries
	return true


## Parses JSON without including the JSON text or URL in diagnostics.
func parse_json(json_text: String, catalog_location: String = "") -> bool:
	var parser := JSON.new()
	var parse_error := parser.parse(json_text)
	if parse_error != OK:
		_entries.clear()
		_errors = PackedStringArray([
			"Malformed catalog JSON near line %d" % parser.get_error_line()
		])
		return false
	var parsed: Variant = parser.data
	if not parsed is Dictionary:
		_entries.clear()
		_errors = PackedStringArray(["Catalog JSON root must be an object"])
		return false
	return parse_dictionary(parsed, catalog_location)


func clear() -> void:
	_entries.clear()
	_errors.clear()


func get_entry_count() -> int:
	return _entries.size()


func get_errors() -> PackedStringArray:
	return _errors.duplicate()


## Returns defensive copies, optionally searched, type-filtered, and sorted.
## sort_field may be "name", "type", or "url".
func get_entries(
	search_text: String = "",
	type_filter: String = "",
	sort_field: String = "name",
	ascending: bool = true
) -> Array[Dictionary]:
	var normalized_query := search_text.strip_edges().to_lower()
	var normalized_filter := type_filter.strip_edges().to_lower()
	if not normalized_filter.is_empty() and not SUPPORTED_TYPES.has(normalized_filter):
		return []

	var matches: Array[Dictionary] = []
	for entry in _entries:
		if not normalized_filter.is_empty() and entry["type"] != normalized_filter:
			continue
		if not normalized_query.is_empty() and not _entry_matches(entry, normalized_query):
			continue
		matches.append(entry.duplicate(true))

	match sort_field.strip_edges().to_lower():
		"type":
			matches.sort_custom(_entry_type_less)
		"url":
			matches.sort_custom(_entry_url_less)
		_:
			matches.sort_custom(_entry_name_less)
	if not ascending:
		matches.reverse()
	return matches


## Builds one normalized entry for a direct editor URL/path field. Relative
## local paths are resolved against the Godot project directory.
static func make_direct_entry(
	name: String,
	url_or_path: String,
	type: String = TYPE_3D_TILES,
	description: String = "",
	attribution: String = ""
) -> Dictionary:
	var errors := PackedStringArray()
	var raw := {
		"name": name,
		"url": url_or_path,
		"type": type,
		"description": description,
		"attribution": attribution,
	}
	var entry := _normalize_entry(raw, "", 0, errors, true)
	return {
		"ok": errors.is_empty(),
		"entry": entry if errors.is_empty() else {},
		"errors": errors,
	}


## Resolves a catalog asset reference without retaining either input. The
## catalog_location is the catalog document's URL/path, not its directory.
static func resolve_asset_url(
	reference: String,
	catalog_location: String = ""
) -> Dictionary:
	var errors := PackedStringArray()
	var resolved := _normalize_asset_url(
		reference,
		catalog_location,
		false,
		"Asset URL",
		errors
	)
	return {
		"ok": errors.is_empty(),
		"url": resolved if errors.is_empty() else "",
		"errors": errors,
	}


## Converts Unix, Windows-drive, UNC, res://, user://, or project-relative
## paths to canonical percent-encoded file URLs.
static func local_path_to_file_url(path: String) -> String:
	return CesiumUrlUtility.local_path_to_file_url(path)


static func _normalize_entry(
	raw_entry: Dictionary,
	catalog_location: String,
	index: int,
	errors: PackedStringArray,
	direct: bool
) -> Dictionary:
	var label := "Direct asset" if direct else "Asset %d" % (index + 1)
	if _has_sensitive_key(raw_entry):
		errors.append("%s contains a credential-like field, which catalog v1 forbids" % label)
		return {}

	if not raw_entry.get("name", null) is String:
		errors.append("%s 'name' must be a string" % label)
		return {}
	var name := str(raw_entry["name"]).strip_edges()
	if name.is_empty():
		errors.append("%s 'name' must not be empty" % label)

	if not raw_entry.get("type", null) is String:
		errors.append("%s 'type' must be a string" % label)
		return {}
	var type := str(raw_entry["type"]).strip_edges().to_lower()
	if not SUPPORTED_TYPES.has(type):
		errors.append("%s type must be 'blank' or '3d_tiles'" % label)

	var description := _optional_string(raw_entry, "description", label, errors)
	var attribution := _optional_string(raw_entry, "attribution", label, errors)
	var url := ""
	var raw_url: Variant = raw_entry.get("url", "")
	if not raw_url is String:
		errors.append("%s 'url' must be a string" % label)
	elif type == TYPE_BLANK:
		if not str(raw_url).strip_edges().is_empty():
			errors.append("%s blank entries must not specify a URL" % label)
	elif type == TYPE_3D_TILES:
		url = _normalize_asset_url(
			str(raw_url),
			catalog_location,
			direct,
			"%s URL" % label,
			errors
		)

	if not errors.is_empty():
		# The caller treats all accumulated errors atomically. Returning a value
		# after any new error would only create a misleading partial catalog.
		return {}
	return {
		"name": name,
		"url": url,
		"description": description,
		"attribution": attribution,
		"type": type,
	}


static func _optional_string(
	dictionary: Dictionary,
	key: String,
	label: String,
	errors: PackedStringArray
) -> String:
	if not dictionary.has(key):
		return ""
	if not dictionary[key] is String:
		errors.append("%s '%s' must be a string" % [label, key])
		return ""
	return str(dictionary[key]).strip_edges()


static func _normalize_asset_url(
	reference: String,
	catalog_location: String,
	allow_project_relative: bool,
	label: String,
	errors: PackedStringArray
) -> String:
	var value := reference.strip_edges()
	if value.is_empty():
		errors.append("%s must not be empty" % label)
		return ""
	if _contains_control_character(value):
		errors.append("%s contains a control character" % label)
		return ""
	if _url_contains_credentials(value):
		errors.append("%s contains embedded credentials, which are not supported" % label)
		return ""

	if _is_http_url(value):
		return _canonical_http_url(value, label, errors)
	if _is_file_url(value):
		return _canonical_file_url(value, label, errors)
	if _looks_like_other_scheme(value):
		errors.append("%s uses an unsupported URL scheme" % label)
		return ""

	var base := catalog_location.strip_edges()
	if not base.is_empty() and _url_contains_credentials(base):
		errors.append("Catalog location contains embedded credentials, which are not supported")
		return ""
	if _is_http_url(base):
		return _resolve_http_reference(value, base, label, errors)
	if _is_file_url(base):
		return _resolve_file_reference(value, base, label, errors)
	if not base.is_empty():
		var base_file_url := local_path_to_file_url(base)
		if base_file_url.is_empty():
			errors.append("Catalog location is not a valid local path or HTTP(S)/file URL")
			return ""
		return _resolve_file_reference(value, base_file_url, label, errors)

	if _is_absolute_local_path(value) or allow_project_relative:
		var file_url := local_path_to_file_url(value)
		if file_url.is_empty():
			errors.append("%s is not a valid local path" % label)
		return file_url
	errors.append("%s is relative but no catalog location was supplied" % label)
	return ""


static func _canonical_http_url(
	url: String,
	label: String,
	errors: PackedStringArray
) -> String:
	var scheme_separator := url.find("://")
	var scheme := url.substr(0, scheme_separator).to_lower()
	var remainder := url.substr(scheme_separator + 3)
	var authority_end := _first_separator(remainder)
	var authority := remainder if authority_end < 0 else remainder.substr(0, authority_end)
	var path_and_suffix := "/" if authority_end < 0 else remainder.substr(authority_end)
	if authority.is_empty() or authority.contains(" "):
		errors.append("%s has an invalid HTTP(S) authority" % label)
		return ""
	var parts := _split_url_suffix(path_and_suffix)
	var path := str(parts[0])
	if path.is_empty() or path.begins_with("?") or path.begins_with("#"):
		path = "/"
	return scheme + "://" + authority + _normalize_url_path(path) + str(parts[1]) + str(parts[2])


static func _resolve_http_reference(
	reference: String,
	base: String,
	label: String,
	errors: PackedStringArray
) -> String:
	var canonical_base := _canonical_http_url(base, "Catalog location", errors)
	if canonical_base.is_empty():
		return ""
	var base_scheme_end := canonical_base.find("://")
	var scheme := canonical_base.substr(0, base_scheme_end)
	if reference.begins_with("//"):
		return _canonical_http_url(scheme + ":" + reference, label, errors)

	var after_scheme := canonical_base.substr(base_scheme_end + 3)
	var authority_end := _first_separator(after_scheme)
	var authority := after_scheme if authority_end < 0 else after_scheme.substr(0, authority_end)
	var base_path_and_suffix := "/" if authority_end < 0 else after_scheme.substr(authority_end)
	var base_parts := _split_url_suffix(base_path_and_suffix)
	var reference_parts := _split_url_suffix(reference)
	var reference_path := str(reference_parts[0])
	var target_path := ""
	if reference_path.is_empty():
		target_path = str(base_parts[0])
	elif reference_path.begins_with("/"):
		target_path = reference_path
	else:
		var base_path := str(base_parts[0])
		var last_slash := base_path.rfind("/")
		var base_directory := "/" if last_slash <= 0 else base_path.substr(0, last_slash + 1)
		target_path = base_directory + reference_path
	var query := str(reference_parts[1])
	if query.is_empty() and reference_path.is_empty():
		query = str(base_parts[1])
	return (
		scheme + "://" + authority + _normalize_url_path(target_path) +
		query + str(reference_parts[2])
	)


static func _canonical_file_url(
	url: String,
	label: String,
	errors: PackedStringArray
) -> String:
	if url.find("?") >= 0 or url.find("#") >= 0:
		errors.append("%s file URLs must not contain query strings or fragments" % label)
		return ""
	var path := _file_url_to_local_path(url)
	if path.is_empty():
		errors.append("%s is not a valid absolute file URL" % label)
		return ""
	return local_path_to_file_url(path)


static func _resolve_file_reference(
	reference: String,
	base_file_url: String,
	label: String,
	errors: PackedStringArray
) -> String:
	if reference.find("?") >= 0 or reference.find("#") >= 0:
		errors.append("%s local references must not contain query strings or fragments" % label)
		return ""
	if _is_absolute_local_path(reference):
		return local_path_to_file_url(reference)
	var base_path := _file_url_to_local_path(base_file_url)
	if base_path.is_empty():
		errors.append("Catalog location is not a valid absolute file URL")
		return ""
	var decoded_reference := _decode_url_path(reference.replace("\\", "/"))
	var resolved_path := base_path.get_base_dir().path_join(decoded_reference).simplify_path()
	var result := local_path_to_file_url(resolved_path)
	if result.is_empty():
		errors.append("%s could not be resolved as a local path" % label)
	return result


static func _file_url_to_local_path(url: String) -> String:
	if not _is_file_url(url):
		return ""
	var tail := url.substr(7)
	if tail.is_empty():
		return ""
	if tail.begins_with("/"):
		var path := _decode_url_path(tail)
		if (
			path.length() >= 4 and path[0] == "/" and
			_is_windows_drive_path(path.substr(1))
		):
			return path.substr(1)
		return path
	var slash := tail.find("/")
	var authority := tail if slash < 0 else tail.substr(0, slash)
	var remote_path := "" if slash < 0 else tail.substr(slash)
	if authority.to_lower() == "localhost":
		return _decode_url_path(remote_path)
	if authority.is_empty():
		return ""
	return "//" + authority.uri_decode() + _decode_url_path(remote_path)


static func _normalize_url_path(path: String) -> String:
	var absolute := path.begins_with("/")
	var output := PackedStringArray()
	for encoded_segment in path.split("/", true):
		var segment := encoded_segment.uri_decode()
		if segment.is_empty() or segment == ".":
			continue
		if segment == "..":
			if not output.is_empty():
				output.remove_at(output.size() - 1)
			continue
		output.append(segment.uri_encode())
	var normalized := "/".join(output)
	if absolute:
		normalized = "/" + normalized
	if normalized.is_empty():
		return "/" if absolute else ""
	if path.ends_with("/") and not normalized.ends_with("/"):
		normalized += "/"
	return normalized


static func _decode_url_path(path: String) -> String:
	var leading_slash := path.begins_with("/")
	var decoded := PackedStringArray()
	for segment in path.split("/", true):
		if segment.is_empty():
			continue
		decoded.append(segment.uri_decode())
	var result := "/".join(decoded)
	return "/" + result if leading_slash else result


static func _split_url_suffix(value: String) -> Array[String]:
	var path_and_query := value
	var fragment := ""
	var fragment_index := path_and_query.find("#")
	if fragment_index >= 0:
		fragment = path_and_query.substr(fragment_index)
		path_and_query = path_and_query.substr(0, fragment_index)
	var path := path_and_query
	var query := ""
	var query_index := path_and_query.find("?")
	if query_index >= 0:
		query = path_and_query.substr(query_index)
		path = path_and_query.substr(0, query_index)
	return [path, query, fragment]


static func _first_separator(value: String) -> int:
	var result := -1
	for separator in ["/", "?", "#"]:
		var position := value.find(separator)
		if position >= 0 and (result < 0 or position < result):
			result = position
	return result


static func _is_http_url(value: String) -> bool:
	var lower := value.to_lower()
	return lower.begins_with("http://") or lower.begins_with("https://")


static func _is_file_url(value: String) -> bool:
	return value.to_lower().begins_with("file://")


static func _looks_like_other_scheme(value: String) -> bool:
	if _is_windows_drive_path(value):
		return false
	var colon := value.find(":")
	if colon <= 0:
		return false
	var slash := value.find("/")
	return slash < 0 or colon < slash


static func _is_absolute_local_path(value: String) -> bool:
	return (
		value.begins_with("/") or value.begins_with("\\\\") or
		value.begins_with("//") or value.begins_with("res://") or
		value.begins_with("user://") or _is_windows_drive_path(value)
	)


static func _is_windows_drive_path(value: String) -> bool:
	if value.length() < 3 or value[1] != ":":
		return false
	var first := value.substr(0, 1).to_lower()
	return first >= "a" and first <= "z" and (value[2] == "/" or value[2] == "\\")


static func _contains_control_character(value: String) -> bool:
	for character in value:
		if character.unicode_at(0) < 32 or character.unicode_at(0) == 127:
			return true
	return false


static func _has_sensitive_key(dictionary: Dictionary) -> bool:
	for key in dictionary.keys():
		if _is_sensitive_key(str(key)):
			return true
	return false


static func _url_contains_credentials(url: String) -> bool:
	var scheme_separator := url.find("://")
	if scheme_separator >= 0:
		var authority_start := scheme_separator + 3
		var authority_tail := url.substr(authority_start)
		var authority_end := _first_separator(authority_tail)
		var authority := (
			authority_tail if authority_end < 0
			else authority_tail.substr(0, authority_end)
		)
		if authority.uri_decode().contains("@"):
			return true
	var query_index := url.find("?")
	if query_index < 0:
		return false
	var query := url.substr(query_index + 1)
	var fragment_index := query.find("#")
	if fragment_index >= 0:
		query = query.substr(0, fragment_index)
	for pair in query.split("&", false):
		var key := pair.get_slice("=", 0).uri_decode()
		if _is_sensitive_key(key):
			return true
	return false


static func _is_sensitive_key(key: String) -> bool:
	var normalized := key.strip_edges().to_lower()
	for separator in ["_", "-", ".", " "]:
		normalized = normalized.replace(separator, "")
	return _SENSITIVE_KEYS.has(normalized)


static func _entry_matches(entry: Dictionary, query: String) -> bool:
	for field in ["name", "url", "description", "attribution", "type"]:
		if str(entry[field]).to_lower().find(query) >= 0:
			return true
	return false


static func _entry_name_less(left: Dictionary, right: Dictionary) -> bool:
	return _compare_entries(left, right, "name") < 0


static func _entry_type_less(left: Dictionary, right: Dictionary) -> bool:
	return _compare_entries(left, right, "type") < 0


static func _entry_url_less(left: Dictionary, right: Dictionary) -> bool:
	return _compare_entries(left, right, "url") < 0


static func _compare_entries(left: Dictionary, right: Dictionary, field: String) -> int:
	var left_primary := str(left[field]).to_lower()
	var right_primary := str(right[field]).to_lower()
	if left_primary < right_primary:
		return -1
	if left_primary > right_primary:
		return 1
	var left_name := str(left["name"]).to_lower()
	var right_name := str(right["name"]).to_lower()
	if left_name < right_name:
		return -1
	if left_name > right_name:
		return 1
	var left_url := str(left["url"]).to_lower()
	var right_url := str(right["url"]).to_lower()
	if left_url < right_url:
		return -1
	if left_url > right_url:
		return 1
	return 0
