////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/lev2/aud/audiodevice.h>
#include <ork/lev2/aud/spatializer.h>
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
  auto type_codec = python::pb11_typecodec_t::instance();
  /////////////////////////////////////////////////////////////////////////////////
  auto synth_type_t =
      py::class_<synth, synth_ptr_t>(singmodule, "synth") //
          .def_static(
              "instance",
              [] -> synth_ptr_t { //
                auto the_synth = synth::instance();
                //printf("the_synth<%p>\n", (void*)the_synth.get());
                return the_synth;
              })
          .def_property_readonly(
              "statusString", //
              [](synth_ptr_t synth) -> std::string { return synth->statusString(); })
          .def("panic", &synth::panic)
          .def(
              "nextEffect", //
              [](synth_ptr_t synth, outbus_ptr_t obus) { synth->nextEffect(obus); })
          .def(
              "prevEffect", //
              [](synth_ptr_t synth, outbus_ptr_t obus) { synth->prevEffect(obus); })
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
                return prginst_rawptr_t(synth->liveKeyOn(note, vel, prg, kmods));
              })
          .def(
              "keyOff",                                                         //
              [](synth_ptr_t synth, prginst_rawptr_t prgi, int note, int vel) { //
                synth->liveKeyOff(prgi.get(), note, vel);
              })
          .def(
              "mainThreadHandler",    //
              [](synth_ptr_t synth) { //
                synth->mainThreadHandler();
              })
          .def(
              "resetTimer", //
              [](synth_ptr_t synth) { synth->_timeaccum = 0.0f; })
          .def(
              "disableMasterEq", //
              [](synth_ptr_t synth ) { 
                synth->disableMasterEq();
            })
          .def(
              "enableMasterEq", //
              [](synth_ptr_t synth ) { 
                synth->enableMasterEq();
            })
          .def(
              "setMasterEqBand", //
              [](synth_ptr_t synth, int band, float frqHZ, float widthHZ, float gainDB ) { 
                synth->setMasterEqBand(band,frqHZ,widthHZ,gainDB);
            })
          .def_property(
              "velCurvePower", //
              [](synth_ptr_t synth) -> float { return synth->_velcurvepower; },
              [](synth_ptr_t synth, float pwr) { synth->_velcurvepower = pwr; })
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
              [](synth_ptr_t synth, outbus_ptr_t bus) { synth->_curprogrambus = bus; })
          .def_property_readonly(
              "sequencer", //
              [](synth_ptr_t synth) -> sequencer_ptr_t { return synth->_sequencer; })
          .def_property_readonly(
              "time", //
              [](synth_ptr_t synth) -> float { return synth->_timeaccum; })
          .def_property(
              "system_tempo", //
              [](synth_ptr_t synth) -> float { return synth->_system_tempo; },
              [](synth_ptr_t synth, float tempo) { synth->_system_tempo = tempo; })
          .def_property(
              "listener_matrix", //
              [](synth_ptr_t synth) -> fmtx4 { return synth->_listener_matrix; },
              [](synth_ptr_t synth, fmtx4 pos) { synth->_listener_matrix = pos; //
                                                 synth->_inv_listener_matrix = pos.inverse(); })
          /////////////////////////////////////////////////////////////////////////////////
          // DAW channel strip support
          /////////////////////////////////////////////////////////////////////////////////
          .def_property_readonly(
              "effectPresetNames", //
              [](synth_ptr_t synth) -> py::list {
                py::list rval;
                for (auto& preset : synth->_fxpresets) {
                  rval.append(preset->_name);
                }
                return rval;
              })
          .def_property_readonly(
              "outputBusNames", //
              [](synth_ptr_t synth) -> py::list {
                py::list rval;
                for (auto& item : synth->_outputBusses) {
                  rval.append(item.first);
                }
                return rval;
              })
          .def_property_readonly(
              "globalBank", //
              [](synth_ptr_t synth) -> bankdata_ptr_t { return synth->_globalbank; })
          .def_property_readonly(
              "numSoloed", //
              [](synth_ptr_t synth) -> int { return synth->_num_soloed.load(); });
  type_codec->registerStdCodec<synth_ptr_t>(synth_type_t);
  /////////////////////////////////////////////////////////////////////////////////
  auto prgi_type = py::class_<prginst_rawptr_t>(singmodule, "ProgramInst")
                       .def_property_readonly(
                           "program", //
                           [](prginst_rawptr_t prgi) -> prgdata_constptr_t { return prgi->_progdata; })
                       .def_property_readonly(
                           "keymods", //
                           [](prginst_rawptr_t prgi) -> keyonmod_ptr_t { return prgi->_keymods; })
                       .def_property_readonly("note", [](prginst_rawptr_t prgi) -> int { return prgi->_note; })
                       .def_property_readonly("velocity", [](prginst_rawptr_t prgi) -> int { return prgi->_velocity; })
                       .def_property(
                           "emitter_matrix", //
                           [](prginst_rawptr_t prgi) -> fmtx4 { return prgi->_emitter_matrix; },
                           [](prginst_rawptr_t prgi, fmtx4 pos) { prgi->_emitter_matrix = pos; })
                          .def_property("gain", //
                           [](prginst_rawptr_t prgi) -> float { return prgi->_gain; },
                           [](prginst_rawptr_t prgi, float gain) { prgi->_gain = gain; });
  type_codec->registerStdCodec<prginst_rawptr_t>(prgi_type);
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
                       .def(
                           "createScopeSource",
                           [](outbus_ptr_t bus) -> scopesource_ptr_t { //
                             return bus->createScopeSource();
                           })
                       .def_property(
                           "uiprogram", //
                           [](outbus_ptr_t bus) -> prgdata_constptr_t { return bus->_uiprogram; },
                           [](outbus_ptr_t bus, prgdata_constptr_t pd) { bus->_uiprogram = pd; })
                       .def_property(
                           "layer", //
                           [](outbus_ptr_t bus) -> lyrdata_constptr_t { return bus->_dsplayerdata; },
                           [](outbus_ptr_t bus, lyrdata_ptr_t ld) { bus->setBusDSP(ld); })

                       .def_property(
                           "gain", //
                           [](outbus_ptr_t bus) -> float { return bus->_prog_gain; },
                           [](outbus_ptr_t bus, float g) { bus->_prog_gain = g; })
                       /////////////////////////////////////////////////////////////////////////////////
                       // DAW channel strip controls
                       /////////////////////////////////////////////////////////////////////////////////
                       .def_property(
                           "mute", //
                           [](outbus_ptr_t bus) -> bool { return bus->_mute; },
                           [](outbus_ptr_t bus, bool m) {
                             auto syn = synth::instance();
                             syn->addEvent(0, [bus, m]() { bus->_mute = m; });
                           })
                       .def_property(
                           "solo", //
                           [](outbus_ptr_t bus) -> bool { return bus->_solo; },
                           [](outbus_ptr_t bus, bool s) {
                             auto syn = synth::instance();
                             syn->addEvent(0, [bus, s, syn]() {
                               if (s && !bus->_solo) {
                                 syn->_num_soloed++;
                               } else if (!s && bus->_solo) {
                                 syn->_num_soloed--;
                               }
                               bus->_solo = s;
                             });
                           })
                       .def_property(
                           "pan", //
                           [](outbus_ptr_t bus) -> float { return bus->_pan; },
                           [](outbus_ptr_t bus, float p) {
                             auto syn = synth::instance();
                             syn->addEvent(0, [bus, p]() { bus->_pan = p; });
                           })
                       .def_property_readonly(
                           "effectName", //
                           [](outbus_ptr_t bus) -> std::string { return bus->_fxname; })
                       /////////////////////////////////////////////////////////////////////////////////
                       // Insert effects chain
                       /////////////////////////////////////////////////////////////////////////////////
                       .def_property_readonly(
                           "numInsertGroups", //
                           [](outbus_ptr_t bus) -> int { return bus->_insertGroups.size(); })
                       .def(
                           "insertGroup", //
                           [](outbus_ptr_t bus, int index) -> InsertGroup* {
                             if (index < 0 || index >= bus->_insertGroups.size()) {
                               return nullptr;
                             }
                             return &bus->_insertGroups[index];
                           },
                           py::return_value_policy::reference)
                       .def(
                           "addSerialInsert", //
                           [](outbus_ptr_t bus, lyrdata_ptr_t layer) {
                             auto syn = synth::instance();
                             syn->addEvent(0, [bus, layer]() {
                               InsertGroup group;
                               group._layerdatas.push_back(layer);
                               group._mixGain = 1.0f;
                               bus->_insertGroups.push_back(group);
                             });
                           })
                       .def(
                           "addParallelInsert", //
                           [](outbus_ptr_t bus, py::list layers, float gain) {
                             auto syn = synth::instance();
                             std::vector<lyrdata_ptr_t> layer_vec;
                             for (auto item : layers) {
                               layer_vec.push_back(item.cast<lyrdata_ptr_t>());
                             }
                             syn->addEvent(0, [bus, layer_vec, gain]() {
                               InsertGroup group;
                               for (auto& ld : layer_vec) {
                                 group._layerdatas.push_back(ld);
                               }
                               group._mixGain = gain;
                               bus->_insertGroups.push_back(group);
                             });
                           },
                           py::arg("layers"),
                           py::arg("gain") = 1.0f)
                       .def(
                           "removeInsertGroup", //
                           [](outbus_ptr_t bus, int index) {
                             auto syn = synth::instance();
                             syn->addEvent(0, [bus, index]() {
                               if (index >= 0 && index < bus->_insertGroups.size()) {
                                 bus->_insertGroups.erase(bus->_insertGroups.begin() + index);
                               }
                             });
                           })
                       .def(
                           "clearInserts", //
                           [](outbus_ptr_t bus) {
                             auto syn = synth::instance();
                             syn->addEvent(0, [bus]() { bus->_insertGroups.clear(); });
                           });
  type_codec->registerStdCodec<outbus_ptr_t>(obus_type);
  /////////////////////////////////////////////////////////////////////////////////
  // InsertGroup binding
  /////////////////////////////////////////////////////////////////////////////////
  auto insertgroup_type =
      py::class_<InsertGroup>(singmodule, "InsertGroup")
          .def_property(
              "mixGain", //
              [](InsertGroup& grp) -> float { return grp._mixGain; },
              [](InsertGroup& grp, float gain) { grp._mixGain = gain; })
          .def_property_readonly(
              "numLayers", //
              [](InsertGroup& grp) -> int { return grp.numLayers(); })
          .def_property_readonly(
              "isParallel", //
              [](InsertGroup& grp) -> bool { return grp.isParallel(); })
          .def(
              "layer", //
              [](InsertGroup& grp, int index) -> lyrdata_ptr_t {
                if (index < 0 || index >= grp._layerdatas.size()) {
                  return nullptr;
                }
                return grp._layerdatas[index];
              })
          .def(
              "addLayer", //
              [](InsertGroup& grp, lyrdata_ptr_t layer) {
                auto syn = synth::instance();
                syn->addEvent(0, [&grp, layer]() { grp._layerdatas.push_back(layer); });
              })
          .def(
              "removeLayer", //
              [](InsertGroup& grp, int index) {
                auto syn = synth::instance();
                syn->addEvent(0, [&grp, index]() {
                  if (index >= 0 && index < grp._layerdatas.size()) {
                    grp._layerdatas.erase(grp._layerdatas.begin() + index);
                  }
                });
              });
  /////////////////////////////////////////////////////////////////////////////////
  // Spatializer bindings
  /////////////////////////////////////////////////////////////////////////////////
  {
    auto spatdata_type = //
        py::class_<SpatializerData, ork::Object, spatializerdata_ptr_t>(
            singmodule, "SpatializerData")
            .def(
                "__repr__",
                [](spatializerdata_ptr_t sd) -> std::string {
                  fxstring<256> fxs;
                  fxs.format("audio::SpatializerData(%p)", sd.get());
                  return fxs.c_str();
                });
    type_codec->registerStdCodec<spatializerdata_ptr_t>(spatdata_type);

    auto pannerspatdata_type = //
        py::class_<PannerSpatializerData, SpatializerData, pannerspatializerdata_ptr_t>(
            singmodule, "PannerSpatializerData")
            .def(py::init<>())
            .def(
                "__repr__",
                [](pannerspatializerdata_ptr_t sd) -> std::string {
                  fxstring<256> fxs;
                  fxs.format("audio::PannerSpatializerData(%p)", sd.get());
                  return fxs.c_str();
                })
            .def_property(
                "refDistance",
                [](pannerspatializerdata_ptr_t sd) -> float { return sd->_refDistance; },
                [](pannerspatializerdata_ptr_t sd, float val) { sd->_refDistance = val; })
            .def_property(
                "maxDistance",
                [](pannerspatializerdata_ptr_t sd) -> float { return sd->_maxDistance; },
                [](pannerspatializerdata_ptr_t sd, float val) { sd->_maxDistance = val; })
            .def_property(
                "rolloff",
                [](pannerspatializerdata_ptr_t sd) -> float { return sd->_rolloff; },
                [](pannerspatializerdata_ptr_t sd, float val) { sd->_rolloff = val; })
            .def_property(
                "minGainDB",
                [](pannerspatializerdata_ptr_t sd) -> float { return sd->_minGainDB; },
                [](pannerspatializerdata_ptr_t sd, float val) { sd->_minGainDB = val; })
            .def_property(
                "headShadowMix",
                [](pannerspatializerdata_ptr_t sd) -> float { return sd->_headShadowMix; },
                [](pannerspatializerdata_ptr_t sd, float val) { sd->_headShadowMix = val; })
            .def_property(
                "iidBaseFreq",
                [](pannerspatializerdata_ptr_t sd) -> float { return sd->_iidBaseFreq; },
                [](pannerspatializerdata_ptr_t sd, float val) { sd->_iidBaseFreq = val; })
            .def_property(
                "iidMaxFreq",
                [](pannerspatializerdata_ptr_t sd) -> float { return sd->_iidMaxFreq; },
                [](pannerspatializerdata_ptr_t sd, float val) { sd->_iidMaxFreq = val; });
    type_codec->registerStdCodec<pannerspatializerdata_ptr_t>(pannerspatdata_type);
  }
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::audio::singularity
///////////////////////////////////////////////////////////////////////////////
