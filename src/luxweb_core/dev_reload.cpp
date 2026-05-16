#include <luxweb/dev_reload.hpp>

namespace lux {

void ReloadHub::notify() {
  version_.fetch_add(1);
  cv_.notify_all();
}

std::string ReloadHub::wait_event(std::uint64_t last_seen) {
  std::unique_lock lock(mutex_);
  cv_.wait(lock, [&] { return version_.load() > last_seen; });
  return "event: reload\ndata: " + std::to_string(version_.load()) + "\n\n";
}

std::uint64_t ReloadHub::version() const {
  return version_.load();
}

std::string dev_reload_script() {
  return R"JS(<script>
(() => {
  const source = new EventSource('/lux/reload');
  source.addEventListener('reload', () => location.reload());
})();
</script>)JS";
}

}  // namespace lux
