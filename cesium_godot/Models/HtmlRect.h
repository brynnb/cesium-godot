#ifndef HTML_RECT_H
#define HTML_RECT_H

#include "godot/view_rect/view_rect.hpp"
#include "godot_cpp/classes/wrapped.hpp"
#include <string_view>

using namespace godot;

class HtmlRect : public ViewRect {
	GDCLASS(HtmlRect, ViewRect)

public:
	void set_html(const std::string_view& html);

private:
	std::string m_html;
}

#endif
