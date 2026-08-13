# Credits and attribution

`CesiumGDCreditSystem` collects and presents the credits required by the
currently selected tiles, imagery overlays, tileset providers, and an optional
application-supplied tileset credit. It is the Godot counterpart of Cesium for
Unreal's scene-wide credit actor and screen-credit widget.

Attribution is runtime state, not a static list. Cesium Native adds references
for data visible in the current view and removes those references when the
content leaves the view. All tilesets use one scene collector by default, so
identical credits are de-duplicated while another source still needs them.

## Default use

No setup is required for a normal scene. The first tileset creates a persistent
`CesiumGDCreditSystem` under the current scene if none exists. Applications may
instead add one explicitly and assign it to `Cesium3DTileset.credit_system`.
Separate explicitly assigned systems remain isolated.

Each `Cesium3DTileset` also exposes:

- `credit`: optional HTML attribution supplied by the application;
- `show_credits_on_screen`: whether that tileset's credits must be displayed
  directly rather than behind the complete-attribution popup; and
- `resolved_credit_system`: the lifetime-checked scene node currently used.

Changing any of these recreates the Native source atomically. This is
intentional: already loaded glTF content captures its credits when renderer
preparation completes.

The built-in presenter places mandatory credits along the lower edge of the
viewport and adds a **Data attribution** button whenever other current credits
may be shown in its popup. Links are clickable. Text, links, and image logos are
parsed from source HTML without treating that HTML as Godot BBCode.

Remote PNG, JPEG, WebP, and SVG logos are loaded asynchronously with four
concurrent requests, a 4 MiB response limit, a 4096-pixel dimension limit, and
a 128-entry soft cache. Current required logos are never evicted merely to
meet the cache cap. Base64 `data:image/...` logos are decoded locally. Until an
image is ready—or if it fails—its `alt`, `title`, or source-domain fallback is
shown as accessible text.

`remote_credit_images_enabled` prevents new HTTP logo requests when false;
inline images and already loaded cache entries remain available. If a provider
legally requires its logo rather than a text equivalent, the application must
not disable remote loading unless it supplies an equivalent custom presenter.

## Lifetime-safe application API

`current_credits` is an array of immutable `CesiumCredit` Resources. Each
snapshot copies:

- exact source `html`;
- normalized `plain_text` and `accessible_text`;
- the `show_on_screen` policy;
- structured text/image `runs`;
- link text and URLs; and
- image URLs.

Snapshots contain no Native credit handles or tile pointers and remain valid
after their source unloads. `current_html`, `current_plain_text`,
`on_screen_text`, and `popup_text` provide aggregate forms. Image state is
available through `loaded_credit_image_count`,
`pending_credit_image_count`, and `failed_credit_image_count`.
Custom presenters can retrieve a decoded cached `Texture2D` with
`get_credit_image(url)` after `credit_image_loaded` fires.

Signals are emitted only for meaningful state changes:

- `credits_changed(current_credits)`;
- `credit_link_clicked(url)`;
- `credit_image_loaded(url)`;
- `credit_image_failed(url, message)`; and
- `presenter_enabled_changed(enabled)`.

Call `retry_failed_credit_images()` after connectivity returns. Set
`open_links_externally` false to receive link signals without launching the
operating-system browser.

## Custom presentation and legal responsibility

Set `presenter_enabled` false when the application has its own interface and
render the lifetime-safe snapshot instead. Disabling the built-in presenter
does not disable collection. The application is then responsible for keeping
all legally required on-screen credits and logos visible and for making the
remaining attribution discoverable.

glTF `asset.copyright` uses semicolons as credit separators in Cesium Native.
Authors should therefore supply separate semicolon-delimited copyright entries
and avoid embedding HTML entity syntax containing semicolons there. Provider
and application credits are ordinary complete HTML strings and do not have
that glTF delimiter restriction.

The implementation is validated with offline glTF copyright, application
credit, UTF-8, link, inline-logo, shared-source, source-recreation, removal,
retained-snapshot, isolated-system, stale-default, and deterministic localhost
remote-logo cases.
