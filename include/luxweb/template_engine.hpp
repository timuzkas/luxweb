#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

#include <inja/inja.hpp>
#include <nlohmann/json.hpp>

namespace lux {

class TemplateEngine {
 public:
  TemplateEngine();

  void add_search_path(std::filesystem::path path);
  void add_template(std::string name, std::string body);
  [[nodiscard]] std::string render(std::string_view name, const nlohmann::json& data = {}) const;
  [[nodiscard]] const std::vector<std::filesystem::path>& search_paths() const;

 private:
  std::vector<std::filesystem::path> search_paths_;
  std::unordered_map<std::string, std::string> embedded_templates_;
};

}  // namespace lux
