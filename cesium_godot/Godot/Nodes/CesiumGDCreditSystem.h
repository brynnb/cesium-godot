// Godot adaptation reviewed against Cesium for Unreal v2.29.0:
// - Source/CesiumRuntime/Public/CesiumCreditSystem.h
// - Source/CesiumRuntime/Private/CesiumCreditSystem.cpp
// - Source/CesiumRuntime/Private/ScreenCreditsWidget.*
// Last upstream review: Cesium for Unreal v2.29.0.

#ifndef CESIUM_GD_CREDIT_SYSTEM_H
#define CESIUM_GD_CREDIT_SYSTEM_H

#include <godot_cpp/classes/box_container.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/core/object_id.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/string.hpp>

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using namespace godot;

namespace CesiumUtility {
class CreditSystem;
}

namespace godot {
class Button;
class PanelContainer;
class PopupPanel;
class RichTextLabel;
}

/**
 * Owns the scene's shared Cesium Native credit system and presents its
 * current-frame attribution using ordinary Godot controls.
 *
 * All Cesium tilesets in a scene use this one Native collector by default,
 * matching Cesium for Unreal. This makes Native reference-counting perform the
 * cross-tileset de-duplication and ensures that a source recreation cannot
 * leave an obsolete collector registered with the UI.
 */
class CesiumGDCreditSystem : public BoxContainer {
	GDCLASS(CesiumGDCreditSystem, BoxContainer)

public:
	CesiumGDCreditSystem();
	~CesiumGDCreditSystem() override;

	static CesiumGDCreditSystem* get_singleton(Node* context);

	const std::shared_ptr<CesiumUtility::CreditSystem>&
	get_native_credit_system() const;

	Array get_current_credits() const;
	int32_t get_current_credit_count() const;
	String get_current_html() const;
	String get_current_plain_text() const;
	String get_on_screen_text() const;
	String get_popup_text() const;
	bool get_credits_updated() const;
	bool get_has_hidden_credits() const;
	int32_t get_loaded_credit_image_count() const;
	int32_t get_pending_credit_image_count() const;
	int32_t get_failed_credit_image_count() const;
	Ref<Texture2D> get_credit_image(const String& url) const;

	void set_presenter_enabled(bool enabled);
	bool get_presenter_enabled() const;
	void set_open_links_externally(bool enabled);
	bool get_open_links_externally() const;
	void set_remote_credit_images_enabled(bool enabled);
	bool get_remote_credit_images_enabled() const;
	void retry_failed_credit_images();

	void show_attribution_popup();
	void hide_attribution_popup();
	bool is_attribution_popup_visible() const;

	void update_credits();
	void _process(double delta) override;
	void _enter_tree() override;
	void _exit_tree() override;

	/**
	 * Compatibility helpers. Applications are responsible for keeping legally
	 * required attribution visible if they disable the built-in presenter.
	 */
	static void turn_off();
	static void turn_on();

protected:
	static void _bind_methods();

private:
	struct CreditKey {
		std::string html;
		bool showOnScreen = false;

		bool operator==(const CreditKey& other) const {
			return this->showOnScreen == other.showOnScreen &&
				this->html == other.html;
		}
	};

	void ensure_presenter();
	void rebuild_presenter();
	void append_credit(RichTextLabel* label, const Variant& creditValue);
	void append_run(RichTextLabel* label, const Dictionary& run);
	void queue_credit_images();
	void start_queued_credit_image_requests();
	void complete_credit_image_request(
		int32_t result,
		int32_t responseCode,
		const PackedStringArray& headers,
		const PackedByteArray& body,
		const String& url
	);
	bool load_data_credit_image(const String& url);
	bool store_credit_image(
		const String& url,
		const PackedByteArray& body,
		const String& mimeType,
		String& errorMessage
	);
	void record_credit_image_failure(
		const String& url,
		const String& message
	);
	void prune_credit_image_cache();
	void on_popup_button_pressed();
	void on_credit_link_clicked(const Variant& metadata);
	static CesiumGDCreditSystem* get_last_instance();

	std::shared_ptr<CesiumUtility::CreditSystem> m_creditSystem;
	std::vector<CreditKey> m_lastCreditKeys;
	Array m_currentCredits;
	String m_currentHtml;
	String m_currentPlainText;
	String m_onScreenText;
	String m_popupText;
	bool m_creditsUpdated = false;
	bool m_hasHiddenCredits = false;
	bool m_presenterEnabled = true;
	bool m_openLinksExternally = true;
	bool m_remoteCreditImagesEnabled = true;
	int32_t m_maximumConcurrentImageRequests = 4;
	int32_t m_maximumCachedCreditImages = 128;
	int32_t m_maximumCreditImageBytes = 4 * 1024 * 1024;
	int32_t m_maximumCreditImageDimension = 4096;
	std::unordered_map<std::string, Ref<Texture2D>> m_creditImages;
	std::unordered_map<std::string, ObjectID> m_creditImageRequests;
	std::unordered_set<std::string> m_failedCreditImages;
	std::unordered_set<std::string> m_currentImageUrls;
	std::vector<std::string> m_queuedCreditImageUrls;
	std::vector<std::string> m_creditImageCacheOrder;

	PanelContainer* m_onScreenPanel = nullptr;
	RichTextLabel* m_onScreenLabel = nullptr;
	Button* m_popupButton = nullptr;
	PopupPanel* m_popup = nullptr;
	RichTextLabel* m_popupLabel = nullptr;

	static inline ObjectID s_lastInstanceId;
};

#endif
