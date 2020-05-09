#pragma once

#include "dspblocks.h"

namespace ork::audio::singularity {
///////////////////////////////////////////////////////////////////////////////
// amp blocks
///////////////////////////////////////////////////////////////////////////////

struct AMP : public DspBlock {
  static void initBlock(dspblkdata_ptr_t blockdata);
  AMP(const DspBlockData& dbd);
  void compute(DspBuffer& dspbuf) final;
  void doKeyOn(const DspKeyOnInfo& koi) final;
  float _filt;
  float _upan, _lpan;
};
struct PLUSAMP : public DspBlock {
  PLUSAMP(const DspBlockData& dbd);
  void compute(DspBuffer& dspbuf) final;
  void doKeyOn(const DspKeyOnInfo& koi) final;
  float _filt;
};
struct XAMP : public DspBlock {
  XAMP(const DspBlockData& dbd);
  void compute(DspBuffer& dspbuf) final;
  void doKeyOn(const DspKeyOnInfo& koi) final;
  float _filt;
};
struct GAIN : public DspBlock {
  GAIN(const DspBlockData& dbd);
  void compute(DspBuffer& dspbuf) final;
  float _filt;
};

struct BANGAMP : public DspBlock {
  BANGAMP(const DspBlockData& dbd);
  void compute(DspBuffer& dspbuf) final;
  void doKeyOn(const DspKeyOnInfo& koi) final;
  float _smooth;
};

struct AMPU_AMPL : public DspBlock {
  AMPU_AMPL(const DspBlockData& dbd);
  void compute(DspBuffer& dspbuf) final;
  void doKeyOn(const DspKeyOnInfo& koi) final;
  float _filtU, _filtL;
  float _upan, _lpan;
};

struct BAL_AMP : public DspBlock {
  BAL_AMP(const DspBlockData& dbd);
  void compute(DspBuffer& dspbuf) final;
  void doKeyOn(const DspKeyOnInfo& koi) final;
  float _filt;
};

struct XGAIN : public DspBlock {
  XGAIN(const DspBlockData& dbd);
  void compute(DspBuffer& dspbuf) final;
  void doKeyOn(const DspKeyOnInfo& koi) final;
  float _filt;
};

struct XFADE : public DspBlock {
  XFADE(const DspBlockData& dbd);
  void compute(DspBuffer& dspbuf) final;
  void doKeyOn(const DspKeyOnInfo& koi) final;
  float _pumix, _plmix; // for smoothing
};
struct PANNER : public DspBlock {
  PANNER(const DspBlockData& dbd);
  void compute(DspBuffer& dspbuf) final;
  void doKeyOn(const DspKeyOnInfo& koi) final;
  float _plmix, _prmix; // for smoothing
};

} // namespace ork::audio::singularity
