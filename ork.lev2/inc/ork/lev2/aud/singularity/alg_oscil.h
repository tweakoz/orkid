////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include "dspblocks.h"

namespace ork::audio::singularity {

///////////////////////////////////////////////////////////////////////////////
// oscils
///////////////////////////////////////////////////////////////////////////////

struct SWPLUSSHP : public DspBlock {
  SWPLUSSHP(const DspBlockData* dbd);
  void compute(DspBuffer& dspbuf) final;
  PolyBLEP _pblep;
  void doKeyOn(const KeyOnInfo& koi) final;
};
struct SAWPLUS : public DspBlock {
  SAWPLUS(const DspBlockData* dbd);
  void compute(DspBuffer& dspbuf) final;
  PolyBLEP _pblep;
  void doKeyOn(const KeyOnInfo& koi) final;
};
struct SINE : public DspBlock {
  SINE(const DspBlockData* dbd);
  void compute(DspBuffer& dspbuf) final;
  PolyBLEP _pblep;
  void doKeyOn(const KeyOnInfo& koi) final;
};
struct SAW : public DspBlock {
  SAW(const DspBlockData* dbd);
  void compute(DspBuffer& dspbuf) final;
  PolyBLEP _pblep;
  void doKeyOn(const KeyOnInfo& koi) final;
};
struct SQUARE : public DspBlock {
  SQUARE(const DspBlockData* dbd);
  void compute(DspBuffer& dspbuf) final;
  PolyBLEP _pblep;
  void doKeyOn(const KeyOnInfo& koi) final;
};
struct SINEPLUS : public DspBlock {
  SINEPLUS(const DspBlockData* dbd);
  void compute(DspBuffer& dspbuf) final;
  PolyBLEP _pblep;
  void doKeyOn(const KeyOnInfo& koi) final;
};
struct SHAPEMODOSC : public DspBlock {
  SHAPEMODOSC(const DspBlockData* dbd);
  void compute(DspBuffer& dspbuf) final;
  PolyBLEP _pblep;
  void doKeyOn(const KeyOnInfo& koi) final;
};
struct PLUSSHAPEMODOSC : public DspBlock {
  PLUSSHAPEMODOSC(const DspBlockData* dbd);
  void compute(DspBuffer& dspbuf) final;
  PolyBLEP _pblep;
  void doKeyOn(const KeyOnInfo& koi) final;
};
struct SYNCM : public DspBlock {
  SYNCM(const DspBlockData* dbd);
  void compute(DspBuffer& dspbuf) final;
  float _phase;
  float _phaseInc;
  void doKeyOn(const KeyOnInfo& koi) final;
};
struct SYNCS : public DspBlock {
  SYNCS(const DspBlockData* dbd);
  void compute(DspBuffer& dspbuf) final;
  PolyBLEP _pblep;
  float _prvmaster;
  void doKeyOn(const KeyOnInfo& koi) final;
};
struct PWM : public DspBlock {
  PWM(const DspBlockData* dbd);
  void compute(DspBuffer& dspbuf) final;
  PolyBLEP _pblep;
  void doKeyOn(const KeyOnInfo& koi) final;
};

struct NOISE : public DspBlock {
  NOISE(const DspBlockData* dbd);
  void compute(DspBuffer& dspbuf) final;
  void doKeyOn(const KeyOnInfo& koi) final;
  void doKeyOff() final;
};

} // namespace ork::audio::singularity
