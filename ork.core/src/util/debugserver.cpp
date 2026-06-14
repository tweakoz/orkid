////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
//
// debugserver.cpp — in-process zmq debug console. See debugserver.h.
//
////////////////////////////////////////////////////////////////

#include <ork/util/debugserver.h>

#include <zmq.h>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <map>
#include <mutex>
#include <sstream>
#include <thread>
#include <unistd.h>

#if defined(__APPLE__)
#include <mach/mach.h>
#include <pthread.h>
#endif

namespace ork::debugserver {

///////////////////////////////////////////////////////////////////////////////

struct HandlerEntry {
  handler_t _fn;
  std::string _help;
};

static std::mutex g_mutex;
static std::map<std::string, HandlerEntry> g_handlers;
static std::atomic<bool> g_running{false};
static std::thread g_thread;
static std::string g_ipcpath;
static std::chrono::steady_clock::time_point g_t0 = std::chrono::steady_clock::now();

///////////////////////////////////////////////////////////////////////////////

void registerHandler(const std::string& name, handler_t fn, const std::string& help) {
  std::lock_guard<std::mutex> lock(g_mutex);
  g_handlers[name] = HandlerEntry{std::move(fn), help};
}

///////////////////////////////////////////////////////////////////////////////
// built-in probes
///////////////////////////////////////////////////////////////////////////////

static std::string _probe_help(const std::string&) {
  std::ostringstream oss;
  oss << "ork::debugserver probes:\n";
  std::lock_guard<std::mutex> lock(g_mutex);
  for (const auto& item : g_handlers) {
    oss << "  " << item.first;
    if (not item.second._help.empty())
      oss << " — " << item.second._help;
    oss << "\n";
  }
  return oss.str();
}

static std::string _probe_pid(const std::string&) {
  std::ostringstream oss;
  oss << getpid() << "\n";
  return oss.str();
}

static std::string _probe_uptime(const std::string&) {
  auto secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - g_t0).count();
  std::ostringstream oss;
  oss << secs << " sec\n";
  return oss.str();
}

// per-thread name / cpu time / state — the "general thread metrics" probe.
static std::string _probe_threads(const std::string&) {
#if defined(__APPLE__)
  std::ostringstream oss;
  thread_act_array_t threads = nullptr;
  mach_msg_type_number_t count = 0;
  if (task_threads(mach_task_self(), &threads, &count) != KERN_SUCCESS)
    return "task_threads failed\n";
  oss << count << " threads:\n";
  for (mach_msg_type_number_t i = 0; i < count; i++) {
    thread_basic_info_data_t info;
    mach_msg_type_number_t info_count = THREAD_BASIC_INFO_COUNT;
    if (thread_info(threads[i], THREAD_BASIC_INFO, (thread_info_t)&info, &info_count) != KERN_SUCCESS)
      continue;
    char name[64] = {0};
    if (pthread_t pt = pthread_from_mach_thread_np(threads[i]))
      pthread_getname_np(pt, name, sizeof(name));
    double user_s = info.user_time.seconds + info.user_time.microseconds * 1e-6;
    double sys_s  = info.system_time.seconds + info.system_time.microseconds * 1e-6;
    const char* state = "?";
    switch (info.run_state) {
      case TH_STATE_RUNNING:         state = "RUN";   break;
      case TH_STATE_STOPPED:         state = "STOP";  break;
      case TH_STATE_WAITING:         state = "WAIT";  break;
      case TH_STATE_UNINTERRUPTIBLE: state = "UNINT"; break;
      case TH_STATE_HALTED:          state = "HALT";  break;
    }
    char line[256];
    snprintf(
        line,
        sizeof(line),
        "  [%2u] %-24s state<%-5s> cpu<%5.1f%%> user<%8.3fs> sys<%8.3fs>\n",
        i,
        name[0] ? name : "(unnamed)",
        state,
        info.cpu_usage * 100.0 / TH_USAGE_SCALE,
        user_s,
        sys_s);
    oss << line;
    mach_port_deallocate(mach_task_self(), threads[i]);
  }
  vm_deallocate(mach_task_self(), (vm_address_t)threads, count * sizeof(thread_act_t));
  return oss.str();
#else
  return "threads probe: unimplemented on this platform\n";
#endif
}

///////////////////////////////////////////////////////////////////////////////
// server loop — poll-driven REP so stop() can exit cleanly.
///////////////////////////////////////////////////////////////////////////////

static void _serverLoop() {
  void* zctx = zmq_ctx_new();
  void* sock = zmq_socket(zctx, ZMQ_REP);
  int rc     = zmq_bind(sock, g_ipcpath.c_str());
  if (rc != 0) {
    printf("[debugserver] bind FAILED <%s>: %s\n", g_ipcpath.c_str(), zmq_strerror(zmq_errno()));
    zmq_close(sock);
    zmq_ctx_term(zctx);
    g_running = false;
    return;
  }
  printf("[debugserver] serving at %s\n", g_ipcpath.c_str());
  while (g_running.load()) {
    zmq_pollitem_t item{sock, 0, ZMQ_POLLIN, 0};
    if (zmq_poll(&item, 1, 250) <= 0)
      continue;
    char buf[4096];
    int n = zmq_recv(sock, buf, sizeof(buf) - 1, 0);
    if (n < 0)
      continue;
    buf[n] = 0;
    std::string cmd(buf);
    std::string name = cmd, args;
    if (auto sp = cmd.find(' '); sp != std::string::npos) {
      name = cmd.substr(0, sp);
      args = cmd.substr(sp + 1);
    }
    std::string reply;
    handler_t fn;
    {
      std::lock_guard<std::mutex> lock(g_mutex);
      auto it = g_handlers.find(name);
      if (it != g_handlers.end())
        fn = it->second._fn;
    }
    if (fn) {
      try {
        reply = fn(args);
      } catch (const std::exception& e) {
        reply = std::string("probe threw: ") + e.what() + "\n";
      }
    } else {
      reply = "unknown probe <" + name + "> (try: help)\n";
    }
    zmq_send(sock, reply.data(), reply.size(), 0);
  }
  zmq_close(sock);
  zmq_ctx_term(zctx);
  unlink(g_ipcpath.substr(6).c_str()); // strip "ipc://"
}

///////////////////////////////////////////////////////////////////////////////

void start(const std::string& sessionid) {
  bool expected = false;
  if (not g_running.compare_exchange_strong(expected, true))
    return; // already serving
  {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_handlers.find("help") == g_handlers.end()) {
      g_handlers["help"]    = {_probe_help, "list probes"};
      g_handlers["pid"]     = {_probe_pid, "process id"};
      g_handlers["uptime"]  = {_probe_uptime, "seconds since debugserver init"};
      g_handlers["threads"] = {_probe_threads, "per-thread name/state/cpu metrics"};
    }
  }
  g_ipcpath = "ipc:///tmp/ork.debugserve." + sessionid + ".ipc";
  g_thread  = std::thread(_serverLoop);
  g_thread.detach();
}

bool startFromEnv() {
  if (const char* sid = getenv("ORKID_DEBUG_SERVER")) {
    if (sid[0]) {
      start(sid);
      return true;
    }
  }
  return false;
}

void stop() {
  g_running = false;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::debugserver
