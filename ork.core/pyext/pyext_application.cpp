////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/application/application.h>
#include <ork/application/subsystem_fsm.h>
#include <ork/util/logger.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork {

void pyinit_application(py::module& module_core) {

  auto type_codec = python::pb11_typecodec_t::instance();

  ///////////////////////////////////////////////////////////////
  // SubsystemFsm - Base class for all subsystems
  ///////////////////////////////////////////////////////////////

  auto sub_t = py::class_<SubsystemFsm, subsystemfsm_ptr_t>(module_core, "SubsystemFsm")
      .def(py::init<const std::string&>(), py::arg("name"))

      // Methods
      .def("initialize", &SubsystemFsm::initialize, "Initialize the subsystem")
      .def("shutdown", &SubsystemFsm::shutdown, "Shutdown the subsystem")
      .def("update", &SubsystemFsm::update, "Update the subsystem FSM")

      // Accessors
      .def("currentState", &SubsystemFsm::currentState, "Get current FSM state")
      .def_property_readonly("name", [](const SubsystemFsm& self) { return self._name; })
      .def_property_readonly("nameHash", [](const SubsystemFsm& self) { return self._name_hash; })

      // Dependency management
      .def("addDependency", &SubsystemFsm::addDependency, py::arg("dep"), "Add a dependency")
      .def("removeDependency", &SubsystemFsm::removeDependency, py::arg("token"), "Remove a dependency by name/token")
      .def("hasDependency", &SubsystemFsm::hasDependency, py::arg("token"), "Check if has dependency by name/token")

      // Public members for configuration
      .def_readwrite("impl", &SubsystemFsm::_impl, "Implementation storage (pimpl)")
      .def_readwrite("vars", &SubsystemFsm::_vars, "Variable map for properties")

      // FSM access
      .def_readwrite("instance", &SubsystemFsm::_instance, "FSM instance")
      .def_readwrite("data", &SubsystemFsm::_data, "FSM data")

      // FSM states (read-only)
      .def_readonly("state_uninitialized", &SubsystemFsm::_state_uninitialized)
      .def_readonly("state_initializing", &SubsystemFsm::_state_initializing)
      .def_readonly("state_ready", &SubsystemFsm::_state_ready)
      .def_readonly("state_error", &SubsystemFsm::_state_error)
      .def_readonly("state_shutting_down", &SubsystemFsm::_state_shutting_down)
      .def_readonly("state_terminated", &SubsystemFsm::_state_terminated);

  type_codec->registerStdCodec<subsystemfsm_ptr_t>(sub_t);

  ///////////////////////////////////////////////////////////////
  // Application - Base application class with HFSM lifecycle
  ///////////////////////////////////////////////////////////////

  auto app_t = py::class_<Application, application_ptr_t>(module_core, "Application")
      .def_static("create", &Application::create, "Factory method to create Application instance")

      // Subsystem management
      .def("registerSubsystem",
           static_cast<void (Application::*)(subsystemfsm_ptr_t, bool)>(&Application::registerSubsystem),
           py::arg("subsystem"),
           py::arg("is_static") = false,
           "Register a subsystem (dependencies must be set in subsystem._dependencies)")

      .def("registerSubsystem",
           static_cast<void (Application::*)(const std::string&, subsystemfsm_ptr_t, bool)>(&Application::registerSubsystem),
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
           static_cast<subsystemfsm_ptr_t (Application::*)(uint64_t) const>(&Application::getSubsystem),
           py::arg("name_hash"),
           "Get subsystem by hash")

      .def("getSubsystem",
           static_cast<subsystemfsm_ptr_t (Application::*)(const std::string&) const>(&Application::getSubsystem),
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
