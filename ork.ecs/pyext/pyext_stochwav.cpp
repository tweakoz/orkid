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
              "probability",
              [](stochwavsnd_ptr_t s) -> float { return s->_probability; },
              [](stochwavsnd_ptr_t s, float val) { s->_probability = val; })
          .def_property(
              "postSilenceMin",
              [](stochwavsnd_ptr_t s) -> float { return s->_postSilenceMin; },
              [](stochwavsnd_ptr_t s, float val) { s->_postSilenceMin = val; })
          .def_property(
              "postSilenceMax",
              [](stochwavsnd_ptr_t s) -> float { return s->_postSilenceMax; },
              [](stochwavsnd_ptr_t s, float val) { s->_postSilenceMax = val; })
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
              "outputBusName",
              [](stochwavemitterdata_ptr_t cd) -> std::string { return cd->_outputBusName; },
              [](stochwavemitterdata_ptr_t cd, std::string val) { cd->_outputBusName = val; })
          .def_property(
              "masterGainDB",
              [](stochwavemitterdata_ptr_t cd) -> float { return cd->_masterGainDB; },
              [](stochwavemitterdata_ptr_t cd, float val) { cd->_masterGainDB = val; })
          .def_property(
              "maxVoices",
              [](stochwavemitterdata_ptr_t cd) -> int { return cd->_maxVoices; },
              [](stochwavemitterdata_ptr_t cd, int val) { cd->_maxVoices = val; })
          .def_property(
              "enabled",
              [](stochwavemitterdata_ptr_t cd) -> bool { return cd->_enabled; },
              [](stochwavemitterdata_ptr_t cd, bool val) { cd->_enabled = val; })
          .def_property(
              "spatializer",
              [](stochwavemitterdata_ptr_t cd) -> audio::singularity::spatializerdata_ptr_t { return cd->_spatializer; },
              [](stochwavemitterdata_ptr_t cd, audio::singularity::spatializerdata_ptr_t val) { cd->_spatializer = val; })
          .def_property_readonly(
              "sounds",
              [](stochwavemitterdata_ptr_t cd) -> std::map<std::string, stochwavsnd_ptr_t>& { return cd->_sounds; },
              py::return_value_policy::reference_internal)
          .def(
              "addSound",
              [](stochwavemitterdata_ptr_t cd, std::string name, stochwavsnd_ptr_t snd) { cd->_sounds[name] = snd; })
          .def(
              "removeSound",
              [](stochwavemitterdata_ptr_t cd, std::string name) { cd->_sounds.erase(name); })
          .def(
              "clearSounds",
              [](stochwavemitterdata_ptr_t cd) { cd->_sounds.clear(); });
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
              });
  type_codec->registerStdCodec<stochwavemittersysdata_ptr_t>(sysdata_type);
}
} // namespace ork::ecs
