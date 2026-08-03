////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <pybind11/numpy.h>
#include <ork/lev2/aud/audiodevice.h>
#include <ork/lev2/aud/singularity/cz1.h>
#include <ork/lev2/aud/singularity/krzdata.h>
#include <ork/lev2/aud/singularity/tx81z.h>
#include <ork/lev2/aud/singularity/fxgen.h>
#include <ork/lev2/aud/singularity/hud.h>
#include <ork/lev2/aud/singularity/filters.h>
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
void pyinit_aud_singularity_synth(py::module& module_lev2);
void pyinit_aud_singularity_datas(py::module& module_lev2);
void pyinit_aud_singularity_ui(py::module& module_lev2);
void pyinit_aud_singularity_sequencer(py::module& singmodule);
void pyinit_aud_singularity_soundfield(py::module& singmodule);
///////////////////////////////////////////////////////////////////////////////
void pyinit_aud_singularity(py::module& module_lev2) {
  auto singmodule = module_lev2.def_submodule("singularity", "orkid audio synthesizer");
  singmodule.def("decibelsToLinear", [](float dB) -> float { return decibel_to_linear_amp_ratio(dB); });
  singmodule.def("midiNoteToFrequency", [](float midinote) -> float {
    return midi_note_to_frequency(midinote);
    });
  singmodule.def("frequencyToMidiNote", [](float frq) -> float {
    return frequency_to_midi_note(frq);
    });
  singmodule.def("linearFrequencyRatioToCents", [](float fratio) -> float { //
    return linear_freq_ratio_to_cents(fratio);
  });
  singmodule.def("baseDataPath", [] -> file::Path { return basePath(); });

  /////////////////////////////////////////////////////////////////////////////
  // BiQuad filter
  /////////////////////////////////////////////////////////////////////////////
  auto biquad_type = py::class_<BiQuad, biquad_ptr_t>(singmodule, "BiQuad")
    .def("__repr__", [](biquad_ptr_t bq) -> std::string {
      return FormatString("BiQuad(%p)", bq.get());
    })
    .def("clear", &BiQuad::Clear)
    .def("compute", &BiQuad::compute)
    .def("setLpf", &BiQuad::SetLpf)
    .def("setHpf", &BiQuad::SetHpf)
    .def("setLpfNoQ", &BiQuad::SetLpfNoQ)
    .def("setLpfReson", &BiQuad::SetLpfReson)
    .def("setBpfWithQ", &BiQuad::SetBpfWithQ)
    .def("setBpfWithBWoct", &BiQuad::SetBpfWithBWoct)
    .def("setNotchWithQ", &BiQuad::SetNotchWithQ)
    .def("setNotchWithBWoct", &BiQuad::SetNotchWithBWoct)
    .def("setLowShelf", &BiQuad::SetLowShelf)
    .def("setHighShelf", &BiQuad::SetHighShelf)
    .def("setParametric", &BiQuad::SetParametric);
  singmodule.def("biquad", []() -> biquad_ptr_t { return std::make_shared<BiQuad>(); });

  /////////////////////////////////////////////////////////////////////////////
  // Dynamics Compressor
  /////////////////////////////////////////////////////////////////////////////
  auto compressor_type = py::class_<Compressor, compressor_ptr_t>(singmodule, "Compressor")
    .def("__repr__", [](compressor_ptr_t comp) -> std::string {
      return FormatString("Compressor(%p)", comp.get());
    })
    .def("clear", &Compressor::clear)
    .def("compute", &Compressor::compute)
    .def("computeBlock", [](Compressor& comp, py::array_t<float> samples) {
      auto buf = samples.mutable_unchecked<1>();
      comp.computeBlock(buf.mutable_data(0), buf.shape(0));
    })
    .def_property("threshold", &Compressor::getThreshold, &Compressor::setThreshold)
    .def_property("ratio", &Compressor::getRatio, &Compressor::setRatio)
    .def_property("attack", [](Compressor&) { return 0.0f; }, &Compressor::setAttack)  // write-only (no stored ms)
    .def_property("release", [](Compressor&) { return 0.0f; }, &Compressor::setRelease)  // write-only (no stored ms)
    .def_property("makeup_gain", &Compressor::getMakeupGain, &Compressor::setMakeupGain)
    .def_property("knee", &Compressor::getKnee, &Compressor::setKnee)
    .def("setupForVoice", &Compressor::setupForVoice,
         py::arg("threshold_dB") = -12.0f, py::arg("ratio") = 3.0f)
    .def("setupGentle", &Compressor::setupGentle)
    .def("setupMedium", &Compressor::setupMedium)
    .def("setupHeavy", &Compressor::setupHeavy)
    .def_property_readonly("gain_reduction", &Compressor::getGainReduction);
  singmodule.def("compressor", []() -> compressor_ptr_t { return std::make_shared<Compressor>(); });

  /////////////////////////////////////////////////////////////////////////////
  // NoiseGate
  /////////////////////////////////////////////////////////////////////////////
  auto noisegate_type = py::class_<NoiseGate, noisegate_ptr_t>(singmodule, "NoiseGate")
    .def("__repr__", [](noisegate_ptr_t ng) -> std::string {
      return FormatString("NoiseGate(%p)", ng.get());
    })
    .def("clear", &NoiseGate::clear)
    .def("compute", &NoiseGate::compute)
    .def("computeBlock", [](NoiseGate& ng, py::array_t<float> samples) {
      auto buf = samples.mutable_unchecked<1>();
      ng.computeBlock(buf.mutable_data(0), buf.shape(0));
    })
    .def_property("threshold", &NoiseGate::getThreshold, &NoiseGate::setThreshold)
    .def_property("attack", &NoiseGate::getAttack, &NoiseGate::setAttack)
    .def_property("release", &NoiseGate::getRelease, &NoiseGate::setRelease)
    .def_property("max_attenuation", &NoiseGate::getMaxAttenuation, &NoiseGate::setMaxAttenuation)
    .def_property("input_gain", &NoiseGate::getInputGain, &NoiseGate::setInputGain)
    .def_property("output_gain", &NoiseGate::getOutputGain, &NoiseGate::setOutputGain)
    .def("setupForVoice", &NoiseGate::setupForVoice)
    .def_property_readonly("envelope", &NoiseGate::getEnvelope)
    .def_property_readonly("energy", &NoiseGate::getEnergy);
  singmodule.def("noisegate", []() -> noisegate_ptr_t { return std::make_shared<NoiseGate>(); });

  /////////////////////////////////////////////////////////////////////////////
  // RNNoise Denoiser
  /////////////////////////////////////////////////////////////////////////////
  auto rnnoise_type = py::class_<RNNoiseDenoise, rnnoise_ptr_t>(singmodule, "RNNoiseDenoise")
    .def(py::init<>())
    .def("__repr__", [](rnnoise_ptr_t rn) -> std::string {
      return FormatString("RNNoiseDenoise(%p)", rn.get());
    })
    .def("clear", &RNNoiseDenoise::clear)
    .def("compute", &RNNoiseDenoise::compute)
    .def("computeBlock", [](RNNoiseDenoise& rn, py::array_t<float> samples) {
      auto buf = samples.mutable_unchecked<1>();
      rn.computeBlock(buf.mutable_data(0), buf.shape(0));
    })
    .def_property_readonly("vad_probability", &RNNoiseDenoise::getVadProbability);
  singmodule.def("rnnoise", []() -> rnnoise_ptr_t { return std::make_shared<RNNoiseDenoise>(); });

  pyinit_aud_singularity_synth(singmodule);
  pyinit_aud_singularity_datas(singmodule);
  pyinit_aud_singularity_ui(singmodule);
  pyinit_aud_singularity_sequencer(singmodule);
  pyinit_aud_singularity_soundfield(singmodule);
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::audio::singularity
///////////////////////////////////////////////////////////////////////////////
