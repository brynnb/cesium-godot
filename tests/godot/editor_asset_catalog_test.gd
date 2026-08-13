extends SceneTree

const CatalogScript = preload(
	"res://addons/cesium_godot/editor/catalog/editor_asset_catalog.gd"
)


func _initialize() -> void:
	if not _check_http_catalog_and_queries():
		return
	if not _check_local_resolution_and_direct_entries():
		return
	if not _check_validation_and_credential_safety():
		return
	print("Cesium editor asset catalog test passed")
	quit(0)


func _check_http_catalog_and_queries() -> bool:
	var catalog = CatalogScript.new()
	var document := {
		"schema": CatalogScript.SCHEMA,
		"assets": [
			{
				"name": "Village",
				"type": "3d_tiles",
				"url": "/tiles/village/tileset.json",
				"description": "Stone homes",
				"attribution": "Village Survey",
				"future_field": "ignored",
			},
			{
				"name": "Blank scene",
				"type": "blank",
				"description": "Configure this later",
			},
			{
				"name": "Road One",
				"type": "3D_TILES",
				"url": "../tiles/road%20one/tileset.json?version=2#root",
				"attribution": "Road Survey",
			},
		],
	}
	if not catalog.parse_dictionary(
		document,
		"https://assets.example.test/catalogs/world/catalog.json"
	):
		return _fail("Valid HTTP catalog failed: %s" % [catalog.get_errors()])
	if catalog.get_entry_count() != 3:
		return _fail("Valid catalog did not retain all entries")

	var entries: Array[Dictionary] = catalog.get_entries()
	if (
		entries[0].name != "Blank scene" or
		entries[1].name != "Road One" or
		entries[2].name != "Village" or
		entries[0].url != "" or
		entries[1].url != (
			"https://assets.example.test/catalogs/tiles/road%20one/tileset.json?version=2#root"
		) or
		entries[2].url != "https://assets.example.test/tiles/village/tileset.json" or
		entries[2].has("future_field")
	):
		return _fail("HTTP entries were not normalized or deterministically sorted: %s" % [entries])

	var filtered: Array[Dictionary] = catalog.get_entries("survey", "3d_tiles", "url", false)
	if filtered.size() != 2 or filtered[0].name != "Village" or filtered[1].name != "Road One":
		return _fail("Search, type filtering, or descending URL sort was incorrect: %s" % [filtered])
	if not catalog.get_entries("later", "blank").size() == 1:
		return _fail("Description search did not find the blank entry")
	if not catalog.get_entries("", "unsupported").is_empty():
		return _fail("Unsupported query type filter returned entries")

	# Returned entries are defensive copies.
	entries[0].name = "Mutated"
	entries.clear()
	if catalog.get_entries()[0].name != "Blank scene":
		return _fail("Catalog entries were exposed as mutable aliases")

	var json_catalog = CatalogScript.new()
	if not json_catalog.parse_json(JSON.stringify(document), "https://assets.example.test/catalog.json"):
		return _fail("The JSON entry point rejected the same valid document")
	return true


func _check_local_resolution_and_direct_entries() -> bool:
	var local_catalog = CatalogScript.new()
	var document := {
		"schema": CatalogScript.SCHEMA,
		"assets": [{
			"name": "World One",
			"type": "3d_tiles",
			"url": "../World One/terrain/tileset.json",
		}],
	}
	if not local_catalog.parse_dictionary(document, "/tmp/Catalog Folder/catalog.json"):
		return _fail("Valid local catalog failed: %s" % [local_catalog.get_errors()])
	if local_catalog.get_entries()[0].url != "file:///tmp/World%20One/terrain/tileset.json":
		return _fail("Relative local URL was resolved incorrectly: %s" % [local_catalog.get_entries()])

	var resolved: Dictionary = CatalogScript.resolve_asset_url(
		"../tiles/terrain.json",
		"file:///srv/catalogs/world/catalog.json"
	)
	if not resolved.ok or resolved.url != "file:///srv/catalogs/tiles/terrain.json":
		return _fail("file:// reference resolution was incorrect: %s" % [resolved])

	if CatalogScript.local_path_to_file_url("/tmp/Kōjan World/tileset.json") != (
		"file:///tmp/K%C5%8Djan%20World/tileset.json"
	):
		return _fail("Unicode Unix paths were not encoded as canonical file URLs")
	if CatalogScript.local_path_to_file_url("C:\\World Data\\tileset.json") != (
		"file:///C:/World%20Data/tileset.json"
	):
		return _fail("Windows-drive paths were not encoded as canonical file URLs")
	if CatalogScript.local_path_to_file_url("\\\\server\\share name\\tileset.json") != (
		"file://server/share%20name/tileset.json"
	):
		return _fail("UNC paths were not encoded as canonical file URLs")

	var direct_http: Dictionary = CatalogScript.make_direct_entry(
		"Direct world",
		"https://world.example.test/tileset.json",
		"3d_tiles",
		"A direct source",
		"World Team"
	)
	if (
		not direct_http.ok or direct_http.entry.name != "Direct world" or
		direct_http.entry.type != "3d_tiles" or
		direct_http.entry.url != "https://world.example.test/tileset.json"
	):
		return _fail("Direct HTTP entry was not normalized: %s" % [direct_http])

	var direct_local: Dictionary = CatalogScript.make_direct_entry(
		"Fixture",
		"fixtures/lifecycle/tileset.json"
	)
	if (
		not direct_local.ok or
		not str(direct_local.entry.url).begins_with("file:///") or
		not str(direct_local.entry.url).ends_with("/tests/godot/fixtures/lifecycle/tileset.json")
	):
		return _fail("Project-relative direct path was not normalized: %s" % [direct_local])
	return true


func _check_validation_and_credential_safety() -> bool:
	var catalog = CatalogScript.new()
	var valid := {
		"schema": CatalogScript.SCHEMA,
		"assets": [{
			"name": "Valid",
			"type": "3d_tiles",
			"url": "https://example.test/tileset.json",
		}],
	}
	if not catalog.parse_dictionary(valid):
		return _fail("Validation setup catalog failed")

	var secret := "do-not-retain-or-report-this-value"
	var with_secret_field := {
		"schema": CatalogScript.SCHEMA,
		"assets": [{
			"name": "Unsafe",
			"type": "3d_tiles",
			"url": "https://example.test/tileset.json",
			"access_token": secret,
		}],
	}
	if catalog.parse_dictionary(with_secret_field):
		return _fail("Credential-shaped catalog fields were accepted")
	if catalog.get_entry_count() != 0:
		return _fail("An invalid catalog did not atomically clear the model")
	if str(catalog.get_errors()).find(secret) >= 0:
		return _fail("Catalog diagnostics retained or reported a credential value")

	var unsafe_urls := [
		"https://user:password@example.test/tileset.json",
		"https://example.test/tileset.json?access_token=" + secret,
		"https://example.test/tileset.json?x-amz-signature=" + secret,
	]
	for unsafe_url in unsafe_urls:
		var result: Dictionary = CatalogScript.make_direct_entry("Unsafe", unsafe_url)
		if result.ok or str(result.errors).find(secret) >= 0:
			return _fail("Credential-bearing URL was accepted or echoed")

	var invalid_documents := [
		{"schema": "wrong", "assets": []},
		{"schema": CatalogScript.SCHEMA, "assets": {}},
		{
			"schema": CatalogScript.SCHEMA,
			"assets": [{"name": "Voxel", "type": "voxel", "url": "https://example.test"}],
		},
		{
			"schema": CatalogScript.SCHEMA,
			"assets": [{"name": "Relative", "type": "3d_tiles", "url": "tileset.json"}],
		},
		{
			"schema": CatalogScript.SCHEMA,
			"assets": [{"name": "S3", "type": "3d_tiles", "url": "s3://bucket/tileset.json"}],
		},
		{
			"schema": CatalogScript.SCHEMA,
			"assets": [{"name": "Bad blank", "type": "blank", "url": "https://example.test"}],
		},
	]
	for invalid_document in invalid_documents:
		if catalog.parse_dictionary(invalid_document):
			return _fail("An invalid catalog document was accepted: %s" % [invalid_document])
	if catalog.parse_json("{not-json"):
		return _fail("Malformed JSON was accepted")
	if str(catalog.get_errors()).find("not-json") >= 0:
		return _fail("Malformed JSON diagnostics echoed source content")
	return true


func _fail(message: String) -> bool:
	push_error(message)
	quit(1)
	return false
