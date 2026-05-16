#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <string>

namespace lux {

class ReloadHub {
 public:
  void notify();
  [[nodiscard]] std::string wait_event(std::uint64_t last_seen);
  [[nodiscard]] std::uint64_t version() const;

 private:
  mutable std::mutex mutex_;
  std::condition_variable cv_;
  std::atomic_uint64_t version_{0};
};

std::string dev_reload_script();

}  // namespace lux
