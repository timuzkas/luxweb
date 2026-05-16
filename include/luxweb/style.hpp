#pragma once

#include <string>

namespace lux {

struct StyleTheme {
  std::string brand = "#2563eb";
  std::string accent = "#0f766e";
  std::string surface = "#ffffff";
  std::string background = "#f6f7f9";
  std::string text = "#172033";
  std::string muted = "#667085";
  std::string border = "#d8dee8";
  std::string radius = "8px";
  std::string font_family = "Inter, ui-sans-serif, system-ui, -apple-system, BlinkMacSystemFont, \"Segoe UI\", sans-serif";
  int spacing = 8;
};

std::string generate_css(const StyleTheme& theme = {});
std::string generate_js();

}  // namespace lux
