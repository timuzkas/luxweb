#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <sqlite3.h>

namespace lux {

struct User {
  int id = 0;
  std::string email;
  std::string name;
  std::string password_hash;
};

struct Session {
  std::string token;
  int user_id = 0;
  std::string expires_at;
};

class UserStore {
 public:
  UserStore();
  explicit UserStore(std::filesystem::path path);
  ~UserStore();

  UserStore(const UserStore&) = delete;
  UserStore& operator=(const UserStore&) = delete;
  UserStore(UserStore&& other) noexcept;
  UserStore& operator=(UserStore&& other) noexcept;

  static UserStore sqlite(std::filesystem::path path);

  void migrate();
  User create_user(std::string email, std::string name, std::string password_hash);
  [[nodiscard]] std::optional<User> find_user_by_email(std::string_view email) const;
  [[nodiscard]] std::optional<User> find_user_by_id(int id) const;
  std::string create_session(int user_id, int ttl_seconds);
  [[nodiscard]] std::optional<User> user_for_session(std::string_view token) const;
  void delete_session(std::string_view token);
  void prune_sessions();

 private:
  sqlite3* db_ = nullptr;
  std::filesystem::path path_;

  void open(std::filesystem::path path);
  void exec(std::string_view sql) const;
};

}  // namespace lux
