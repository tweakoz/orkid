////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/lev2/midi/context.h>
#include <ork/lev2/midi/tweakables.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork::lev2 {

static py::object ON_MIDI_PYLAMBDA;

void on_midi_input_message(double deltatime, midi::message_t* message, void* userData){

  midi::message_t copy = *message;
  auto op = [=](){
    py::gil_scoped_acquire acquire;
    ON_MIDI_PYLAMBDA(deltatime,*message);
  };
  opq::mainSerialQueue()->enqueue(op);
}

void pyinit_midi(py::module& module_lev2) {
  auto midi_module   = module_lev2.def_submodule("midi", "MIDI operations");
  auto type_codec = python::pb11_typecodec_t::instance();
  static int unused; // the capsule needs something to reference
  py::capsule cleanup(&unused, [](PyObject *) {
    //std::cout << "Cleanup!" << std::endl;
    ON_MIDI_PYLAMBDA = py::none();
  });
  midi_module.add_object("_cleanup", cleanup);
  /////////////////////////////////////////////////////////////////////////////////
  auto inpctx_type =                                   //
      py::class_<midi::InputContext, midi::inputcontext_ptr_t>(midi_module, "InputContext") //
          .def(py::init<>())
          .def_property_readonly(
              "inputs",                     //
              [](midi::inputcontext_ptr_t ctx) -> std::map<std::string, int> { //
                return ctx->enumerateMidiInputs();
              })
          .def_property_readonly(
              "num_ports",                     //
              [](midi::inputcontext_ptr_t ctx) -> int { //
                return ctx->numPorts();
              })
          .def(
              "portName",                     //
              [](midi::inputcontext_ptr_t ctx, int index) -> std::string { //
                return ctx->portName(index);
              })
          .def(
              "startInputByIndex",                     //
              [](midi::inputcontext_ptr_t ctx, int index, py::object lambda)  { //
                ON_MIDI_PYLAMBDA = lambda;
                ctx->startMidiInputByIndex(index,on_midi_input_message);
              });
  type_codec->registerStdCodec<midi::inputcontext_ptr_t>(inpctx_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto outctx_type =                                   //
      py::class_<midi::OutputContext, midi::outputcontext_ptr_t>(midi_module, "OutputContext") //
          .def(py::init<>())
          .def_property_readonly(
              "outputs",                     //
              [](midi::outputcontext_ptr_t ctx) -> std::map<std::string, int> { //
                return ctx->enumerateMidiOutputs();
              })
          .def_property_readonly(
              "num_ports",                     //
              [](midi::outputcontext_ptr_t ctx) -> int { //
                return ctx->numPorts();
              })
          .def(
              "openPort",                     //
              [](midi::outputcontext_ptr_t ctx, int index)  { //
                ctx->openPort(index);
              })
          .def(
              "portName",                     //
              [](midi::outputcontext_ptr_t ctx, int index) -> std::string { //
                return ctx->portName(index);
              })
          .def(
              "send",                     //
              [](midi::outputcontext_ptr_t ctx, midi::message_t msg)  { //
                ctx->sendMessage(msg);
              });
  type_codec->registerStdCodec<midi::outputcontext_ptr_t>(outctx_type);

  /////////////////////////////////////////////////////////////////////////////////
  // TweakableTransport - base class for MIDI I/O transports
  /////////////////////////////////////////////////////////////////////////////////
  auto transport_type =
      py::class_<midi::TweakableTransport, midi::tweakable_transport_ptr_t>(midi_module, "TweakableTransport");
  type_codec->registerStdCodec<midi::tweakable_transport_ptr_t>(transport_type);

  /////////////////////////////////////////////////////////////////////////////////
  // DirectTransport - for same-process MIDI I/O
  /////////////////////////////////////////////////////////////////////////////////
  auto direct_transport_type =
      py::class_<midi::DirectTransport, midi::TweakableTransport, midi::direct_transport_ptr_t>(midi_module, "DirectTransport")
          .def(py::init<>())
          .def(
              "open",
              [](midi::direct_transport_ptr_t transport, const std::string& device_name) -> bool {
                return transport->open(device_name);
              })
          .def(
              "close",
              [](midi::direct_transport_ptr_t transport) {
                transport->close();
              });
  type_codec->registerStdCodec<midi::direct_transport_ptr_t>(direct_transport_type);

  /////////////////////////////////////////////////////////////////////////////////
  // Tweakable base class
  /////////////////////////////////////////////////////////////////////////////////
  auto tweakable_type =
      py::class_<midi::Tweakable, midi::tweakable_ptr_t>(midi_module, "Tweakable")
          .def_property_readonly("name", [](midi::tweakable_ptr_t twk) -> std::string { return twk->_name; })
          .def_property_readonly("knobID", [](midi::tweakable_ptr_t twk) -> int { return twk->_knobID; })
          .def_property(
              "color",
              [](midi::tweakable_ptr_t twk) -> int { return twk->_switch_color; },
              [](midi::tweakable_ptr_t twk, int color) { twk->updateColor(color); });
  type_codec->registerStdCodec<midi::tweakable_ptr_t>(tweakable_type);

  /////////////////////////////////////////////////////////////////////////////////
  // FloatTweakable
  /////////////////////////////////////////////////////////////////////////////////
  auto f32_tweakable_type =
      py::class_<midi::FloatTweakable, midi::Tweakable, midi::f32tweakable_ptr_t>(midi_module, "FloatTweakable")
          .def_property_readonly("value", [](midi::f32tweakable_ptr_t twk) -> float { return twk->_value; })
          .def_property_readonly("ivalue", [](midi::f32tweakable_ptr_t twk) -> int { return twk->_ivalue; })
          .def_property_readonly("uvalue", [](midi::f32tweakable_ptr_t twk) -> float { return twk->valToUnit(twk->_value); });
  type_codec->registerStdCodec<midi::f32tweakable_ptr_t>(f32_tweakable_type);

  /////////////////////////////////////////////////////////////////////////////////
  // ToggleTweakable
  /////////////////////////////////////////////////////////////////////////////////
  auto toggle_tweakable_type =
      py::class_<midi::ToggleTweakable, midi::Tweakable, midi::toggletweakable_ptr_t>(midi_module, "ToggleTweakable")
          .def_property_readonly("value", [](midi::toggletweakable_ptr_t twk) -> bool { return twk->_value; });
  type_codec->registerStdCodec<midi::toggletweakable_ptr_t>(toggle_tweakable_type);

  /////////////////////////////////////////////////////////////////////////////////
  // EnumTweakable
  /////////////////////////////////////////////////////////////////////////////////
  auto enum_tweakable_type =
      py::class_<midi::EnumTweakable, midi::Tweakable, midi::enumtweakable_ptr_t>(midi_module, "EnumTweakable")
          .def_property_readonly("value", [](midi::enumtweakable_ptr_t twk) -> std::string { return twk->_value; });
  type_codec->registerStdCodec<midi::enumtweakable_ptr_t>(enum_tweakable_type);

  /////////////////////////////////////////////////////////////////////////////////
  // EventTweakable
  /////////////////////////////////////////////////////////////////////////////////
  auto event_tweakable_type =
      py::class_<midi::EventTweakable, midi::Tweakable, midi::eventtweakable_ptr_t>(midi_module, "EventTweakable");
  type_codec->registerStdCodec<midi::eventtweakable_ptr_t>(event_tweakable_type);

  /////////////////////////////////////////////////////////////////////////////////
  // TweakablePropProxy - convenience wrapper for accessing tweakables by name
  /////////////////////////////////////////////////////////////////////////////////
  struct TweakablePropProxy {
    midi::tweakableset_ptr_t _tset;
    TweakablePropProxy(midi::tweakableset_ptr_t tset) : _tset(tset) {}
  };
  using tweakablepropproxy_ptr_t = std::shared_ptr<TweakablePropProxy>;
  auto propproxy_type =
      py::class_<TweakablePropProxy, tweakablepropproxy_ptr_t>(midi_module, "TweakablePropProxy")
          .def("__getattr__", [type_codec](TweakablePropProxy* self, const std::string& name) -> py::object {
            auto it = self->_tset->_tweakable_by_name.find(name);
            if (it != self->_tset->_tweakable_by_name.end()) {
              auto twk = it->second;
              if (twk) {
                return type_codec->encode(twk);
              }
            }
            return py::none();
          })
          .def("contains", [](TweakablePropProxy* self, const std::string& name) -> bool {
            return self->_tset->_tweakable_by_name.find(name) != self->_tset->_tweakable_by_name.end();
          });
  type_codec->registerStdCodec<tweakablepropproxy_ptr_t>(propproxy_type);

  /////////////////////////////////////////////////////////////////////////////////
  // TweakableSet
  /////////////////////////////////////////////////////////////////////////////////
  auto tweakableset_type =
      py::class_<midi::TweakableSet, midi::tweakableset_ptr_t>(midi_module, "TweakableSet")
          .def(py::init([](midi::tweakable_transport_ptr_t transport) {
            return std::make_shared<midi::TweakableSet>(transport);
          }))
          .def(
              "finalize",
              [](midi::tweakableset_ptr_t tset) {
                tset->finalize();
              })
          .def(
              "setPage",
              [](midi::tweakableset_ptr_t tset, int page) {
                tset->setPage(page);
              })
          .def(
              "createFloat",
              [](midi::tweakableset_ptr_t tset, py::kwargs kwargs) -> midi::f32tweakable_ptr_t {
                auto twk = std::make_shared<midi::FloatTweakable>(tset.get());
                if (kwargs) {
                  for (auto item : kwargs) {
                    auto key = py::cast<std::string>(item.first);
                    if (key == "name") {
                      twk->_name = py::cast<std::string>(item.second);
                    } else if (key == "knobID") {
                      twk->_knobID = py::cast<int>(item.second);
                    } else if (key == "color") {
                      twk->_switch_color = py::cast<int>(item.second);
                    } else if (key == "min") {
                      twk->_minval = py::cast<float>(item.second);
                    } else if (key == "max") {
                      twk->_maxval = py::cast<float>(item.second);
                    } else if (key == "default") {
                      twk->_defval = py::cast<float>(item.second);
                    } else if (key == "shape") {
                      twk->_shape = py::cast<float>(item.second);
                    } else if (key == "steps") {
                      twk->_steps = py::cast<int>(item.second);
                    } else if (key == "coarse_step") {
                      twk->_coarse_step = py::cast<int>(item.second);
                    } else if (key == "on_changed") {
                      auto py_fn = std::make_shared<py::function>(py::cast<py::function>(item.second));
                      twk->_userOnChanged = [py_fn](midi::tweakable_ptr_t t) {
                        py::gil_scoped_acquire acquire;
                        (*py_fn)(t);
                      };
                    }
                  }
                }
                twk->recompute();
                tset->_addTweakable(twk);
                return twk;
              })
          .def(
              "createToggle",
              [](midi::tweakableset_ptr_t tset, py::kwargs kwargs) -> midi::toggletweakable_ptr_t {
                auto twk = std::make_shared<midi::ToggleTweakable>(tset.get());
                if (kwargs) {
                  for (auto item : kwargs) {
                    auto key = py::cast<std::string>(item.first);
                    if (key == "name") {
                      twk->_name = py::cast<std::string>(item.second);
                    } else if (key == "knobID") {
                      twk->_knobID = py::cast<int>(item.second);
                    } else if (key == "color") {
                      twk->_switch_color = py::cast<int>(item.second);
                    } else if (key == "default") {
                      twk->_value = py::cast<bool>(item.second);
                    } else if (key == "on_changed") {
                      auto py_fn = std::make_shared<py::function>(py::cast<py::function>(item.second));
                      twk->_userOnChanged = [py_fn](midi::tweakable_ptr_t t) {
                        py::gil_scoped_acquire acquire;
                        (*py_fn)(t);
                      };
                    }
                  }
                }
                tset->_addTweakable(twk);
                return twk;
              })
          .def(
              "createEnum",
              [](midi::tweakableset_ptr_t tset, py::kwargs kwargs) -> midi::enumtweakable_ptr_t {
                auto twk = std::make_shared<midi::EnumTweakable>(tset.get());
                if (kwargs) {
                  for (auto item : kwargs) {
                    auto key = py::cast<std::string>(item.first);
                    if (key == "name") {
                      twk->_name = py::cast<std::string>(item.second);
                    } else if (key == "knobID") {
                      twk->_knobID = py::cast<int>(item.second);
                    } else if (key == "on_changed") {
                      auto py_fn = std::make_shared<py::function>(py::cast<py::function>(item.second));
                      twk->_userOnChanged = [py_fn](midi::tweakable_ptr_t t) {
                        py::gil_scoped_acquire acquire;
                        (*py_fn)(t);
                      };
                    }
                  }
                }
                tset->_addTweakable(twk);
                return twk;
              })
          .def(
              "createEvent",
              [](midi::tweakableset_ptr_t tset, py::kwargs kwargs) -> midi::eventtweakable_ptr_t {
                auto twk = std::make_shared<midi::EventTweakable>(tset.get());
                if (kwargs) {
                  for (auto item : kwargs) {
                    auto key = py::cast<std::string>(item.first);
                    if (key == "name") {
                      twk->_name = py::cast<std::string>(item.second);
                    } else if (key == "knobID") {
                      twk->_knobID = py::cast<int>(item.second);
                    } else if (key == "on_changed") {
                      auto py_fn = std::make_shared<py::function>(py::cast<py::function>(item.second));
                      twk->_userOnChanged = [py_fn](midi::tweakable_ptr_t t) {
                        py::gil_scoped_acquire acquire;
                        (*py_fn)(t);
                      };
                    }
                  }
                }
                tset->_addTweakable(twk);
                return twk;
              })
          .def(
              "setParamsPath",
              [](midi::tweakableset_ptr_t tset, const std::string& path) {
                tset->_params_path = path;
              })
          .def_property_readonly(
              "p",
              [](midi::tweakableset_ptr_t tset) -> tweakablepropproxy_ptr_t {
                return std::make_shared<TweakablePropProxy>(tset);
              });
  type_codec->registerStdCodec<midi::tweakableset_ptr_t>(tweakableset_type);

  /////////////////////////////////////////////////////////////////////////////////
  // Helper constants for LED colors (Midi Fighter Twister)
  /////////////////////////////////////////////////////////////////////////////////
  midi_module.attr("COLOR_BLUE") = 1;
  midi_module.attr("COLOR_CYAN") = 29;
  midi_module.attr("COLOR_GREEN") = 43;
  midi_module.attr("COLOR_YELLOW") = 61;
  midi_module.attr("COLOR_ORANGE") = 69;
  midi_module.attr("COLOR_RED") = 78;
  midi_module.attr("COLOR_MAGENTA") = 100;
  midi_module.attr("COLOR_OFF") = 0;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2 {

