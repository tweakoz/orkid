////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/util/logger.h>

// Forward declaration for NotCurses installation

namespace ork {

  void installNotCursesToBackend(LoggerBackend* backend);

void pyinit_logger(py::module& module_core) {
  auto type_codec = python::pb11_typecodec_t::instance();

  /////////////////////////////////////////////////////////////////////////////////
  // Logger Backend
  /////////////////////////////////////////////////////////////////////////////////
  auto logger_backend_type = py::class_<LoggerBackend, logger_backend_ptr_t>(module_core, "LoggerBackend")
    .def("__repr__", [](logger_backend_ptr_t backend) -> std::string {
      return FormatString("LoggerBackend(%p)", (void*)backend.get());
    });
  type_codec->registerStdCodec<logger_backend_ptr_t>(logger_backend_type);

  /////////////////////////////////////////////////////////////////////////////////
  // LogChannel
  /////////////////////////////////////////////////////////////////////////////////
  auto logchannel_type = py::class_<LogChannel, logchannel_ptr_t>(module_core, "LogChannel")
    .def_property("enabled", 
      [](logchannel_ptr_t chan) -> bool { return chan->_enabled; },
      [](logchannel_ptr_t chan, bool enabled) { chan->_enabled = enabled; })
    .def_property("color", 
      [](logchannel_ptr_t chan) -> ork::fvec3 { return chan->_color; },
      [](logchannel_ptr_t chan, const ork::fvec3& color) { 
        chan->_color = color; 
        chan->_c1_prefix = ork::deco::asciic_rgb(color);
      })
    .def_property_readonly("name", 
      [](logchannel_ptr_t chan) -> std::string { return chan->_name; })
    .def("log", 
      [](logchannel_ptr_t chan, const std::string& msg) {
        chan->log("%s", msg.c_str());
      })
    .def("log_begin", 
      [](logchannel_ptr_t chan, const std::string& msg) {
        chan->log_begin("%s", msg.c_str());
      })
    .def("log_continue", 
      [](logchannel_ptr_t chan, const std::string& msg) {
        chan->log_continue("%s", msg.c_str());
      })
    .def("status", 
      [](logchannel_ptr_t chan, const std::string& subchannel, const std::string& msg) {
        chan->status(subchannel, "%s", msg.c_str());
      })
    .def("__repr__", [](logchannel_ptr_t chan) -> std::string {
      return FormatString("LogChannel(%s, enabled=%s)", 
        chan->_name.c_str(), chan->_enabled ? "true" : "false");
    });
  type_codec->registerStdCodec<logchannel_ptr_t>(logchannel_type);

  /////////////////////////////////////////////////////////////////////////////////
  // Logger
  /////////////////////////////////////////////////////////////////////////////////
  auto logger_type = py::class_<Logger, logger_ptr_t>(module_core, "Logger")
    .def_static("instance", &logger, py::return_value_policy::reference)
    .def("configureChannel", 
      [](logger_ptr_t logger, const std::string& name, const ork::fvec3& color, bool enabled) -> logchannel_ptr_t {
        return logger->configureChannel(name, color, enabled);
      },
      py::arg("name"), py::arg("color"), py::arg("enabled") = true)
    .def("getChannel", 
      [](logger_ptr_t logger, const std::string& name) -> logchannel_ptr_t {
        return logger->getChannel(name);
      })
    .def_property_readonly("defaultChannel", 
      [](logger_ptr_t logger) -> logchannel_ptr_t {
        return logger->defaultChannel();
      })
    .def("enableNotCurses", 
      [](logger_ptr_t logger) {
        installNotCursesToBackend(logger->_backend.get());
      })
    .def("__repr__", [](logger_ptr_t logger) -> std::string {
      return FormatString("Logger(%p)", (void*)logger.get());
    });
  type_codec->registerStdCodec<logger_ptr_t>(logger_type);

  /////////////////////////////////////////////////////////////////////////////////
  // Module-level functions
  /////////////////////////////////////////////////////////////////////////////////
  module_core.def("logger", &logger, py::return_value_policy::reference);
  module_core.def("disableLogging", []() { 
    extern bool _ENABLE_LOGGING;
    _ENABLE_LOGGING = false; 
  });
  
  /////////////////////////////////////////////////////////////////////////////////
  // Logger utilities
  /////////////////////////////////////////////////////////////////////////////////
  module_core.def("setupNotCursesCleanup", []() {
    // Python-callable function to set up signal handlers for NotCurses cleanup
    // This can be called from Python scripts to ensure proper cleanup
    static bool setup_done = false;
    if (!setup_done) {
      setup_done = true;
      // Additional Python-specific cleanup setup can go here
    }
  });
}

} // namespace ork 