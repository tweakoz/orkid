////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/util/logger.h>
#include <ork/lev2/ui/ui.h>
#include <mutex>
#include <vector>

namespace ork::ui {

///////////////////////////////////////////////////////////////////////////////

struct LoggerUIBackend {

  static logger_backend_ptr_t create();

  void registerGroup(loggergroup_ptr_t group);
  void unregisterGroup(loggergroup_ptr_t group);

  // Static callbacks for LoggerBackend
  static void add_log_line(const ork::LogChannel* channel, const std::string& msg);
  static void begin_log_line(const ork::LogChannel* channel, const std::string& msg);
  static void continue_log_line(const ork::LogChannel* channel, const std::string& msg);
  static void end_log_line(const ork::LogChannel* channel, const std::string& msg);
  static void warn(const ork::LogChannel* channel, const std::string& msg);
  static void error(const ork::LogChannel* channel, const std::string& msg);
  static void status(const ork::LogChannel* channel, std::string subchan, const std::string& msg);
  static void on_perf_item(const ork::LogChannel* channel, std::string name, svar64_t& value);

private:
  std::mutex _groups_mutex;
  std::vector<loggergroup_ptr_t> _registered_groups;

  void _broadcast(std::function<void(loggergroup_ptr_t)> fn);
};

using loggeruibackend_ptr_t = std::shared_ptr<LoggerUIBackend>;

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::ui
