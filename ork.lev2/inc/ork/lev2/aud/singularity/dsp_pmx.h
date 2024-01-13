////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once
#include <ork/kernel/svariant.h>
#include "synthdata.h"
#include "dspblocks.h"
#include "fmosc.h"
#include <ork/file/path.h>

namespace ork::audio::singularity {

struct Tx81zProgData;
using tx81zprgdata_ptr_t = std::shared_ptr<Tx81zProgData>;

struct DspBuffer;

///////////////////////////////////////////////////////////////////////////////

struct op4frame {
  float _envout = 0.0f;
  int _envph    = 0;
  float _mi     = 0.0f;
  float _r      = 0.0f;
  float _f      = 0.0f;
  int _ar       = 0;
  int _d1r      = 0;
  int _d1l      = 0;
  int _d2r      = 0;
  int _rr       = 0;
  int _egshift  = 0;
  int _wav      = 0;
  int _olev     = 0;
};

hudpanel_ptr_t create_op4panel();

////////////////////////////////////////////////////////////////
// PMX : Yamaha (Dx/Tx) Style Phase Modulation Oscillator
//  supports:
//   up to 8 modulator inputs
//   definable waveforms
//   arbitrary modulation inputs
//    (can be samples, or any other singularity 1-8ch dspblock)
////////////////////////////////////////////////////////////////

struct PMXData final : public DspBlockData {

  DeclareConcreteX(PMXData, DspBlockData);

  static constexpr int kmaxmodulators = 8;
  using inpchanarray_t                = int[kmaxmodulators];

  PMXData(std::string name = "X");
  dspblk_ptr_t createInstance() const override;
  void addPmInput(int dspchannel);

  int _inpchannel = 0;
  float _feedback = 0.0f;
  float _modIndex = 1.0f;
  std::vector<int> _pmInpChannels;
  PmOscData _pmoscdata;
  int _opindex = 0;
  tx81zprgdata_ptr_t _txprogramdata; // temp for debugging
  bool _modulator = false;
};

struct PMX final : public DspBlock {
  using dataclass_t = PMXData;
  PMX(const DspBlockData* dbd);
  void compute(DspBuffer& dspbuf) override;
  void doKeyOn(const KeyOnInfo& koi) override;
  void doKeyOff() override;
  PmOsc _pmosc;
  float _modIndex         = 1.0f;
  const PMXData* _pmxdata = nullptr;
  bool _modulator         = false;
  float _amp              = 0.0f;
  float _frq              = 0.0f;
};

////////////////////////////////////////////////////////////////
// PMXMix : Yamaha (Dx/Tx) Style Phase Modulation Oscillator mixer
//  mix up to 8PM oscillators (each on their own dsp-channel) into 1 dsp-channel
////////////////////////////////////////////////////////////////

struct PMXMixData final : public DspBlockData {
  DeclareConcreteX(PMXMixData, DspBlockData);
  PMXMixData(std::string name = "");
  dspblk_ptr_t createInstance() const override;
  void addInputChannel(int chan);
  static constexpr int kmaxinputs = 8;
  std::vector<int> _pmixInpChannels;
};

struct PMXMix final : public DspBlock {
  using dataclass_t = PMXMixData;
  PMXMix(const DspBlockData* dbd);
  void compute(DspBuffer& dspbuf) override;
  void doKeyOn(const KeyOnInfo& koi) override;
  void doKeyOff() override;
  const PMXMixData* _pmixdata = nullptr;
  float _finalamp             = 1.0f;
};

} // namespace ork::audio::singularity
