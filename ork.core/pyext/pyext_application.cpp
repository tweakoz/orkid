////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/application/application.h>
#include <ork/application/subsystem.h>
#include <ork/util/logger.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork {

void pyinit_application(py::module& module_core) {

  auto type_codec = python::pb11_typecodec_t::instance();

  ///////////////////////////////////////////////////////////////
  // Subsystem - Base class for all subsystems
  ///////////////////////////////////////////////////////////////

  auto sub_t = py::class_<Subsystem, subsystem_ptr_t>(module_core, "Subsystem")
      .def(py::init<const std::string&>(), py::arg("name"))

      // Methods
      .def("initialize", &Subsystem::initialize, "Initialize the subsystem")
      .def("shutdown", &Subsystem::shutdown, "Shutdown the subsystem")
      .def("update", &Subsystem::update, "Update the subsystem FSM")

      // Accessors
      .def("currentState", &Subsystem::currentState, "Get current FSM state")
      .def_property_readonly("name", [](const Subsystem& self) { return self._name; })
      .def_property_readonly("nameHash", [](const Subsystem& self) { return self._name_hash; })

      // Dependency management
      .def("addDependency", &Subsystem::addDependency, py::arg("dep"), "Add a dependency")
      .def("removeDependency", &Subsystem::removeDependency, py::arg("token"), "Remove a dependency by name/token")
      .def("hasDependency", &Subsystem::hasDependency, py::arg("token"), "Check if has dependency by name/token")

      // Public members for configuration
      .def_readwrite("impl", &Subsystem::_impl, "Implementation storage (pimpl)")
      .def_readwrite("vars", &Subsystem::_vars, "Variable map for properties")

      // FSM access
      .def_readwrite("instance", &Subsystem::_instance, "FSM instance")
      .def_readwrite("data", &Subsystem::_data, "FSM data")

      // FSM states (read-only)
      .def_readonly("state_uninitialized", &Subsystem::_state_uninitialized)
      .def_readonly("state_initializing", &Subsystem::_state_initializing)
      .def_readonly("state_ready", &Subsystem::_state_ready)
      .def_readonly("state_error", &Subsystem::_state_error)
      .def_readonly("state_shutting_down", &Subsystem::_state_shutting_down)
      .def_readonly("state_terminated", &Subsystem::_state_terminated);

  type_codec->registerStdCodec<subsystem_ptr_t>(sub_t);

  ///////////////////////////////////////////////////////////////
  // Application - Base application class with HFSM lifecycle
  ///////////////////////////////////////////////////////////////

  auto app_t = py::class_<Application, application_ptr_t>(module_core, "Application")
      .def_static(
          "create",
          [](py::kwargs kwargs) -> application_ptr_t {
            auto appinit = appinitdata(); // Use the singleton

            // Process kwargs to configure AppInitData before Application creation
            if (kwargs) {
              for (auto item : kwargs) {
                auto key = py::cast<std::string>(item.first);
                if (key == "std_asset_catalog") {
                  appinit->_std_asset_catalog = py::cast<bool>(item.second);
                }
                // Future options can be added here:
                // else if (key == "enable_audio") { ... }
                // else if (key == "enable_graphics") { ... }
              }
            }

            return Application::create();
          },
          "Factory method to create Application instance.\n"
          "Accepts kwargs:\n"
          "  std_asset_catalog (bool): Enable/disable asset catalog subsystem (default: True)")

      // Subsystem management
      .def("registerSubsystem",
           static_cast<void (Application::*)(subsystem_ptr_t, bool)>(&Application::registerSubsystem),
           py::arg("subsystem"),
           py::arg("is_static") = false,
           "Register a subsystem (dependencies must be set in subsystem._dependencies)")

      .def("registerSubsystem",
           static_cast<void (Application::*)(const std::string&, subsystem_ptr_t, bool)>(&Application::registerSubsystem),
           py::arg("name"),
           py::arg("subsystem"),
           py::arg("is_static") = false,
           "Register a subsystem by name")

      .def("unregisterSubsystem",
           static_cast<void (Application::*)(uint64_t)>(&Application::unregisterSubsystem),
           py::arg("name_hash"),
           "Unregister a subsystem by hash")

      .def("unregisterSubsystem",
           static_cast<void (Application::*)(const std::string&)>(&Application::unregisterSubsystem),
           py::arg("name"),
           "Unregister a subsystem by name")

      .def("getSubsystem",
           static_cast<subsystem_ptr_t (Application::*)(uint64_t) const>(&Application::getSubsystem),
           py::arg("name_hash"),
           "Get subsystem by hash")

      .def("getSubsystem",
           static_cast<subsystem_ptr_t (Application::*)(const std::string&) const>(&Application::getSubsystem),
           py::arg("name"),
           "Get subsystem by name")

      // Operation queues
      .def_readonly("mainq", &Application::_mainq, "Main/GPU thread operation queue")
      .def_readonly("updq", &Application::_updq, "UPDATE thread operation queue")
      .def_readonly("conq", &Application::_conq, "AUDIO thread operation queue")

      // String pool
      .def_readonly("stringpoolctx", &Application::_stringpoolctx, "String pool context");

     type_codec->registerStdCodec<application_ptr_t>(app_t);
}

} // namespace ork
