#include <luxweb/template_engine.hpp>

#include <filesystem>
#include <stdexcept>

namespace lux {
namespace {

std::string normalize_template_name(std::string_view name) {
  std::filesystem::path path{std::string(name)};
  if (!path.has_extension()) {
    path += ".html";
  }
  return path.lexically_normal().generic_string();
}

std::string trim_template_extension(std::string value) {
  std::filesystem::path path(std::move(value));
  path.replace_extension();
  return path.generic_string();
}

}  // namespace

TemplateEngine::TemplateEngine() = default;

void TemplateEngine::add_search_path(std::filesystem::path path) {
  search_paths_.push_back(std::move(path));
}

void TemplateEngine::add_template(std::string name, std::string body) {
  embedded_templates_[normalize_template_name(name)] = std::move(body);
}

std::string TemplateEngine::render(std::string_view name, const nlohmann::json& data) const {
  if (!embedded_templates_.empty()) {
    auto template_name = normalize_template_name(name);
    auto it = embedded_templates_.find(template_name);
    if (it != embedded_templates_.end()) {
      inja::Environment env;
      env.set_trim_blocks(true);
      env.set_lstrip_blocks(true);
      env.set_include_callback([this, &env](const std::string& path, const std::string& requested) {
        std::vector<std::string> candidates;
        candidates.push_back(normalize_template_name(requested));
        candidates.push_back(normalize_template_name(trim_template_extension(requested)));
        auto stripped = requested;
        while (stripped.rfind("../", 0) == 0) {
          stripped.erase(0, 3);
        }
        candidates.push_back(normalize_template_name(stripped));
        if (!path.empty()) {
          auto relative = (std::filesystem::path(path).parent_path() / requested).lexically_normal().generic_string();
          candidates.push_back(normalize_template_name(relative));
          candidates.push_back(normalize_template_name(trim_template_extension(relative)));
        }

        for (const auto& candidate : candidates) {
          auto found = embedded_templates_.find(candidate);
          if (found != embedded_templates_.end()) {
            return env.parse(found->second);
          }
        }
        throw std::runtime_error("embedded template not found: " + requested);
      });
      return env.render(env.parse(it->second), data);
    }
  }

  for (const auto& base : search_paths_) {
    auto path = base / std::string(name);
    if (!path.has_extension()) {
      path += ".html";
    }
    if (std::filesystem::exists(path)) {
      auto input_path = base.string();
      if (!input_path.ends_with(std::filesystem::path::preferred_separator)) {
        input_path += std::filesystem::path::preferred_separator;
      }
      inja::Environment env(input_path);
      env.set_trim_blocks(true);
      env.set_lstrip_blocks(true);
      return env.render_file(path.lexically_relative(base).string(), data);
    }
  }
  throw std::runtime_error("template not found: " + std::string(name));
}

const std::vector<std::filesystem::path>& TemplateEngine::search_paths() const {
  return search_paths_;
}

}  // namespace lux
