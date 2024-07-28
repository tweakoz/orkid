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
void pyinit_aud_singularity_synth(py::module& module_lev2);
void pyinit_aud_singularity_datas(py::module& module_lev2);
void pyinit_aud_singularity_ui(py::module& module_lev2);
void pyinit_aud_singularity_sequencer(py::module& singmodule);
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
  pyinit_aud_singularity_synth(singmodule);
  pyinit_aud_singularity_datas(singmodule);
  pyinit_aud_singularity_ui(singmodule);
  pyinit_aud_singularity_sequencer(singmodule);
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::audio::singularity
///////////////////////////////////////////////////////////////////////////////
