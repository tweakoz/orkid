////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

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
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
struct ParametricEqData : public DspBlockData {
  ParametricEqData(std::string name);
  // krzname  "PARAMETRIC EQ"
  dspblk_ptr_t createInstance() const override;
};
struct ParametricEq : public DspBlock {
  using dataclass_t = ParametricEqData;
  ParametricEq(const ParametricEqData* dbd);
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
