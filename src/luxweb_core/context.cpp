#include <luxweb/context.hpp>

#include <cstdlib>
#include <stdexcept>

namespace lux {

Context::Context(Request request, TemplateEngine* templates, std::shared_ptr<ContextState> state)
    : request_(std::move(request)), templates_(templates), state_(std::move(state)) {}

const Request& Context::request() const {
  return request_;
}

Request& Context::request() {
  return request_;
}

Response Context::render(std::string_view template_name, Json data) const {
  if (!templates_) {
    throw std::runtime_error("no template engine configured");
  }
  if (state_->user) {
    data["current_user"] = {{"id", state_->user->id}, {"email", state_->user->email}, {"name", state_->user->name}};
  } else {
    data["current_user"] = nullptr;
  }
  return Response::html(templates_->render(template_name, data));
}

Response Context::json(const Json& data, int code) const {
  return Response::json(data, code);
}

Response Context::redirect(std::string location, int code) const {
  return Response::redirect(std::move(location), code);
}

std::optional<CurrentUser> Context::current_user() const {
  return state_->user;
}

std::optional<std::string> Context::env(std::string_view name) const {
  const std::string key(name);
  if (const char* value = std::getenv(key.c_str())) {
    return std::string(value);
  }
  return std::nullopt;
}

std::string Context::env(std::string_view name, std::string fallback) const {
  return env(name).value_or(std::move(fallback));
}

void Context::set_current_user(CurrentUser user) {
  state_->user = std::move(user);
}

void Context::clear_current_user() {
  state_->user.reset();
}

}  // namespace lux
