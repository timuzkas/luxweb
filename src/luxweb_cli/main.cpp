#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <luxweb/luxweb.hpp>

namespace fs = std::filesystem;

namespace {

struct ProjectConfig {
  std::string name;
  std::uint16_t port = 18080;
};

struct BuildOptions {
  std::string target;
  fs::path build_dir = "build";
  std::string build_type = "Release";
  bool embed_assets = false;
};

std::string shell_quote(const fs::path& value) {
  std::string input = value.string();
  std::string out = "'";
  for (char c : input) {
    if (c == '\'') {
      out += "'\\''";
    } else {
      out.push_back(c);
    }
  }
  out += "'";
  return out;
}

std::string shell_quote(const std::string& value) {
  return shell_quote(fs::path(value));
}

std::string trim(std::string value) {
  const auto first = value.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) {
    return "";
  }
  const auto last = value.find_last_not_of(" \t\r\n");
  return value.substr(first, last - first + 1);
}

std::string app_name_from_path(const fs::path& path) {
  auto name = path.filename().string();
  if (name.empty()) {
    name = "luxweb_app";
  }
  for (char& c : name) {
    if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-')) {
      c = '_';
    }
  }
  return name;
}

void write_file(const fs::path& path, const std::string& body) {
  fs::create_directories(path.parent_path());
  std::ofstream out(path);
  out << body;
}

std::optional<ProjectConfig> read_config(const fs::path& root) {
  std::ifstream in(root / "luxweb.toml");
  if (!in) {
    return std::nullopt;
  }
  ProjectConfig config;
  std::string line;
  while (std::getline(in, line)) {
    line = trim(line);
    if (line.empty() || line.starts_with('#')) {
      continue;
    }
    const auto eq = line.find('=');
    if (eq == std::string::npos) {
      continue;
    }
    auto key = trim(line.substr(0, eq));
    auto value = trim(line.substr(eq + 1));
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
      value = value.substr(1, value.size() - 2);
    }
    if (key == "name") {
      config.name = value;
    } else if (key == "port") {
      config.port = static_cast<std::uint16_t>(std::stoi(value));
    }
  }
  if (config.name.empty()) {
    config.name = app_name_from_path(root);
  }
  return config;
}

std::string cxx_string_literal(std::string_view value) {
  std::string out = "\"";
  for (unsigned char c : value) {
    switch (c) {
      case '\\':
        out += "\\\\";
        break;
      case '"':
        out += "\\\"";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        if (c < 32 || c > 126) {
          out += "\\";
          out.push_back(static_cast<char>('0' + ((c >> 6) & 0x07)));
          out.push_back(static_cast<char>('0' + ((c >> 3) & 0x07)));
          out.push_back(static_cast<char>('0' + (c & 0x07)));
        } else {
          out.push_back(static_cast<char>(c));
        }
        break;
    }
  }
  out += "\"";
  return out;
}

std::string read_file_string(const fs::path& path) {
  std::ifstream in(path, std::ios::binary);
  std::ostringstream buffer;
  buffer << in.rdbuf();
  return buffer.str();
}

std::string embedded_mime_for(const fs::path& path) {
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
  if (ext == ".webp") {
    return "image/webp";
  }
  return "application/octet-stream";
}

std::vector<fs::path> regular_files_under(const fs::path& root) {
  std::vector<fs::path> files;
  if (!fs::exists(root)) {
    return files;
  }
  for (const auto& entry : fs::recursive_directory_iterator(root)) {
    if (entry.is_regular_file()) {
      files.push_back(entry.path());
    }
  }
  std::ranges::sort(files);
  return files;
}

void generate_embedded_assets(const fs::path& root) {
  const auto output = root / ".luxweb/embedded_assets.cpp";
  fs::create_directories(output.parent_path());
  std::ostringstream out;
  out << "#include <luxweb/luxweb.hpp>\n\n";
  out << "namespace luxweb_generated {\n\n";
  out << "void register_embedded_assets(lux::App& app) {\n";

  std::vector<std::string> page_templates;
  for (const auto& path : regular_files_under(root / "templates")) {
    if (path.extension() != ".html") {
      continue;
    }
    auto name = path.lexically_relative(root / "templates").generic_string();
    out << "  app.add_template(" << cxx_string_literal(name) << ", " << cxx_string_literal(read_file_string(path)) << ");\n";
    const auto rel_pages = path.lexically_relative(root / "templates/pages");
    if (!rel_pages.empty() && rel_pages.native().rfind("..", 0) != 0) {
      page_templates.push_back(rel_pages.generic_string());
    }
  }

  out << "  app.embedded_pages({";
  for (std::size_t i = 0; i < page_templates.size(); ++i) {
    if (i > 0) {
      out << ", ";
    }
    out << cxx_string_literal("pages/" + page_templates[i]);
  }
  out << "});\n";

  out << "  app.embedded_static_files(\"/assets\", {\n";
  bool first_asset = true;
  for (const auto& path : regular_files_under(root / "public")) {
    if (!first_asset) {
      out << ",\n";
    }
    first_asset = false;
    out << "    {" << cxx_string_literal(path.lexically_relative(root / "public").generic_string()) << ", "
        << cxx_string_literal(embedded_mime_for(path)) << ", " << cxx_string_literal(read_file_string(path)) << "}";
  }
  out << "\n  });\n";
  out << "}\n\n";
  out << "}  // namespace luxweb_generated\n";

  write_file(output, out.str());
}

int new_app(const std::string& name) {
  fs::path root = name;
  const auto target_name = app_name_from_path(root);
  fs::create_directories(root / "src");
  fs::create_directories(root / "templates/layouts");
  fs::create_directories(root / "templates/pages");
  fs::create_directories(root / "public");

  write_file(root / "luxweb.toml",
             "name = \"" + target_name + "\"\n"
             "port = 18080\n");
  write_file(root / "CMakeLists.txt",
             "cmake_minimum_required(VERSION 3.24)\n\n"
             "project(" + target_name + " LANGUAGES CXX)\n\n"
             "set(CMAKE_CXX_STANDARD 23)\n"
             "set(CMAKE_CXX_STANDARD_REQUIRED ON)\n"
             "set(CMAKE_CXX_EXTENSIONS OFF)\n\n"
             "set(LUXWEB_SOURCE_DIR \"\" CACHE PATH \"Path to the Luxweb source tree\")\n"
             "if(NOT LUXWEB_SOURCE_DIR AND DEFINED ENV{LUXWEB_SOURCE_DIR})\n"
             "  set(LUXWEB_SOURCE_DIR \"$ENV{LUXWEB_SOURCE_DIR}\")\n"
             "endif()\n"
             "if(NOT LUXWEB_SOURCE_DIR AND EXISTS \"${CMAKE_CURRENT_SOURCE_DIR}/../luxweb/CMakeLists.txt\")\n"
             "  set(LUXWEB_SOURCE_DIR \"${CMAKE_CURRENT_SOURCE_DIR}/../luxweb\")\n"
             "endif()\n"
             "if(NOT LUXWEB_SOURCE_DIR AND EXISTS \"${CMAKE_CURRENT_SOURCE_DIR}/vendor/luxweb/CMakeLists.txt\")\n"
             "  set(LUXWEB_SOURCE_DIR \"${CMAKE_CURRENT_SOURCE_DIR}/vendor/luxweb\")\n"
             "endif()\n"
             "if(NOT LUXWEB_SOURCE_DIR)\n"
             "  message(FATAL_ERROR \"Luxweb source tree not found. Set -DLUXWEB_SOURCE_DIR=/path/to/luxweb, export LUXWEB_SOURCE_DIR, place luxweb next to this app, or vendor it under ./vendor/luxweb.\")\n"
             "endif()\n"
             "if(NOT EXISTS \"${LUXWEB_SOURCE_DIR}/CMakeLists.txt\")\n"
             "  message(FATAL_ERROR \"LUXWEB_SOURCE_DIR does not point to a Luxweb source tree: ${LUXWEB_SOURCE_DIR}\")\n"
             "endif()\n\n"
             "if(EXISTS \"${LUXWEB_SOURCE_DIR}/build/_deps/asio-src\")\n"
             "  set(FETCHCONTENT_SOURCE_DIR_ASIO \"${LUXWEB_SOURCE_DIR}/build/_deps/asio-src\" CACHE PATH \"\" FORCE)\n"
             "endif()\n"
             "if(EXISTS \"${LUXWEB_SOURCE_DIR}/build/_deps/crow-src\")\n"
             "  set(FETCHCONTENT_SOURCE_DIR_CROW \"${LUXWEB_SOURCE_DIR}/build/_deps/crow-src\" CACHE PATH \"\" FORCE)\n"
             "endif()\n"
             "if(EXISTS \"${LUXWEB_SOURCE_DIR}/build/_deps/inja-src\")\n"
             "  set(FETCHCONTENT_SOURCE_DIR_INJA \"${LUXWEB_SOURCE_DIR}/build/_deps/inja-src\" CACHE PATH \"\" FORCE)\n"
             "endif()\n\n"
             "set(LUXWEB_BUILD_EXAMPLES OFF CACHE BOOL \"\" FORCE)\n"
             "set(LUXWEB_BUILD_TESTS OFF CACHE BOOL \"\" FORCE)\n"
             "set(LUXWEB_EMBED_APP OFF CACHE BOOL \"Embed templates and public assets\")\n"
             "add_subdirectory(\"${LUXWEB_SOURCE_DIR}\" \"${CMAKE_BINARY_DIR}/_luxweb\")\n\n"
             "add_executable(" + target_name + " src/main.cpp)\n"
             "target_link_libraries(" + target_name + " PRIVATE luxweb::core)\n"
             "if(LUXWEB_EMBED_APP AND EXISTS \"${CMAKE_SOURCE_DIR}/.luxweb/embedded_assets.cpp\")\n"
             "  target_sources(" + target_name + " PRIVATE \"${CMAKE_SOURCE_DIR}/.luxweb/embedded_assets.cpp\")\n"
             "  target_compile_definitions(" + target_name + " PRIVATE LUXWEB_EMBEDDED_APP=1)\n"
             "endif()\n");
  write_file(root / "src/main.cpp",
             "#include <cstdlib>\n"
             "#include <cstdint>\n"
             "#include <stdexcept>\n"
             "#include <string>\n\n"
             "#include <luxweb/luxweb.hpp>\n\n"
             "#ifdef LUXWEB_EMBEDDED_APP\n"
             "namespace luxweb_generated {\n"
             "void register_embedded_assets(lux::App& app);\n"
             "}\n"
             "#endif\n\n"
             "namespace {\n\n"
             "std::uint16_t parse_port(const std::string& value) {\n"
             "  const auto port = std::stoi(value);\n"
             "  if (port <= 0 || port > 65535) {\n"
             "    throw std::out_of_range(\"port must be between 1 and 65535\");\n"
             "  }\n"
             "  return static_cast<std::uint16_t>(port);\n"
             "}\n\n"
             "std::uint16_t configured_port(int argc, char** argv) {\n"
             "  std::uint16_t port = 18080;\n"
             "  if (const char* env = std::getenv(\"LUXWEB_PORT\")) {\n"
             "    port = parse_port(env);\n"
             "  } else if (const char* env = std::getenv(\"PORT\")) {\n"
             "    port = parse_port(env);\n"
             "  }\n"
             "  for (int i = 1; i < argc; ++i) {\n"
             "    std::string arg = argv[i];\n"
             "    if ((arg == \"--port\" || arg == \"-p\") && i + 1 < argc) {\n"
             "      port = parse_port(argv[++i]);\n"
             "    } else if (arg.starts_with(\"--port=\")) {\n"
             "      port = parse_port(arg.substr(7));\n"
             "    } else if (i == 1 && !arg.starts_with('-')) {\n"
             "      port = parse_port(arg);\n"
             "    }\n"
             "  }\n"
             "  return port;\n"
             "}\n\n"
             "}  // namespace\n\n"
             "int main(int argc, char** argv) {\n"
             "  lux::App app;\n"
             "  app.port(configured_port(argc, argv));\n"
             "#ifdef LUXWEB_EMBEDDED_APP\n"
             "  luxweb_generated::register_embedded_assets(app);\n"
             "#else\n"
             "  app.add_template_path(\"templates\");\n"
             "  app.pages(\"templates/pages\");\n"
             "  app.static_files(\"/assets\", \"public\");\n\n"
             "#endif\n"
             "  app.api(\"/api\").get(\"/env\", [](lux::Context& ctx) {\n"
             "    return ctx.json({{\"api_key_configured\", ctx.env(\"API_KEY\").has_value()}});\n"
             "  });\n\n"
             "  app.api(\"/api\").get(\"/health\", [](lux::Context& ctx) {\n"
             "    return ctx.json({{\"ok\", true}, {\"app\", \"" + target_name + "\"}});\n"
             "  });\n\n"
             "  app.run();\n"
             "}\n");
  write_file(root / "templates/layouts/base.html",
             "<!doctype html>\n"
             "<html lang=\"en\">\n"
             "<head>\n"
             "  <meta charset=\"utf-8\">\n"
             "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
             "  <title>{{ title }}</title>\n"
             "  <link rel=\"stylesheet\" href=\"/lux/lux.css\">\n"
             "  <link rel=\"stylesheet\" href=\"/assets/app.css\">\n"
             "  <script defer src=\"/lux/lux.js\"></script>\n"
             "  <script defer src=\"/assets/app.js\"></script>\n"
             "</head>\n"
             "<body>\n"
             "  <main class=\"lux-shell\">\n"
             "    {% block content %}{% endblock %}\n"
             "  </main>\n"
             "</body>\n"
             "</html>\n");
  write_file(root / "templates/pages/index.html",
             "{% extends \"../layouts/base.html\" %}\n"
             "{% block content %}\n"
             "<section class=\"lux-panel\">\n"
             "  <h1>{{ title }}</h1>\n"
             "  <p class=\"lux-muted\">Your Luxweb app is running.</p>\n"
             "  <div class=\"counter\">\n"
             "    <button type=\"button\" data-lux-on:click=\"counter.decrement\">-</button>\n"
             "    <strong data-lux-text=\"counter.count\">0</strong>\n"
             "    <button type=\"button\" data-lux-on:click=\"counter.increment\">+</button>\n"
             "  </div>\n"
             "  <label>\n"
             "    Counter label\n"
             "    <input data-lux-model=\"counter.label\" autocomplete=\"off\">\n"
             "  </label>\n"
             "  <p class=\"lux-muted\">Label: <span data-lux-text=\"counter.label\"></span></p>\n"
             "  <p><a href=\"/api/health\">API health</a></p>\n"
             "</section>\n"
             "{% endblock %}\n");
  write_file(root / "public/app.css",
             "body { padding-top: 2rem; }\n"
             ".counter { display: inline-grid; grid-template-columns: 40px 64px 40px; align-items: center; gap: 8px; margin: 16px 0; }\n"
             ".counter strong { text-align: center; font-size: 28px; }\n");
  write_file(root / "public/app.js",
             "const counter = lux.store('counter', { count: 0, label: 'Clicks' }, { persist: true });\n\n"
             "lux.action('counter.increment', () => {\n"
             "  counter.set('count', counter.get('count') + 1);\n"
             "});\n\n"
             "lux.action('counter.decrement', () => {\n"
             "  counter.set('count', counter.get('count') - 1);\n"
             "});\n");
  std::cout << "Created " << root << "\n";
  std::cout << "Run it with:\n";
  std::cout << "  cd " << root << "\n";
  std::cout << "  luxweb dev\n";
  return 0;
}

int run_command(const std::string& command) {
  std::cout << command << "\n";
  return std::system(command.c_str());
}

int build_project(const ProjectConfig& config, const BuildOptions& options) {
  if (options.embed_assets) {
    const auto cmake = read_file_string(fs::current_path() / "CMakeLists.txt");
    if (cmake.find("LUXWEB_EMBED_APP") == std::string::npos || cmake.find(".luxweb/embedded_assets.cpp") == std::string::npos) {
      std::cerr << "This app does not have the Luxweb embedded build hook in CMakeLists.txt.\n";
      std::cerr << "Create a new app with this luxweb binary or add the LUXWEB_EMBED_APP target_sources hook.\n";
      return 2;
    }
    generate_embedded_assets(fs::current_path());
  }

  fs::create_directories(options.build_dir);
  auto status = run_command("cmake -S . -B " + shell_quote(options.build_dir) + " -G \"Unix Makefiles\" -DCMAKE_BUILD_TYPE=" +
                            shell_quote(options.build_type) + " -DLUXWEB_EMBED_APP=" + (options.embed_assets ? "ON" : "OFF"));
  if (status != 0) {
    return status;
  }

  const auto target = options.target.empty() ? config.name : options.target;
  status = run_command("cmake --build " + shell_quote(options.build_dir) + " --target " + shell_quote(target));
  if (status != 0) {
    return status;
  }

  std::cout << "Built " << (options.build_dir / target) << "\n";
  if (options.embed_assets) {
    std::cout << "Embedded templates and public assets into the executable.\n";
  }
  return 0;
}

BuildOptions parse_build_options(int argc, char** argv, int start) {
  BuildOptions options;
  for (int i = start; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--debug") {
      options.build_type = "Debug";
    } else if (arg == "--release") {
      options.build_type = "Release";
    } else if ((arg == "--target" || arg == "-t") && i + 1 < argc) {
      options.target = argv[++i];
    } else if (arg.starts_with("--target=")) {
      options.target = arg.substr(std::string("--target=").size());
    } else if ((arg == "--build-dir" || arg == "-B") && i + 1 < argc) {
      options.build_dir = argv[++i];
    } else if (arg.starts_with("--build-dir=")) {
      options.build_dir = arg.substr(std::string("--build-dir=").size());
    } else {
      std::cerr << "Unknown build option: " << arg << "\n";
      std::exit(2);
    }
  }
  return options;
}

int build(int argc, char** argv) {
  auto config = read_config(fs::current_path());
  if (!config) {
    std::cerr << "No luxweb.toml found. Run this inside an app created by `luxweb new <name>`.\n";
    return 2;
  }
  auto options = parse_build_options(argc, argv, 2);
  options.embed_assets = true;
  return build_project(*config, options);
}

int serve(int argc, char** argv) {
  if (argc >= 3) {
    std::string command = shell_quote(std::string(argv[2]));
    if (argc > 3) {
      command += " ";
      command += argv[3];
    }
    return run_command(command);
  }
  auto config = read_config(fs::current_path());
  if (!config) {
    std::cerr << "Usage: luxweb serve <binary> [port]\n";
    std::cerr << "Or run inside a Luxweb project with luxweb.toml.\n";
    return 2;
  }
  return run_command(shell_quote(fs::current_path() / "build" / config->name) + " " + std::to_string(config->port));
}

int dev() {
  auto config = read_config(fs::current_path());
  if (!config) {
    std::cerr << "No luxweb.toml found. Run this inside an app created by `luxweb new <name>`.\n";
    return 2;
  }
  BuildOptions options;
  options.build_type = "Debug";
  auto status = build_project(*config, options);
  if (status != 0) {
    return status;
  }
  std::cout << "Starting " << config->name << " at http://127.0.0.1:" << config->port << "\n";
  return run_command(shell_quote(fs::current_path() / "build" / config->name) + " " + std::to_string(config->port));
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    std::cerr << "Usage: luxweb <new|build|dev|serve> ...\n";
    return 2;
  }
  const std::string command = argv[1];
  if (command == "new") {
    if (argc < 3) {
      std::cerr << "Usage: luxweb new <name>\n";
      return 2;
    }
    return new_app(argv[2]);
  }
  if (command == "dev") {
    return dev();
  }
  if (command == "build") {
    return build(argc, argv);
  }
  if (command == "serve") {
    return serve(argc, argv);
  }
  std::cerr << "Unknown command: " << command << "\n";
  return 2;
}
