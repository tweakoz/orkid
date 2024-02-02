////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/rtti/Class.h>
#include <ork/rtti/Category.h>
#include <ork/rtti/downcast.h>
#include <ork/object/Object.h>
#include <ork/lev2/aud/audiodevice.h>
#include <ork/lev2/aud/singularity/cz1.h>
#include <ork/lev2/aud/singularity/krzdata.h>
#include <ork/lev2/aud/singularity/tx81z.h>
#include <ork/lev2/aud/singularity/fxgen.h>
#include <ork/lev2/aud/singularity/hud.h>
#include <ork/lev2/aud/singularity/alg_oscil.h>
#include <ork/lev2/aud/singularity/sampler.h>
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
void pyinit_aud_singularity_datas(py::module& singmodule) {
  auto type_codec = python::TypeCodec::instance();
  /////////////////////////////////////////////////////////////////////////////////
  auto ctrl_type = py::class_<ControllerData, Object, controllerdata_ptr_t>(singmodule, "ControllerData") //
                       .def_property_readonly(
                           "name",
                           [](controllerdata_ptr_t ctrl) -> std::string { //
                             return ctrl->_name;
                           });
  type_codec->registerStdCodec<controllerdata_ptr_t>(ctrl_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto ratlevenv_type =
      py::class_<RateLevelEnvData, ControllerData, ratelevelenvdata_ptr_t>(singmodule, "RateLevelEnvData") //
          .def_property(
              "ampenv",
              [](ratelevelenvdata_ptr_t env) -> bool { //
                return env->_ampenv;
              },
              [](ratelevelenvdata_ptr_t env, bool val) { //
                env->_ampenv = val;
              })
          .def_property(
              "bipolar",
              [](ratelevelenvdata_ptr_t env) -> bool { //
                return env->_bipolar;
              },
              [](ratelevelenvdata_ptr_t env, bool val) { //
                env->_bipolar = val;
              })
          .def_property(
              "releaseSegment",                       //
              [](ratelevelenvdata_ptr_t env) -> int { //
                return env->_releaseSegment;
              },
              [](ratelevelenvdata_ptr_t env, int val) { //
                env->_releaseSegment = val;
              })
          .def_property(
              "sustainSegment",                       //
              [](ratelevelenvdata_ptr_t env) -> int { //
                return env->_sustainSegment;
              },
              [](ratelevelenvdata_ptr_t env, int val) { //
                env->_sustainSegment = val;
              })
          .def("addSegment", [](ratelevelenvdata_ptr_t env, std::string name, float time, float level, float power) { //
            env->addSegment(name, time, level, power);
          });
  type_codec->registerStdCodec<ratelevelenvdata_ptr_t>(ratlevenv_type);
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
  auto dspparammoddata_type = py::class_<BlockModulationData, dspparammod_ptr_t>(singmodule, "DspBlockModulationData") //
                                  .def_property(
                                      "src1",
                                      [](dspparammod_ptr_t mod) -> controllerdata_ptr_t { //
                                        return mod->_src1;
                                      },
                                      [](dspparammod_ptr_t mod, controllerdata_ptr_t ctrl) { //
                                        mod->_src1 = ctrl;
                                      })
                                  .def_property(
                                      "src2",
                                      [](dspparammod_ptr_t mod) -> controllerdata_ptr_t { //
                                        return mod->_src2;
                                      },
                                      [](dspparammod_ptr_t mod, controllerdata_ptr_t ctrl) { //
                                        mod->_src2 = ctrl;
                                      })
                                  .def_property(
                                      "src1scale",
                                      [](dspparammod_ptr_t mod) -> float { //
                                        return mod->_src1Scale;
                                      },
                                      [](dspparammod_ptr_t mod, float val) { //
                                        mod->_src1Scale = val;
                                      })
                                  .def_property(
                                      "src1bias",
                                      [](dspparammod_ptr_t mod) -> float { //
                                        return mod->_src1Bias;
                                      },
                                      [](dspparammod_ptr_t mod, float val) { //
                                        mod->_src1Bias = val;
                                      })
                                  .def_property(
                                      "src2mindepth",
                                      [](dspparammod_ptr_t mod) -> float { //
                                        return mod->_src2MinDepth;
                                      },
                                      [](dspparammod_ptr_t mod, float val) { //
                                        mod->_src2MinDepth = val;
                                      })
                                  .def_property(
                                      "src2maxdepth",
                                      [](dspparammod_ptr_t mod) -> float { //
                                        return mod->_src2MaxDepth;
                                      },
                                      [](dspparammod_ptr_t mod, float val) { //
                                        mod->_src2MaxDepth = val;
                                      });
  type_codec->registerStdCodec<dspparammod_ptr_t>(dspparammoddata_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto dspparamdata_type = py::class_<DspParamData, dspparam_ptr_t>(singmodule, "DspParamData") //
                               .def_property_readonly(
                                   "name",
                                   [](dspparam_ptr_t param) -> std::string { //
                                     return param->_name;
                                   })
                               .def_property(
                                   "coarse",
                                   [](dspparam_ptr_t param) -> float { //
                                     return param->_coarse;
                                   },
                                   [](dspparam_ptr_t param, float val) { //
                                     param->_coarse = val;
                                   })
                               .def_property(
                                   "fine",
                                   [](dspparam_ptr_t param) -> float { //
                                     return param->_fine;
                                   },
                                   [](dspparam_ptr_t param, float val) { //
                                     param->_fine = val;
                                   })
                               .def_property(
                                   "fineHZ",
                                   [](dspparam_ptr_t param) -> float { //
                                     return param->_fineHZ;
                                   },
                                   [](dspparam_ptr_t param, float val) { //
                                     param->_fineHZ = val;
                                   })
                               .def_property(
                                   "keyTrack",
                                   [](dspparam_ptr_t param) -> float { //
                                     return param->_keyTrack;
                                   },
                                   [](dspparam_ptr_t param, float val) { //
                                     param->_keyTrack = val;
                                   })
                               .def_property(
                                   "velTrack",
                                   [](dspparam_ptr_t param) -> float { //
                                     return param->_velTrack;
                                   },
                                   [](dspparam_ptr_t param, float val) { //
                                     param->_velTrack = val;
                                   })
                               .def_property(
                                   "keystartNote",
                                   [](dspparam_ptr_t param) -> int { //
                                     return param->_keystartNote;
                                   },
                                   [](dspparam_ptr_t param, int val) { //
                                     param->_keystartNote = val;
                                   })
                               .def_property(
                                   "debug",
                                   [](dspparam_ptr_t param) -> bool { //
                                     return param->_debug;
                                   },
                                   [](dspparam_ptr_t param, bool val) { //
                                     param->_debug = val;
                                   })
                               .def_property(
                                   "evaluatorID",
                                   [](dspparam_ptr_t param) -> std::string { //
                                     return param->_evaluatorid;
                                   },
                                   [](dspparam_ptr_t param, std::string evalid) { //
                                     if (evalid == "default") {
                                       param->useDefaultEvaluator();
                                     } else if (evalid == "pitch") {
                                       param->usePitchEvaluator();
                                     } else if (evalid == "frequency") {
                                       param->useFrequencyEvaluator();
                                     } else if (evalid == "amplitude") {
                                       param->useAmplitudeEvaluator();
                                     } else if (evalid == "krzpos") {
                                       param->useKrzPosEvaluator();
                                     } else if (evalid == "krzevnodd") {
                                       param->useKrzEvnOddEvaluator();
                                     } else {
                                       OrkAssert(false);
                                     }
                                   })
                               .def_property_readonly(
                                   "mods",
                                   [](dspparam_ptr_t param) -> dspparammod_ptr_t { //
                                     return param->_mods;
                                   })
                               .def("__repr__", [](dspparam_ptr_t param) -> std::string {
                                 auto str = FormatString("dspparam<%s> ", param->_name.c_str());
                                 str += FormatString("coarse: %g ", param->_coarse);
                                 str += FormatString("fine: %g ", param->_fine);
                                 str += FormatString("units: %s ", param->_units.c_str());
                                 str += FormatString("eval: %s", param->_evaluatorid.c_str());
                                 return str;
                               });
  type_codec->registerStdCodec<dspparam_ptr_t>(dspparamdata_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto dspdata_type =
      py::class_<DspBlockData, ::ork::Object, dspblkdata_ptr_t>(singmodule, "DspBlockData") //
          .def_property_readonly(
              "name",
              [](dspblkdata_ptr_t blkdata) -> std::string { //
                return blkdata->_name;
              })
          .def("paramByIndex", [](dspblkdata_ptr_t blkdata, int index) -> dspparam_ptr_t { return blkdata->param(index); })
          .def(
              "paramByName",
              [](dspblkdata_ptr_t blkdata, std::string named) -> dspparam_ptr_t { return blkdata->paramByName(named); })
          .def_property_readonly(
              "params",
              [type_codec](dspblkdata_ptr_t blkdata) -> py::dict { //
                py::dict rval;
                for (auto par : blkdata->_paramd) {
                  auto name                      = par->_name;
                  rval[type_codec->encode(name)] = type_codec->encode(par);
                }
                return rval;
              })
          .def_property(
              "bypass",
              [](dspblkdata_ptr_t blkdata) -> bool { //
                return blkdata->_bypass;
              },
              [](dspblkdata_ptr_t blkdata, bool val) { //
                blkdata->_bypass = val;
              })
          .def("__repr__", [](dspblkdata_ptr_t blkdata) -> std::string {
            auto str       = FormatString("dspblock<%s> ", blkdata->_name.c_str());
            auto clazz     = blkdata->GetClass();
            auto& desc     = clazz->Description();
            auto clazzname = clazz->Name();
            str += FormatString("class: %s", clazzname.c_str());
            return str;
          });
  type_codec->registerStdCodec<dspblkdata_ptr_t>(dspdata_type);
  /////////////////////////////////////////////////////////////////////////////////
  using pitchblk_ptr_t = std::shared_ptr<PITCH_DATA>;
  auto pitchdata_type  = py::class_<PITCH_DATA, DspBlockData, pitchblk_ptr_t>(singmodule, "PitchBlockData");
  type_codec->registerStdCodec<pitchblk_ptr_t>(pitchdata_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto stgdata_type =
      py::class_<DspStageData, dspstagedata_ptr_t>(singmodule, "DspStageData") //
          .def_property_readonly(
              "name",
              [](dspstagedata_ptr_t stgdata) -> std::string { //
                return stgdata->_name;
              })
          .def_property_readonly("ioconfig", [](dspstagedata_ptr_t stgdata) -> ioconfig_ptr_t { return stgdata->_ioconfig; })
          .def(
              "appendDspBlock",
              [](dspstagedata_ptr_t stgdata, std::string classname, std::string blockname) -> dspblkdata_ptr_t {
                auto base_clazz    = rtti::Class::FindClass("SynDspBlock");
                auto base_objclazz = dynamic_cast<object::ObjectClass*>(base_clazz);
                auto clazz         = rtti::Class::FindClass("Dsp" + classname);
                auto objclazz      = dynamic_cast<object::ObjectClass*>(clazz);
                OrkAssert(objclazz);
                if (objclazz->Parent() != base_objclazz) {
                  printf(
                      "appendDspBlock<%s> objclazz<%p> base_objclazz<%p> parent mismatch",
                      classname.c_str(),
                      objclazz,
                      base_objclazz);
                  OrkAssert(false);
                }
                // printf("appendDspBlock objclazz<%p>", objclazz );
                const auto& description = objclazz->Description();
                auto instance           = objclazz->createShared();
                auto rval               = std::dynamic_pointer_cast<DspBlockData>(instance);
                rval->_name             = blockname;
                OrkAssert(rval != nullptr);
                stgdata->_blockdatas[stgdata->_numblocks++] = rval;
                auto it                                     = stgdata->_namedblockdatas.find(blockname);
                OrkAssert(it == stgdata->_namedblockdatas.end());
                stgdata->_namedblockdatas[blockname] = rval;
                return rval;
              })
          .def(
              "replaceDspBlock",
              [](dspstagedata_ptr_t stgdata, std::string oldclassname, std::string newclassname, std::string blockname)
                  -> dspblkdata_ptr_t {
                auto base_clazz    = rtti::Class::FindClass("SynDspBlock");
                auto base_objclazz = dynamic_cast<object::ObjectClass*>(base_clazz);
                auto newclazz      = rtti::Class::FindClass("Dsp" + newclassname);
                auto newobjclazz   = dynamic_cast<object::ObjectClass*>(newclazz);
                auto oldclazz      = rtti::Class::FindClass("Dsp" + oldclassname);
                auto oldobjclazz   = dynamic_cast<object::ObjectClass*>(oldclazz);
                OrkAssert(newobjclazz);
                if (newclazz->Parent() != base_objclazz) {
                  printf(
                      "appendDspBlock<%s> objclazz<%p> base_objclazz<%p> parent mismatch",
                      newclassname.c_str(),
                      newclazz,
                      base_objclazz);
                  OrkAssert(false);
                }
                // printf("appendDspBlock objclazz<%p>", objclazz );
                const auto& description = newobjclazz->Description();
                auto instance           = newobjclazz->createShared();
                auto rval               = std::dynamic_pointer_cast<DspBlockData>(instance);
                rval->_name             = blockname;
                OrkAssert(rval != nullptr);
                int itoreplace = -1;
                for (int i = 0; i < stgdata->_numblocks; i++) {
                  auto oldbd      = stgdata->_blockdatas[i];
                  auto oldbdclazz = oldbd->GetClass();
                  if (oldobjclazz == oldbdclazz) {
                    auto name                       = oldbd->_name;
                    stgdata->_namedblockdatas[name] = rval;
                    stgdata->_blockdatas[i]         = rval;
                    return rval;
                  }
                }
                return nullptr;
              })
          .def(
              "appendPitchChorus",
              [](dspstagedata_ptr_t stgdata, lyrdata_ptr_t layer, float wetness, float cents, float feedback) {
                appendPitchChorus(layer, stgdata, wetness, cents, feedback);
              })
          .def("dspblock", [](dspstagedata_ptr_t stgdata, int index) -> dspblkdata_ptr_t { return stgdata->_blockdatas[index]; })
          .def("dump", [](dspstagedata_ptr_t stgdata) { stgdata->dump(); });
  type_codec->registerStdCodec<dspstagedata_ptr_t>(stgdata_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto ldata_type = py::class_<LayerData, lyrdata_ptr_t>(singmodule, "LayerData") //
                        .def(py::init<>())
                        .def(
                            "stage",
                            [](lyrdata_ptr_t ldata, std::string named) -> dspstagedata_ptr_t { //
                              return ldata->stageByName(named);
                            })
                        .def(
                            "appendStage",
                            [](lyrdata_ptr_t ldata, std::string named) -> dspstagedata_ptr_t { //
                              return ldata->appendStage(named);
                            })
                        .def(
                            "appendController",
                            [](lyrdata_ptr_t ldata, std::string classname, std::string named) -> controllerdata_ptr_t { //
                              auto base_clazz    = rtti::Class::FindClass("SynControllerData");
                              auto base_objclazz = dynamic_cast<object::ObjectClass*>(base_clazz);
                              auto clazz         = rtti::Class::FindClass("Syn" + classname);
                              auto objclazz      = dynamic_cast<object::ObjectClass*>(clazz);
                              OrkAssert(objclazz);
                              if (objclazz->Parent() != base_objclazz) {
                                printf(
                                    "appendSynController<%s> objclazz<%p> base_objclazz<%p> parent mismatch",
                                    classname.c_str(),
                                    objclazz,
                                    base_objclazz);
                                OrkAssert(false);
                              }
                              // printf("appendDspBlock objclazz<%p>", objclazz );
                              const auto& description = objclazz->Description();
                              auto instance           = objclazz->createShared();
                              auto rval               = std::dynamic_pointer_cast<ControllerData>(instance);
                              rval->_name             = named;
                              auto ctrlblok           = ldata->_ctrlBlock;
                              // ctrlblok->addController<T>(named);
                              ctrlblok->_controller_datas[ctrlblok->_numcontrollers++] = rval;
                              ctrlblok->_controllers_by_name[named]                    = rval;
                              OrkAssert(rval != nullptr);
                              return rval;
                            })
                        .def(
                            "clone",
                            [](lyrdata_ptr_t ldata) -> lyrdata_ptr_t { //
                              return ldata->clone();
                            })
                        .def(
                            "createScopeSource",
                            [](lyrdata_ptr_t ldata) -> scopesource_ptr_t { //
                              return ldata->createScopeSource();
                            })
                        .def_property_readonly(
                            "pitchBlock",                               //
                            [](lyrdata_ptr_t ldata) -> pitchblk_ptr_t { //
                              return std::dynamic_pointer_cast<PITCH_DATA>(ldata->_pchBlock);
                            })
                        .def_property(
                            "gain",                            //
                            [](lyrdata_ptr_t ldata) -> float { //
                              return linear_amp_ratio_to_decibel(ldata->_layerLinGain);
                            },
                            [](lyrdata_ptr_t ldata, float gainDB) { //
                              ldata->_layerLinGain = decibel_to_linear_amp_ratio(gainDB);
                            })
                        .def_property(
                            "outputBus",                             //
                            [](lyrdata_ptr_t ldata) -> std::string { //
                              return ldata->_outbus;
                            },
                            [](lyrdata_ptr_t ldata, std::string busname) { //
                              ldata->_outbus = busname;
                            })
                        .def_property(
                            "keymap",
                            [](lyrdata_ptr_t ldata) -> keymap_ptr_t { //
                              return ldata->_keymap;
                            },
                            [](lyrdata_ptr_t ldata, keymap_ptr_t kmap) { //
                              ldata->_keymap = kmap;
                            })
                        .def("__repr__", [](lyrdata_ptr_t ldata) -> std::string {
                          std::ostringstream oss;
                          oss << "LayerData( name: " << ldata->_name << ", stage_count: " << ldata->_algdata->_numstages << " )";
                          return oss.str();
                        });
  type_codec->registerStdCodec<lyrdata_ptr_t>(ldata_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto kmaprgndata_type = py::class_<KmRegionData, kmregion_ptr_t>(singmodule, "KeyMapRegion");
  type_codec->registerStdCodec<kmregion_ptr_t>(kmaprgndata_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto kmapdata_type = py::class_<KeyMapData, keymap_ptr_t>(singmodule, "KeyMapData") //
                           .def(py::init([](std::string named) -> keymap_ptr_t {
                             auto kmap   = std::make_shared<KeyMapData>();
                             kmap->_name = named;
                             return kmap;
                           }))
                           .def_property(
                               "name",
                               [](keymap_ptr_t kmap) -> std::string { //
                                 return kmap->_name;
                               },
                               [](keymap_ptr_t kmap, std::string named) { //
                                 kmap->_name = named;
                               })
                           .def("addRegion", [](keymap_ptr_t kmap, py::kwargs kwargs) -> kmregion_ptr_t {
                             auto region = std::make_shared<KmRegionData>();
                             kmap->_regions.push_back(region);
                             for (auto item : kwargs) {
                               auto key = item.first.cast<std::string>();
                               if (key == "lokey")
                                 region->_lokey = item.second.cast<int>();
                               if (key == "hikey")
                                 region->_hikey = item.second.cast<int>();
                               if (key == "lovel")
                                 region->_lovel = item.second.cast<int>();
                               if (key == "hivel")
                                 region->_hivel = item.second.cast<int>();
                               if (key == "tuning")
                                 region->_tuning = item.second.cast<int>();
                               // if(key=="loopmode") region->_loopModeOverride = item.second.cast<eLoopMode>();
                               if (key == "voladj")
                                 region->_volAdj = item.second.cast<float>();
                               if (key == "lingain")
                                 region->_linGain = item.second.cast<float>();
                               // if(key=="multsampID") region->_multsampID = item.second.cast<int>();
                               // if(key=="sampID") region->_sampID = item.second.cast<int>();
                               // if(key=="sampleName") region->_sampleName = item.second.cast<std::string>();
                               if (key == "multiSample") {
                                 region->_multiSample = item.second.cast<multisample_constptr_t>();
                               }
                               if (key == "sample") {
                                 region->_sample     = item.second.cast<sample_constptr_t>();
                                 region->_sampleName = region->_sample->_name;
                               }
                             }
                             return region;
                           });
  type_codec->registerStdCodec<keymap_ptr_t>(kmapdata_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto sampdata_type = py::class_<SampleData, sample_ptr_t>(singmodule, "SampleData") //
                           .def(py::init([](py::kwargs kwargs) -> sample_ptr_t {
                             auto sample   = std::make_shared<SampleData>();
                             for (auto item : kwargs) {
                               auto key = item.first.cast<std::string>();
                               if (key == "name") {
                                 auto name = item.second.cast<std::string>();
                                 sample->_name = name;
                               }
                               else if (key == "waveform") {
                                 auto wavedataIN = item.second.cast<py::list>();
                                 // is this a list of floats or a list of ints?
                                  bool isfloat = false;
                                  for (auto wsamp : wavedataIN) {
                                    if (wsamp.is(py::float_())) {
                                      isfloat = true;
                                      break;
                                    }
                                  }
                                  size_t count = wavedataIN.size();
                                  auto& wavedataOUT = sample->_user.make<WaveformData>();
                                  wavedataOUT._sampledata.resize(count);

                                  if (isfloat) {
                                    for( auto wsamp : wavedataIN ) {
                                      float s = wsamp.cast<float>();
                                    }
                                  } else {
                                    for( auto wsamp : wavedataIN ) {
                                      float s = wsamp.cast<int>();
                                    }
                                  }
                               }
                               else if (key == "samplerate") {
                                 sample->_sampleRate = item.second.cast<float>();
                               }
                             }
                             return sample;
                           }))
                           .def_property(
                               "name",
                               [](sample_ptr_t sample) -> std::string { //
                                 return sample->_name;
                               },
                               [](sample_ptr_t sample, std::string named) { //
                                 sample->_name = named;
                               });
  type_codec->registerStdCodec<sample_ptr_t>(sampdata_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto msampdata_type = py::class_<MultiSampleData, multisample_ptr_t>(singmodule, "MultiSampleData") //
                            .def(py::init([](std::string named, py::list samples) -> multisample_ptr_t {
                              auto msample   = std::make_shared<MultiSampleData>();
                              msample->_name = named;
                              for (auto item : samples) {
                                auto sample                            = item.cast<sample_ptr_t>();
                                msample->_samplesByName[sample->_name] = sample;
                              }
                              return msample;
                            }))
                            .def_property(
                                "name",
                                [](multisample_ptr_t msample) -> std::string { //
                                  return msample->_name;
                                },
                                [](multisample_ptr_t msample, std::string named) { //
                                  msample->_name = named;
                                });
  type_codec->registerStdCodec<multisample_ptr_t>(msampdata_type);
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
                        .def(
                            "newLayer",
                            [](prgdata_ptr_t pdata) -> lyrdata_ptr_t { //
                              return pdata->newLayer();
                            })
                        .def(
                            "forkLayer",
                            [](prgdata_ptr_t pdata, size_t index) -> lyrdata_ptr_t { //
                              auto original = pdata->getLayer(index);
                              auto clone    = original->clone();
                              pdata->setLayer(index, clone);
                              return clone;
                            })
                        .def_property(
                            "name", //
                            [](prgdata_ptr_t pdata) -> std::string { return pdata->_name; },
                            [](prgdata_ptr_t pdata, std::string named) { pdata->_name = named; })
                        .def_property(
                            "monophonic",
                            [](prgdata_ptr_t pdata) -> bool { //
                              return pdata->_monophonic;
                            },
                            [](prgdata_ptr_t pdata, bool mono) { //
                              pdata->_monophonic = mono;
                            })
                        .def_property(
                            "portamentoRate",
                            [](prgdata_ptr_t pdata) -> float { //
                              return pdata->_portamento_rate;
                            },
                            [](prgdata_ptr_t pdata, float rate) { //
                              pdata->_portamento_rate = rate;
                            })
                        .def("__repr__", [](prgdata_ptr_t pdata) -> std::string {
                          std::ostringstream oss;
                          oss << "ProgramData( name: " << pdata->_name << ", layer_count: " << pdata->_layerdatas.size() << " )";
                          return oss.str();
                        });
  type_codec->registerStdCodec<prgdata_ptr_t>(pdata_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto bankdata_type = py::class_<BankData, ::ork::Object, bankdata_ptr_t>(singmodule, "BankData")
                           .def(py::init<>())
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
                               "newProgram",                                                  //
                               [](bankdata_ptr_t bdata, std::string named) -> prgdata_ptr_t { //
                                 auto prg      = std::make_shared<ProgramData>();
                                 prg->_name    = named;
                                 int highestID = 0;
                                 if (bdata->_programs.size()) {
                                   bdata->_programs.rbegin()->first;
                                 }
                                 bdata->_programs[highestID + 1] = prg;
                                 bdata->_programsByName[named]   = prg;
                                 return prg;
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
                               })
                           .def(
                               "multisampleByName",                                               //
                               [](bankdata_ptr_t bdata, std::string named) -> multisample_ptr_t { //
                                 auto ms = bdata->findMultiSampleByName(named);
                                 return ms;
                               })
                           .def_property_readonly(
                               "multisamplesByName",                            //
                               [type_codec](bankdata_ptr_t bdata) -> py::dict { //
                                 py::dict rval;
                                 for (auto item : bdata->_multisamplesByName) {
                                   auto name                      = item.first;
                                   auto msample                   = item.second;
                                   auto objptr                    = std::dynamic_pointer_cast<Object>(msample);
                                   rval[type_codec->encode(name)] = type_codec->encode(objptr);
                                 }
                                 return rval;
                               })
                           .def(
                               "keymapByName",                                               //
                               [](bankdata_ptr_t bdata, std::string named) -> keymap_ptr_t { //
                                 auto ms = bdata->findKeymapByName(named);
                                 return ms;
                               })
                           .def_property_readonly(
                               "keymapsByName",                                 //
                               [type_codec](bankdata_ptr_t bdata) -> py::dict { //
                                 py::dict rval;
                                 for (auto item : bdata->_keymapsByName) {
                                   auto name                      = item.first;
                                   auto kmap                      = item.second;
                                   auto objptr                    = std::dynamic_pointer_cast<Object>(kmap);
                                   rval[type_codec->encode(name)] = type_codec->encode(objptr);
                                 }
                                 return rval;
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
                             [](keyonmod_ptr_t kmod, uint32_t val) { kmod->_layermask = val; })
                         .def_property(
                             "outputbus", //
                             [](keyonmod_ptr_t kmod) -> outbus_ptr_t { return kmod->_outbus_override; },
                             [](keyonmod_ptr_t kmod, outbus_ptr_t val) { kmod->_outbus_override = val; });
  type_codec->registerStdCodec<keyonmod_ptr_t>(konmod_type);
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::audio::singularity
///////////////////////////////////////////////////////////////////////////////
