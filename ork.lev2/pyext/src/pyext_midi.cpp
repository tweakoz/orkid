////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/lev2/midi/context.h>

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
    std::cout << "Cleanup!" << std::endl;
    ON_MIDI_PYLAMBDA = py::none();
  });
  midi_module.add_object("_cleanup", cleanup);
  /////////////////////////////////////////////////////////////////////////////////
  auto impctx_type =                                   //
      py::class_<midi::InputContext, midi::inputcontext_ptr_t>(midi_module, "InputContext") //
          .def(py::init<>())
          .def_property_readonly(
              "inputs",                     //
              [](midi::inputcontext_ptr_t ctx) -> std::map<std::string, int> { //
                return ctx->enumerateMidiInputs();
              })
          .def(
              "startInputByIndex",                     //
              [](midi::inputcontext_ptr_t ctx, int index, py::object lambda)  { //
                ON_MIDI_PYLAMBDA = lambda;
                ctx->startMidiInputByIndex(index,on_midi_input_message);
              });
  type_codec->registerStdCodec<midi::inputcontext_ptr_t>(impctx_type);

}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2 {

