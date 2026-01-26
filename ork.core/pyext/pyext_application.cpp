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
      .def(py::init<const std::string&,
                    const std::vector<std::string>&,
                    const std::vector<std::string>&,
                    const std::string&>(),
           py::arg("name"),
           py::arg("dependencies") = std::vector<std::string>{},
           py::arg("children") = std::vector<std::string>{},
           py::arg("requires_thread") = std::string{},
           "Create a subsystem with optional dependencies, children, and thread affinity.\n"
           "Dependencies: subsystems that must init before this one (and shutdown after)\n"
           "Children: subsystems that must init before this one (and shutdown before)\n"
           "requires_thread: thread affinity - '', 'main', 'audio', or 'update'")

      // Methods
      .def("initialize", &Subsystem::initialize, "Initialize the subsystem")
      .def("shutdown", &Subsystem::shutdown, "Shutdown the subsystem")
      .def("update", &Subsystem::update, "Update the subsystem FSM")

      // Accessors
      .def("currentState", &Subsystem::currentState, "Get current FSM state")
      .def_property_readonly("name", [](const Subsystem& self) { return self._name; })
      .def_property_readonly("nameHash", [](const Subsystem& self) { return self._name_hash; })

      // Dependency management
      // Dependencies: dep inits first, dep shuts down after (inverse shutdown order)
      .def("addDependency", &Subsystem::addDependency, py::arg("dep"), "Add a dependency (dep inits first, shuts down after)")
      .def("removeDependency", &Subsystem::removeDependency, py::arg("token"), "Remove a dependency by name/token")
      .def("hasDependency", &Subsystem::hasDependency, py::arg("token"), "Check if has dependency by name/token")

      // Child management (for meta-services)
      // Children: child inits first, child shuts down first (same shutdown order as init)
      .def("addChild", &Subsystem::addChild, py::arg("child"), "Add a child (child inits first, shuts down first)")
      .def("removeChild", &Subsystem::removeChild, py::arg("token"), "Remove a child by name/token")
      .def("hasChild", &Subsystem::hasChild, py::arg("token"), "Check if has child by name/token")

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
      .def_readonly("state_terminated", &Subsystem::_state_terminated)

      // Pending relationships (for inspection)
      .def_readonly("_pending_dependencies", &Subsystem::_pending_dependencies)
      .def_readonly("_pending_children", &Subsystem::_pending_children)

      // Thread affinity
      .def_readwrite("requires_thread", &Subsystem::_requires_thread,
                     "Thread affinity: '', 'main', 'audio', or 'update'")

      // Parent relationship (set automatically when added as child)
      .def("hasParent", &Subsystem::hasParent, "Check if this subsystem has a parent")
      .def("parent", &Subsystem::parent, "Get parent subsystem (or None if no parent)")

      // Nested init/shutdown helpers (for meta-services)
      .def("initChildren", &Subsystem::initChildren,
           "Initialize all children in dependency order (called by parent's INITIALIZING callback)")
      .def("shutdownChildren", &Subsystem::shutdownChildren,
           "Shutdown all children in reverse dependency order (called by parent's SHUTTING_DOWN callback)");

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
                if (key == "name") {
                  appinit->_application_name = py::cast<std::string>(item.second);
                } else if (key == "std_asset_catalog") {
                  appinit->_std_asset_catalog = py::cast<bool>(item.second);
                }
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

      // Main thread loop
      .def(
          "mainThreadLoop",
          [](Application& self, py::kwargs kwargs) {
            void_lambda_t on_iter = nullptr;

            if (kwargs && kwargs.contains("on_iter")) {
              py::object py_callback = kwargs["on_iter"];
              if (!py_callback.is_none()) {
                // Wrap Python callback
                on_iter = [py_callback]() {
                  py::gil_scoped_acquire acquire;
                  try {
                    py_callback();
                  } catch (py::error_already_set& e) {
                    // Re-throw Python errors
                    throw;
                  }
                };
              }
            }

            // Release GIL while running the main loop
            py::gil_scoped_release release;
            self.mainThreadLoop(on_iter);
          },
          "Run the main thread loop.\n"
          "Processes queues, updates subsystem FSMs, calls on_iter callback.\n"
          "Blocks until requestExit() is called or SIGINT received.\n"
          "Accepts kwargs:\n"
          "  on_iter (callable): Optional callback called each iteration")

      .def("requestExit", &Application::requestExit,
           "Request clean shutdown (can be called from any thread)")

      .def("shutdown", &Application::shutdown,
           "Explicit shutdown - triggers subsystem teardown in reverse dependency order.\n"
           "Call this before destruction to ensure clean subsystem shutdown.\n"
           "Idempotent - safe to call multiple times.")

      .def("exitRequested", &Application::exitRequested,
           "Check if exit has been requested")

      // String pool
      .def_readonly("stringpoolctx", &Application::_stringpoolctx, "String pool context");

     type_codec->registerStdCodec<application_ptr_t>(app_t);
}

} // namespace ork
