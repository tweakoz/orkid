////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/util/logger.h>
#include <ork/kernel/environment.h>
#include <ork/kernel/timer.h>
#include <cstdio>
#include <unordered_set>

///////////////////////////////////////////////////////////////////////////////

std::unordered_set<std::string> get_args_set();

///////////////////////////////////////////////////////////////////////////////
namespace ork {
///////////////////////////////////////////////////////////////////////////////

bool _ENABLE_LOGGING = true;

bool _ENABLE_NOTCURSES() {
  static auto arg_set = get_args_set();
  std::string envvar;
  if (genviron.get("ORKID_LOG_NOTCURSES", envvar)) {
    if (envvar == "1" || envvar == "true" || envvar == "yes") {
      return true;
    }
  }
  if (arg_set.find("--newlogger") != arg_set.end()) {
    return true;
  }
  return false;
};

void installNotCursesToBackend(LoggerBackend* backend);

///////////////////////////////////////////////////////////////////////////////

LogChannel::LogChannel(Logger* logger, std::string named, ork::fvec3 color, bool enabled) {
  _logger    = logger;
  _enabled   = enabled;
  _color     = color;
  _name      = named;
  _c1_prefix = ork::deco::asciic_rgb(color);
  _reset     = ork::deco::asciic_reset();

  auto envvar = FormatString("ORKID_LOGFILE_%s", named.c_str());
  if (genviron.has(envvar)) {
    std::string logfilename;
    genviron.get(envvar, logfilename);
    _file = std::make_shared<File>(logfilename.c_str(), EFM_WRITE);
    printf("logfilename<%s>\n", logfilename.c_str());
    enabled = true;
  }
}

///////////////////////////////////////////////////////////////////////////////

void LogChannel::log_valist(const char* pMsgFormat, va_list args) const {
  char buf[1024];
  vsnprintf(buf, sizeof(buf), pMsgFormat, args);
  _logger->_backend->_add_log_line(this, buf);
}

///////////////////////////////////////////////////////////////////////////////

void LogChannel::log(const char* pMsgFormat, ...) {
  if (_ENABLE_LOGGING and _enabled) {
    va_list args;
    va_start(args, pMsgFormat);
    log_valist(pMsgFormat, args);
    va_end(args);
  }
}

///////////////////////////////////////////////////////////////////////////////

void LogChannel::log_begin_valist(const char* pMsgFormat, va_list args) const {
  char buf[1024];
  vsnprintf(buf, sizeof(buf), pMsgFormat, args);
  _logger->_backend->_begin_log_line(this, buf);
}

///////////////////////////////////////////////////////////////////////////////

void LogChannel::log_begin(const char* pMsgFormat, ...) {
  if (_ENABLE_LOGGING and _enabled) {
    va_list args;
    va_start(args, pMsgFormat);
    log_begin_valist(pMsgFormat, args);
    va_end(args);
  }
}

///////////////////////////////////////////////////////////////////////////////

void LogChannel::log_continue_valist(const char* pMsgFormat, va_list args) const {
  char buf[1024];
  vsnprintf(buf, sizeof(buf), pMsgFormat, args);
  _logger->_backend->_continue_log_line(this, buf);
}

///////////////////////////////////////////////////////////////////////////////

void LogChannel::log_continue(const char* pMsgFormat, ...) const {
  if (_ENABLE_LOGGING and _enabled) {
    va_list args;
    va_start(args, pMsgFormat);
    log_continue_valist(pMsgFormat, args);
    va_end(args);
  }
}

///////////////////////////////////////////////////////////////////////////////

void LogChannel::status_valist(const std::string& subchannel, const char* pMsgFormat, va_list args) {
  if (!_ENABLE_LOGGING || !_enabled)
    return;
  char buf[1024];
  vsnprintf(buf, sizeof(buf), pMsgFormat, args);
  _logger->_backend->_status(this, subchannel, buf);
}

///////////////////////////////////////////////////////////////////////////////

void LogChannel::perfItem(const std::string& subchannel, svar64_t dd) {
  _logger->_backend->_on_perf_item(this, subchannel, dd);
}

///////////////////////////////////////////////////////////////////////////////

void LogChannel::status(const std::string& subchannel, const char* pMsgFormat, ...) {
  va_list args;
  va_start(args, pMsgFormat);
  status_valist(subchannel, pMsgFormat, args);
  va_end(args);
}

///////////////////////////////////////////////////////////////////////////////

logchannel_ptr_t Logger::configureChannel(std::string named, ork::fvec3 color, bool enabled) {
  logchannel_ptr_t channel;
  _channels.atomicOp([named, color, enabled, &channel, this](channel_map_t& unlocked) { //
    auto it = unlocked.find(named);
    if (it == unlocked.end()) {
      channel         = std::make_shared<LogChannel>(this, named, color, enabled);
      unlocked[named] = channel;

      channel->_status_interval = _ENABLE_NOTCURSES() ? 1.0f : 8.0f;

    } else {
      channel             = it->second;
      channel->_enabled   = enabled; // Update existing channel's enabled state
      channel->_color     = color;   // Update existing channel's color
      channel->_c1_prefix = ork::deco::asciic_rgb(color);
    }
  });
  return channel;
}

///////////////////////////////////////////////////////////////////////////////

logchannel_ptr_t Logger::getChannel(std::string named) {
  logchannel_ptr_t channel;
  _channels.atomicOp([named, &channel, this](channel_map_t& unlocked) {
    auto it = unlocked.find(named);
    if (it != unlocked.end()) {
      channel = it->second;
    } else {
      channel         = std::make_shared<LogChannel>(this, named, fvec3(1.0f, 1.0f, 1.0f), false);
      unlocked[named] = channel; // Create a default channel if not found
    }
  });
  return channel;
}

///////////////////////////////////////////////////////////////////////////////

logger_ptr_t logger() {
  static logger_ptr_t logger = std::make_shared<Logger>();
  return logger;
}

///////////////////////////////////////////////////////////////////////////////

logchannel_ptr_t logerrchannel() {
  logchannel_ptr_t errchan = logger()->getChannel("ERROR");
  if (nullptr == errchan) {
    errchan = logger()->configureChannel("ERROR", fvec3(1, 0, 0));
  }
  return errchan;
}

///////////////////////////////////////////////////////////////////////////////

void default_log_fn(const LogChannel* chan, const std::string& str) {
  printf("%s[%s]\t%s%s\n", chan->_c1_prefix.c_str(), chan->_name.c_str(), str.c_str(),chan->_reset.c_str());
}
void default_status_fn(const LogChannel* chan, std::string subchannel, const std::string& str) {
  printf("%s[%s]\t%s : %s%s\n", chan->_c1_prefix.c_str(), chan->_name.c_str(), subchannel.c_str(), str.c_str(),chan->_reset.c_str());
}

static void default_perfitem(const LogChannel*, std::string subchannel, svar64_t& dd){

}

void nop_log_fn(const LogChannel* chan, const std::string& str) {
}
void nop_status_fn(const LogChannel* chan, std::string subchannel, const std::string& str) {
}
static void nop_perfitem(const LogChannel*, std::string subchannel, svar64_t& dd){

}

///////////////////////////////////////////////////////////////////////////////

Logger::Logger() {
  _backend = std::make_shared<LoggerBackend>();
  {
    std::lock_guard<std::mutex> lock(_backend->_mutex);
    _backend->_add_log_line      = default_log_fn;
    _backend->_begin_log_line    = default_log_fn;
    _backend->_continue_log_line = default_log_fn;
    _backend->_end_log_line      = default_log_fn;
    _backend->_status            = default_status_fn;
    _backend->_on_perf_item      = default_perfitem;
  }
  auto ENV = ork::Environment();
  if (ENV.has("ORKID_LOG_DISABLE")) {
    std::lock_guard<std::mutex> lock(_backend->_mutex);
    _backend->_add_log_line      = nop_log_fn;
    _backend->_begin_log_line    = nop_log_fn;
    _backend->_continue_log_line = nop_log_fn;
    _backend->_end_log_line      = nop_log_fn;
    _backend->_status            = nop_status_fn;
    _backend->_on_perf_item      = nop_perfitem;
  }
  else{
    if (_ENABLE_NOTCURSES()) {
      installNotCursesToBackend(_backend.get());
    }
  }



  _default_channel = configureChannel("DEFAULT", fvec3(1, 1, 1), true);
  _stderr_channel  = configureChannel("STDERR", fvec3(1.0f, 0.25f, 0.0f), true); // Orange color

  // Start stderr redirection
  _stderr_redirector = std::make_shared<StderrRedirector>(_stderr_channel.get());
  _stderr_redirector->start();


}

///////////////////////////////////////////////////////////////////////////////

Logger::~Logger() {
  // Stop stderr redirection
  if (_stderr_redirector) {
    _stderr_redirector->stop();
    _stderr_redirector.reset();
  }
}

///////////////////////////////////////////////////////////////////////////////

logchannel_ptr_t Logger::defaultChannel() const {
  return _default_channel;
}

} // namespace ork
