////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/util/logger.h>
#include <ork/kernel/environment.h>

namespace ork {

////////////////////////////////////////////////////////////////
// Stdout backend function implementations (same as default in logger.cpp)
////////////////////////////////////////////////////////////////

static void stdoutLogFn(const LogChannel* chan, const std::string& str) {
  printf("%s[%s]\t%s%s\n", chan->_c1_prefix.c_str(), chan->_name.c_str(), str.c_str(), chan->_reset.c_str());
  static bool check_flush = genviron.has("ORKID_LOG_ALWAYSFLUSH");
  if (check_flush) {
    fflush(stdout);
  }
}

static void stdoutStatusFn(const LogChannel* chan, std::string subchannel, const std::string& str) {
  printf("%s[%s]\t%s : %s%s\n", chan->_c1_prefix.c_str(), chan->_name.c_str(), subchannel.c_str(), str.c_str(), chan->_reset.c_str());
  static bool check_flush = genviron.has("ORKID_LOG_ALWAYSFLUSH");
  if (check_flush) {
    fflush(stdout);
  }
}

static void stdoutWarnFn(const LogChannel* chan, const std::string& str) {
  fprintf(stderr, "\033[38;5;196m[%s:WARN]\t%s\033[0m\n", chan->_name.c_str(), str.c_str());
  fflush(stderr);
}

static void stdoutErrorFn(const LogChannel* chan, const std::string& str) {
  fprintf(stderr, "\033[38;5;196m[%s:ERROR]\t%s\033[0m\n", chan->_name.c_str(), str.c_str());
  fflush(stderr);
}

static void stdoutPerfItemFn(const LogChannel* chan, std::string name, svar64_t& data) {
  // Performance items not displayed in stdout backend by default
}

////////////////////////////////////////////////////////////////
// Factory function
////////////////////////////////////////////////////////////////

logger_backend_ptr_t createStdoutBackend() {
  auto backend = std::make_shared<LoggerBackend>();

  backend->_add_log_line = stdoutLogFn;
  backend->_begin_log_line = stdoutLogFn;
  backend->_continue_log_line = stdoutLogFn;
  backend->_end_log_line = stdoutLogFn;
  backend->_warn = stdoutWarnFn;
  backend->_error = stdoutErrorFn;
  backend->_status = stdoutStatusFn;
  backend->_on_perf_item = stdoutPerfItemFn;

  return backend;
}

////////////////////////////////////////////////////////////////
} // namespace ork
