////////////////////////////////////////////////////////////////
// async_tracker — see ork/kernel/async_tracker.h
////////////////////////////////////////////////////////////////
#include <ork/kernel/async_tracker.h>
#include <atomic>
#include <map>
#include <mutex>
#include <set>

namespace ork {

static std::atomic<int> g_async_pending{0};
static std::mutex       g_async_mtx;

static std::map<std::string, int>& asyncTagMap() {
  static std::map<std::string, int> tags;
  return tags;
}

// tags declared RECURRING (see the header) — guarded by the same g_async_mtx as
// the tag map, so a census reads both under one lock.
static std::set<std::string>& asyncSteadyTags() {
  static std::set<std::string> steady;
  return steady;
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

int asyncWorkPendingExcluding(const std::string& exclude_tag) {
  std::lock_guard<std::mutex> lk(g_async_mtx);
  int n = 0;
  for (const auto& kv : asyncTagMap())
    if (kv.first != exclude_tag and kv.second > 0)
      n += kv.second;
  return n;
}

void asyncWorkMarkSteady(const std::string& tag) {
  std::lock_guard<std::mutex> lk(g_async_mtx);
  asyncSteadyTags().insert(tag);
}

bool asyncWorkIsSteady(const std::string& tag) {
  std::lock_guard<std::mutex> lk(g_async_mtx);
  auto& steady = asyncSteadyTags();
  return steady.find(tag) != steady.end();
}

int asyncWorkPendingOneShot(const std::string& exclude_tag) {
  std::lock_guard<std::mutex> lk(g_async_mtx);
  auto& steady = asyncSteadyTags();
  int n        = 0;
  for (const auto& kv : asyncTagMap()) {
    if (kv.second <= 0)
      continue;
    if (kv.first == exclude_tag)
      continue;
    if (steady.find(kv.first) != steady.end())
      continue;
    n += kv.second;
  }
  return n;
}

std::string asyncWorkSummary() {
  std::lock_guard<std::mutex> lk(g_async_mtx);
  auto& steady = asyncSteadyTags();
  std::string s;
  for (const auto& kv : asyncTagMap())
    if (kv.second > 0) {
      s += kv.first + ":" + std::to_string(kv.second);
      if (steady.find(kv.first) != steady.end())
        s += " STEADY";
      s += " ";
    }
  return s;
}

} // namespace ork
