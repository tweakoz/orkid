////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/util/logger.h>
#include <ork/kernel/string/string.h>
#include <ork/kernel/mutex.h>
#include <deque>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <fstream>

namespace ork {

////////////////////////////////////////////////////////////////
// FileBackend Implementation
////////////////////////////////////////////////////////////////

struct FileBackendImpl {
  std::string _path;
  bool _enable_ansi = false;
  float _flush_interval_ms = 100.0f;
  bool _flush_on_error = true;

  std::ofstream _file;
  LockedResource<std::deque<std::string>> _queue;
  std::thread _writer_thread;
  std::atomic<bool> _running{false};
  std::mutex _cv_mutex;
  std::condition_variable _cv;
  std::atomic<bool> _immediate_flush{false};

  //////////////////////////////////////////////

  FileBackendImpl(const std::string& path, bool enable_ansi, float flush_interval_ms, bool flush_on_error)
      : _path(path)
      , _enable_ansi(enable_ansi)
      , _flush_interval_ms(flush_interval_ms)
      , _flush_on_error(flush_on_error) {
  }

  //////////////////////////////////////////////

  ~FileBackendImpl() {
    stop();
  }

  //////////////////////////////////////////////

  void start() {
    _file.open(_path, std::ios::out | std::ios::app);
    if (!_file.is_open()) {
      fprintf(stderr, "FileBackend: Failed to open log file: %s\n", _path.c_str());
      return;
    }

    _running = true;
    _writer_thread = std::thread([this]() { writerLoop(); });
  }

  //////////////////////////////////////////////

  void stop() {
    if (_running) {
      _running = false;
      _cv.notify_all();

      if (_writer_thread.joinable()) {
        _writer_thread.join();
      }

      // Final flush
      flushQueue();

      if (_file.is_open()) {
        _file.close();
      }
    }
  }

  //////////////////////////////////////////////

  void enqueue(const std::string& line) {
    _queue.atomicOp([&](std::deque<std::string>& q) {
      q.push_back(line);
    });
    _cv.notify_one();
  }

  //////////////////////////////////////////////

  void requestImmediateFlush() {
    _immediate_flush = true;
    _cv.notify_one();
  }

  //////////////////////////////////////////////

  void writerLoop() {
    while (_running) {
      std::unique_lock<std::mutex> lock(_cv_mutex);
      _cv.wait_for(lock, std::chrono::milliseconds(static_cast<int>(_flush_interval_ms)), [this]() {
        return !_running || _immediate_flush.load();
      });
      lock.unlock();

      _immediate_flush = false;
      flushQueue();
    }
  }

  //////////////////////////////////////////////

  void flushQueue() {
    std::deque<std::string> local_queue;

    // Swap under lock - minimal lock time
    _queue.atomicOp([&](std::deque<std::string>& q) {
      local_queue.swap(q);
    });

    // Write outside lock
    for (const auto& line : local_queue) {
      _file << line;
    }

    if (!local_queue.empty()) {
      _file.flush();
    }
  }

  //////////////////////////////////////////////

  std::string formatLine(const LogChannel* chan, const std::string& msg, const char* level = nullptr) {
    if (_enable_ansi) {
      if (level) {
        return FormatString("%s[%s:%s]\t%s%s\n",
                            chan->_c1_prefix.c_str(), chan->_name.c_str(), level,
                            msg.c_str(), chan->_reset.c_str());
      }
      return FormatString("%s[%s]\t%s%s\n",
                          chan->_c1_prefix.c_str(), chan->_name.c_str(),
                          msg.c_str(), chan->_reset.c_str());
    } else {
      if (level) {
        return FormatString("[%s:%s]\t%s\n", chan->_name.c_str(), level, msg.c_str());
      }
      return FormatString("[%s]\t%s\n", chan->_name.c_str(), msg.c_str());
    }
  }

  //////////////////////////////////////////////

  std::string formatStatus(const LogChannel* chan, const std::string& subchannel, const std::string& msg) {
    if (_enable_ansi) {
      return FormatString("%s[%s:%s]\t%s%s\n",
                          chan->_c1_prefix.c_str(), chan->_name.c_str(), subchannel.c_str(),
                          msg.c_str(), chan->_reset.c_str());
    } else {
      return FormatString("[%s:%s]\t%s\n", chan->_name.c_str(), subchannel.c_str(), msg.c_str());
    }
  }
};

using file_backend_impl_ptr_t = std::shared_ptr<FileBackendImpl>;

////////////////////////////////////////////////////////////////
// File backend function pointers
// Try _backend_impl first (set by fork backend), fall back to logger's backend
////////////////////////////////////////////////////////////////

static file_backend_impl_ptr_t getFileImpl(const LogChannel* chan) {
  // First try _backend_impl (set by fork backend)
  auto impl = chan->_backend_impl.tryAs<file_backend_impl_ptr_t>();
  if (impl) return impl.value();
  // Fall back to logger's backend (direct usage)
  impl = chan->_logger->_backend->_impl.tryAs<file_backend_impl_ptr_t>();
  if (impl) return impl.value();
  return nullptr;
}

static void fileLogFn(const LogChannel* chan, const std::string& str) {
  auto impl = getFileImpl(chan);
  if (impl) {
    impl->enqueue(impl->formatLine(chan, str));
  }
}

static void fileWarnFn(const LogChannel* chan, const std::string& str) {
  auto impl = getFileImpl(chan);
  if (impl) {
    impl->enqueue(impl->formatLine(chan, str, "WARN"));
    if (impl->_flush_on_error) {
      impl->requestImmediateFlush();
    }
  }
}

static void fileErrorFn(const LogChannel* chan, const std::string& str) {
  auto impl = getFileImpl(chan);
  if (impl) {
    impl->enqueue(impl->formatLine(chan, str, "ERROR"));
    if (impl->_flush_on_error) {
      impl->requestImmediateFlush();
    }
  }
}

static void fileStatusFn(const LogChannel* chan, std::string subchannel, const std::string& str) {
  auto impl = getFileImpl(chan);
  if (impl) {
    impl->enqueue(impl->formatStatus(chan, subchannel, str));
  }
}

static void filePerfItemFn(const LogChannel* chan, std::string name, svar64_t& data) {
  // Performance items typically not written to file log
}

////////////////////////////////////////////////////////////////
// Factory function
////////////////////////////////////////////////////////////////

logger_backend_ptr_t createFileBackend(
    const std::string& path,
    bool enable_ansi,
    float flush_interval_ms,
    bool flush_on_error) {

  auto backend = std::make_shared<LoggerBackend>();
  auto impl = std::make_shared<FileBackendImpl>(path, enable_ansi, flush_interval_ms, flush_on_error);

  backend->_impl = impl;
  backend->_add_log_line = fileLogFn;
  backend->_begin_log_line = fileLogFn;
  backend->_continue_log_line = fileLogFn;
  backend->_end_log_line = fileLogFn;
  backend->_warn = fileWarnFn;
  backend->_error = fileErrorFn;
  backend->_status = fileStatusFn;
  backend->_on_perf_item = filePerfItemFn;

  impl->start();

  return backend;
}

////////////////////////////////////////////////////////////////
} // namespace ork
