#include <cstdlib>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>

#include <luxweb/luxweb.hpp>

namespace {

std::uint16_t parse_port(const std::string& value) {
  const auto port = std::stoi(value);
  if (port <= 0 || port > 65535) {
    throw std::out_of_range("port must be between 1 and 65535");
  }
  return static_cast<std::uint16_t>(port);
}

std::uint16_t configured_port(int argc, char** argv) {
  std::uint16_t port = 18080;
  if (const char* env = std::getenv("LUXWEB_PORT")) {
    port = parse_port(env);
  } else if (const char* env = std::getenv("PORT")) {
    port = parse_port(env);
  }
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if ((arg == "--port" || arg == "-p") && i + 1 < argc) {
      port = parse_port(argv[++i]);
    } else if (arg.starts_with("--port=")) {
      port = parse_port(arg.substr(7));
    } else if (i == 1 && !arg.starts_with('-')) {
      port = parse_port(arg);
    }
  }
  return port;
}

}  // namespace

int main(int argc, char** argv) {
  auto root = std::filesystem::current_path();
  if (!std::filesystem::exists(root / "examples/starter/templates")) {
    auto from_binary = std::filesystem::weakly_canonical(std::filesystem::path(argv[0])).parent_path().parent_path();
    if (std::filesystem::exists(from_binary / "examples/starter/templates")) {
      root = from_binary;
    }
  }

  lux::App app;
  app.port(configured_port(argc, argv));
  app.add_template_path(root / "examples/starter/templates");
  app.static_files("/assets", root / "examples/starter/public");

  auto store = lux::UserStore::sqlite(root / "examples/starter/var/luxweb.db");
  lux::Auth auth(store);
  app.use(auth.sessions());

  app.pages(root / "examples/starter/templates/pages");
  app.get("/signup", [](lux::Context& ctx) {
    return ctx.render("auth/signup", {{"title", "Create account"}});
  });
  app.post("/signup", [&](lux::Context& ctx) {
    return auth.signup(ctx);
  });
  app.get("/login", [](lux::Context& ctx) {
    return ctx.render("auth/login", {{"title", "Log in"}});
  });
  app.post("/login", [&](lux::Context& ctx) {
    return auth.login(ctx);
  });
  app.post("/logout", [&](lux::Context& ctx) {
    return auth.logout(ctx);
  });
  app.get("/dashboard", auth.require_user([](lux::Context& ctx) {
    return ctx.render("auth/dashboard", {{"title", "Dashboard"}});
  }));

  auto api = app.api("/api");
  api.get("/health", [](lux::Context& ctx) {
    return ctx.json({{"ok", true}, {"framework", "luxweb"}});
  });
  api.get("/me", [&](lux::Context& ctx) {
    return ctx.json(auth.current_user_json(ctx));
  });

  auto data = app.api("/data");
  data.get("/health", [](lux::Context& ctx) {
    return ctx.json({{"ok", true}, {"served_from", "/data/health"}});
  });
  data.get("/me", [&](lux::Context& ctx) {
    return ctx.json(auth.current_user_json(ctx));
  });

  app.run();
}
