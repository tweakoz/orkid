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
              [](synth_ptr_t synth) { synth->nextEffect(); })
          .def(
              "prevEffect", //
              [](synth_ptr_t synth) { synth->prevEffect(); })
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
              [](synth_ptr_t synth, float gain) { synth->_masterGain = gain; });
  type_codec->registerStdCodec<synth_ptr_t>(synth_type_t);
  /////////////////////////////////////////////////////////////////////////////////
  auto prgi_type = py::class_<prginst_rawptr_t>(singmodule, "ProgramInst");
  type_codec->registerRawPtrCodec<prginst_rawptr_t, programInst*>(prgi_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto obus_type = py::class_<OutputBus, outbus_ptr_t>(singmodule, "OutputBus") //
                       .def_property_readonly(
                           "name", //
                           [](outbus_ptr_t bus) -> std::string { return bus->_name; });
  type_codec->registerStdCodec<outbus_ptr_t>(obus_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto ioc_type = py::class_<IoConfig, ioconfig_ptr_t>(singmodule, "IoConfig") //
                      .def_property_readonly(
                          "numInputs",
                          [](ioconfig_ptr_t ioc) -> int { //
                            return ioc->numInputs();
                          })
                      .def_property_readonly(
                          "numOutputs",
                          [](ioconfig_ptr_t ioc) -> int { //
                            return ioc->numOutputs();
                          })
                      .def_property(
                          "inputs",
                          [](ioconfig_ptr_t ioc) -> py::list { //
                            py::list rval;
                            for (int i = 0; i < ioc->numInputs(); i++) {
                              auto inp = ioc->_inputs[i];
                              rval.append(inp);
                            }
                            return rval;
                          },
                          [](ioconfig_ptr_t ioc, py::list inp_list) {
                            ioc->_inputs.clear();
                            for (int i = 0; i < inp_list.size(); i++) {
                              int inp = inp_list[i].cast<int>();
                              ioc->_inputs.push_back(inp);
                            }
                          })
                      .def_property(
                          "outputs",
                          [](ioconfig_ptr_t ioc) -> py::list { //
                            py::list rval;
                            for (int i = 0; i < ioc->numOutputs(); i++) {
                              auto outp = ioc->_outputs[i];
                              rval.append(outp);
                            }
                            return rval;
                          },
                          [](ioconfig_ptr_t ioc, py::list out_list) {
                            ioc->_outputs.clear();
                            for (int i = 0; i < out_list.size(); i++) {
                              int inp = out_list[i].cast<int>();
                              ioc->_outputs.push_back(inp);
                            }
                          });
  type_codec->registerStdCodec<ioconfig_ptr_t>(ioc_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto stgdata_type = py::class_<DspStageData, dspstagedata_ptr_t>(singmodule, "DspStageData") //
                          .def_property_readonly(
                              "name",
                              [](dspstagedata_ptr_t stgdata) -> std::string { //
                                return stgdata->_name;
                              })
                            .def("dump", [](dspstagedata_ptr_t stgdata) {
                              stgdata->dump();
                            });
  type_codec->registerStdCodec<dspstagedata_ptr_t>(stgdata_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto ldata_type = py::class_<LayerData, lyrdata_ptr_t>(singmodule, "LayerData")                        //
                        .def("stage", [](lyrdata_ptr_t pdata, std::string named) -> dspstagedata_ptr_t { //
                          return pdata->stageByName(named);
                        });
  type_codec->registerStdCodec<lyrdata_ptr_t>(ldata_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto pdata_type = py::class_<ProgramData, prgdata_ptr_t>(singmodule, "ProgramData") //
                        .def(py::init<>())
                        .def(
                            "merge",
                            [](prgdata_ptr_t pdata, prgdata_ptr_t other) { //
                              pdata->merge(*other);
                            })
                        .def(
                            "layer",
                            [](prgdata_ptr_t pdata, size_t index) -> lyrdata_ptr_t { //
                              return pdata->getLayer(index);
                            })
                        .def_property(
                            "name", //
                            [](prgdata_ptr_t pdata) -> std::string { return pdata->_name; },
                            [](prgdata_ptr_t pdata, std::string named) { pdata->_name = named; });
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
                               "addProgram",                                 //
                               [](bankdata_ptr_t bdata, prgdata_ptr_t prg) { //
                                 int highestID                      = bdata->_programs.rbegin()->first;
                                 bdata->_programs[highestID + 1]    = prg;
                                 bdata->_programsByName[prg->_name] = prg;
                               })
                           .def(
                               "merge",                                         //
                               [](bankdata_ptr_t bdata, bankdata_ptr_t other) { //
                                 bdata->merge(*other);
                               })
                           .def(
                               "filterPrograms",                                                 //
                               [](bankdata_ptr_t bdata, py::list allow_list) -> bankdata_ptr_t { //
                                 auto out_bank = std::make_shared<BankData>();
                                 int counter   = 0;
                                 for (auto item : allow_list) {
                                   auto py_item         = item.cast<py::object>();
                                   auto py_item_str     = py_item.attr("__str__")();
                                   auto py_item_str_str = py_item_str.cast<std::string>();
                                   auto program         = bdata->findProgramByName(py_item_str_str);
                                   if (program) {
                                     out_bank->_programsByName[py_item_str_str] = program;
                                     out_bank->_programs[counter++]             = program;
                                   }
                                 }
                                 return out_bank;
                               })
                           .def(
                               "programByName",                                               //
                               [](bankdata_ptr_t bdata, std::string named) -> prgdata_ptr_t { //
                                 auto program = bdata->findProgramByName(named);
                                 return program;
                               })
                           .def(
                               "programByID",                                      //
                               [](bankdata_ptr_t bdata, int id) -> prgdata_ptr_t { //
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
  auto krzdata_type = py::class_<KrzSynthData, SynthData, krzsyndata_ptr_t>(singmodule, "KrzSynthData")
                          .def(py::init<>())
                          .def("loadBank", [](krzsyndata_ptr_t krzdata, std::string bankname, const file::Path& bankpath) {
                            krzdata->loadBank(bankpath);
                          });
  type_codec->registerStdCodec<krzsyndata_ptr_t>(krzdata_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto tx81zdata_type = py::class_<Tx81zData, SynthData, tx81zsyndata_ptr_t>(singmodule, "Tx81zSynthData")
                            .def(py::init<>())
                            .def("loadBank", [](tx81zsyndata_ptr_t txdata, std::string bankname, const file::Path& bankpath) {
                              txdata->loadBank(bankpath);
                            });
  type_codec->registerStdCodec<tx81zsyndata_ptr_t>(tx81zdata_type);
  /////////////////////////////////////////////////////////////////////////////////
  struct KeyModIterator {

    using map_t            = typename KeyOnModifiers::map_t;
    using map_const_iter_t = map_t::const_iterator;

    KeyModIterator(keyonmod_ptr_t kmod)
        : _kmod(kmod) {
    }

    std::string operator*() const {
      return _it->first;
    }

    KeyModIterator operator++() {
      ++_it;
      return *this;
    }

    bool operator==(const KeyModIterator& other) const {
      return _kmod == other._kmod;
    }

    static KeyModIterator _begin(keyonmod_ptr_t kmod) {
      auto it = KeyModIterator(kmod);
      it._it  = kmod->_mods.begin();
      return it;
    }

    static KeyModIterator _end(keyonmod_ptr_t kmod) {
      auto it = KeyModIterator(kmod);
      it._it  = kmod->_mods.end();
      return it;
    }

    keyonmod_ptr_t _kmod;
    map_const_iter_t _it;
  };
  /////////////////////////////////////////////////////////////////////////////////
  struct KonModControllerProxy {
    keyonmod_ptr_t _kmod;
  };
  using konmodctrlproxy_ptr_t = std::shared_ptr<KonModControllerProxy>;
  auto konmodctrlproxy_type   =                                                                     //
      py::class_<KonModControllerProxy, konmodctrlproxy_ptr_t>(singmodule, "KonModControllerProxy") //
          .def(
              "__setattr__",                                                                         //
              [type_codec](konmodctrlproxy_ptr_t proxy, const std::string& key, py::dict inp_dict) { //
                auto builtins   = py::module::import("builtins");
                auto int_type   = builtins.attr("int");
                auto float_type = builtins.attr("float");
                if (key == "generators") {
                  for (auto item : inp_dict) {
                    auto genname          = item.first.cast<std::string>();
                    auto python_generator = item.second.cast<py::object>();
                    KeyOnModifiers::data_ptr_t kdata;
                    auto it = proxy->_kmod->_mods.find(genname);
                    if (it != proxy->_kmod->_mods.end()) {
                      kdata = it->second;
                    } else {
                      kdata                        = std::make_shared<KeyOnModifiers::DATA>();
                      proxy->_kmod->_mods[genname] = kdata;
                      kdata->_name                 = genname;
                    }

                    kdata->_generator = [=]() -> fvec4 {
                      py::gil_scoped_acquire acquire;
                      auto gval   = python_generator();
                      auto pytype = gval.get_type();
                      if (pytype == float_type) {
                        auto fval = gval.cast<float>();
                        return fvec4(fval, fval, fval, fval);
                      } else {
                        OrkAssert(false);
                        return fvec4(0, 0, 0, 0);
                      }
                    };
                  }
                } else if (key == "subscribers") {
                  for (auto item : inp_dict) {
                    auto subname           = item.first.cast<std::string>();
                    auto python_subscriber = item.second.cast<py::object>();
                    KeyOnModifiers::data_ptr_t kdata;
                    auto it = proxy->_kmod->_mods.find(subname);
                    if (it != proxy->_kmod->_mods.end()) {
                      kdata = it->second;
                    } else {
                      kdata                        = std::make_shared<KeyOnModifiers::DATA>();
                      proxy->_kmod->_mods[subname] = kdata;
                      kdata->_name                 = subname;
                    }
                    kdata->_vars.makeValueForKey<py::object>("python_subscriber", python_subscriber);
                    kdata->_subscriber = [kdata, type_codec](std::string name, svar64_t inp) {
                      py::gil_scoped_acquire acquire;
                      auto subscriber = kdata->_vars.typedValueForKey<py::object>("python_subscriber");
                      if (auto as_fvec4 = inp.tryAs<fvec4>()) {
                        auto py_argument = type_codec->encode(as_fvec4.value());
                        subscriber.value()(name, py_argument);
                      } else if (auto as_str = inp.tryAs<std::string>()) {
                        auto py_argument = type_codec->encode(as_str.value());
                        subscriber.value()(name, py_argument);
                      } else {
                        OrkAssert(false);
                      }
                    };
                  }
                } else {
                  OrkAssert(false);
                }

              })
          .def("__len__", [](konmodctrlproxy_ptr_t proxy) -> size_t { return proxy->_kmod->_mods.size(); })
          .def(
              "__iter__",
              [](konmodctrlproxy_ptr_t proxy) { //
                OrkAssert(false);
                return py::make_iterator(                 //
                    KeyModIterator::_begin(proxy->_kmod), //
                    KeyModIterator::_end(proxy->_kmod));
              },
              py::keep_alive<0, 1>())
          .def(
              "__contains__",
              [](konmodctrlproxy_ptr_t proxy, std::string key) { //
                return proxy->_kmod->_mods.contains(key);
              })
          .def(
              "__getitem__",
              [type_codec](konmodctrlproxy_ptr_t proxy, std::string key) -> py::object { //
                auto it = proxy->_kmod->_mods.find(key);
                if (it == proxy->_kmod->_mods.end())
                  throw py::key_error("key not found");
                else {
                  auto varmap_val = it->second;
                  auto python_val = type_codec->encode(varmap_val);
                  return python_val;
                }
              })
          .def("keys", [](konmodctrlproxy_ptr_t proxy) -> py::list {
            py::list rval;
            for (auto item : proxy->_kmod->_mods) {
              rval.append(item.first);
            }
            return rval;
          });
  type_codec->registerStdCodec<konmodctrlproxy_ptr_t>(konmodctrlproxy_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto konmod_type = py::class_<KeyOnModifiers, keyonmod_ptr_t>(singmodule, "KeyOnModifiers")
                         .def(py::init<>())
                         .def(
                             "__repr__",
                             [](keyonmod_ptr_t kmod) -> std::string {
                               std::string rval;
                               size_t numkeys = kmod->_mods.size();
                               rval           = FormatString("KeyOnModifiers(nkeys:%zu)", numkeys);
                               return rval;
                             })
                         .def_property_readonly(
                             "controllers",
                             [](keyonmod_ptr_t kmod) -> konmodctrlproxy_ptr_t {
                               auto proxy   = std::make_shared<KonModControllerProxy>();
                               proxy->_kmod = kmod;
                               return proxy;
                             })
                         .def_property(
                             "layerMask", //
                             [](keyonmod_ptr_t kmod) -> uint32_t { return kmod->_layermask; },
                             [](keyonmod_ptr_t kmod, uint32_t val) { kmod->_layermask = val; });
  type_codec->registerStdCodec<keyonmod_ptr_t>(konmod_type);
  /////////////////////////////////////////////////////////////////////////////////
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::audio::singularity
///////////////////////////////////////////////////////////////////////////////
