#include "Runtime/Public/Credits/CesiumCredit.h"

#include <godot_cpp/core/class_db.hpp>

String CesiumCredit::get_html() const {
	return this->m_html;
}

String CesiumCredit::get_plain_text() const {
	return this->m_plainText;
}

String CesiumCredit::get_accessible_text() const {
	return this->m_accessibleText;
}

bool CesiumCredit::get_show_on_screen() const {
	return this->m_showOnScreen;
}

Array CesiumCredit::get_runs() const {
	return this->m_runs.duplicate(true);
}

Array CesiumCredit::get_links() const {
	return this->m_links.duplicate(true);
}

PackedStringArray CesiumCredit::get_image_urls() const {
	return this->m_imageUrls;
}

void CesiumCredit::initialize(
	const String& html,
	const String& plainText,
	const String& accessibleText,
	bool showOnScreen,
	const Array& runs,
	const Array& links,
	const PackedStringArray& imageUrls
) {
	this->m_html = html;
	this->m_plainText = plainText;
	this->m_accessibleText = accessibleText;
	this->m_showOnScreen = showOnScreen;
	this->m_runs = runs.duplicate(true);
	this->m_links = links.duplicate(true);
	this->m_imageUrls = imageUrls;
}

void CesiumCredit::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_html"), &CesiumCredit::get_html);
	ClassDB::bind_method(
		D_METHOD("get_plain_text"),
		&CesiumCredit::get_plain_text
	);
	ClassDB::bind_method(
		D_METHOD("get_accessible_text"),
		&CesiumCredit::get_accessible_text
	);
	ClassDB::bind_method(
		D_METHOD("get_show_on_screen"),
		&CesiumCredit::get_show_on_screen
	);
	ClassDB::bind_method(D_METHOD("get_runs"), &CesiumCredit::get_runs);
	ClassDB::bind_method(D_METHOD("get_links"), &CesiumCredit::get_links);
	ClassDB::bind_method(
		D_METHOD("get_image_urls"),
		&CesiumCredit::get_image_urls
	);

	ADD_PROPERTY(
		PropertyInfo(Variant::STRING, "html", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE),
		"",
		"get_html"
	);
	ADD_PROPERTY(
		PropertyInfo(Variant::STRING, "plain_text", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE),
		"",
		"get_plain_text"
	);
	ADD_PROPERTY(
		PropertyInfo(Variant::STRING, "accessible_text", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE),
		"",
		"get_accessible_text"
	);
	ADD_PROPERTY(
		PropertyInfo(Variant::BOOL, "show_on_screen", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE),
		"",
		"get_show_on_screen"
	);
	ADD_PROPERTY(
		PropertyInfo(Variant::ARRAY, "runs", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE),
		"",
		"get_runs"
	);
	ADD_PROPERTY(
		PropertyInfo(Variant::ARRAY, "links", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE),
		"",
		"get_links"
	);
	ADD_PROPERTY(
		PropertyInfo(Variant::PACKED_STRING_ARRAY, "image_urls", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE),
		"",
		"get_image_urls"
	);
}
