#pragma once

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <crow.h>

#include <luxweb/auth.hpp>
#include <luxweb/context.hpp>
#include <luxweb/dev_reload.hpp>
#include <luxweb/style.hpp>

namespace lux {

class ApiGroup;

struct EmbeddedAsset {
  std::string path;
  std::string content_type;
  std::string body;
};

class App {
 public:
  App();

  void add_template_path(std::filesystem::path path);
  void add_template(std::string name, std::string body);
  // Registers HTML files as GET routes. index.html maps to /, nested/index.html maps to /nested.
  void pages(std::filesystem::path directory, std::string mount = "/");
  void embedded_pages(std::vector<std::string> templates, std::string mount = "/");
  void static_files(std::string mount, std::filesystem::path directory);
  void embedded_static_files(std::string mount, std::vector<EmbeddedAsset> assets);
  void use(Middleware middleware);
  void get(std::string path, Handler handler);
  void post(std::string path, Handler handler);
  ApiGroup api(std::string prefix);
  void set_theme(StyleTheme theme);
  void enable_dev_reload(ReloadHub* hub);
  void port(std::uint16_t port);
  void host(std::string host);
  void run();

  [[nodiscard]] crow::SimpleApp& crow();
  [[nodiscard]] TemplateEngine& templates();

 private:
  crow::SimpleApp app_;
  TemplateEngine templates_;
  std::vector<Middleware> middleware_;
  StyleTheme theme_;
  std::uint16_t port_ = 18080;
  std::string host_ = "127.0.0.1";
  ReloadHub* reload_hub_ = nullptr;

  void route(std::string method, std::string path, Handler handler);
  [[nodiscard]] crow::response dispatch(const crow::request& req, Handler& handler);
};

class ApiGroup {
 public:
  ApiGroup(App& app, std::string prefix);

  void get(std::string path, Handler handler);
  void post(std::string path, Handler handler);

 private:
  App* app_;
  std::string prefix_;
};

}  // namespace lux
