#include <luxweb/auth.hpp>

#include <stdexcept>

#include <luxweb/crypto.hpp>

namespace lux {

Auth::Auth(UserStore& store, AuthOptions options) : store_(&store), options_(std::move(options)) {}

Middleware Auth::sessions() {
  return [this](Context& ctx) -> std::optional<Response> {
    if (auto token = ctx.request().cookie(options_.cookie_name)) {
      if (auto user = store_->user_for_session(*token)) {
        ctx.set_current_user(CurrentUser{user->id, user->email, user->name});
      }
    }
    return std::nullopt;
  };
}

Handler Auth::require_user(Handler handler) const {
  return [this, handler = std::move(handler)](Context& ctx) mutable -> Response {
    if (!ctx.current_user()) {
      return ctx.redirect(options_.login_path);
    }
    return handler(ctx);
  };
}

Response Auth::signup(Context& ctx) {
  const auto email = ctx.request().form("email").value_or("");
  const auto name = ctx.request().form("name").value_or(email);
  const auto password = ctx.request().form("password").value_or("");
  if (email.empty() || password.size() < 8) {
    return ctx.render("auth/signup", {{"error", "Email and an 8 character password are required."}});
  }
  try {
    auto user = store_->create_user(email, name, password_hash(password));
    auto token = store_->create_session(user.id, options_.session_ttl_seconds);
    auto response = ctx.redirect(options_.after_login_path);
    response.headers["Set-Cookie"] = session_cookie(token);
    return response;
  } catch (const std::exception& error) {
    return ctx.render("auth/signup", {{"error", "That email is already registered."}});
  }
}

Response Auth::login(Context& ctx) {
  const auto email = ctx.request().form("email").value_or("");
  const auto password = ctx.request().form("password").value_or("");
  auto user = store_->find_user_by_email(email);
  if (!user || !verify_password(password, user->password_hash)) {
    return ctx.render("auth/login", {{"error", "Invalid email or password."}});
  }
  auto token = store_->create_session(user->id, options_.session_ttl_seconds);
  auto response = ctx.redirect(options_.after_login_path);
  response.headers["Set-Cookie"] = session_cookie(token);
  return response;
}

Response Auth::logout(Context& ctx) {
  if (auto token = ctx.request().cookie(options_.cookie_name)) {
    store_->delete_session(*token);
  }
  ctx.clear_current_user();
  auto response = ctx.redirect("/");
  response.headers["Set-Cookie"] = clear_cookie();
  return response;
}

Json Auth::current_user_json(const Context& ctx) const {
  if (auto user = ctx.current_user()) {
    return {{"authenticated", true}, {"user", {{"id", user->id}, {"email", user->email}, {"name", user->name}}}};
  }
  return {{"authenticated", false}, {"user", nullptr}};
}

std::string Auth::session_cookie(std::string_view token) const {
  std::string cookie = options_.cookie_name + "=" + std::string(token) + "; Path=/; HttpOnly; SameSite=Lax";
  if (options_.secure_cookie) {
    cookie += "; Secure";
  }
  return cookie;
}

std::string Auth::clear_cookie() const {
  return options_.cookie_name + "=; Path=/; HttpOnly; SameSite=Lax; Max-Age=0";
}

}  // namespace lux
