////////////////////////////////////////////////////////////////
// async_tracker — see ork/kernel/async_tracker.h
////////////////////////////////////////////////////////////////
#include <ork/kernel/async_tracker.h>
#include <atomic>
#include <map>
#include <mutex>

namespace ork {

static std::atomic<int> g_async_pending{0};
static std::mutex       g_async_mtx;

static std::map<std::string, int>& asyncTagMap() {
  static std::map<std::string, int> tags;
  return tags;
}

void asyncWorkBegin(const std::string& tag) {
  g_async_pending.fetch_add(1);
  std::lock_guard<std::mutex> lk(g_async_mtx);
  asyncTagMap()[tag]++;
}

void asyncWorkEnd(const std::string& tag) {
  // decrement, but never below zero (defensive against an unbalanced end)
  int cur = g_async_pending.load();
  while (cur > 0 and not g_async_pending.compare_exchange_weak(cur, cur - 1)) {
  }
  std::lock_guard<std::mutex> lk(g_async_mtx);
  auto& tags = asyncTagMap();
  auto it    = tags.find(tag);
  if (it != tags.end() and it->second > 0)
    it->second--;
}

int asyncWorkPending() {
  int v = g_async_pending.load();
  return v < 0 ? 0 : v;
}

std::string asyncWorkSummary() {
  std::lock_guard<std::mutex> lk(g_async_mtx);
  std::string s;
  for (const auto& kv : asyncTagMap())
    if (kv.second > 0)
      s += kv.first + ":" + std::to_string(kv.second) + " ";
  return s;
}

} // namespace ork
