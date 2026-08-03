////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/lev2/aud/singularity/soundfield.h>
#include <ork/lev2/aud/singularity/synth.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::audio::singularity {
///////////////////////////////////////////////////////////////////////////////
using namespace ork::lev2;
///////////////////////////////////////////////////////////////////////////////
void pyinit_aud_singularity_soundfield(py::module& singmodule) {
  /////////////////////////////////////////////////////////////////////////////////
  auto type_codec = python::pb11_typecodec_t::instance();
  /////////////////////////////////////////////////////////////////////////////////
  auto sfield_type = //
      py::class_<SoundField, soundfield_ptr_t>(singmodule, "SoundField")
          .def_static(
              "instance", //
              []() -> soundfield_ptr_t { return SoundField::instance(); })
          .def_static("tearDown", []() { SoundField::tearDown(); })
          .def(
              "__repr__",
              [](soundfield_ptr_t f) -> std::string {
                return FormatString("SoundField(%p) probes<%zu>", f.get(), f->numProbes());
              })
          .def(
              "createProbe", //
              [](soundfield_ptr_t f, std::string path, bool loop) -> int { return f->createProbe(path, loop); },
              py::arg("path"),
              py::arg("loop") = true)
          .def(
              "destroyProbe", //
              [](soundfield_ptr_t f, int probeID) { f->destroyProbe(probeID); })
          .def(
              "setProbeParams", //
              [](soundfield_ptr_t f,
                 int probeID,
                 fvec3 position,
                 float refDistance,
                 float maxDistance,
                 float rolloff,
                 float gain) {
                SoundFieldProbeParams params;
                params._position    = position;
                params._refDistance = refDistance;
                params._maxDistance = maxDistance;
                params._rolloff     = rolloff;
                params._gain        = gain;
                f->setProbeParams(probeID, params);
              },
              py::arg("probeID"),
              py::arg("position"),
              py::arg("refDistance") = 5.0f,
              py::arg("maxDistance") = 50.0f,
              py::arg("rolloff")     = 1.0f,
              py::arg("gain")        = 1.0f)
          .def(
              "update", //
              [](soundfield_ptr_t f, float dt) { f->update(dt); })
          .def(
              "probeWeight", //
              [](soundfield_ptr_t f, int probeID) -> float { return f->probeWeight(probeID); })
          .def(
              "probeSlot", //
              [](soundfield_ptr_t f, int probeID) -> int { return f->probeSlot(probeID); })
          .def_property(
              "masterGain", //
              [](soundfield_ptr_t f) -> float { return f->_masterGain; },
              [](soundfield_ptr_t f, float gain) { f->_masterGain = gain; })
          .def_property(
              "slewTau", //
              [](soundfield_ptr_t f) -> float { return f->_slewTau; },
              [](soundfield_ptr_t f, float tau) { f->_slewTau = tau; })
          .def_property_readonly(
              "numProbes", //
              [](soundfield_ptr_t f) -> int { return int(f->numProbes()); })
          .def_property_readonly(
              "numActiveSlots", //
              [](soundfield_ptr_t f) -> int { return f->numActiveSlots(); })
          .def_property_readonly(
              "underrunCount", //
              [](soundfield_ptr_t f) -> int { return f->underrunCount(); })
          .def_property_readonly(
              "maxActiveProbes", //
              [](soundfield_ptr_t f) -> int { return kmaxActiveProbes; });
  type_codec->registerStdCodec<soundfield_ptr_t>(sfield_type);
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::audio::singularity
///////////////////////////////////////////////////////////////////////////////
