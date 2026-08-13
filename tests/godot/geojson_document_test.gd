extends SceneTree

const FIXTURE := "res://fixtures/vector/overlay.geojson"


func _initialize() -> void:
	_run.call_deferred()


func _run() -> void:
	var file := FileAccess.open(FIXTURE, FileAccess.READ)
	if file == null:
		_fail("Could not open GeoJSON fixture")
		return
	var source := file.get_as_text()
	file.close()

	var document := CesiumGeoJsonDocument.new()
	if not document.load_from_string(source, [{
		"html": "<a href='https://example.test'>Example</a>",
		"show_on_screen": true,
	}]):
		_fail("Valid GeoJSON was rejected: %s" % [document.errors])
		return
	if not document.is_valid() or not document.errors.is_empty():
		_fail("Parsed document validity or errors were incorrect")
		return
	if document.warnings.size() != 1:
		_fail("Repaired open polygon ring did not preserve its warning: %s" % [document.warnings])
		return
	var stats: Dictionary = document.get_statistics()
	if (
		int(stats.objects) != 7 or int(stats.features) != 3 or
		int(stats.polygons) != 1 or int(stats.line_strings) != 1 or
		int(stats.points) != 1
	):
		_fail("GeoJSON object statistics were incorrect: %s" % [stats])
		return
	var attributions: Array = document.get_attributions()
	if (
		attributions.size() != 1 or
		not bool(attributions[0].show_on_screen) or
		str(attributions[0].html).find("Example") < 0
	):
		_fail("GeoJSON attributions were not retained: %s" % [attributions])
		return

	var root_object := document.get_root_object()
	if (
		root_object == null or not root_object.is_valid() or
		root_object.object_type != CesiumGeoJsonObject.FeatureCollection or
		root_object.get_object_type_name() != "FeatureCollection" or
		root_object.get_foreign_members().get("vendor", "") !=
			"foreign-root-member" or
		root_object.get_children().size() != 3
	):
		_fail("Root object or its owned child view was incomplete")
		return

	var polygon_feature: CesiumGeoJsonObject
	var line_feature: CesiumGeoJsonObject
	var point_geometry: CesiumGeoJsonObject
	for value in document.get_objects():
		var object := value as CesiumGeoJsonObject
		if object == null:
			continue
		var feature_id: Variant = object.get_feature_id()
		if object.is_feature() and typeof(feature_id) == TYPE_INT and int(feature_id) == 7:
			polygon_feature = object
		elif object.is_feature() and typeof(feature_id) == TYPE_STRING and str(feature_id) == "road":
			line_feature = object
		elif object.object_type == CesiumGeoJsonObject.Point:
			point_geometry = object
	if polygon_feature == null or line_feature == null or point_geometry == null:
		_fail("Typed object traversal did not return all feature and geometry views")
		return
	var polygon_geometry := polygon_feature.get_feature_geometry()
	var polygons: Array = polygon_geometry.get_polygons_exact()
	if (
		polygons.size() != 1 or (polygons[0] as Array).size() != 1 or
		((polygons[0] as Array)[0] as PackedFloat64Array).size() != 15
	):
		_fail("Polygon exact coordinates did not include Native's closed ring")
		return
	var lines: Array = line_feature.get_feature_geometry().get_line_strings_exact()
	if lines.size() != 1 or (lines[0] as PackedFloat64Array) != PackedFloat64Array([
		-0.01, 0.0, 0.0, 0.01, 0.0, 0.0,
	]):
		_fail("LineString exact coordinates changed shape or precision: %s" % [lines])
		return
	if point_geometry.get_points_exact() != PackedFloat64Array([0.0, 0.0, 12.5]):
		_fail("Point exact coordinates changed precision")
		return
	var properties: Dictionary = polygon_feature.get_feature_properties()
	if (
		properties.name != "test polygon" or
		not bool(properties.nested.enabled) or
		properties.large_unsigned_as_text != "18446744073709551615"
	):
		_fail("Feature properties were not converted losslessly: %s" % [properties])
		return

	var style := CesiumVectorStyle.new()
	style.line_color = Color(0.1, 0.2, 0.3, 0.4)
	style.line_color_mode = CesiumVectorStyle.ColorRandom
	style.line_width = 3.5
	style.line_width_mode = CesiumVectorStyle.WidthMeters
	style.polygon_fill_enabled = false
	style.polygon_outline_enabled = true
	style.polygon_outline_color = Color(0.9, 0.8, 0.7, 0.6)
	style.polygon_outline_width = 4.5
	if not polygon_geometry.set_style(style) or not polygon_geometry.has_style():
		_fail("Per-object vector style could not be assigned")
		return
	var round_trip := polygon_geometry.get_style()
	if (
		round_trip == null or round_trip.line_color_mode != CesiumVectorStyle.ColorRandom or
		round_trip.line_width_mode != CesiumVectorStyle.WidthMeters or
		not is_equal_approx(round_trip.line_width, 3.5) or
		round_trip.polygon_fill_enabled or not round_trip.polygon_outline_enabled or
		not is_equal_approx(round_trip.polygon_outline_width, 4.5)
	):
		_fail("Vector style did not round-trip through Cesium Native")
		return
	polygon_geometry.clear_style()
	if polygon_geometry.has_style():
		_fail("Per-object vector style could not be cleared")
		return

	# The object view owns the parsed Native document independently.
	document.clear()
	if document.is_valid() or not polygon_feature.is_valid() or not polygon_geometry.is_valid():
		_fail("GeoJSON object views did not survive source Resource release")
		return
	if polygon_feature.get_feature_properties().name != "test polygon":
		_fail("Retained object view lost feature properties")
		return

	var invalid := CesiumGeoJsonDocument.new()
	if invalid.load_from_string("{ deliberately malformed"):
		_fail("Malformed GeoJSON unexpectedly parsed")
		return
	if invalid.is_valid() or invalid.errors.is_empty():
		_fail("Malformed GeoJSON did not expose Native parser errors")
		return

	print("Cesium GeoJSON document, object, and vector-style test passed")
	quit(0)


func _fail(message: String) -> void:
	push_error(message)
	quit(1)
