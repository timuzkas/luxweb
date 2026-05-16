#pragma once

#include <memory>
#include <optional>
#include <string>

#include <luxweb/http.hpp>
#include <luxweb/template_engine.hpp>

namespace lux {

struct CurrentUser {
  int id = 0;
  std::string email;
  std::string name;
};

struct ContextState {
  std::optional<CurrentUser> user;
};

class Context {
 public:
  Context(Request request, TemplateEngine* templates, std::shared_ptr<ContextState> state);

  [[nodiscard]] const Request& request() const;
  [[nodiscard]] Request& request();
  [[nodiscard]] Response render(std::string_view template_name, Json data = {}) const;
  [[nodiscard]] Response json(const Json& data, int code = 200) const;
  [[nodiscard]] Response redirect(std::string location, int code = 302) const;
  [[nodiscard]] std::optional<CurrentUser> current_user() const;
  [[nodiscard]] std::optional<std::string> env(std::string_view name) const;
  [[nodiscard]] std::string env(std::string_view name, std::string fallback) const;
  void set_current_user(CurrentUser user);
  void clear_current_user();

 private:
  Request request_;
  TemplateEngine* templates_;
  std::shared_ptr<ContextState> state_;
};

}  // namespace lux
