#include "Godot/Nodes/CesiumGDCreditSystem.h"

#include "CesiumUtility/CreditSystem.h"
#include "Runtime/Public/Credits/CesiumCredit.h"
#include "Utils/AssetManipulation.h"

#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/http_request.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/margin_container.hpp>
#include <godot_cpp/classes/marshalls.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/panel_container.hpp>
#include <godot_cpp/classes/popup_panel.hpp>
#include <godot_cpp/classes/rich_text_label.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/error_macros.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/core/object.hpp>
#include <godot_cpp/variant/callable.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector2i.hpp>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <exception>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {

struct ParsedRun {
	std::string text;
	std::string url;
	std::string imageUrl;
};

struct ParsedCredit {
	std::vector<ParsedRun> runs;
	std::string plainText;
	std::string accessibleText;
	std::vector<std::pair<std::string, std::string>> links;
	std::vector<std::string> imageUrls;
};

std::string ascii_lower(std::string value) {
	std::transform(
		value.begin(),
		value.end(),
		value.begin(),
		[](unsigned char character) {
			return static_cast<char>(std::tolower(character));
		}
	);
	return value;
}

std::string to_utf8(const String& value) {
	const CharString encoded = value.utf8();
	return std::string(encoded.get_data(), encoded.length());
}

bool begins_with_ascii(const PackedByteArray& data, std::string_view prefix) {
	if (data.size() < static_cast<int64_t>(prefix.size())) {
		return false;
	}
	for (size_t index = 0; index < prefix.size(); ++index) {
		if (data[static_cast<int64_t>(index)] !=
			static_cast<uint8_t>(prefix[index])) {
			return false;
		}
	}
	return true;
}

std::string trim_ascii(std::string value) {
	auto isWhitespace = [](unsigned char character) {
		return std::isspace(character) != 0;
	};
	value.erase(
		value.begin(),
		std::find_if(value.begin(), value.end(), [&](char character) {
			return !isWhitespace(static_cast<unsigned char>(character));
		})
	);
	value.erase(
		std::find_if(value.rbegin(), value.rend(), [&](char character) {
			return !isWhitespace(static_cast<unsigned char>(character));
		}).base(),
		value.end()
	);
	return value;
}

void append_utf8(std::string& output, uint32_t codePoint) {
	if (codePoint <= 0x7f) {
		output.push_back(static_cast<char>(codePoint));
	} else if (codePoint <= 0x7ff) {
		output.push_back(static_cast<char>(0xc0 | (codePoint >> 6)));
		output.push_back(static_cast<char>(0x80 | (codePoint & 0x3f)));
	} else if (codePoint <= 0xffff) {
		output.push_back(static_cast<char>(0xe0 | (codePoint >> 12)));
		output.push_back(
			static_cast<char>(0x80 | ((codePoint >> 6) & 0x3f))
		);
		output.push_back(static_cast<char>(0x80 | (codePoint & 0x3f)));
	} else if (codePoint <= 0x10ffff) {
		output.push_back(static_cast<char>(0xf0 | (codePoint >> 18)));
		output.push_back(
			static_cast<char>(0x80 | ((codePoint >> 12) & 0x3f))
		);
		output.push_back(
			static_cast<char>(0x80 | ((codePoint >> 6) & 0x3f))
		);
		output.push_back(static_cast<char>(0x80 | (codePoint & 0x3f)));
	}
}

std::string decode_html_entities(std::string_view input) {
	static const std::unordered_map<std::string, std::string> namedEntities {
		{"amp", "&"},
		{"apos", "'"},
		{"copy", "\xc2\xa9"},
		{"gt", ">"},
		{"hellip", "\xe2\x80\xa6"},
		{"lt", "<"},
		{"middot", "\xc2\xb7"},
		{"nbsp", " "},
		{"quot", "\""},
		{"reg", "\xc2\xae"},
		{"trade", "\xe2\x84\xa2"}
	};

	std::string output;
	output.reserve(input.size());
	for (size_t index = 0; index < input.size();) {
		if (input[index] != '&') {
			output.push_back(input[index++]);
			continue;
		}
		const size_t semicolon = input.find(';', index + 1);
		if (semicolon == std::string_view::npos || semicolon - index > 16) {
			output.push_back(input[index++]);
			continue;
		}
		const std::string entity(input.substr(index + 1, semicolon - index - 1));
		const auto named = namedEntities.find(entity);
		if (named != namedEntities.end()) {
			output += named->second;
			index = semicolon + 1;
			continue;
		}
		if (!entity.empty() && entity[0] == '#') {
			try {
				size_t parsed = 0;
				const int base = entity.size() > 1 &&
					(entity[1] == 'x' || entity[1] == 'X')
					? 16
					: 10;
				const size_t numberOffset = base == 16 ? 2 : 1;
				const unsigned long value = std::stoul(
					entity.substr(numberOffset),
					&parsed,
					base
				);
				if (parsed == entity.size() - numberOffset && value <= 0x10ffff) {
					append_utf8(output, static_cast<uint32_t>(value));
					index = semicolon + 1;
					continue;
				}
			} catch (const std::exception&) {
			}
		}
		output.append(input.substr(index, semicolon - index + 1));
		index = semicolon + 1;
	}
	return output;
}

std::unordered_map<std::string, std::string>
parse_html_attributes(std::string_view input) {
	std::unordered_map<std::string, std::string> attributes;
	size_t index = 0;
	while (index < input.size()) {
		while (index < input.size() &&
			std::isspace(static_cast<unsigned char>(input[index])) != 0) {
			++index;
		}
		if (index >= input.size() || input[index] == '/') {
			break;
		}
		const size_t nameBegin = index;
		while (index < input.size() &&
			std::isspace(static_cast<unsigned char>(input[index])) == 0 &&
			input[index] != '=' && input[index] != '/') {
			++index;
		}
		std::string name = ascii_lower(std::string(input.substr(
			nameBegin,
			index - nameBegin
		)));
		while (index < input.size() &&
			std::isspace(static_cast<unsigned char>(input[index])) != 0) {
			++index;
		}
		std::string value;
		if (index < input.size() && input[index] == '=') {
			++index;
			while (index < input.size() &&
				std::isspace(static_cast<unsigned char>(input[index])) != 0) {
				++index;
			}
			if (index < input.size() &&
				(input[index] == '\'' || input[index] == '\"')) {
				const char quote = input[index++];
				const size_t valueBegin = index;
				while (index < input.size() && input[index] != quote) {
					++index;
				}
				value = std::string(input.substr(valueBegin, index - valueBegin));
				if (index < input.size()) {
					++index;
				}
			} else {
				const size_t valueBegin = index;
				while (index < input.size() &&
					std::isspace(static_cast<unsigned char>(input[index])) == 0 &&
					input[index] != '/') {
					++index;
				}
				value = std::string(input.substr(valueBegin, index - valueBegin));
			}
		}
		if (!name.empty()) {
			attributes.insert_or_assign(name, decode_html_entities(value));
		}
	}
	return attributes;
}

std::string attribution_fallback(const std::string& url) {
	if (url.empty()) {
		return "Image attribution";
	}
	const size_t scheme = url.find("://");
	const size_t hostBegin = scheme == std::string::npos ? 0 : scheme + 3;
	const size_t hostEnd = url.find_first_of("/?#", hostBegin);
	const std::string host = url.substr(hostBegin, hostEnd - hostBegin);
	return host.empty() ? "Image attribution" : "Attribution from " + host;
}

ParsedCredit parse_credit_html(const std::string& html) {
	ParsedCredit result;
	std::vector<std::string> linkStack;
	bool pendingSpace = false;

	auto currentUrl = [&]() -> std::string {
		return linkStack.empty() ? std::string() : linkStack.back();
	};
	auto addRun = [&](std::string text, std::string url, std::string imageUrl) {
		text = trim_ascii(decode_html_entities(text));
		if (text.empty() && !imageUrl.empty()) {
			text = attribution_fallback(!url.empty() ? url : imageUrl);
		}
		if (text.empty()) {
			return;
		}
		if (pendingSpace && !result.runs.empty()) {
			ParsedRun& previous = result.runs.back();
			if (!previous.text.empty() && previous.text.back() != ' ') {
				previous.text.push_back(' ');
			}
		}
		pendingSpace = false;
		if (!result.runs.empty() && result.runs.back().url == url &&
			result.runs.back().imageUrl.empty() && imageUrl.empty()) {
			result.runs.back().text += text;
		} else {
			result.runs.push_back(ParsedRun {
				std::move(text),
				std::move(url),
				std::move(imageUrl)
			});
		}
	};

	for (size_t index = 0; index < html.size();) {
		if (html.compare(index, 4, "<!--") == 0) {
			const size_t end = html.find("-->", index + 4);
			index = end == std::string::npos ? html.size() : end + 3;
			continue;
		}
		if (html[index] != '<') {
			const size_t tag = html.find('<', index);
			const size_t end = tag == std::string::npos ? html.size() : tag;
			const std::string_view chunk(html.data() + index, end - index);
			const bool leadingSpace = !chunk.empty() &&
				std::isspace(static_cast<unsigned char>(chunk.front())) != 0;
			const bool trailingSpace = !chunk.empty() &&
				std::isspace(static_cast<unsigned char>(chunk.back())) != 0;
			pendingSpace = pendingSpace || leadingSpace;
			addRun(std::string(chunk), currentUrl(), std::string());
			pendingSpace = pendingSpace || trailingSpace;
			index = end;
			continue;
		}

		const size_t close = html.find('>', index + 1);
		if (close == std::string::npos) {
			addRun(html.substr(index), currentUrl(), std::string());
			break;
		}
		std::string tag = trim_ascii(html.substr(index + 1, close - index - 1));
		bool closing = !tag.empty() && tag.front() == '/';
		if (closing) {
			tag.erase(tag.begin());
			tag = trim_ascii(tag);
		}
		const size_t nameEnd = tag.find_first_of(" \t\r\n/");
		const std::string name = ascii_lower(tag.substr(0, nameEnd));
		const std::string_view attributeText = nameEnd == std::string::npos
			? std::string_view()
			: std::string_view(tag).substr(nameEnd);

		if (closing && name == "a") {
			if (!linkStack.empty()) {
				linkStack.pop_back();
			}
		} else if (!closing && name == "a") {
			const auto attributes = parse_html_attributes(attributeText);
			const auto href = attributes.find("href");
			linkStack.push_back(href == attributes.end() ? std::string() : href->second);
		} else if (!closing && name == "img") {
			const auto attributes = parse_html_attributes(attributeText);
			const auto source = attributes.find("src");
			const auto alt = attributes.find("alt");
			const auto title = attributes.find("title");
			const std::string imageUrl = source == attributes.end()
				? std::string()
				: source->second;
			std::string text = alt == attributes.end() ? std::string() : alt->second;
			if (text.empty() && title != attributes.end()) {
				text = title->second;
			}
			addRun(std::move(text), currentUrl(), imageUrl);
		} else if (
			name == "br" || name == "p" || name == "div" || name == "li"
		) {
			pendingSpace = true;
		}
		index = close + 1;
	}

	std::unordered_set<std::string> linkKeys;
	std::unordered_set<std::string> imageKeys;
	for (ParsedRun& run : result.runs) {
		run.text = trim_ascii(run.text);
		if (run.text.empty()) {
			continue;
		}
		if (!result.plainText.empty()) {
			result.plainText += ' ';
		}
		result.plainText += run.text;
		if (!run.url.empty()) {
			const std::string key = run.url + "\n" + run.text;
			if (linkKeys.insert(key).second) {
				result.links.emplace_back(run.text, run.url);
			}
		}
		if (!run.imageUrl.empty() && imageKeys.insert(run.imageUrl).second) {
			result.imageUrls.push_back(run.imageUrl);
		}
	}
	result.plainText = trim_ascii(result.plainText);
	result.accessibleText = result.plainText.empty()
		? std::string("Data attribution")
		: result.plainText;
	return result;
}

Ref<CesiumCredit> make_credit(
	const std::string& html,
	bool showOnScreen
) {
	const ParsedCredit parsed = parse_credit_html(html);
	Array runs;
	for (const ParsedRun& parsedRun : parsed.runs) {
		if (parsedRun.text.empty()) {
			continue;
		}
		Dictionary run;
		run["kind"] = parsedRun.imageUrl.empty() ? "text" : "image";
		run["text"] = String::utf8(parsedRun.text.c_str());
		run["url"] = String::utf8(parsedRun.url.c_str());
		run["image_url"] = String::utf8(parsedRun.imageUrl.c_str());
		runs.push_back(run);
	}
	Array links;
	for (const auto& [text, url] : parsed.links) {
		Dictionary link;
		link["text"] = String::utf8(text.c_str());
		link["url"] = String::utf8(url.c_str());
		links.push_back(link);
	}
	PackedStringArray imageUrls;
	for (const std::string& url : parsed.imageUrls) {
		imageUrls.push_back(String::utf8(url.c_str()));
	}

	Ref<CesiumCredit> credit;
	credit.instantiate();
	credit->initialize(
		String::utf8(html.c_str()),
		String::utf8(parsed.plainText.c_str()),
		String::utf8(parsed.accessibleText.c_str()),
		showOnScreen,
		runs,
		links,
		imageUrls
	);
	return credit;
}

bool is_safe_external_credit_url(const String& url) {
	const String normalized = url.strip_edges().to_lower();
	return normalized.begins_with("https://") ||
		normalized.begins_with("http://");
}

} // namespace

CesiumGDCreditSystem::CesiumGDCreditSystem()
	: m_creditSystem(std::make_shared<CesiumUtility::CreditSystem>()) {}

CesiumGDCreditSystem::~CesiumGDCreditSystem() {
	if (s_lastInstanceId == ObjectID(this->get_instance_id())) {
		s_lastInstanceId = ObjectID();
	}
}

CesiumGDCreditSystem* CesiumGDCreditSystem::get_last_instance() {
	if (s_lastInstanceId == ObjectID()) {
		return nullptr;
	}
	return Object::cast_to<CesiumGDCreditSystem>(
		ObjectDB::get_instance(s_lastInstanceId)
	);
}

CesiumGDCreditSystem* CesiumGDCreditSystem::get_singleton(Node* context) {
	ERR_FAIL_NULL_V(context, nullptr);
	SceneTree* tree = context->get_tree();
	ERR_FAIL_NULL_V(tree, nullptr);

	CesiumGDCreditSystem* result =
		CesiumGodot::AssetManipulation::find_or_create_credit_system(
			context,
			false
		);
	if (result == nullptr) {
		ERR_PRINT(
			"Could not find or create the Cesium credit system for this scene"
		);
		return nullptr;
	}
	s_lastInstanceId = ObjectID(result->get_instance_id());
	return result;
}

const std::shared_ptr<CesiumUtility::CreditSystem>&
CesiumGDCreditSystem::get_native_credit_system() const {
	return this->m_creditSystem;
}

Array CesiumGDCreditSystem::get_current_credits() const {
	return this->m_currentCredits.duplicate(false);
}

int32_t CesiumGDCreditSystem::get_current_credit_count() const {
	return static_cast<int32_t>(this->m_currentCredits.size());
}

String CesiumGDCreditSystem::get_current_html() const {
	return this->m_currentHtml;
}

String CesiumGDCreditSystem::get_current_plain_text() const {
	return this->m_currentPlainText;
}

String CesiumGDCreditSystem::get_on_screen_text() const {
	return this->m_onScreenText;
}

String CesiumGDCreditSystem::get_popup_text() const {
	return this->m_popupText;
}

bool CesiumGDCreditSystem::get_credits_updated() const {
	return this->m_creditsUpdated;
}

bool CesiumGDCreditSystem::get_has_hidden_credits() const {
	return this->m_hasHiddenCredits;
}

int32_t CesiumGDCreditSystem::get_loaded_credit_image_count() const {
	return static_cast<int32_t>(this->m_creditImages.size());
}

int32_t CesiumGDCreditSystem::get_pending_credit_image_count() const {
	return static_cast<int32_t>(
		this->m_creditImageRequests.size() +
		this->m_queuedCreditImageUrls.size()
	);
}

int32_t CesiumGDCreditSystem::get_failed_credit_image_count() const {
	return static_cast<int32_t>(this->m_failedCreditImages.size());
}

Ref<Texture2D> CesiumGDCreditSystem::get_credit_image(const String& url) const {
	const auto found = this->m_creditImages.find(to_utf8(url.strip_edges()));
	return found == this->m_creditImages.end()
		? Ref<Texture2D>()
		: found->second;
}

void CesiumGDCreditSystem::set_presenter_enabled(bool enabled) {
	if (this->m_presenterEnabled == enabled) {
		return;
	}
	this->m_presenterEnabled = enabled;
	if (!enabled) {
		this->hide_attribution_popup();
	}
	this->rebuild_presenter();
	this->emit_signal("presenter_enabled_changed", enabled);
}

bool CesiumGDCreditSystem::get_presenter_enabled() const {
	return this->m_presenterEnabled;
}

void CesiumGDCreditSystem::set_open_links_externally(bool enabled) {
	this->m_openLinksExternally = enabled;
}

bool CesiumGDCreditSystem::get_open_links_externally() const {
	return this->m_openLinksExternally;
}

void CesiumGDCreditSystem::set_remote_credit_images_enabled(bool enabled) {
	if (this->m_remoteCreditImagesEnabled == enabled) {
		return;
	}
	this->m_remoteCreditImagesEnabled = enabled;
	if (!enabled) {
		for (const auto& [url, requestId] : this->m_creditImageRequests) {
			(void)url;
			HTTPRequest* request = Object::cast_to<HTTPRequest>(
				ObjectDB::get_instance(requestId)
			);
			if (request != nullptr) {
				request->cancel_request();
				request->queue_free();
			}
		}
		this->m_creditImageRequests.clear();
		this->m_queuedCreditImageUrls.clear();
	} else {
		this->queue_credit_images();
		this->start_queued_credit_image_requests();
	}
}

bool CesiumGDCreditSystem::get_remote_credit_images_enabled() const {
	return this->m_remoteCreditImagesEnabled;
}

void CesiumGDCreditSystem::retry_failed_credit_images() {
	this->m_failedCreditImages.clear();
	this->queue_credit_images();
	this->start_queued_credit_image_requests();
}

void CesiumGDCreditSystem::show_attribution_popup() {
	if (!this->m_presenterEnabled || !this->m_hasHiddenCredits) {
		return;
	}
	this->ensure_presenter();
	if (this->m_popup != nullptr) {
		this->m_popup->popup_centered_clamped(Vector2i(720, 480), 0.85);
	}
}

void CesiumGDCreditSystem::hide_attribution_popup() {
	if (this->m_popup != nullptr && this->m_popup->is_visible()) {
		this->m_popup->hide();
	}
}

bool CesiumGDCreditSystem::is_attribution_popup_visible() const {
	return this->m_popup != nullptr && this->m_popup->is_visible();
}

void CesiumGDCreditSystem::update_credits() {
	this->m_creditsUpdated = false;
	if (this->m_creditSystem == nullptr) {
		return;
	}

	const CesiumUtility::CreditsSnapshot& snapshot =
		this->m_creditSystem->getSnapshot();
	std::vector<CreditKey> nextKeys;
	nextKeys.reserve(snapshot.currentCredits.size());
	for (const CesiumUtility::Credit& credit : snapshot.currentCredits) {
		nextKeys.push_back(CreditKey {
			this->m_creditSystem->getHtml(credit),
			this->m_creditSystem->shouldBeShownOnScreen(credit)
		});
	}
	if (nextKeys == this->m_lastCreditKeys) {
		return;
	}

	this->m_lastCreditKeys = nextKeys;
	this->m_currentCredits.clear();
	this->m_currentHtml = String();
	this->m_currentPlainText = String();
	this->m_onScreenText = String();
	this->m_popupText = String();
	this->m_hasHiddenCredits = false;

	for (const CreditKey& key : this->m_lastCreditKeys) {
		Ref<CesiumCredit> credit = make_credit(key.html, key.showOnScreen);
		this->m_currentCredits.push_back(credit);
		if (!this->m_currentHtml.is_empty()) {
			this->m_currentHtml += "\n";
		}
		this->m_currentHtml += credit->get_html();
		if (!this->m_currentPlainText.is_empty()) {
			this->m_currentPlainText += "\n";
		}
		this->m_currentPlainText += credit->get_accessible_text();
		String& target = key.showOnScreen
			? this->m_onScreenText
			: this->m_popupText;
		if (!target.is_empty()) {
			target += key.showOnScreen ? " \u2022 " : "\n";
		}
		target += credit->get_accessible_text();
		this->m_hasHiddenCredits =
			this->m_hasHiddenCredits || !key.showOnScreen;
	}

	this->m_creditsUpdated = true;
	this->queue_credit_images();
	this->start_queued_credit_image_requests();
	this->rebuild_presenter();
	this->emit_signal(
		"credits_changed",
		this->m_currentCredits.duplicate(false)
	);
}

void CesiumGDCreditSystem::_process(double delta) {
	(void)delta;
	this->update_credits();
}

void CesiumGDCreditSystem::_enter_tree() {
	if (get_last_instance() == nullptr) {
		s_lastInstanceId = ObjectID(this->get_instance_id());
	}
	this->set_process(true);
	this->ensure_presenter();
	this->queue_credit_images();
	this->start_queued_credit_image_requests();
	this->rebuild_presenter();
}

void CesiumGDCreditSystem::_exit_tree() {
	this->set_process(false);
	this->hide_attribution_popup();
	for (const auto& [url, requestId] : this->m_creditImageRequests) {
		(void)url;
		HTTPRequest* request = Object::cast_to<HTTPRequest>(
			ObjectDB::get_instance(requestId)
		);
		if (request != nullptr) {
			request->cancel_request();
			request->queue_free();
		}
	}
	this->m_creditImageRequests.clear();
	this->m_queuedCreditImageUrls.clear();
	if (s_lastInstanceId == ObjectID(this->get_instance_id())) {
		s_lastInstanceId = ObjectID();
	}
}

void CesiumGDCreditSystem::ensure_presenter() {
	if (this->m_onScreenPanel != nullptr) {
		return;
	}

	this->set_anchors_and_offsets_preset(
		Control::LayoutPreset::PRESET_BOTTOM_WIDE,
		Control::LayoutPresetMode::PRESET_MODE_MINSIZE,
		8
	);
	this->set_mouse_filter(Control::MouseFilter::MOUSE_FILTER_PASS);

	this->m_onScreenPanel = memnew(PanelContainer);
	this->m_onScreenPanel->set_name("OnScreenAttributionPanel");
	this->add_child(this->m_onScreenPanel, false, INTERNAL_MODE_FRONT);

	HBoxContainer* row = memnew(HBoxContainer);
	row->set_name("AttributionRow");
	this->m_onScreenPanel->add_child(row, false, INTERNAL_MODE_FRONT);

	this->m_onScreenLabel = memnew(RichTextLabel);
	this->m_onScreenLabel->set_name("OnScreenCredits");
	this->m_onScreenLabel->set_use_bbcode(false);
	this->m_onScreenLabel->set_fit_content(true);
	this->m_onScreenLabel->set_scroll_active(false);
	this->m_onScreenLabel->set_selection_enabled(true);
	this->m_onScreenLabel->set_custom_minimum_size(Vector2(360.0, 28.0));
	this->m_onScreenLabel->set_tooltip_text("Current Cesium data attribution");
	row->add_child(this->m_onScreenLabel, false, INTERNAL_MODE_FRONT);
	this->m_onScreenLabel->connect(
		"meta_clicked",
		Callable(this, "_on_credit_link_clicked")
	);

	this->m_popupButton = memnew(Button);
	this->m_popupButton->set_name("DataAttributionButton");
	this->m_popupButton->set_text("Data attribution");
	this->m_popupButton->set_tooltip_text(
		"Show complete data-source attribution"
	);
	row->add_child(this->m_popupButton, false, INTERNAL_MODE_FRONT);
	this->m_popupButton->connect(
		"pressed",
		Callable(this, "_on_popup_button_pressed")
	);

	this->m_popup = memnew(PopupPanel);
	this->m_popup->set_name("AttributionPopup");
	this->m_popup->set_title("Data attribution");
	this->m_popup->set_transient(true);
	this->add_child(this->m_popup, false, INTERNAL_MODE_FRONT);

	MarginContainer* margin = memnew(MarginContainer);
	margin->set_name("AttributionPopupMargin");
	margin->set_anchors_and_offsets_preset(Control::LayoutPreset::PRESET_FULL_RECT);
	this->m_popup->add_child(margin, false, INTERNAL_MODE_FRONT);

	VBoxContainer* column = memnew(VBoxContainer);
	column->set_name("AttributionPopupColumn");
	margin->add_child(column, false, INTERNAL_MODE_FRONT);

	this->m_popupLabel = memnew(RichTextLabel);
	this->m_popupLabel->set_name("PopupCredits");
	this->m_popupLabel->set_use_bbcode(false);
	this->m_popupLabel->set_selection_enabled(true);
	this->m_popupLabel->set_scroll_active(true);
	this->m_popupLabel->set_custom_minimum_size(Vector2(680.0, 420.0));
	this->m_popupLabel->set_tooltip_text(
		"Complete current Cesium data attribution"
	);
	column->add_child(this->m_popupLabel, false, INTERNAL_MODE_FRONT);
	this->m_popupLabel->connect(
		"meta_clicked",
		Callable(this, "_on_credit_link_clicked")
	);
}

void CesiumGDCreditSystem::queue_credit_images() {
	this->m_currentImageUrls.clear();
	for (int64_t creditIndex = 0;
		creditIndex < this->m_currentCredits.size();
		++creditIndex) {
		const Variant creditValue = this->m_currentCredits[creditIndex];
		Ref<CesiumCredit> credit = creditValue;
		if (credit.is_null()) {
			continue;
		}
		const PackedStringArray urls = credit->get_image_urls();
		for (int64_t index = 0; index < urls.size(); ++index) {
			const String url = urls[index].strip_edges();
			if (url.is_empty()) {
				continue;
			}
			const std::string key = to_utf8(url);
			this->m_currentImageUrls.insert(key);
			if (
				this->m_creditImages.contains(key) ||
				this->m_creditImageRequests.contains(key) ||
				this->m_failedCreditImages.contains(key) ||
				std::find(
					this->m_queuedCreditImageUrls.begin(),
					this->m_queuedCreditImageUrls.end(),
					key
				) != this->m_queuedCreditImageUrls.end()
			) {
				continue;
			}
			if (url.begins_with("data:image/")) {
				this->load_data_credit_image(url);
				continue;
			}
			if (!is_safe_external_credit_url(url)) {
				this->record_credit_image_failure(
					url,
					"Only data:image, HTTPS, and HTTP credit images are supported"
				);
				continue;
			}
			if (this->m_remoteCreditImagesEnabled) {
				this->m_queuedCreditImageUrls.push_back(key);
			}
		}
	}
	this->prune_credit_image_cache();
}

void CesiumGDCreditSystem::start_queued_credit_image_requests() {
	if (!this->is_inside_tree() || !this->m_remoteCreditImagesEnabled) {
		return;
	}
	while (
		static_cast<int32_t>(this->m_creditImageRequests.size()) <
			this->m_maximumConcurrentImageRequests &&
		!this->m_queuedCreditImageUrls.empty()
	) {
		const std::string key = this->m_queuedCreditImageUrls.front();
		this->m_queuedCreditImageUrls.erase(
			this->m_queuedCreditImageUrls.begin()
		);
		if (
			this->m_creditImages.contains(key) ||
			this->m_creditImageRequests.contains(key) ||
			this->m_failedCreditImages.contains(key)
		) {
			continue;
		}

		const String url = String::utf8(key.c_str());
		HTTPRequest* request = memnew(HTTPRequest);
		request->set_name("CreditImageRequest");
		request->set_use_threads(true);
		request->set_body_size_limit(this->m_maximumCreditImageBytes);
		request->set_max_redirects(4);
		request->set_timeout(15.0);
		this->add_child(request, false, INTERNAL_MODE_FRONT);
		request->connect(
			"request_completed",
			Callable(this, "_on_credit_image_request_completed").bind(url)
		);
		this->m_creditImageRequests.emplace(
			key,
			ObjectID(request->get_instance_id())
		);
		const Error error = request->request(
			url,
			PackedStringArray({
				"Accept: image/png,image/jpeg,image/webp,image/svg+xml,image/*;q=0.8",
			})
		);
		if (error != OK) {
			this->m_creditImageRequests.erase(key);
			request->queue_free();
			this->record_credit_image_failure(
				url,
				"Could not start credit image request (Godot error " +
					itos(static_cast<int64_t>(error)) + ")"
			);
		}
	}
}

bool CesiumGDCreditSystem::load_data_credit_image(const String& url) {
	const int64_t comma = url.find(",");
	if (comma < 0) {
		this->record_credit_image_failure(url, "Malformed data image URI");
		return false;
	}
	const String header = url.substr(5, comma - 5);
	const PackedStringArray parts = header.split(";");
	if (parts.is_empty() || !header.to_lower().ends_with(";base64")) {
		this->record_credit_image_failure(
			url,
			"Only base64 data image URIs are supported"
		);
		return false;
	}
	const String encoded = url.substr(comma + 1);
	if (encoded.length() > (this->m_maximumCreditImageBytes * 4 / 3) + 16) {
		this->record_credit_image_failure(url, "Credit image exceeds byte limit");
		return false;
	}
	Marshalls* marshalls = Marshalls::get_singleton();
	if (marshalls == nullptr) {
		this->record_credit_image_failure(url, "Godot base64 decoder is unavailable");
		return false;
	}
	const PackedByteArray body = marshalls->base64_to_raw(encoded);
	String errorMessage;
	if (!this->store_credit_image(url, body, parts[0], errorMessage)) {
		this->record_credit_image_failure(url, errorMessage);
		return false;
	}
	return true;
}

bool CesiumGDCreditSystem::store_credit_image(
	const String& url,
	const PackedByteArray& body,
	const String& mimeType,
	String& errorMessage
) {
	if (body.is_empty()) {
		errorMessage = "Credit image response was empty";
		return false;
	}
	if (body.size() > this->m_maximumCreditImageBytes) {
		errorMessage = "Credit image exceeds byte limit";
		return false;
	}

	const String normalizedMime =
		mimeType.get_slice(";", 0).strip_edges().to_lower();
	Ref<Image> image;
	image.instantiate();
	Error decodeError = ERR_FILE_UNRECOGNIZED;
	if (
		normalizedMime == "image/png" ||
		(body.size() >= 8 && body[0] == 0x89 && begins_with_ascii(
			body.slice(1),
			"PNG\r\n\x1a\n"
		))
	) {
		decodeError = image->load_png_from_buffer(body);
	} else if (
		normalizedMime == "image/jpeg" || normalizedMime == "image/jpg" ||
		(body.size() >= 2 && body[0] == 0xff && body[1] == 0xd8)
	) {
		decodeError = image->load_jpg_from_buffer(body);
	} else if (
		normalizedMime == "image/webp" ||
		(begins_with_ascii(body, "RIFF") && body.size() >= 12 &&
			body[8] == 'W' && body[9] == 'E' && body[10] == 'B' &&
			body[11] == 'P')
	) {
		decodeError = image->load_webp_from_buffer(body);
	} else if (
		normalizedMime == "image/svg+xml" ||
		begins_with_ascii(body, "<svg") || begins_with_ascii(body, "<?xml")
	) {
		decodeError = image->load_svg_from_buffer(body, 1.0);
	}
	if (decodeError != OK || image->is_empty()) {
		errorMessage = "Credit image format could not be decoded";
		return false;
	}
	if (
		image->get_width() > this->m_maximumCreditImageDimension ||
		image->get_height() > this->m_maximumCreditImageDimension
	) {
		errorMessage = "Credit image dimensions exceed limit";
		return false;
	}

	Ref<ImageTexture> texture = ImageTexture::create_from_image(image);
	if (texture.is_null()) {
		errorMessage = "Godot could not create the credit image texture";
		return false;
	}
	const std::string key = to_utf8(url);
	this->m_creditImages.insert_or_assign(key, texture);
	this->m_creditImageCacheOrder.erase(
		std::remove(
			this->m_creditImageCacheOrder.begin(),
			this->m_creditImageCacheOrder.end(),
			key
		),
		this->m_creditImageCacheOrder.end()
	);
	this->m_creditImageCacheOrder.push_back(key);
	this->m_failedCreditImages.erase(key);
	this->prune_credit_image_cache();
	this->emit_signal("credit_image_loaded", url);
	return true;
}

void CesiumGDCreditSystem::record_credit_image_failure(
	const String& url,
	const String& message
) {
	const std::string key = to_utf8(url);
	if (this->m_failedCreditImages.insert(key).second) {
		this->emit_signal("credit_image_failed", url, message);
	}
}

void CesiumGDCreditSystem::prune_credit_image_cache() {
	while (
		static_cast<int32_t>(this->m_creditImages.size()) >
			this->m_maximumCachedCreditImages
	) {
		auto candidate = std::find_if(
			this->m_creditImageCacheOrder.begin(),
			this->m_creditImageCacheOrder.end(),
			[this](const std::string& key) {
				return !this->m_currentImageUrls.contains(key);
			}
		);
		if (candidate == this->m_creditImageCacheOrder.end()) {
			// Current, legally required logos take priority over the soft cache cap.
			break;
		}
		this->m_creditImages.erase(*candidate);
		this->m_creditImageCacheOrder.erase(candidate);
	}
}

void CesiumGDCreditSystem::complete_credit_image_request(
	int32_t result,
	int32_t responseCode,
	const PackedStringArray& headers,
	const PackedByteArray& body,
	const String& url
) {
	const std::string key = to_utf8(url);
	const auto found = this->m_creditImageRequests.find(key);
	// A canceled request may still have queued its completion signal before the
	// node left the tree or remote loading was disabled. Its map entry is the
	// generation guard; never publish a result from a stale request.
	if (found == this->m_creditImageRequests.end()) {
		return;
	}
	HTTPRequest* request = Object::cast_to<HTTPRequest>(
		ObjectDB::get_instance(found->second)
	);
	if (request != nullptr) {
		request->queue_free();
	}
	this->m_creditImageRequests.erase(found);

	String mimeType;
	for (int64_t index = 0; index < headers.size(); ++index) {
		const String header = headers[index];
		if (header.to_lower().begins_with("content-type:")) {
			mimeType = header.substr(header.find(":") + 1).strip_edges();
			break;
		}
	}
	String errorMessage;
	if (
		result != HTTPRequest::RESULT_SUCCESS || responseCode < 200 ||
		responseCode >= 300
	) {
		errorMessage = "Credit image request failed (result " + itos(result) +
			", HTTP " + itos(responseCode) + ")";
	} else {
		this->store_credit_image(
			url,
			body,
			mimeType,
			errorMessage
		);
	}
	if (!errorMessage.is_empty()) {
		this->record_credit_image_failure(url, errorMessage);
	}
	this->start_queued_credit_image_requests();
	this->rebuild_presenter();
}

void CesiumGDCreditSystem::append_run(
	RichTextLabel* label,
	const Dictionary& run
) {
	ERR_FAIL_NULL(label);
	const String text = run.get("text", String());
	const String url = run.get("url", String());
	const String imageUrl = run.get("image_url", String());
	if (!imageUrl.is_empty()) {
		const auto image = this->m_creditImages.find(to_utf8(imageUrl));
		if (image != this->m_creditImages.end() && image->second.is_valid()) {
			const Vector2 sourceSize = image->second->get_size();
			const real_t maximumHeight = label == this->m_onScreenLabel ? 24.0 : 64.0;
			const real_t maximumWidth = label == this->m_onScreenLabel ? 180.0 : 320.0;
			const real_t scale = std::min<real_t>(
				1.0,
				std::min(
					maximumWidth / std::max<real_t>(sourceSize.x, 1.0),
					maximumHeight / std::max<real_t>(sourceSize.y, 1.0)
				)
			);
			if (!url.is_empty()) {
				label->push_meta(url);
			}
			label->add_image(
				image->second,
				static_cast<int32_t>(std::max<real_t>(1.0, sourceSize.x * scale)),
				static_cast<int32_t>(std::max<real_t>(1.0, sourceSize.y * scale)),
				Color(1.0, 1.0, 1.0, 1.0),
				InlineAlignment::INLINE_ALIGNMENT_CENTER,
				Rect2(),
				nullptr,
				false,
				text
			);
			if (!url.is_empty()) {
				label->pop();
			}
			return;
		}
	}
	if (text.is_empty()) {
		return;
	}
	if (!url.is_empty()) {
		label->push_meta(url);
		label->push_underline();
	}
	label->add_text(text);
	if (!url.is_empty()) {
		label->pop();
		label->pop();
	}
}

void CesiumGDCreditSystem::append_credit(
	RichTextLabel* label,
	const Variant& creditValue
) {
	Ref<CesiumCredit> credit = creditValue;
	if (credit.is_null()) {
		return;
	}
	const Array runs = credit->get_runs();
	if (runs.is_empty()) {
		label->add_text(credit->get_accessible_text());
		return;
	}
	for (int64_t index = 0; index < runs.size(); ++index) {
		this->append_run(label, runs[index]);
	}
}

void CesiumGDCreditSystem::rebuild_presenter() {
	if (!this->is_inside_tree()) {
		return;
	}
	this->ensure_presenter();
	if (
		this->m_onScreenPanel == nullptr || this->m_onScreenLabel == nullptr ||
		this->m_popupButton == nullptr || this->m_popupLabel == nullptr
	) {
		return;
	}

	this->m_onScreenLabel->clear();
	this->m_popupLabel->clear();
	bool firstOnScreen = true;
	bool firstPopup = true;
	for (int64_t index = 0; index < this->m_currentCredits.size(); ++index) {
		Ref<CesiumCredit> credit = this->m_currentCredits[index];
		if (credit.is_null()) {
			continue;
		}
		RichTextLabel* target = credit->get_show_on_screen()
			? this->m_onScreenLabel
			: this->m_popupLabel;
		bool& first = credit->get_show_on_screen()
			? firstOnScreen
			: firstPopup;
		if (!first) {
			target->add_text(
				credit->get_show_on_screen() ? " \u2022 " : "\n"
			);
		}
		first = false;
		this->append_credit(target, credit);
	}

	this->m_onScreenLabel->set_visible(!firstOnScreen);
	this->m_popupButton->set_visible(this->m_hasHiddenCredits);
	this->m_onScreenPanel->set_tooltip_text(this->m_currentPlainText);
	this->m_onScreenPanel->set_visible(
		this->m_presenterEnabled && !this->m_currentCredits.is_empty()
	);
	if (!this->m_hasHiddenCredits) {
		this->hide_attribution_popup();
	}
}

void CesiumGDCreditSystem::on_popup_button_pressed() {
	if (this->is_attribution_popup_visible()) {
		this->hide_attribution_popup();
	} else {
		this->show_attribution_popup();
	}
}

void CesiumGDCreditSystem::on_credit_link_clicked(const Variant& metadata) {
	if (metadata.get_type() != Variant::STRING &&
		metadata.get_type() != Variant::STRING_NAME) {
		return;
	}
	const String url = metadata;
	this->emit_signal("credit_link_clicked", url);
	if (!this->m_openLinksExternally || !is_safe_external_credit_url(url)) {
		return;
	}
	const Error error = OS::get_singleton()->shell_open(url);
	if (error != Error::OK) {
		WARN_PRINT("Could not open Cesium attribution URL: " + url);
	}
}

void CesiumGDCreditSystem::turn_off() {
	CesiumGDCreditSystem* instance = get_last_instance();
	if (instance != nullptr) {
		instance->set_presenter_enabled(false);
	}
}

void CesiumGDCreditSystem::turn_on() {
	CesiumGDCreditSystem* instance = get_last_instance();
	if (instance != nullptr) {
		instance->set_presenter_enabled(true);
	}
}

void CesiumGDCreditSystem::_bind_methods() {
	ClassDB::bind_method(
		D_METHOD("get_current_credits"),
		&CesiumGDCreditSystem::get_current_credits
	);
	ClassDB::bind_method(
		D_METHOD("get_current_credit_count"),
		&CesiumGDCreditSystem::get_current_credit_count
	);
	ClassDB::bind_method(
		D_METHOD("get_current_html"),
		&CesiumGDCreditSystem::get_current_html
	);
	ClassDB::bind_method(
		D_METHOD("get_current_plain_text"),
		&CesiumGDCreditSystem::get_current_plain_text
	);
	ClassDB::bind_method(
		D_METHOD("get_on_screen_text"),
		&CesiumGDCreditSystem::get_on_screen_text
	);
	ClassDB::bind_method(
		D_METHOD("get_popup_text"),
		&CesiumGDCreditSystem::get_popup_text
	);
	ClassDB::bind_method(
		D_METHOD("get_credits_updated"),
		&CesiumGDCreditSystem::get_credits_updated
	);
	ClassDB::bind_method(
		D_METHOD("get_has_hidden_credits"),
		&CesiumGDCreditSystem::get_has_hidden_credits
	);
	ClassDB::bind_method(
		D_METHOD("get_loaded_credit_image_count"),
		&CesiumGDCreditSystem::get_loaded_credit_image_count
	);
	ClassDB::bind_method(
		D_METHOD("get_pending_credit_image_count"),
		&CesiumGDCreditSystem::get_pending_credit_image_count
	);
	ClassDB::bind_method(
		D_METHOD("get_failed_credit_image_count"),
		&CesiumGDCreditSystem::get_failed_credit_image_count
	);
	ClassDB::bind_method(
		D_METHOD("get_credit_image", "url"),
		&CesiumGDCreditSystem::get_credit_image
	);
	ClassDB::bind_method(
		D_METHOD("set_presenter_enabled", "enabled"),
		&CesiumGDCreditSystem::set_presenter_enabled
	);
	ClassDB::bind_method(
		D_METHOD("get_presenter_enabled"),
		&CesiumGDCreditSystem::get_presenter_enabled
	);
	ClassDB::bind_method(
		D_METHOD("set_open_links_externally", "enabled"),
		&CesiumGDCreditSystem::set_open_links_externally
	);
	ClassDB::bind_method(
		D_METHOD("get_open_links_externally"),
		&CesiumGDCreditSystem::get_open_links_externally
	);
	ClassDB::bind_method(
		D_METHOD("set_remote_credit_images_enabled", "enabled"),
		&CesiumGDCreditSystem::set_remote_credit_images_enabled
	);
	ClassDB::bind_method(
		D_METHOD("get_remote_credit_images_enabled"),
		&CesiumGDCreditSystem::get_remote_credit_images_enabled
	);
	ClassDB::bind_method(
		D_METHOD("retry_failed_credit_images"),
		&CesiumGDCreditSystem::retry_failed_credit_images
	);
	ClassDB::bind_method(
		D_METHOD("show_attribution_popup"),
		&CesiumGDCreditSystem::show_attribution_popup
	);
	ClassDB::bind_method(
		D_METHOD("hide_attribution_popup"),
		&CesiumGDCreditSystem::hide_attribution_popup
	);
	ClassDB::bind_method(
		D_METHOD("is_attribution_popup_visible"),
		&CesiumGDCreditSystem::is_attribution_popup_visible
	);
	ClassDB::bind_method(
		D_METHOD("update_credits"),
		&CesiumGDCreditSystem::update_credits
	);
	ClassDB::bind_method(
		D_METHOD("_on_popup_button_pressed"),
		&CesiumGDCreditSystem::on_popup_button_pressed
	);
	ClassDB::bind_method(
		D_METHOD("_on_credit_link_clicked", "metadata"),
		&CesiumGDCreditSystem::on_credit_link_clicked
	);
	ClassDB::bind_method(
		D_METHOD(
			"_on_credit_image_request_completed",
			"result",
			"response_code",
			"headers",
			"body",
			"url"
		),
		&CesiumGDCreditSystem::complete_credit_image_request
	);
	ClassDB::bind_static_method(
		"CesiumGDCreditSystem",
		D_METHOD("turn_on"),
		&CesiumGDCreditSystem::turn_on
	);
	ClassDB::bind_static_method(
		"CesiumGDCreditSystem",
		D_METHOD("turn_off"),
		&CesiumGDCreditSystem::turn_off
	);

	ADD_PROPERTY(
		PropertyInfo(Variant::ARRAY, "current_credits", PROPERTY_HINT_ARRAY_TYPE, "CesiumCredit", PROPERTY_USAGE_NONE),
		"",
		"get_current_credits"
	);
	ADD_PROPERTY(
		PropertyInfo(Variant::INT, "current_credit_count", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE),
		"",
		"get_current_credit_count"
	);
	ADD_PROPERTY(
		PropertyInfo(Variant::STRING, "current_html", PROPERTY_HINT_MULTILINE_TEXT, "", PROPERTY_USAGE_NONE),
		"",
		"get_current_html"
	);
	ADD_PROPERTY(
		PropertyInfo(Variant::STRING, "current_plain_text", PROPERTY_HINT_MULTILINE_TEXT, "", PROPERTY_USAGE_NONE),
		"",
		"get_current_plain_text"
	);
	ADD_PROPERTY(
		PropertyInfo(Variant::STRING, "on_screen_text", PROPERTY_HINT_MULTILINE_TEXT, "", PROPERTY_USAGE_NONE),
		"",
		"get_on_screen_text"
	);
	ADD_PROPERTY(
		PropertyInfo(Variant::STRING, "popup_text", PROPERTY_HINT_MULTILINE_TEXT, "", PROPERTY_USAGE_NONE),
		"",
		"get_popup_text"
	);
	ADD_PROPERTY(
		PropertyInfo(Variant::BOOL, "credits_updated", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE),
		"",
		"get_credits_updated"
	);
	ADD_PROPERTY(
		PropertyInfo(Variant::BOOL, "has_hidden_credits", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE),
		"",
		"get_has_hidden_credits"
	);
	ADD_PROPERTY(
		PropertyInfo(Variant::INT, "loaded_credit_image_count", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE),
		"",
		"get_loaded_credit_image_count"
	);
	ADD_PROPERTY(
		PropertyInfo(Variant::INT, "pending_credit_image_count", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE),
		"",
		"get_pending_credit_image_count"
	);
	ADD_PROPERTY(
		PropertyInfo(Variant::INT, "failed_credit_image_count", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE),
		"",
		"get_failed_credit_image_count"
	);
	ADD_PROPERTY(
		PropertyInfo(Variant::BOOL, "presenter_enabled"),
		"set_presenter_enabled",
		"get_presenter_enabled"
	);
	ADD_PROPERTY(
		PropertyInfo(Variant::BOOL, "open_links_externally"),
		"set_open_links_externally",
		"get_open_links_externally"
	);
	ADD_PROPERTY(
		PropertyInfo(Variant::BOOL, "remote_credit_images_enabled"),
		"set_remote_credit_images_enabled",
		"get_remote_credit_images_enabled"
	);

	ADD_SIGNAL(MethodInfo(
		"credits_changed",
		PropertyInfo(
			Variant::ARRAY,
			"current_credits",
			PROPERTY_HINT_ARRAY_TYPE,
			"CesiumCredit"
		)
	));
	ADD_SIGNAL(MethodInfo(
		"credit_link_clicked",
		PropertyInfo(Variant::STRING, "url")
	));
	ADD_SIGNAL(MethodInfo(
		"credit_image_loaded",
		PropertyInfo(Variant::STRING, "url")
	));
	ADD_SIGNAL(MethodInfo(
		"credit_image_failed",
		PropertyInfo(Variant::STRING, "url"),
		PropertyInfo(Variant::STRING, "message")
	));
	ADD_SIGNAL(MethodInfo(
		"presenter_enabled_changed",
		PropertyInfo(Variant::BOOL, "enabled")
	));
}
