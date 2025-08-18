#pragma once

#include <ork/kernel/string/deco.inl>
#include <ork/kernel/mutex.h>
#include <ork/file/file.h>
#include <thread>
#include <functional>

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
    Logger* _logger = nullptr; // Backpointer to logger for this channel
    
    mutable svar64_t _backend_impl;
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
		using channel_map_t = std::map<std::string,logchannel_ptr_t>;
		ork::LockedResource<channel_map_t> _channels;
    logchannel_ptr_t _default_channel;
    logchannel_ptr_t _stderr_channel;
    stderr_redirector_ptr_t _stderr_redirector;
    logger_backend_ptr_t _backend;
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
}
