////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/util/fsm.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork {
using namespace fsm;
///////////////////////////////////////////////////////////////////////////////

void pyinit_fsm(py::module& module_core) {
  auto fsmmodule  = module_core.def_submodule("fsm", "Hierarchical Finite State Machine");
  auto type_codec = python::pb11_typecodec_t::instance();

  /////////////////////////////////////////////////////////////////////////////
  // State base class
  /////////////////////////////////////////////////////////////////////////////
  auto state_type = py::class_<State, state_ptr_t>(fsmmodule, "State")
      .def_readonly("name", &State::_name)
      .def_property_readonly(
          "parent",
          [](state_ptr_t s) -> state_ptr_t { return s->_parent; })
      .def_property_readonly(
          "index",
          [](state_ptr_t s) -> size_t { return s->index(); })
      .def("__repr__", [](state_ptr_t s) -> std::string {
        return FormatString("fsm.State(%s:%p)", s->_name.c_str(), (void*)s.get());
      });
  type_codec->registerStdCodec<state_ptr_t>(state_type);

  /////////////////////////////////////////////////////////////////////////////
  // LambdaState - states with Python callbacks
  // Callbacks receive fsminstance_ptr_t
  /////////////////////////////////////////////////////////////////////////////
  auto lambdastate_type = py::class_<LambdaState, State, lambdastate_ptr_t>(fsmmodule, "LambdaState")
      .def_property(
          "onEnter",
          [](lambdastate_ptr_t s) -> py::object {
            return py::none();
          },
          [](lambdastate_ptr_t s, py::object pyfn) {
            s->_onenter = [pyfn](fsminstance_ptr_t inst) {
              py::gil_scoped_acquire acquire;
              pyfn(inst);
            };
          })
      .def_property(
          "onExit",
          [](lambdastate_ptr_t s) -> py::object {
            return py::none();
          },
          [](lambdastate_ptr_t s, py::object pyfn) {
            s->_onexit = [pyfn](fsminstance_ptr_t inst) {
              py::gil_scoped_acquire acquire;
              pyfn(inst);
            };
          })
      .def_property(
          "onUpdate",
          [](lambdastate_ptr_t s) -> py::object {
            return py::none();
          },
          [](lambdastate_ptr_t s, py::object pyfn) {
            s->_onupdate = [pyfn](fsminstance_ptr_t inst) {
              py::gil_scoped_acquire acquire;
              pyfn(inst);
            };
          })
      .def("__repr__", [](lambdastate_ptr_t s) -> std::string {
        return FormatString("fsm.LambdaState(%s:%p)", s->_name.c_str(), (void*)s.get());
      });
  type_codec->registerStdCodec<lambdastate_ptr_t>(lambdastate_type);

  /////////////////////////////////////////////////////////////////////////////
  // FsmData - shared state machine definition
  /////////////////////////////////////////////////////////////////////////////
  auto fsmdata_type = py::class_<FsmData, fsmdata_ptr_t>(fsmmodule, "FsmData")
      .def(py::init([]() -> fsmdata_ptr_t {
        return std::make_shared<FsmData>();
      }))
      .def(
          "createState",
          [](fsmdata_ptr_t data, state_ptr_t parent, std::string name) -> lambdastate_ptr_t {
            return data->newState<LambdaState>(parent, name);
          },
          py::arg("parent") = nullptr,
          py::arg("name") = "")
      .def(
          "addTransition",
          [](fsmdata_ptr_t data, state_ptr_t from, fsm_event_t event, state_ptr_t to) {
            data->addTransition(from, event, to);
          },
          py::arg("from_state"),
          py::arg("event"),
          py::arg("to_state"))
      .def(
          "addTransition",
          [](fsmdata_ptr_t data, state_ptr_t from, std::string event_name, state_ptr_t to) {
            data->addTransition(from, event_name, to);
          },
          py::arg("from_state"),
          py::arg("event_name"),
          py::arg("to_state"))
      .def(
          "addPredicatedTransition",
          [](fsmdata_ptr_t data, state_ptr_t from, std::string event_name, state_ptr_t to, py::object predicate) {
            predicate_callback_t pred = [predicate](fsminstance_ptr_t inst) -> bool {
              py::gil_scoped_acquire acquire;
              return py::cast<bool>(predicate(inst));
            };
            PredicatedTransition pt(to, pred);
            data->addTransition(from, event_name, pt);
          },
          py::arg("from_state"),
          py::arg("event_name"),
          py::arg("to_state"),
          py::arg("predicate"))
      .def(
          "findState",
          [](fsmdata_ptr_t data, std::string name) -> state_ptr_t {
            return data->findState(name);
          },
          py::arg("name"))
      .def_property_readonly(
          "states",
          [](fsmdata_ptr_t data) -> py::list {
            py::list result;
            for (auto& s : data->states()) {
              result.append(s);
            }
            return result;
          })
      .def(
          "generateDot",
          [](fsmdata_ptr_t data,
             std::string name,
             float K,
             float sep,
             float size_w,
             float size_h,
             int dpi,
             bool splines,
             bool overlap) -> std::string {
            DotConfig config;
            config.graph_name = name;
            config.K = K;
            config.sep = sep;
            config.size_w = size_w;
            config.size_h = size_h;
            config.dpi = dpi;
            config.splines = splines;
            config.overlap = overlap;
            return data->generateDot(config);
          },
          py::arg("graph_name") = "",
          py::arg("K") = 2.0f,
          py::arg("sep") = 25.0f,
          py::arg("size_w") = 12.0f,
          py::arg("size_h") = 9.0f,
          py::arg("dpi") = 100,
          py::arg("splines") = true,
          py::arg("overlap") = false,
          "Generate DOT graph representation (without current state highlight)")
      .def("__repr__", [](fsmdata_ptr_t data) -> std::string {
        return FormatString("fsm.FsmData(states=%zu)", data->states().size());
      });
  type_codec->registerStdCodec<fsmdata_ptr_t>(fsmdata_type);

  /////////////////////////////////////////////////////////////////////////////
  // FsmGroup - coordinate multiple instances
  /////////////////////////////////////////////////////////////////////////////
  auto fsmgroup_type = py::class_<FsmGroup, fsmgroup_ptr_t>(fsmmodule, "FsmGroup")
      .def(py::init([]() -> fsmgroup_ptr_t {
        return std::make_shared<FsmGroup>();
      }))
      .def(
          "createInstance",
          [](fsmgroup_ptr_t grp, fsmdata_ptr_t data) -> fsminstance_ptr_t {
            return FsmGroup::createInstance(grp, data);
          },
          py::arg("data"))
      .def(
          "broadcastEvent",
          [](fsmgroup_ptr_t grp, fsm_event_t event) {
            grp->broadcastEvent(event);
          },
          py::arg("event"))
      .def(
          "broadcastEvent",
          [](fsmgroup_ptr_t grp, std::string event_name) {
            grp->broadcastEvent(event_name);
          },
          py::arg("event_name"))
      .def(
          "allInState",
          [](fsmgroup_ptr_t grp, state_ptr_t state) -> bool {
            return grp->allInState(state);
          },
          py::arg("state"))
      .def(
          "allInState",
          [](fsmgroup_ptr_t grp, std::string state_name) -> bool {
            return grp->allInState(state_name);
          },
          py::arg("state_name"))
      .def(
          "waitForAllInState",
          [](fsmgroup_ptr_t grp, state_ptr_t state) {
            py::gil_scoped_release release;
            grp->waitForAllInState(state);
          },
          py::arg("state"))
      .def(
          "waitForAllInState",
          [](fsmgroup_ptr_t grp, std::string state_name) {
            py::gil_scoped_release release;
            grp->waitForAllInState(state_name);
          },
          py::arg("state_name"))
      .def_property_readonly(
          "count",
          [](fsmgroup_ptr_t grp) -> size_t {
            return grp->count();
          })
      .def("__repr__", [](fsmgroup_ptr_t grp) -> std::string {
        return FormatString("fsm.FsmGroup(count=%zu)", grp->count());
      });
  type_codec->registerStdCodec<fsmgroup_ptr_t>(fsmgroup_type);

  /////////////////////////////////////////////////////////////////////////////
  // FsmInstance - per-entity runtime state
  /////////////////////////////////////////////////////////////////////////////
  auto fsminstance_type = py::class_<FsmInstance, fsminstance_ptr_t>(fsmmodule, "FsmInstance")
      .def(py::init([](fsmdata_ptr_t data) -> fsminstance_ptr_t {
        return FsmInstance::create(data);
      }), py::arg("data"))
      .def_static(
          "create",
          [](fsmdata_ptr_t data) -> fsminstance_ptr_t {
            return FsmInstance::create(data);
          },
          py::arg("data"))
      .def(
          "changeState",
          [](fsminstance_ptr_t self, state_ptr_t target) {
            self->changeState(target);
          },
          py::arg("target"))
      .def(
          "sendEvent",
          [](fsminstance_ptr_t self, fsm_event_t event) {
            self->sendEvent(event);
          },
          py::arg("event"))
      .def(
          "sendEvent",
          [](fsminstance_ptr_t self, std::string event_name) {
            self->sendEvent(event_name);
          },
          py::arg("event_name"))
      .def(
          "update",
          [](fsminstance_ptr_t self) {
            FsmInstance::update(self);
          })
      .def_property_readonly(
          "currentState",
          [](fsminstance_ptr_t inst) -> state_ptr_t {
            return inst->currentState();
          })
      .def_property_readonly(
          "data",
          [](fsminstance_ptr_t inst) -> fsmdata_ptr_t {
            return inst->data();
          })
      .def_property_readonly(
          "vars",
          [](fsminstance_ptr_t inst) -> varmap::varmap_ptr_t {
            return inst->vars();
          })
      .def_property(
          "userdata",
          [type_codec](fsminstance_ptr_t inst) -> py::object {
            return type_codec->encode(inst->_userdata);  // varval_t -> py::object
          },
          [type_codec](fsminstance_ptr_t inst, py::object value) {
            inst->_userdata = type_codec->decode(value);  // py::object -> varval_t
          },
          "User data (arbitrary Python object)")
      .def(
          "generateDot",
          [](fsminstance_ptr_t inst,
             std::string name,
             float K,
             float sep,
             float size_w,
             float size_h,
             int dpi,
             bool splines,
             bool overlap) -> std::string {
            DotConfig config;
            config.graph_name = name;
            config.K = K;
            config.sep = sep;
            config.size_w = size_w;
            config.size_h = size_h;
            config.dpi = dpi;
            config.splines = splines;
            config.overlap = overlap;
            return inst->generateDot(config);
          },
          py::arg("graph_name") = "",
          py::arg("K") = 2.0f,
          py::arg("sep") = 25.0f,
          py::arg("size_w") = 12.0f,
          py::arg("size_h") = 9.0f,
          py::arg("dpi") = 100,
          py::arg("splines") = true,
          py::arg("overlap") = false,
          "Generate DOT graph with current state highlighted")
      .def("__repr__", [](fsminstance_ptr_t inst) -> std::string {
        auto cur = inst->currentState();
        std::string cur_name = cur ? cur->_name : "null";
        return FormatString("fsm.FsmInstance(current=%s)", cur_name.c_str());
      });
  type_codec->registerStdCodec<fsminstance_ptr_t>(fsminstance_type);

  /////////////////////////////////////////////////////////////////////////////
  // Utility function to create event tokens from strings
  /////////////////////////////////////////////////////////////////////////////
  fsmmodule.def(
      "event",
      [](std::string event_name) -> fsm_event_t {
        return CrcString(event_name.c_str()).hashed();
      },
      py::arg("event_name"),
      "Create an event token from a string (CRC64 hash)");
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork
///////////////////////////////////////////////////////////////////////////////
