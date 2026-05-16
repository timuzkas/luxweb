#include <luxweb/user_store.hpp>

#include <chrono>
#include <filesystem>
#include <stdexcept>

#include <luxweb/crypto.hpp>

namespace lux {
namespace {

std::string column_text(sqlite3_stmt* stmt, int index) {
  const auto* text = sqlite3_column_text(stmt, index);
  return text == nullptr ? std::string{} : reinterpret_cast<const char*>(text);
}

std::string expires_after(int seconds) {
  return std::to_string(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now() + std::chrono::seconds(seconds)));
}

}  // namespace

UserStore::UserStore() = default;

UserStore::UserStore(std::filesystem::path path) {
  open(std::move(path));
  migrate();
}

UserStore::~UserStore() {
  if (db_) {
    sqlite3_close(db_);
  }
}

UserStore::UserStore(UserStore&& other) noexcept : db_(other.db_), path_(std::move(other.path_)) {
  other.db_ = nullptr;
}

UserStore& UserStore::operator=(UserStore&& other) noexcept {
  if (this != &other) {
    if (db_) {
      sqlite3_close(db_);
    }
    db_ = other.db_;
    path_ = std::move(other.path_);
    other.db_ = nullptr;
  }
  return *this;
}

UserStore UserStore::sqlite(std::filesystem::path path) {
  return UserStore(std::move(path));
}

void UserStore::open(std::filesystem::path path) {
  path_ = std::move(path);
  if (!path_.parent_path().empty()) {
    std::filesystem::create_directories(path_.parent_path());
  }
  if (sqlite3_open(path_.string().c_str(), &db_) != SQLITE_OK) {
    throw std::runtime_error("sqlite open failed: " + std::string(sqlite3_errmsg(db_)));
  }
  exec("PRAGMA foreign_keys = ON");
}

void UserStore::exec(std::string_view sql) const {
  char* error = nullptr;
  if (sqlite3_exec(db_, std::string(sql).c_str(), nullptr, nullptr, &error) != SQLITE_OK) {
    std::string message = error == nullptr ? "sqlite exec failed" : error;
    sqlite3_free(error);
    throw std::runtime_error(message);
  }
}

void UserStore::migrate() {
  exec("CREATE TABLE IF NOT EXISTS users ("
       "id INTEGER PRIMARY KEY AUTOINCREMENT,"
       "email TEXT NOT NULL UNIQUE,"
       "name TEXT NOT NULL,"
       "password_hash TEXT NOT NULL,"
       "created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP)");
  exec("CREATE TABLE IF NOT EXISTS sessions ("
       "token TEXT PRIMARY KEY,"
       "user_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,"
       "expires_at INTEGER NOT NULL,"
       "created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP)");
}

User UserStore::create_user(std::string email, std::string name, std::string hash) {
  sqlite3_stmt* stmt = nullptr;
  const char* sql = "INSERT INTO users(email, name, password_hash) VALUES (?, ?, ?)";
  if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    throw std::runtime_error(sqlite3_errmsg(db_));
  }
  sqlite3_bind_text(stmt, 1, email.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, name.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 3, hash.c_str(), -1, SQLITE_TRANSIENT);
  if (sqlite3_step(stmt) != SQLITE_DONE) {
    std::string message = sqlite3_errmsg(db_);
    sqlite3_finalize(stmt);
    throw std::runtime_error(message);
  }
  sqlite3_finalize(stmt);
  return User{static_cast<int>(sqlite3_last_insert_rowid(db_)), std::move(email), std::move(name), std::move(hash)};
}

std::optional<User> UserStore::find_user_by_email(std::string_view email) const {
  sqlite3_stmt* stmt = nullptr;
  const char* sql = "SELECT id, email, name, password_hash FROM users WHERE email = ?";
  sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
  sqlite3_bind_text(stmt, 1, std::string(email).c_str(), -1, SQLITE_TRANSIENT);
  std::optional<User> user;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    user = User{sqlite3_column_int(stmt, 0), column_text(stmt, 1), column_text(stmt, 2), column_text(stmt, 3)};
  }
  sqlite3_finalize(stmt);
  return user;
}

std::optional<User> UserStore::find_user_by_id(int id) const {
  sqlite3_stmt* stmt = nullptr;
  const char* sql = "SELECT id, email, name, password_hash FROM users WHERE id = ?";
  sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
  sqlite3_bind_int(stmt, 1, id);
  std::optional<User> user;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    user = User{sqlite3_column_int(stmt, 0), column_text(stmt, 1), column_text(stmt, 2), column_text(stmt, 3)};
  }
  sqlite3_finalize(stmt);
  return user;
}

std::string UserStore::create_session(int user_id, int ttl_seconds) {
  auto token = random_token();
  sqlite3_stmt* stmt = nullptr;
  const char* sql = "INSERT INTO sessions(token, user_id, expires_at) VALUES (?, ?, ?)";
  sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
  sqlite3_bind_text(stmt, 1, token.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, 2, user_id);
  auto expires = expires_after(ttl_seconds);
  sqlite3_bind_text(stmt, 3, expires.c_str(), -1, SQLITE_TRANSIENT);
  if (sqlite3_step(stmt) != SQLITE_DONE) {
    std::string message = sqlite3_errmsg(db_);
    sqlite3_finalize(stmt);
    throw std::runtime_error(message);
  }
  sqlite3_finalize(stmt);
  return token;
}

std::optional<User> UserStore::user_for_session(std::string_view token) const {
  sqlite3_stmt* stmt = nullptr;
  const char* sql =
      "SELECT u.id, u.email, u.name, u.password_hash FROM sessions s "
      "JOIN users u ON u.id = s.user_id WHERE s.token = ? AND CAST(s.expires_at AS INTEGER) > ?";
  sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
  sqlite3_bind_text(stmt, 1, std::string(token).c_str(), -1, SQLITE_TRANSIENT);
  auto now = std::to_string(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
  sqlite3_bind_text(stmt, 2, now.c_str(), -1, SQLITE_TRANSIENT);
  std::optional<User> user;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    user = User{sqlite3_column_int(stmt, 0), column_text(stmt, 1), column_text(stmt, 2), column_text(stmt, 3)};
  }
  sqlite3_finalize(stmt);
  return user;
}

void UserStore::delete_session(std::string_view token) {
  sqlite3_stmt* stmt = nullptr;
  const char* sql = "DELETE FROM sessions WHERE token = ?";
  sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
  sqlite3_bind_text(stmt, 1, std::string(token).c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_step(stmt);
  sqlite3_finalize(stmt);
}

void UserStore::prune_sessions() {
  sqlite3_stmt* stmt = nullptr;
  const char* sql = "DELETE FROM sessions WHERE CAST(expires_at AS INTEGER) <= ?";
  sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
  auto now = std::to_string(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
  sqlite3_bind_text(stmt, 1, now.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_step(stmt);
  sqlite3_finalize(stmt);
}

}  // namespace lux
