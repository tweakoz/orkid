#pragma once

#include "dspblocks.h"
#include "shelveeq.h"

namespace ork::audio::singularity {

struct PARABASS : public DspBlock {
  PARABASS(const DspBlockData* dbd);
  LoShelveEq _lsq;
  void compute(DspBuffer& dspbuf) final;
  void doKeyOn(const KeyOnInfo& koi) final;
};
struct STEEP_RESONANT_BASS : public DspBlock {
  STEEP_RESONANT_BASS(const DspBlockData* dbd);
  LoShelveEq _lsq;
  TrapSVF _svf;
  float _filtFC;
  void compute(DspBuffer& dspbuf) final;
  void doKeyOn(const KeyOnInfo& koi) final;
};
struct PARAMID : public DspBlock {
  PARAMID(const DspBlockData* dbd);
  BiQuad _biquad;
  void compute(DspBuffer& dspbuf) final;
  void doKeyOn(const KeyOnInfo& koi) final;
};
struct PARATREBLE : public DspBlock {
  PARATREBLE(const DspBlockData* dbd);
  HiShelveEq _lsq;
  void compute(DspBuffer& dspbuf) final;
  void doKeyOn(const KeyOnInfo& koi) final;
};
struct PARAMETRIC_EQ : public DspBlock {
  PARAMETRIC_EQ(const DspBlockData* dbd);
  BiQuad _biquad;
  Fil4Paramsect _peq;
  ParaOne _peq1;
  float _smoothFC;
  float _smoothW;
  float _smoothG;
  void compute(DspBuffer& dspbuf) final;
  void doKeyOn(const KeyOnInfo& koi) final;
};

} // namespace ork::audio::singularity
