#include <luxweb/crypto.hpp>

#include <array>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <vector>

#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>

namespace lux {
namespace {

std::string hex(const unsigned char* data, std::size_t size) {
  std::ostringstream out;
  out << std::hex << std::setfill('0');
  for (std::size_t i = 0; i < size; ++i) {
    out << std::setw(2) << static_cast<int>(data[i]);
  }
  return out.str();
}

std::vector<unsigned char> from_hex(std::string_view value) {
  if (value.size() % 2 != 0) {
    throw std::runtime_error("invalid hex length");
  }
  std::vector<unsigned char> out(value.size() / 2);
  for (std::size_t i = 0; i < out.size(); ++i) {
    unsigned int byte = 0;
    std::istringstream in(std::string(value.substr(i * 2, 2)));
    in >> std::hex >> byte;
    if (in.fail()) {
      throw std::runtime_error("invalid hex");
    }
    out[i] = static_cast<unsigned char>(byte);
  }
  return out;
}

}  // namespace

std::string random_token(std::size_t bytes) {
  std::vector<unsigned char> buffer(bytes);
  if (RAND_bytes(buffer.data(), static_cast<int>(buffer.size())) != 1) {
    throw std::runtime_error("RAND_bytes failed");
  }
  return hex(buffer.data(), buffer.size());
}

std::string password_hash(std::string_view password) {
  constexpr int kIterations = 210000;
  constexpr std::size_t kSaltBytes = 16;
  constexpr std::size_t kHashBytes = 32;
  std::array<unsigned char, kSaltBytes> salt{};
  std::array<unsigned char, kHashBytes> hash{};
  if (RAND_bytes(salt.data(), static_cast<int>(salt.size())) != 1) {
    throw std::runtime_error("RAND_bytes failed");
  }
  if (PKCS5_PBKDF2_HMAC(password.data(), static_cast<int>(password.size()), salt.data(), static_cast<int>(salt.size()),
                        kIterations, EVP_sha256(), static_cast<int>(hash.size()), hash.data()) != 1) {
    throw std::runtime_error("PBKDF2 failed");
  }
  return "pbkdf2_sha256$" + std::to_string(kIterations) + "$" + hex(salt.data(), salt.size()) + "$" +
         hex(hash.data(), hash.size());
}

bool verify_password(std::string_view password, std::string_view encoded) {
  const std::string marker = "pbkdf2_sha256$";
  if (!encoded.starts_with(marker)) {
    return false;
  }
  std::string_view rest = encoded.substr(marker.size());
  const auto first = rest.find('$');
  const auto second = first == std::string_view::npos ? std::string_view::npos : rest.find('$', first + 1);
  if (first == std::string_view::npos || second == std::string_view::npos) {
    return false;
  }
  const int iterations = std::stoi(std::string(rest.substr(0, first)));
  const auto salt = from_hex(rest.substr(first + 1, second - first - 1));
  const auto expected = from_hex(rest.substr(second + 1));
  std::vector<unsigned char> actual(expected.size());
  if (PKCS5_PBKDF2_HMAC(password.data(), static_cast<int>(password.size()), salt.data(), static_cast<int>(salt.size()),
                        iterations, EVP_sha256(), static_cast<int>(actual.size()), actual.data()) != 1) {
    return false;
  }
  return CRYPTO_memcmp(actual.data(), expected.data(), expected.size()) == 0;
}

std::string sha256_hex(std::string_view value) {
  std::array<unsigned char, SHA256_DIGEST_LENGTH> digest{};
  SHA256(reinterpret_cast<const unsigned char*>(value.data()), value.size(), digest.data());
  return hex(digest.data(), digest.size());
}

}  // namespace lux
