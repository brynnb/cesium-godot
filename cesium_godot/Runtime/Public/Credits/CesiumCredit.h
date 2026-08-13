// Godot adaptation reviewed against Cesium for Unreal v2.29.0:
// - Source/CesiumRuntime/Public/CesiumCreditSystem.h
// - Source/CesiumRuntime/Private/CesiumCreditSystem.cpp
// - Source/CesiumRuntime/Private/ScreenCreditsWidget.*
// Last upstream review: Cesium for Unreal v2.29.0.

#ifndef CESIUM_GODOT_CREDIT_H
#define CESIUM_GODOT_CREDIT_H

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/string.hpp>

using namespace godot;

/**
 * An immutable, current-frame attribution snapshot.
 *
 * The HTML is preserved exactly for applications that provide a complete HTML
 * presenter. The other fields are safe, engine-native representations for
 * Godot UI, accessibility, logging, and tests. Instances do not retain Cesium
 * Native credit handles or their source tileset.
 */
class CesiumCredit final : public RefCounted {
	GDCLASS(CesiumCredit, RefCounted)

public:
	String get_html() const;
	String get_plain_text() const;
	String get_accessible_text() const;
	bool get_show_on_screen() const;
	Array get_runs() const;
	Array get_links() const;
	PackedStringArray get_image_urls() const;

	void initialize(
		const String& html,
		const String& plainText,
		const String& accessibleText,
		bool showOnScreen,
		const Array& runs,
		const Array& links,
		const PackedStringArray& imageUrls
	);

protected:
	static void _bind_methods();

private:
	String m_html;
	String m_plainText;
	String m_accessibleText;
	bool m_showOnScreen = false;
	Array m_runs;
	Array m_links;
	PackedStringArray m_imageUrls;
};

#endif
