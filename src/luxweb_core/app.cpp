#include <luxweb/app.hpp>

#include <algorithm>
#include <fstream>
#include <regex>
#include <sstream>
#include <stdexcept>

#include <spdlog/spdlog.h>

namespace lux {
namespace {

std::string lower(std::string_view value) {
  std::string out(value);
  std::ranges::transform(out, out.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return out;
}

Request to_request(const crow::request& req) {
  Request out;
  out.method = crow::method_name(req.method);
  out.path = req.url;
  out.body = req.body;
  out.remote_ip = req.remote_ip_address;
  for (const auto& header : req.headers) {
    out.headers[lower(header.first)] = header.second;
  }
  if (auto cookie = out.header("cookie")) {
    out.cookies = parse_cookies(*cookie);
  }
  const auto question = req.raw_url.find('?');
  if (question != std::string::npos) {
    out.query = parse_query(std::string_view(req.raw_url).substr(question + 1));
  }
  return out;
}

crow::response to_crow(Response response) {
  crow::response out(response.code);
  out.body = std::move(response.body);
  for (const auto& [key, value] : response.headers) {
    out.add_header(key, value);
  }
  return out;
}

std::string mime_for(const std::filesystem::path& path) {
  const auto ext = path.extension().string();
  if (ext == ".css") {
    return "text/css; charset=utf-8";
  }
  if (ext == ".js") {
    return "text/javascript; charset=utf-8";
  }
  if (ext == ".json") {
    return "application/json";
  }
  if (ext == ".svg") {
    return "image/svg+xml";
  }
  if (ext == ".png") {
    return "image/png";
  }
  if (ext == ".jpg" || ext == ".jpeg") {
    return "image/jpeg";
  }
  return "application/octet-stream";
}

std::string read_file(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    throw std::runtime_error("file not found");
  }
  std::ostringstream buffer;
  buffer << in.rdbuf();
  return buffer.str();
}

std::string trim_extension(std::filesystem::path path) {
  path.replace_extension();
  return path.generic_string();
}

bool is_under(const std::filesystem::path& child, const std::filesystem::path& base) {
  const auto rel = child.lexically_relative(base);
  return !rel.empty() && rel.native().rfind("..", 0) != 0;
}

std::string join_route(std::string mount, std::string relative) {
  if (!mount.starts_with('/')) {
    mount = "/" + mount;
  }
  if (!mount.ends_with('/')) {
    mount += "/";
  }
  if (relative == "index") {
    relative.clear();
  } else if (relative.ends_with("/index")) {
    relative.resize(relative.size() - std::string("/index").size());
  }
  auto route = mount + relative;
  route = std::regex_replace(route, std::regex("/+"), "/");
  if (route.size() > 1 && route.ends_with('/')) {
    route.pop_back();
  }
  return route;
}

}  // namespace

App::App() {
  get("/lux/lux.css", [this](Context&) {
    Response response = Response::html(generate_css(theme_));
    response.headers["Content-Type"] = "text/css; charset=utf-8";
    return response;
  });
  get("/lux/lux.js", [](Context&) {
    Response response;
    response.body = generate_js();
    response.headers["Content-Type"] = "text/javascript; charset=utf-8";
    return response;
  });
}

void App::add_template_path(std::filesystem::path path) {
  templates_.add_search_path(std::move(path));
}

void App::add_template(std::string name, std::string body) {
  templates_.add_template(std::move(name), std::move(body));
}

void App::pages(std::filesystem::path directory, std::string mount) {
  if (!std::filesystem::exists(directory)) {
    return;
  }

  auto canonical_directory = std::filesystem::weakly_canonical(directory);
  bool covered_by_search_path = false;
  for (const auto& base : templates_.search_paths()) {
    if (is_under(canonical_directory, std::filesystem::weakly_canonical(base)) ||
        canonical_directory == std::filesystem::weakly_canonical(base)) {
      covered_by_search_path = true;
      break;
    }
  }
  if (!covered_by_search_path) {
    add_template_path(canonical_directory);
  }

  for (const auto& entry : std::filesystem::recursive_directory_iterator(canonical_directory)) {
    if (!entry.is_regular_file() || entry.path().extension() != ".html") {
      continue;
    }

    const auto rel_from_pages = trim_extension(entry.path().lexically_relative(canonical_directory));
    auto route_path = join_route(mount, rel_from_pages);

    std::string template_name;
    for (const auto& base : templates_.search_paths()) {
      auto canonical_base = std::filesystem::weakly_canonical(base);
      if (is_under(entry.path(), canonical_base) || entry.path().parent_path() == canonical_base) {
        template_name = trim_extension(entry.path().lexically_relative(canonical_base));
        break;
      }
    }
    if (template_name.empty()) {
      template_name = rel_from_pages;
    }

    get(route_path, [template_name, route_path](Context& ctx) {
      return ctx.render(template_name, {{"title", route_path == "/" ? "Home" : route_path}});
    });
  }
}

void App::embedded_pages(std::vector<std::string> templates, std::string mount) {
  for (auto& template_name : templates) {
    auto relative_route = trim_extension(template_name);
    if (relative_route.starts_with("pages/")) {
      relative_route = relative_route.substr(std::string("pages/").size());
    }
    auto route_path = join_route(mount, relative_route);
    get(route_path, [template_name, route_path](Context& ctx) {
      return ctx.render(template_name, {{"title", route_path == "/" ? "Home" : route_path}});
    });
  }
}

void App::static_files(std::string mount, std::filesystem::path directory) {
  if (!mount.ends_with('/')) {
    mount += "/";
  }
  const std::string pattern = mount + "<path>";
  app_.route_dynamic(pattern)([directory = std::move(directory)](const crow::request&, std::string path) {
    auto full = std::filesystem::weakly_canonical(directory / path);
    auto root = std::filesystem::weakly_canonical(directory);
    if (full.string().rfind(root.string(), 0) != 0 || !std::filesystem::is_regular_file(full)) {
      return crow::response(404);
    }
    crow::response response(read_file(full));
    response.add_header("Content-Type", mime_for(full));
    return response;
  });
}

void App::embedded_static_files(std::string mount, std::vector<EmbeddedAsset> assets) {
  if (!mount.starts_with('/')) {
    mount = "/" + mount;
  }
  if (mount.size() > 1 && mount.ends_with('/')) {
    mount.pop_back();
  }

  for (auto& asset : assets) {
    auto path = asset.path;
    if (!path.starts_with('/')) {
      path = "/" + path;
    }
    auto route_path = std::regex_replace(mount + path, std::regex("/+"), "/");
    get(route_path, [asset = std::move(asset)](Context&) {
      Response response;
      response.body = asset.body;
      response.headers["Content-Type"] = asset.content_type;
      return response;
    });
  }
}

void App::use(Middleware middleware) {
  middleware_.push_back(std::move(middleware));
}

void App::get(std::string path, Handler handler) {
  route("GET", std::move(path), std::move(handler));
}

void App::post(std::string path, Handler handler) {
  route("POST", std::move(path), std::move(handler));
}

ApiGroup App::api(std::string prefix) {
  return ApiGroup(*this, std::move(prefix));
}

void App::set_theme(StyleTheme theme) {
  theme_ = std::move(theme);
}

void App::enable_dev_reload(ReloadHub* hub) {
  reload_hub_ = hub;
  CROW_ROUTE(app_, "/lux/reload")([this](const crow::request& req) {
    if (!reload_hub_) {
      return crow::response(404);
    }
    std::uint64_t last = 0;
    if (req.url_params.get("last")) {
      last = std::stoull(req.url_params.get("last"));
    }
    crow::response response(reload_hub_->wait_event(last));
    response.add_header("Content-Type", "text/event-stream");
    response.add_header("Cache-Control", "no-cache");
    return response;
  });
}

void App::port(std::uint16_t port) {
  port_ = port;
}

void App::run() {
  spdlog::info("luxweb listening on http://127.0.0.1:{}", port_);
  app_.port(port_).multithreaded().run();
}

crow::SimpleApp& App::crow() {
  return app_;
}

TemplateEngine& App::templates() {
  return templates_;
}

void App::route(std::string method, std::string path, Handler handler) {
  if (method == "GET") {
    app_.route_dynamic(path).methods(crow::HTTPMethod::GET)([this, handler = std::move(handler)](const crow::request& req) mutable {
      return dispatch(req, handler);
    });
  } else if (method == "POST") {
    app_.route_dynamic(path).methods(crow::HTTPMethod::POST)([this, handler = std::move(handler)](const crow::request& req) mutable {
      return dispatch(req, handler);
    });
  }
}

crow::response App::dispatch(const crow::request& req, Handler& handler) {
  try {
    auto state = std::make_shared<ContextState>();
    Context ctx(to_request(req), &templates_, state);
    for (auto& middleware : middleware_) {
      if (auto response = middleware(ctx)) {
        return to_crow(std::move(*response));
      }
    }
    return to_crow(handler(ctx));
  } catch (const std::exception& error) {
    spdlog::error("request failed: {}", error.what());
    return crow::response(500, "Internal Server Error");
  }
}

ApiGroup::ApiGroup(App& app, std::string prefix) : app_(&app), prefix_(std::move(prefix)) {}

void ApiGroup::get(std::string path, Handler handler) {
  app_->get(prefix_ + path, std::move(handler));
}

void ApiGroup::post(std::string path, Handler handler) {
  app_->post(prefix_ + path, std::move(handler));
}

}  // namespace lux
