#pragma once

#include <functional>
#include <string>

#include <luxweb/context.hpp>
#include <luxweb/user_store.hpp>

namespace lux {

using Handler = std::function<Response(Context&)>;
using Middleware = std::function<std::optional<Response>(Context&)>;

struct AuthOptions {
  std::string cookie_name = "lux_session";
  std::string login_path = "/login";
  std::string after_login_path = "/dashboard";
  int session_ttl_seconds = 60 * 60 * 24 * 14;
  bool secure_cookie = false;
};

class Auth {
 public:
  Auth(UserStore& store, AuthOptions options = {});

  [[nodiscard]] Middleware sessions();
  [[nodiscard]] Handler require_user(Handler handler) const;
  [[nodiscard]] Response signup(Context& ctx);
  [[nodiscard]] Response login(Context& ctx);
  [[nodiscard]] Response logout(Context& ctx);
  [[nodiscard]] Json current_user_json(const Context& ctx) const;

 private:
  UserStore* store_;
  AuthOptions options_;

  [[nodiscard]] std::string session_cookie(std::string_view token) const;
  [[nodiscard]] std::string clear_cookie() const;
};

}  // namespace lux
