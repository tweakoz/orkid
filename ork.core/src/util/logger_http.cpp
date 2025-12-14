////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/util/logger.h>
#include <ork/kernel/mutex.h>
#include <ork/kernel/string/string.h>
#include <ork/application/application.h>
#include <zmq.hpp>
#include <deque>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <chrono>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <cstdlib>
#include <unistd.h>

namespace ork {

////////////////////////////////////////////////////////////////
// HTTP Backend Implementation
//
// Architecture:
// - Shared server model: user starts ork.logger.httpserver.py manually
// - Server BINDS ZMQ SUB socket, multiple C++ clients CONNECT
// - Each client identified by app_name + pid
// - Heartbeat mechanism for crash detection
// - Server shows tabbed UI per client
////////////////////////////////////////////////////////////////

struct HttpBackendImpl;
using http_backend_impl_ptr_t = std::shared_ptr<HttpBackendImpl>;

struct HttpBackendImpl {
  std::string _zmq_uri;
  pid_t _pid;

  zmq::context_t _zmq_ctx{1};
  zmq::socket_t _pub_socket{_zmq_ctx, zmq::socket_type::pub};

  LockedResource<std::deque<std::string>> _queue;
  std::thread _writer_thread;
  std::thread _heartbeat_thread;
  std::atomic<bool> _running{false};
  std::mutex _cv_mutex;
  std::condition_variable _cv;
  std::atomic<bool> _immediate_flush{false};

  HttpBackendImpl(const std::string& zmq_uri)
      : _zmq_uri(zmq_uri)
      , _pid(getpid()) {
  }

  // Get app name dynamically (EzApp sets it after backend creation)
  std::string getAppName() const {
    if (gappinitdata && !gappinitdata->_application_name.empty()) {
      return gappinitdata->_application_name;
    }
    return "orkid_app";
  }

  // Get hostname
  std::string getHostname() const {
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == 0) {
      return std::string(hostname);
    }
    return "unknown";
  }

  ~HttpBackendImpl() {
    stop();
  }

  void start() {
    // Connect to server's ZMQ SUB socket (reverse pub/sub)
    try {
      _pub_socket.set(zmq::sockopt::sndhwm, 10000);  // High water mark
      _pub_socket.set(zmq::sockopt::linger, 100);    // Brief linger for disconnect msg
      _pub_socket.connect(_zmq_uri);
    } catch (const zmq::error_t& e) {
      fprintf(stderr, "HttpBackend: Failed to connect to %s: %s\n",
              _zmq_uri.c_str(), e.what());
      fprintf(stderr, "HttpBackend: Start server with: ork.logger.httpserver.py\n");
      return;
    }

    _running = true;

    // Start writer thread
    _writer_thread = std::thread([this]() { writerLoop(); });

    // Start heartbeat thread
    _heartbeat_thread = std::thread([this]() { heartbeatLoop(); });

    // Give ZMQ a moment to establish connection
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Send registration message
    sendControl("register");

    fprintf(stderr, "HttpBackend: Connected to server at %s [pid %d]\n",
            _zmq_uri.c_str(), _pid);
  }

  void stop() {
    if (_running) {
      _running = false;

      // Send disconnect message before shutting down
      sendControl("disconnect");

      _cv.notify_all();
      if (_writer_thread.joinable()) {
        _writer_thread.join();
      }
      if (_heartbeat_thread.joinable()) {
        _heartbeat_thread.join();
      }
      flushQueue();

      _pub_socket.close();
      _zmq_ctx.close();
    }
  }

  void sendControl(const std::string& type) {
    std::ostringstream ss;
    ss << "{\"type\":\"" << type << "\"";
    ss << ",\"app\":\"" << escapeJsonString(getAppName()) << "\"";
    ss << ",\"pid\":" << _pid;
    ss << ",\"host\":\"" << escapeJsonString(getHostname()) << "\"";
    ss << "}";
    std::string msg = ss.str();
    try {
      zmq::message_t zmsg(msg.data(), msg.size());
      _pub_socket.send(zmsg, zmq::send_flags::dontwait);
    } catch (...) {}
  }

  void heartbeatLoop() {
    while (_running) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
      if (_running) {
        sendControl("heartbeat");
      }
    }
  }

  void enqueue(const std::string& json_entry) {
    _queue.atomicOp([&json_entry](std::deque<std::string>& q) {
      q.push_back(json_entry);
    });
    _cv.notify_one();
  }

  void enqueueImmediate(const std::string& json_entry) {
    _queue.atomicOp([&json_entry](std::deque<std::string>& q) {
      q.push_back(json_entry);
    });
    _immediate_flush = true;
    _cv.notify_one();
  }

  void writerLoop() {
    while (_running) {
      std::unique_lock<std::mutex> lock(_cv_mutex);
      _cv.wait_for(lock, std::chrono::milliseconds(10));
      flushQueue();
    }
  }

  void flushQueue() {
    std::deque<std::string> to_send;
    _queue.atomicOp([&to_send](std::deque<std::string>& q) {
      to_send.swap(q);
    });

    for (const auto& json : to_send) {
      try {
        zmq::message_t msg(json.data(), json.size());
        _pub_socket.send(msg, zmq::send_flags::dontwait);
      } catch (const zmq::error_t& e) {
        // Drop message if can't send
      }
    }
    _immediate_flush = false;
  }

  static std::string escapeJsonString(const std::string& text) {
    std::string result;
    result.reserve(text.size() * 1.2);
    for (char c : text) {
      switch (c) {
        case '"': result += "\\\""; break;
        case '\\': result += "\\\\"; break;
        case '\n': result += "\\n"; break;
        case '\r': result += "\\r"; break;
        case '\t': result += "\\t"; break;
        default:
          if (static_cast<unsigned char>(c) < 32) {
            char buf[8];
            snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
            result += buf;
          } else {
            result += c;
          }
          break;
      }
    }
    return result;
  }

  static std::string colorToHex(const fvec3& color) {
    char buf[8];
    int r = static_cast<int>(color.x * 255);
    int g = static_cast<int>(color.y * 255);
    int b = static_cast<int>(color.z * 255);
    snprintf(buf, sizeof(buf), "#%02x%02x%02x", r, g, b);
    return std::string(buf);
  }

  static std::string getTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::tm tm_buf;
    localtime_r(&time, &tm_buf);

    char buf[32];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d.%03d",
             tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec,
             static_cast<int>(ms.count()));
    return std::string(buf);
  }

  std::string formatEntry(const LogChannel* chan, const std::string& msg,
                          const std::string& type = "",
                          const std::string& subchannel = "") {
    std::ostringstream ss;
    ss << "{\"type\":\"log\"";
    ss << ",\"app\":\"" << escapeJsonString(getAppName()) << "\"";
    ss << ",\"pid\":" << _pid;
    ss << ",\"ts\":\"" << getTimestamp() << "\"";
    ss << ",\"ch\":\"" << escapeJsonString(chan->_name) << "\"";
    ss << ",\"color\":\"" << colorToHex(chan->_color) << "\"";
    if (!type.empty()) {
      ss << ",\"level\":\"" << type << "\"";
    }
    if (!subchannel.empty()) {
      ss << ",\"sub\":\"" << escapeJsonString(subchannel) << "\"";
    }
    ss << ",\"msg\":\"" << escapeJsonString(msg) << "\"";
    ss << "}";
    return ss.str();
  }

  std::string formatPerfItem(const LogChannel* chan, const std::string& name, double value) {
    std::ostringstream ss;
    ss << "{\"type\":\"perf\"";
    ss << ",\"app\":\"" << escapeJsonString(getAppName()) << "\"";
    ss << ",\"pid\":" << _pid;
    ss << ",\"ts\":\"" << getTimestamp() << "\"";
    ss << ",\"ch\":\"" << escapeJsonString(chan->_name) << "\"";
    ss << ",\"color\":\"" << colorToHex(chan->_color) << "\"";
    ss << ",\"sub\":\"" << escapeJsonString(name) << "\"";
    ss << std::fixed << std::setprecision(6);
    ss << ",\"value\":" << value;
    ss << "}";
    return ss.str();
  }

  std::string formatStatus(const LogChannel* chan, const std::string& subchannel, const std::string& msg) {
    std::ostringstream ss;
    ss << "{\"type\":\"status\"";
    ss << ",\"app\":\"" << escapeJsonString(getAppName()) << "\"";
    ss << ",\"pid\":" << _pid;
    ss << ",\"ts\":\"" << getTimestamp() << "\"";
    ss << ",\"ch\":\"" << escapeJsonString(chan->_name) << "\"";
    ss << ",\"color\":\"" << colorToHex(chan->_color) << "\"";
    ss << ",\"sub\":\"" << escapeJsonString(subchannel) << "\"";
    ss << ",\"msg\":\"" << escapeJsonString(msg) << "\"";
    ss << "}";
    return ss.str();
  }
};

////////////////////////////////////////////////////////////////
// HTTP backend function pointers
////////////////////////////////////////////////////////////////

static http_backend_impl_ptr_t getHttpImpl(const LogChannel* chan) {
  auto impl = chan->_backend_impl.tryAs<http_backend_impl_ptr_t>();
  if (impl) return impl.value();
  impl = chan->_logger->_backend->_impl.tryAs<http_backend_impl_ptr_t>();
  if (impl) return impl.value();
  return nullptr;
}

static void httpLogFn(const LogChannel* chan, const std::string& str) {
  auto impl = getHttpImpl(chan);
  if (impl) {
    impl->enqueue(impl->formatEntry(chan, str));
  }
}

static void httpWarnFn(const LogChannel* chan, const std::string& str) {
  auto impl = getHttpImpl(chan);
  if (impl) {
    impl->enqueueImmediate(impl->formatEntry(chan, str, "warn"));
  }
}

static void httpErrorFn(const LogChannel* chan, const std::string& str) {
  auto impl = getHttpImpl(chan);
  if (impl) {
    impl->enqueueImmediate(impl->formatEntry(chan, str, "error"));
  }
}

static void httpStatusFn(const LogChannel* chan, std::string subchannel, const std::string& str) {
  auto impl = getHttpImpl(chan);
  if (impl) {
    impl->enqueue(impl->formatStatus(chan, subchannel, str));
  }
}

static void httpPerfItemFn(const LogChannel* chan, std::string name, svar64_t& data) {
  auto impl = getHttpImpl(chan);
  if (impl) {
    double value = 0.0;
    if (data.isA<float>()) {
      value = static_cast<double>(data.get<float>());
    } else if (data.isA<int>()) {
      value = static_cast<double>(data.get<int>());
    } else if (data.isA<double>()) {
      value = data.get<double>();
    } else if (data.isA<int64_t>()) {
      value = static_cast<double>(data.get<int64_t>());
    } else if (data.isA<uint64_t>()) {
      value = static_cast<double>(data.get<uint64_t>());
    }
    impl->enqueue(impl->formatPerfItem(chan, name, value));
  }
}

////////////////////////////////////////////////////////////////
// Factory function
////////////////////////////////////////////////////////////////

logger_backend_ptr_t createHttpBackend(const std::string& zmq_uri) {
  auto backend = std::make_shared<LoggerBackend>();
  auto impl = std::make_shared<HttpBackendImpl>(zmq_uri);

  impl->start();

  backend->_impl = impl;
  backend->_add_log_line = httpLogFn;
  backend->_begin_log_line = httpLogFn;
  backend->_continue_log_line = httpLogFn;
  backend->_end_log_line = httpLogFn;
  backend->_warn = httpWarnFn;
  backend->_error = httpErrorFn;
  backend->_status = httpStatusFn;
  backend->_on_perf_item = httpPerfItemFn;

  return backend;
}

////////////////////////////////////////////////////////////////
} // namespace ork
