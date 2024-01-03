////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/lev2/aud/audiodevice.h>
#include <ork/lev2/aud/singularity/cz1.h>
#include <ork/lev2/aud/singularity/krzdata.h>
#include <ork/lev2/aud/singularity/tx81z.h>
#include <ork/lev2/aud/singularity/fxgen.h>
#include <ork/lev2/aud/singularity/hud.h>
#include <ork/lev2/ui/widget.h>
#include <ork/lev2/ui/group.h>
#include <ork/lev2/ui/surface.h>
#include <ork/lev2/ui/viewport.h>
#include <ork/lev2/ui/layoutgroup.inl>
#include <ork/lev2/ui/anchor.h>
#include <ork/lev2/ui/box.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::audio::singularity {
///////////////////////////////////////////////////////////////////////////////
using namespace ork::lev2;

using prginst_rawptr_t = ork::python::unmanaged_ptr<programInst>;

void pyinit_aud_singularity_synth(py::module& singmodule) {
  /////////////////////////////////////////////////////////////////////////////////
  auto type_codec = python::TypeCodec::instance();
  /////////////////////////////////////////////////////////////////////////////////
  auto auddev_t = py::class_<AudioDevice, audiodevice_ptr_t>(singmodule, "device") //
                      .def_static("instance", []() -> audiodevice_ptr_t {          //
                        auto the_dev = AudioDevice::instance();
                        printf("the_dev<%p>\n", (void*)the_dev.get());
                        return the_dev;
                      });
  type_codec->registerStdCodec<audiodevice_ptr_t>(auddev_t);
  /////////////////////////////////////////////////////////////////////////////////
  auto synth_type_t =
      py::class_<synth, synth_ptr_t>(singmodule, "synth") //
          .def_static(
              "instance",
              []() -> synth_ptr_t { //
                auto the_synth = synth::instance();
                printf("the_synth<%p>\n", (void*)the_synth.get());
                return the_synth;
              })
          .def(
              "nextEffect", //
              [](synth_ptr_t synth,outbus_ptr_t obus) { synth->nextEffect(obus); })
          .def(
              "prevEffect", //
              [](synth_ptr_t synth,outbus_ptr_t obus) { synth->prevEffect(obus); })
          .def(
              "setEffect", //
              [](synth_ptr_t synth, outbus_ptr_t bus, std::string name) { synth->setEffect(bus, name); })
          .def(
              "outputBus", //
              [](synth_ptr_t synth, std::string named) -> outbus_ptr_t { return synth->outputBus(named); })
          .def(
              "createOutputBus", //
              [](synth_ptr_t synth, std::string named) -> outbus_ptr_t { return synth->createOutputBus(named); })
          .def(
              "keyOn",                                                                                                          //
              [](synth_ptr_t synth, int note, int vel, prgdata_ptr_t prg, keyonmod_ptr_t kmods = nullptr) -> prginst_rawptr_t { //
                return synth->liveKeyOn(note, vel, prg, kmods);
              })
          .def(
              "keyOff",                                      //
              [](synth_ptr_t synth, prginst_rawptr_t prgi) { //
                synth->liveKeyOff(prgi.get());
              })
          .def_property(
              "masterGain", //
              [](synth_ptr_t synth) -> float { return synth->_masterGain; },
              [](synth_ptr_t synth, float gain) { synth->_masterGain = gain; })
          .def_property(
              "soloLayer", //
              [](synth_ptr_t synth) -> int { return synth->_soloLayer; },
              [](synth_ptr_t synth, int index) { synth->_soloLayer = index; })
          .def_property(
              "programbus", //
              [](synth_ptr_t synth) -> outbus_ptr_t { return synth->_curprogrambus; },
              [](synth_ptr_t synth, outbus_ptr_t bus) { synth->_curprogrambus = bus; });
  type_codec->registerStdCodec<synth_ptr_t>(synth_type_t);
  /////////////////////////////////////////////////////////////////////////////////
  auto prgi_type = py::class_<prginst_rawptr_t>(singmodule, "ProgramInst");
  type_codec->registerRawPtrCodec<prginst_rawptr_t, programInst*>(prgi_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto obus_type = py::class_<OutputBus, outbus_ptr_t>(singmodule, "OutputBus") //
                       .def_property_readonly(
                           "name", //
                           [](outbus_ptr_t bus) -> std::string { return bus->_name; })
                       .def(
                           "addChildBus",
                           [](outbus_ptr_t parent, outbus_ptr_t child) { //
                             return parent->_children.push_back(child);
                           })
                       .def("createScopeSource", [](outbus_ptr_t bus) -> scopesource_ptr_t { //
                         return bus->createScopeSource();
                       })
                       .def_property("uiprogram", //
                                     [](outbus_ptr_t bus) -> prgdata_constptr_t { return bus->_uiprogram; },
                                     [](outbus_ptr_t bus, prgdata_constptr_t pd) { bus->_uiprogram = pd; });
  type_codec->registerStdCodec<outbus_ptr_t>(obus_type);
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::audio::singularity
///////////////////////////////////////////////////////////////////////////////