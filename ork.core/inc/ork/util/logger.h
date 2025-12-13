#pragma once

#include <ork/kernel/string/deco.inl>
#include <ork/kernel/mutex.h>
#include <ork/file/file.h>
#include <thread>
#include <functional>
#include <vector>

namespace ork {

  extern bool _ENABLE_LOGGING;
  struct Logger;
  struct LogChannel; 

  struct StderrRedirector {

    int _pipe_fd[2];
    int _original_stderr;
    std::thread _reader_thread;
    std::atomic<bool> _running{false};
    LogChannel* _logchan = nullptr;

    StderrRedirector(LogChannel* logchan);
    ~StderrRedirector();
    void start();
    void stop();
    void read_loop();

  };

  using stderr_redirector_ptr_t = std::shared_ptr<StderrRedirector>;
  struct LogChannel {

    LogChannel(Logger* logger, std::string named, ork::fvec3 color,bool enabled=true);
    void log_valist(const char *pMsgFormat, va_list args) const;
    void log(const char *pMsgFormat, ...);
    void log_begin_valist(const char *pMsgFormat, va_list args) const;
    void log_begin(const char *pMsgFormat, ...);
    void log_continue_valist(const char *pMsgFormat, va_list args) const;
    void log_continue(const char *pMsgFormat, ...) const;
    void warn(const char *pMsgFormat, ...);
    void error(const char *pMsgFormat, ...);

    void status(const std::string& subchannel, const char *pMsgFormat, ...);
    void status_valist(const std::string& subchannel, const char *pMsgFormat, va_list args);
    void perfItem(const std::string& name, svar64_t data);

    ork::fvec3 _color;
    std::string _name;
    std::string _c1_prefix;
    std::string _reset;
    bool _enabled;
    file_ptr_t _file; // if not null, log to file
    float _status_interval = 5.0f;
    float _perf_interval = 1.0f;  // Sampling rate for pull-based perfItems (in seconds)
    Logger* _logger = nullptr; // Backpointer to logger for this channel

    mutable svar64_t _backend_impl;

    // Pull-based perfItem lambdas (sampled at _perf_interval rate)
    // Store lambda in svar64_t - check with isA<float_lambda_t> or isA<int_lambda_t>
    struct PerfItemLambda {
      std::string name;
      svar64_t lambda;  // Can hold float_lambda_t or int_lambda_t
    };
    std::vector<PerfItemLambda> _perf_lambdas;
  };

  using logchannel_ptr_t = std::shared_ptr<LogChannel>;

  struct LoggerBackend {
    using log_fn_t = void (*)(const LogChannel*, const std::string&);
    using status_fn_t = void (*)(const LogChannel*, std::string subchannel, const std::string&);
    using perfitem_fn_t = void (*)(const LogChannel*, std::string subchannel, svar64_t&);
    log_fn_t _add_log_line = nullptr;
    log_fn_t _begin_log_line = nullptr;
    log_fn_t _continue_log_line = nullptr;
    log_fn_t _end_log_line = nullptr;
    log_fn_t _warn = nullptr;
    log_fn_t _error = nullptr;
    status_fn_t _status = nullptr;
    perfitem_fn_t _on_perf_item = nullptr;
    svar64_t _impl;
    std::mutex _mutex;
  };

  using logger_backend_ptr_t = std::shared_ptr<LoggerBackend>;

  struct Logger {
    Logger();
    ~Logger();
    logchannel_ptr_t configureChannel(std::string named, ork::fvec3 color, bool enabled=true);
    logchannel_ptr_t getChannel(std::string named);
    logchannel_ptr_t defaultChannel() const;

    // Backend management
    void setBackend(logger_backend_ptr_t backend);

		using channel_map_t = std::map<std::string,logchannel_ptr_t>;
		ork::LockedResource<channel_map_t> _channels;
    logchannel_ptr_t _default_channel;
    logchannel_ptr_t _stderr_channel;
    stderr_redirector_ptr_t _stderr_redirector;
    logger_backend_ptr_t _backend;
    bool _backend_set_by_code = false;
  };

  using logger_ptr_t = std::shared_ptr<Logger>;

  logger_ptr_t logger();
  logchannel_ptr_t logchannel(const std::string& named);
  logchannel_ptr_t logerrchannel();

  // Global log file manager functions
  void setGlobalLogFile(const std::string& path);
  void writeToGlobalLog(const std::string& channel, const std::string& message);
  bool isGlobalLogEnabled();

  /////////////////////////////////////////////////////////////////////
  // Backend factory functions
  /////////////////////////////////////////////////////////////////////

  // Create stdout backend (default behavior)
  logger_backend_ptr_t createStdoutBackend();

  // Create file backend with async writing
  // - path: log file path
  // - enable_ansi: include ANSI color codes (useful for `less -R`)
  // - flush_interval_ms: how often to flush (default 100ms)
  // - flush_on_error: immediate flush on warn/error (default true)
  logger_backend_ptr_t createFileBackend(
      const std::string& path,
      bool enable_ansi = false,
      float flush_interval_ms = 100.0f,
      bool flush_on_error = true);

  // Create fork backend that forwards to multiple child backends
  logger_backend_ptr_t createForkBackend(std::vector<logger_backend_ptr_t> children);
  logger_backend_ptr_t createForkBackend();  // empty, use forkBackendAddChild

  // Add child to existing fork backend
  void forkBackendAddChild(logger_backend_ptr_t fork_backend, logger_backend_ptr_t child);

  // Create HTML backend with interactive channel toggles
  // - path: output HTML file path
  // - flush_interval_ms: how often to flush (default 100ms)
  logger_backend_ptr_t createHtmlBackend(
      const std::string& path,
      float flush_interval_ms = 100.0f);

  /////////////////////////////////////////////////////////////////////
}
