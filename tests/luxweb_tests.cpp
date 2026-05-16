#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>

#include <luxweb/luxweb.hpp>

int main() {
  auto cookies = lux::parse_cookies("a=1; session=abc%20123");
  assert(cookies["a"] == "1");
  assert(cookies["session"] == "abc 123");

  assert(lux::html_escape("<x&y>") == "&lt;x&amp;y&gt;");

  auto response = lux::Response::json({{"ok", true}});
  assert(response.code == 200);
  assert(response.headers["Content-Type"] == "application/json");
  assert(response.body.find("\"ok\":true") != std::string::npos);

  auto hash = lux::password_hash("correct horse battery staple");
  assert(lux::verify_password("correct horse battery staple", hash));
  assert(!lux::verify_password("wrong", hash));

  auto db_path = std::filesystem::temp_directory_path() / ("luxweb-test-" + lux::random_token(4) + ".db");
  auto store = lux::UserStore::sqlite(db_path);
  auto user = store.create_user("a@example.com", "A", lux::password_hash("password123"));
  assert(user.id > 0);
  assert(store.find_user_by_email("a@example.com")->id == user.id);
  auto token = store.create_session(user.id, 60);
  assert(store.user_for_session(token)->email == "a@example.com");
  store.delete_session(token);
  assert(!store.user_for_session(token));

  auto template_dir = std::filesystem::temp_directory_path() / ("luxweb-template-" + lux::random_token(4));
  std::filesystem::create_directories(template_dir);
  {
    std::ofstream out(template_dir / "hello.html");
    out << "Hello {{ name }}";
  }
  lux::TemplateEngine templates;
  templates.add_search_path(template_dir);
  assert(templates.render("hello", {{"name", "Lux"}}) == "Hello Lux");

  lux::TemplateEngine embedded_templates;
  embedded_templates.add_template("layouts/base.html", "<main>{% block content %}{% endblock %}</main>");
  embedded_templates.add_template("pages/index.html", "{% extends \"../layouts/base.html\" %}{% block content %}Embedded{% endblock %}");
  assert(embedded_templates.render("pages/index").find("<main>Embedded</main>") != std::string::npos);

  setenv("LUXWEB_TEST_ENV", "available", 1);
  lux::Context env_context({}, nullptr, std::make_shared<lux::ContextState>());
  assert(env_context.env("LUXWEB_TEST_ENV") == "available");
  assert(env_context.env("LUXWEB_MISSING_ENV", "fallback") == "fallback");

  lux::TemplateEngine starter_templates;
  starter_templates.add_search_path(std::filesystem::path(LUXWEB_SOURCE_DIR) / "examples/starter/templates");
  auto home = starter_templates.render("pages/index", {{"title", "Luxweb Starter"}, {"current_user", nullptr}});
  assert(home.find("Fast native routes") != std::string::npos);
  assert(home.find("<script defer src=\"/lux/lux.js\"></script>") != std::string::npos);

  auto runtime = lux::generate_js();
  assert(runtime.find("lux.signal = signal") != std::string::npos);
  assert(runtime.find("lux.store = store") != std::string::npos);
  assert(runtime.find("data-lux-model") != std::string::npos);
  assert(runtime.find("document.readyState === \"complete\"") != std::string::npos);

  std::filesystem::remove(db_path);
  std::filesystem::remove_all(template_dir);
}
