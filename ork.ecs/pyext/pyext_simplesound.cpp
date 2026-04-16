////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/ecs/SimpleSoundEmitter.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork::ecs {
void pyinit_simplesound(py::module& module_ecs) {
  auto type_codec = python::pb11_typecodec_t::instance();
  /////////////////////////////////////////////////////////////////////////////////
  // SimpleSoundData
  /////////////////////////////////////////////////////////////////////////////////
  auto snddata_type = //
      py::class_<SimpleSoundData, ork::Object, simplesnddata_ptr_t>(
          module_ecs, "SimpleSoundData")
          .def(py::init<>())
          .def(
              "__repr__",
              [](const simplesnddata_ptr_t& s) -> std::string {
                fxstring<256> fxs;
                fxs.format("ecs::SimpleSoundData(%p)", s.get());
                return fxs.c_str();
              })
          .def_property(
              "wavfile_path",
              [](simplesnddata_ptr_t s) -> file::Path { return s->_wavFilePath; },
              [](simplesnddata_ptr_t s, py::object val) {
                if (py::isinstance<file::Path>(val)) {
                  s->_wavFilePath = val.cast<file::Path>();
                } else if (py::isinstance<py::str>(val)) {
                  s->_wavFilePath = file::Path(val.cast<std::string>().c_str());
                }
              })
          .def_property(
              "looping",
              [](simplesnddata_ptr_t s) -> bool { return s->_looping; },
              [](simplesnddata_ptr_t s, bool val) { s->_looping = val; })
          .def_property(
              "spatialize",
              [](simplesnddata_ptr_t s) -> bool { return s->_spatialize; },
              [](simplesnddata_ptr_t s, bool val) { s->_spatialize = val; })
          .def_property(
              "gainDB",
              [](simplesnddata_ptr_t s) -> float { return s->_gainDB; },
              [](simplesnddata_ptr_t s, float val) { s->_gainDB = val; })
          .def_property(
              "pitchOffsetCents",
              [](simplesnddata_ptr_t s) -> float { return s->_pitchOffsetCents; },
              [](simplesnddata_ptr_t s, float val) { s->_pitchOffsetCents = val; })
          .def_property(
              "fadeInTime",
              [](simplesnddata_ptr_t s) -> float { return s->_fadeInTime; },
              [](simplesnddata_ptr_t s, float val) { s->_fadeInTime = val; })
          .def_property(
              "fadeOutTime",
              [](simplesnddata_ptr_t s) -> float { return s->_fadeOutTime; },
              [](simplesnddata_ptr_t s, float val) { s->_fadeOutTime = val; })
          .def_property(
              "outputBusName",
              [](simplesnddata_ptr_t s) -> std::string { return s->_outputBusName; },
              [](simplesnddata_ptr_t s, std::string val) { s->_outputBusName = val; });
  type_codec->registerStdCodec<simplesnddata_ptr_t>(snddata_type);
  /////////////////////////////////////////////////////////////////////////////////
  // SimpleSoundEmitterData
  /////////////////////////////////////////////////////////////////////////////////
  auto emitterdata_type = //
      py::class_<SimpleSoundEmitterData, ComponentData, simplesoundemitterdata_ptr_t>(
          module_ecs, "SimpleSoundEmitterData")
          .def(
              "__repr__",
              [](const simplesoundemitterdata_ptr_t& cd) -> std::string {
                fxstring<256> fxs;
                fxs.format("ecs::SimpleSoundEmitterData(%p)", cd.get());
                return fxs.c_str();
              })
          .def_property(
              "soundName",
              [](simplesoundemitterdata_ptr_t cd) -> std::string { return cd->_soundName; },
              [](simplesoundemitterdata_ptr_t cd, std::string val) { cd->_soundName = val; })
          .def_property(
              "autoPlay",
              [](simplesoundemitterdata_ptr_t cd) -> bool { return cd->_autoPlay; },
              [](simplesoundemitterdata_ptr_t cd, bool val) { cd->_autoPlay = val; })
          .def_property(
              "enabled",
              [](simplesoundemitterdata_ptr_t cd) -> bool { return cd->_enabled; },
              [](simplesoundemitterdata_ptr_t cd, bool val) { cd->_enabled = val; })
          .def_property(
              "gainOffsetDB",
              [](simplesoundemitterdata_ptr_t cd) -> float { return cd->_gainOffsetDB; },
              [](simplesoundemitterdata_ptr_t cd, float val) { cd->_gainOffsetDB = val; })
          .def_property(
              "pitchOffsetCents",
              [](simplesoundemitterdata_ptr_t cd) -> float { return cd->_pitchOffsetCents; },
              [](simplesoundemitterdata_ptr_t cd, float val) { cd->_pitchOffsetCents = val; });
  type_codec->registerStdCodec<simplesoundemitterdata_ptr_t>(emitterdata_type);
  /////////////////////////////////////////////////////////////////////////////////
  // SimpleSoundEmitterSystemData
  /////////////////////////////////////////////////////////////////////////////////
  auto sysdata_type = //
      py::class_<SimpleSoundEmitterSystemData, SystemData, simplesoundemittersysdata_ptr_t>(
          module_ecs, "SimpleSoundEmitterSystemData")
          .def(
              "__repr__",
              [](simplesoundemittersysdata_ptr_t sd) -> std::string {
                fxstring<256> fxs;
                fxs.format("ecs::SimpleSoundEmitterSystemData(%p)", sd.get());
                return fxs.c_str();
              })
          .def_property_readonly(
              "sounds",
              [](simplesoundemittersysdata_ptr_t sd) -> std::map<std::string, simplesnddata_ptr_t>& { return sd->_sounds; },
              py::return_value_policy::reference_internal)
          .def(
              "addSound",
              [](simplesoundemittersysdata_ptr_t sd, std::string name, simplesnddata_ptr_t snd) { sd->_sounds[name] = snd; })
          .def(
              "removeSound",
              [](simplesoundemittersysdata_ptr_t sd, std::string name) { sd->_sounds.erase(name); })
          .def_property(
              "spatializer",
              [](simplesoundemittersysdata_ptr_t sd) -> audio::singularity::spatializerdata_ptr_t { return sd->_spatializer; },
              [](simplesoundemittersysdata_ptr_t sd, audio::singularity::spatializerdata_ptr_t val) { sd->_spatializer = val; });
  type_codec->registerStdCodec<simplesoundemittersysdata_ptr_t>(sysdata_type);
}
} // namespace ork::ecs
