////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/lev2/aud/audiodevice.h>
#include <ork/lev2/aud/singularity/cz1.h>
#include <ork/lev2/aud/singularity/fxgen.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::audio::singularity {
///////////////////////////////////////////////////////////////////////////////
using namespace ork::lev2;

using prginst_rawptr_t = ork::python::unmanaged_ptr<programInst>;

void pyinit_aud_singularity(py::module& module_lev2) {
  auto singmodule = module_lev2.def_submodule("singularity", "orkid audio synthesizer");
  singmodule.def("decibelsToLinear", [](float dB) -> float { return decibel_to_linear_amp_ratio(dB); });
  singmodule.def("baseDataPath", []() -> file::Path { return basePath(); });
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
  auto synth_type_t = py::class_<synth, synth_ptr_t>(singmodule, "synth") //
                          .def_static(
                              "instance",
                              []() -> synth_ptr_t { //
                                auto the_synth = synth::instance();
                                printf("the_synth<%p>\n", (void*)the_synth.get());
                                return the_synth;
                              })
                          .def(
                              "nextEffect", //
                              [](synth_ptr_t synth) { synth->nextEffect(); })
                          .def(
                              "outputBus", //
                              [](synth_ptr_t synth, std::string named) -> outbus_ptr_t { return synth->outputBus(named); })
                          .def(
                              "createOutputBus", //
                              [](synth_ptr_t synth, std::string named) -> outbus_ptr_t { return synth->createOutputBus(named); })
                          .def(
                              "keyOn", //
                              [](synth_ptr_t synth, int note, int vel, prgdata_ptr_t prg) -> prginst_rawptr_t { //
                                return synth->liveKeyOn(note,vel,prg); 
                            })
                          .def(
                              "keyOff", //
                              [](synth_ptr_t synth, prginst_rawptr_t prgi) { //
                                synth->liveKeyOff(prgi.get()); 
                            })
                          .def_property(
                              "masterGain", //
                              [](synth_ptr_t synth) -> float { return synth->_masterGain; },
                              [](synth_ptr_t synth, float gain) { synth->_masterGain = gain; });
  type_codec->registerStdCodec<synth_ptr_t>(synth_type_t);
  /////////////////////////////////////////////////////////////////////////////////
  auto prgi_type = py::class_<prginst_rawptr_t>(singmodule, "ProgramInst");
  type_codec->registerRawPtrCodec<prginst_rawptr_t,programInst*>(prgi_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto obus_type = py::class_<OutputBus, outbus_ptr_t>(singmodule, "OutputBus") //
                       .def_property_readonly(
                           "name", //
                           [](outbus_ptr_t bus) -> std::string { return bus->_name; });
  type_codec->registerStdCodec<outbus_ptr_t>(obus_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto pdata_type = py::class_<ProgramData, prgdata_ptr_t>(singmodule, "ProgramData") //
                        .def_property_readonly(
                            "name", //
                            [](prgdata_ptr_t pdata) -> std::string { return pdata->_name; });
  type_codec->registerStdCodec<prgdata_ptr_t>(pdata_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto bankdata_type = py::class_<BankData, ::ork::Object, bankdata_ptr_t>(singmodule, "BankData")
                           .def_property_readonly(
                               "programsByName",                                //
                               [type_codec](bankdata_ptr_t bdata) -> py::dict { //
                                 py::dict rval;
                                 for (auto item : bdata->_programs) {
                                   int id                         = item.first;
                                   auto prog                      = item.second;
                                   auto name                      = prog->_name;
                                   rval[type_codec->encode(name)] = type_codec->encode(id);
                                 }
                                 return rval;
                               })
                           .def_property_readonly(
                               "programsByID",                                  //
                               [type_codec](bankdata_ptr_t bdata) -> py::dict { //
                                 py::dict rval;
                                 for (auto item : bdata->_programs) {
                                   int id                       = item.first;
                                   auto prog                    = item.second;
                                   auto name                    = prog->_name;
                                   rval[type_codec->encode(id)] = type_codec->encode(name);
                                 }
                                 return rval;
                               })
                           .def(
                               "programByName",                                  //
                               [](bankdata_ptr_t bdata,std::string named) -> prgdata_ptr_t { //
                                auto program = bdata->findProgramByName(named);
                                 return program;
                               })
                           .def(
                               "programByID",                                  //
                               [](bankdata_ptr_t bdata,int id) -> prgdata_ptr_t { //
                                auto program = bdata->findProgram(id);
                                 return program;
                               });
  type_codec->registerStdCodec<bankdata_ptr_t>(bankdata_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto syndata_type = py::class_<SynthData, syndata_ptr_t>(singmodule, "SynthData")
                          .def_property_readonly(
                              "bankData", //
                              [](syndata_ptr_t sdata) -> bankdata_ptr_t { return sdata->_bankdata; })
                          .def_property_readonly(
                              "bankName", //
                              [](syndata_ptr_t sdata) -> std::string { return sdata->_staticBankName; });

  type_codec->registerStdCodec<syndata_ptr_t>(syndata_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto czdata_type = py::class_<CzData, SynthData, czsyndata_ptr_t>(singmodule, "CzSynthData")
                         .def(py::init<>())
                         .def("loadBank", [](czsyndata_ptr_t czdata, std::string bankname, const file::Path& bankpath) {
                           czdata->appendBank(bankpath, bankname);
                         });
  type_codec->registerStdCodec<czsyndata_ptr_t>(czdata_type);
  /////////////////////////////////////////////////////////////////////////////////
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::audio::singularity
///////////////////////////////////////////////////////////////////////////////
