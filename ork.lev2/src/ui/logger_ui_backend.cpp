////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/ui/logger_ui_backend.h>
#include <ork/lev2/ui/logger_group.h>

namespace ork::ui {

///////////////////////////////////////////////////////////////////////////////

logger_backend_ptr_t LoggerUIBackend::create() {
  auto backend = std::make_shared<LoggerBackend>();
  auto impl = std::make_shared<LoggerUIBackend>();

  backend->_impl.set<loggeruibackend_ptr_t>(impl);
  backend->_add_log_line = add_log_line;
  backend->_begin_log_line = begin_log_line;
  backend->_continue_log_line = continue_log_line;
  backend->_end_log_line = end_log_line;
  backend->_warn = warn;
  backend->_error = error;
  backend->_status = status;
  backend->_on_perf_item = on_perf_item;

  return backend;
}

///////////////////////////////////////////////////////////////////////////////

void LoggerUIBackend::registerGroup(loggergroup_ptr_t group) {
  std::lock_guard<std::mutex> lock(_groups_mutex);
  _registered_groups.push_back(group);
}

///////////////////////////////////////////////////////////////////////////////

void LoggerUIBackend::unregisterGroup(loggergroup_ptr_t group) {
  std::lock_guard<std::mutex> lock(_groups_mutex);
  _registered_groups.erase(
    std::remove(_registered_groups.begin(), _registered_groups.end(), group),
    _registered_groups.end()
  );
}

///////////////////////////////////////////////////////////////////////////////

void LoggerUIBackend::_broadcast(std::function<void(loggergroup_ptr_t)> fn) {
  std::lock_guard<std::mutex> lock(_groups_mutex);

  for (auto& group : _registered_groups) {
    fn(group);
  }
}

///////////////////////////////////////////////////////////////////////////////

void LoggerUIBackend::add_log_line(const ork::LogChannel* channel, const std::string& msg) {
  if (!channel || !channel->_logger || !channel->_logger->_backend)
    return;

  auto impl_var = channel->_logger->_backend->_impl;
  if (impl_var.isA<loggeruibackend_ptr_t>()) {
    auto impl = impl_var.get<loggeruibackend_ptr_t>();
    impl->_broadcast([&](loggergroup_ptr_t group) {
      group->onLogMessage(channel->_name, msg);
    });
  }
}

///////////////////////////////////////////////////////////////////////////////

void LoggerUIBackend::begin_log_line(const ork::LogChannel* channel, const std::string& msg) {
  // For now, treat same as add_log_line
  add_log_line(channel, msg);
}

///////////////////////////////////////////////////////////////////////////////

void LoggerUIBackend::continue_log_line(const ork::LogChannel* channel, const std::string& msg) {
  // For now, treat same as add_log_line
  add_log_line(channel, msg);
}

///////////////////////////////////////////////////////////////////////////////

void LoggerUIBackend::end_log_line(const ork::LogChannel* channel, const std::string& msg) {
  // For now, treat same as add_log_line
  add_log_line(channel, msg);
}

///////////////////////////////////////////////////////////////////////////////

void LoggerUIBackend::warn(const ork::LogChannel* channel, const std::string& msg) {
  if (!channel || !channel->_logger || !channel->_logger->_backend)
    return;

  auto impl_var = channel->_logger->_backend->_impl;
  if (impl_var.isA<loggeruibackend_ptr_t>()) {
    auto impl = impl_var.get<loggeruibackend_ptr_t>();
    impl->_broadcast([&](loggergroup_ptr_t group) {
      group->onLogMessage(channel->_name, "[WARN] " + msg);
    });
  }
}

///////////////////////////////////////////////////////////////////////////////

void LoggerUIBackend::error(const ork::LogChannel* channel, const std::string& msg) {
  if (!channel || !channel->_logger || !channel->_logger->_backend)
    return;

  auto impl_var = channel->_logger->_backend->_impl;
  if (impl_var.isA<loggeruibackend_ptr_t>()) {
    auto impl = impl_var.get<loggeruibackend_ptr_t>();
    impl->_broadcast([&](loggergroup_ptr_t group) {
      group->onLogMessage(channel->_name, "[ERROR] " + msg);
    });
  }
}

///////////////////////////////////////////////////////////////////////////////

void LoggerUIBackend::status(const ork::LogChannel* channel, std::string subchan, const std::string& msg) {
  if (!channel || !channel->_logger || !channel->_logger->_backend)
    return;

  auto impl_var = channel->_logger->_backend->_impl;
  if (impl_var.isA<loggeruibackend_ptr_t>()) {
    auto impl = impl_var.get<loggeruibackend_ptr_t>();
    impl->_broadcast([&](loggergroup_ptr_t group) {
      group->onStatus(channel->_name, subchan, msg);
    });
  }
}

///////////////////////////////////////////////////////////////////////////////

void LoggerUIBackend::on_perf_item(const ork::LogChannel* channel, std::string name, svar64_t& value) {
  if (!channel || !channel->_logger || !channel->_logger->_backend)
    return;

  auto impl_var = channel->_logger->_backend->_impl;
  if (impl_var.isA<loggeruibackend_ptr_t>()) {
    auto impl = impl_var.get<loggeruibackend_ptr_t>();
    impl->_broadcast([&](loggergroup_ptr_t group) {
      group->onPerfItem(channel->_name, name, value);
    });
  }
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::ui
