////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/ecs/StochWavSoundEmitter.h>
#include <ork/lev2/aud/spatializer.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork::ecs {
void pyinit_stochwav(py::module& module_ecs) {
  auto type_codec = python::pb11_typecodec_t::instance();
  /////////////////////////////////////////////////////////////////////////////////
  // StochWavSound
  /////////////////////////////////////////////////////////////////////////////////
  auto stochwav_type = //
      py::class_<StochWavSound, ork::Object, stochwavsnd_ptr_t>(
          module_ecs, "StochWavSound")
          .def(py::init<>())
          .def(
              "__repr__",
              [](const stochwavsnd_ptr_t& s) -> std::string {
                fxstring<256> fxs;
                fxs.format("ecs::StochWavSound(%p)", s.get());
                return fxs.c_str();
              })
          .def_property(
              "wavFilePath",
              [](stochwavsnd_ptr_t s) -> file::Path { return s->_wavFilePath; },
              [](stochwavsnd_ptr_t s, file::Path val) { s->_wavFilePath = val; })
          .def_property(
              "burstRate",
              [](stochwavsnd_ptr_t s) -> float { return s->_burstRate; },
              [](stochwavsnd_ptr_t s, float val) { s->_burstRate = val; })
          .def_property(
              "burstCountMin",
              [](stochwavsnd_ptr_t s) -> int { return s->_burstCountMin; },
              [](stochwavsnd_ptr_t s, int val) { s->_burstCountMin = val; })
          .def_property(
              "burstCountMax",
              [](stochwavsnd_ptr_t s) -> int { return s->_burstCountMax; },
              [](stochwavsnd_ptr_t s, int val) { s->_burstCountMax = val; })
          .def_property(
              "intraBurstRate",
              [](stochwavsnd_ptr_t s) -> float { return s->_intraBurstRate; },
              [](stochwavsnd_ptr_t s, float val) { s->_intraBurstRate = val; })
          .def_property(
              "selectionWeight",
              [](stochwavsnd_ptr_t s) -> float { return s->_selectionWeight; },
              [](stochwavsnd_ptr_t s, float val) { s->_selectionWeight = val; })
          .def_property(
              "pitchVarianceCents",
              [](stochwavsnd_ptr_t s) -> float { return s->_pitchVarianceCents; },
              [](stochwavsnd_ptr_t s, float val) { s->_pitchVarianceCents = val; })
          .def_property(
              "gainMinDB",
              [](stochwavsnd_ptr_t s) -> float { return s->_gainMinDB; },
              [](stochwavsnd_ptr_t s, float val) { s->_gainMinDB = val; })
          .def_property(
              "gainMaxDB",
              [](stochwavsnd_ptr_t s) -> float { return s->_gainMaxDB; },
              [](stochwavsnd_ptr_t s, float val) { s->_gainMaxDB = val; })
          .def_property(
              "fadeInTime",
              [](stochwavsnd_ptr_t s) -> float { return s->_fadeInTime; },
              [](stochwavsnd_ptr_t s, float val) { s->_fadeInTime = val; })
          .def_property(
              "fadeOutTime",
              [](stochwavsnd_ptr_t s) -> float { return s->_fadeOutTime; },
              [](stochwavsnd_ptr_t s, float val) { s->_fadeOutTime = val; });
  type_codec->registerStdCodec<stochwavsnd_ptr_t>(stochwav_type);
  /////////////////////////////////////////////////////////////////////////////////
  // StochSoundGroup
  /////////////////////////////////////////////////////////////////////////////////
  auto soundgroup_type = //
      py::class_<StochSoundGroup, ork::Object, stochsoundgroup_ptr_t>(
          module_ecs, "StochSoundGroup")
          .def(py::init<>())
          .def(
              "__repr__",
              [](const stochsoundgroup_ptr_t& g) -> std::string {
                fxstring<256> fxs;
                fxs.format("ecs::StochSoundGroup(%p)", g.get());
                return fxs.c_str();
              })
          .def_property(
              "outputBusName",
              [](stochsoundgroup_ptr_t g) -> std::string { return g->_outputBusName; },
              [](stochsoundgroup_ptr_t g, std::string val) { g->_outputBusName = val; })
          .def_property(
              "masterGainDB",
              [](stochsoundgroup_ptr_t g) -> float { return g->_masterGainDB; },
              [](stochsoundgroup_ptr_t g, float val) { g->_masterGainDB = val; })
          .def_property(
              "maxVoicesPerGroup",
              [](stochsoundgroup_ptr_t g) -> int { return g->_maxVoicesPerGroup; },
              [](stochsoundgroup_ptr_t g, int val) { g->_maxVoicesPerGroup = val; })
          .def_property(
              "minSpacing",
              [](stochsoundgroup_ptr_t g) -> float { return g->_minSpacing; },
              [](stochsoundgroup_ptr_t g, float val) { g->_minSpacing = val; })
          .def_property(
              "fadeInTime",
              [](stochsoundgroup_ptr_t g) -> float { return g->_fadeInTime; },
              [](stochsoundgroup_ptr_t g, float val) { g->_fadeInTime = val; })
          .def_property(
              "fadeOutTime",
              [](stochsoundgroup_ptr_t g) -> float { return g->_fadeOutTime; },
              [](stochsoundgroup_ptr_t g, float val) { g->_fadeOutTime = val; })
          .def_property_readonly(
              "sounds",
              [](stochsoundgroup_ptr_t g) -> std::map<std::string, stochwavsnd_ptr_t>& { return g->_sounds; },
              py::return_value_policy::reference_internal)
          .def(
              "addSound",
              [](stochsoundgroup_ptr_t g, std::string name, stochwavsnd_ptr_t snd) { g->_sounds[name] = snd; })
          .def(
              "removeSound",
              [](stochsoundgroup_ptr_t g, std::string name) { g->_sounds.erase(name); })
          .def(
              "clearSounds",
              [](stochsoundgroup_ptr_t g) { g->_sounds.clear(); });
  type_codec->registerStdCodec<stochsoundgroup_ptr_t>(soundgroup_type);
  /////////////////////////////////////////////////////////////////////////////////
  // StochWavSoundEmitterData
  /////////////////////////////////////////////////////////////////////////////////
  auto emitterdata_type = //
      py::class_<StochWavSoundEmitterData, ComponentData, stochwavemitterdata_ptr_t>(
          module_ecs, "StochWavSoundEmitterData")
          .def(
              "__repr__",
              [](const stochwavemitterdata_ptr_t& cd) -> std::string {
                fxstring<256> fxs;
                fxs.format("ecs::StochWavSoundEmitterData(%p)", cd.get());
                return fxs.c_str();
              })
          .def_property(
              "groupName",
              [](stochwavemitterdata_ptr_t cd) -> std::string { return cd->_groupName; },
              [](stochwavemitterdata_ptr_t cd, std::string val) { cd->_groupName = val; })
          .def_property(
              "pitchOffsetCents",
              [](stochwavemitterdata_ptr_t cd) -> float { return cd->_pitchOffsetCents; },
              [](stochwavemitterdata_ptr_t cd, float val) { cd->_pitchOffsetCents = val; })
          .def_property(
              "gainOffsetDB",
              [](stochwavemitterdata_ptr_t cd) -> float { return cd->_gainOffsetDB; },
              [](stochwavemitterdata_ptr_t cd, float val) { cd->_gainOffsetDB = val; })
          .def_property(
              "rateScale",
              [](stochwavemitterdata_ptr_t cd) -> float { return cd->_rateScale; },
              [](stochwavemitterdata_ptr_t cd, float val) { cd->_rateScale = val; })
          .def_property(
              "enabled",
              [](stochwavemitterdata_ptr_t cd) -> bool { return cd->_enabled; },
              [](stochwavemitterdata_ptr_t cd, bool val) { cd->_enabled = val; });
  type_codec->registerStdCodec<stochwavemitterdata_ptr_t>(emitterdata_type);
  /////////////////////////////////////////////////////////////////////////////////
  // StochWavSoundEmitterSystemData
  /////////////////////////////////////////////////////////////////////////////////
  auto sysdata_type = //
      py::class_<StochWavSoundEmitterSystemData, SystemData, stochwavemittersysdata_ptr_t>(
          module_ecs, "StochWavSoundEmitterSystemData")
          .def(
              "__repr__",
              [](stochwavemittersysdata_ptr_t sd) -> std::string {
                fxstring<256> fxs;
                fxs.format("ecs::StochWavSoundEmitterSystemData(%p)", sd.get());
                return fxs.c_str();
              })
          .def_property_readonly(
              "soundGroups",
              [](stochwavemittersysdata_ptr_t sd) -> std::map<std::string, stochsoundgroup_ptr_t>& { return sd->_soundGroups; },
              py::return_value_policy::reference_internal)
          .def(
              "addSoundGroup",
              [](stochwavemittersysdata_ptr_t sd, std::string name, stochsoundgroup_ptr_t grp) { sd->_soundGroups[name] = grp; })
          .def(
              "removeSoundGroup",
              [](stochwavemittersysdata_ptr_t sd, std::string name) { sd->_soundGroups.erase(name); })
          .def_property(
              "spatializer",
              [](stochwavemittersysdata_ptr_t sd) -> audio::singularity::spatializerdata_ptr_t { return sd->_spatializer; },
              [](stochwavemittersysdata_ptr_t sd, audio::singularity::spatializerdata_ptr_t val) { sd->_spatializer = val; });
  type_codec->registerStdCodec<stochwavemittersysdata_ptr_t>(sysdata_type);
}
} // namespace ork::ecs
