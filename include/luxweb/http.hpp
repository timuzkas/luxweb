#pragma once

#include <optional>
#include <string>
#include <unordered_map>

#include <nlohmann/json.hpp>

namespace lux {

using Json = nlohmann::json;

struct Request {
  std::string method;
  std::string path;
  std::string body;
  std::string remote_ip;
  std::unordered_map<std::string, std::string> headers;
  std::unordered_map<std::string, std::string> query;
  std::unordered_map<std::string, std::string> cookies;

  [[nodiscard]] std::optional<std::string> header(std::string_view name) const;
  [[nodiscard]] std::optional<std::string> cookie(std::string_view name) const;
  [[nodiscard]] std::optional<std::string> form(std::string_view name) const;
};

struct Response {
  int code = 200;
  std::string body;
  std::unordered_map<std::string, std::string> headers;

  static Response html(std::string body, int code = 200);
  static Response json(const Json& data, int code = 200);
  static Response redirect(std::string location, int code = 302);
};

std::unordered_map<std::string, std::string> parse_cookies(std::string_view header);
std::unordered_map<std::string, std::string> parse_query(std::string_view query);
std::unordered_map<std::string, std::string> parse_form_urlencoded(std::string_view body);
std::string url_decode(std::string_view value);
std::string html_escape(std::string_view value);

}  // namespace lux
