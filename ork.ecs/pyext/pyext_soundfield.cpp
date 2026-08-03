////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/ecs/SoundFieldProbe.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork::ecs {
void pyinit_soundfield(py::module& module_ecs) {
  auto type_codec = python::pb11_typecodec_t::instance();
  /////////////////////////////////////////////////////////////////////////////////
  // SoundFieldProbeData
  /////////////////////////////////////////////////////////////////////////////////
  auto probedata_type = //
      py::class_<SoundFieldProbeData, ComponentData, soundfieldprobedata_ptr_t>(
          module_ecs, "SoundFieldProbeData")
          .def(
              "__repr__",
              [](const soundfieldprobedata_ptr_t& c) -> std::string {
                fxstring<256> fxs;
                fxs.format("ecs::SoundFieldProbeData(%p)", c.get());
                return fxs.c_str();
              })
          .def_property(
              "ambixAsset",
              [](soundfieldprobedata_ptr_t c) -> file::Path { return c->_ambixAsset; },
              // str or Path, same as StochWavSound.wavFilePath — scene authors
              // write these as bare "data://..." literals.
              [](soundfieldprobedata_ptr_t c, py::object val) {
                if (py::isinstance<file::Path>(val)) {
                  c->_ambixAsset = val.cast<file::Path>();
                } else if (py::isinstance<py::str>(val)) {
                  c->_ambixAsset = file::Path(val.cast<std::string>().c_str());
                } else {
                  throw std::runtime_error("SoundFieldProbeData.ambixAsset: expected str or Path");
                }
              })
          .def_property(
              "gain",
              [](soundfieldprobedata_ptr_t c) -> float { return c->_gain; },
              [](soundfieldprobedata_ptr_t c, float val) { c->_gain = val; })
          .def_property(
              "loop",
              [](soundfieldprobedata_ptr_t c) -> bool { return c->_loop; },
              [](soundfieldprobedata_ptr_t c, bool val) { c->_loop = val; })
          .def_property(
              "startPaused",
              [](soundfieldprobedata_ptr_t c) -> bool { return c->_startPaused; },
              [](soundfieldprobedata_ptr_t c, bool val) { c->_startPaused = val; })
          .def_property(
              "mode", // 0 == RADIAL; ZONE arrives at SF3
              [](soundfieldprobedata_ptr_t c) -> int { return c->_mode; },
              [](soundfieldprobedata_ptr_t c, int val) { c->_mode = val; })
          .def_property(
              "refDistance",
              [](soundfieldprobedata_ptr_t c) -> float { return c->_refDistance; },
              [](soundfieldprobedata_ptr_t c, float val) { c->_refDistance = val; })
          .def_property(
              "maxDistance",
              [](soundfieldprobedata_ptr_t c) -> float { return c->_maxDistance; },
              [](soundfieldprobedata_ptr_t c, float val) { c->_maxDistance = val; })
          .def_property(
              "rolloff",
              [](soundfieldprobedata_ptr_t c) -> float { return c->_rolloff; },
              [](soundfieldprobedata_ptr_t c, float val) { c->_rolloff = val; });
  type_codec->registerStdCodec<soundfieldprobedata_ptr_t>(probedata_type);
  /////////////////////////////////////////////////////////////////////////////////
  // SoundFieldSystemData
  /////////////////////////////////////////////////////////////////////////////////
  auto sysdata_type = //
      py::class_<SoundFieldSystemData, SystemData, soundfieldsysdata_ptr_t>(
          module_ecs, "SoundFieldSystemData")
          .def(
              "__repr__",
              [](soundfieldsysdata_ptr_t sd) -> std::string {
                fxstring<256> fxs;
                fxs.format("ecs::SoundFieldSystemData(%p)", sd.get());
                return fxs.c_str();
              })
          .def_property(
              "masterGainDB",
              [](soundfieldsysdata_ptr_t sd) -> float { return sd->_masterGainDB; },
              [](soundfieldsysdata_ptr_t sd, float val) { sd->_masterGainDB = val; })
          .def_property(
              "slewTime",
              [](soundfieldsysdata_ptr_t sd) -> float { return sd->_slewTime; },
              [](soundfieldsysdata_ptr_t sd, float val) { sd->_slewTime = val; });
  type_codec->registerStdCodec<soundfieldsysdata_ptr_t>(sysdata_type);
}
} // namespace ork::ecs
