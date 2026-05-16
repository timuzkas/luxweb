#include <luxweb/http.hpp>

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>

namespace lux {
namespace {

std::string lower(std::string_view value) {
  std::string result(value);
  std::ranges::transform(result, result.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return result;
}

std::string trim(std::string_view value) {
  auto first = value.begin();
  auto last = value.end();
  while (first != last && std::isspace(static_cast<unsigned char>(*first))) {
    ++first;
  }
  while (first != last && std::isspace(static_cast<unsigned char>(*(last - 1)))) {
    --last;
  }
  return std::string(first, last);
}

std::unordered_map<std::string, std::string> parse_pairs(std::string_view input, char sep) {
  std::unordered_map<std::string, std::string> result;
  std::size_t pos = 0;
  while (pos <= input.size()) {
    const auto next = input.find(sep, pos);
    const auto part = input.substr(pos, next == std::string_view::npos ? input.size() - pos : next - pos);
    if (!part.empty()) {
      const auto eq = part.find('=');
      auto key = trim(part.substr(0, eq));
      auto value = eq == std::string_view::npos ? std::string{} : trim(part.substr(eq + 1));
      result[url_decode(key)] = url_decode(value);
    }
    if (next == std::string_view::npos) {
      break;
    }
    pos = next + 1;
  }
  return result;
}

}  // namespace

std::optional<std::string> Request::header(std::string_view name) const {
  const auto it = headers.find(lower(name));
  if (it == headers.end()) {
    return std::nullopt;
  }
  return it->second;
}

std::optional<std::string> Request::cookie(std::string_view name) const {
  const auto it = cookies.find(std::string(name));
  if (it == cookies.end()) {
    return std::nullopt;
  }
  return it->second;
}

std::optional<std::string> Request::form(std::string_view name) const {
  auto values = parse_form_urlencoded(body);
  const auto it = values.find(std::string(name));
  if (it == values.end()) {
    return std::nullopt;
  }
  return it->second;
}

Response Response::html(std::string body, int code) {
  Response response;
  response.code = code;
  response.body = std::move(body);
  response.headers["Content-Type"] = "text/html; charset=utf-8";
  return response;
}

Response Response::json(const Json& data, int code) {
  Response response;
  response.code = code;
  response.body = data.dump();
  response.headers["Content-Type"] = "application/json";
  return response;
}

Response Response::redirect(std::string location, int code) {
  Response response;
  response.code = code;
  response.headers["Location"] = std::move(location);
  return response;
}

std::unordered_map<std::string, std::string> parse_cookies(std::string_view header) {
  return parse_pairs(header, ';');
}

std::unordered_map<std::string, std::string> parse_query(std::string_view query) {
  if (!query.empty() && query.front() == '?') {
    query.remove_prefix(1);
  }
  return parse_pairs(query, '&');
}

std::unordered_map<std::string, std::string> parse_form_urlencoded(std::string_view body) {
  return parse_pairs(body, '&');
}

std::string url_decode(std::string_view value) {
  std::string out;
  out.reserve(value.size());
  for (std::size_t i = 0; i < value.size(); ++i) {
    if (value[i] == '+') {
      out.push_back(' ');
    } else if (value[i] == '%' && i + 2 < value.size()) {
      unsigned int byte = 0;
      std::istringstream hex(std::string(value.substr(i + 1, 2)));
      hex >> std::hex >> byte;
      if (!hex.fail()) {
        out.push_back(static_cast<char>(byte));
        i += 2;
      } else {
        out.push_back(value[i]);
      }
    } else {
      out.push_back(value[i]);
    }
  }
  return out;
}

std::string html_escape(std::string_view value) {
  std::string out;
  out.reserve(value.size());
  for (const char c : value) {
    switch (c) {
      case '&':
        out += "&amp;";
        break;
      case '<':
        out += "&lt;";
        break;
      case '>':
        out += "&gt;";
        break;
      case '"':
        out += "&quot;";
        break;
      case '\'':
        out += "&#39;";
        break;
      default:
        out.push_back(c);
    }
  }
  return out;
}

}  // namespace lux
