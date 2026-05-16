#pragma once

#include <string>
#include <string_view>

namespace lux {

std::string random_token(std::size_t bytes = 32);
std::string password_hash(std::string_view password);
bool verify_password(std::string_view password, std::string_view encoded);
std::string sha256_hex(std::string_view value);

}  // namespace lux
