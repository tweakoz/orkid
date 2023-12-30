////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include "dspblocks.h"

namespace ork::audio::singularity {
///////////////////////////////////////////////////////////////////////////////
// amp blocks
///////////////////////////////////////////////////////////////////////////////
struct AMP_MONOIO_DATA : public DspBlockData {
  AMP_MONOIO_DATA(std::string name);
  dspblk_ptr_t createInstance() const override;
};
struct AMP_MONOIO : public DspBlock {
  using dataclass_t = AMP_MONOIO_DATA;
  AMP_MONOIO(const DspBlockData* dbd);
  void compute(DspBuffer& dspbuf) final;
  void doKeyOn(const KeyOnInfo& koi) final;
  float _filt;
};
///////////////////////////////////////////////////////////////////////////////
struct PLUSAMP_DATA : public DspBlockData {
  PLUSAMP_DATA(std::string name);
  dspblk_ptr_t createInstance() const override;
};
struct PLUSAMP : public DspBlock {
  using dataclass_t = PLUSAMP_DATA;
  static void initBlock(dspblkdata_ptr_t blockdata);
  PLUSAMP(const DspBlockData* dbd);
  void compute(DspBuffer& dspbuf) final;
  void doKeyOn(const KeyOnInfo& koi) final;
  float _filt;
};
///////////////////////////////////////////////////////////////////////////////
struct XAMP_DATA : public DspBlockData {
  XAMP_DATA(std::string name);
  dspblk_ptr_t createInstance() const override;
};
struct XAMP : public DspBlock {
  using dataclass_t = XAMP_DATA;
  static void initBlock(dspblkdata_ptr_t blockdata);
  XAMP(const DspBlockData* dbd);
  void compute(DspBuffer& dspbuf) final;
  void doKeyOn(const KeyOnInfo& koi) final;
  float _filt;
};
///////////////////////////////////////////////////////////////////////////////
struct GAIN_DATA : public DspBlockData {
  GAIN_DATA(std::string name);
  dspblk_ptr_t createInstance() const override;
};
struct GAIN : public DspBlock {
  using dataclass_t = GAIN_DATA;
  GAIN(const DspBlockData* dbd);
  void compute(DspBuffer& dspbuf) final;
  float _filt;
};
///////////////////////////////////////////////////////////////////////////////
struct BANGAMP_DATA : public DspBlockData {
  BANGAMP_DATA(std::string name);
  dspblk_ptr_t createInstance() const override;
};
struct BANGAMP : public DspBlock {
  using dataclass_t = BANGAMP_DATA;
  BANGAMP(const DspBlockData* dbd);
  void compute(DspBuffer& dspbuf) final;
  void doKeyOn(const KeyOnInfo& koi) final;
  float _smooth;
};
///////////////////////////////////////////////////////////////////////////////
struct AMPU_AMPL_DATA : public DspBlockData {
  AMPU_AMPL_DATA(std::string name);
  dspblk_ptr_t createInstance() const override;
};
struct AMPU_AMPL : public DspBlock {
  using dataclass_t = AMPU_AMPL_DATA;
  AMPU_AMPL(const DspBlockData* dbd);
  void compute(DspBuffer& dspbuf) final;
  void doKeyOn(const KeyOnInfo& koi) final;
  float _filtU, _filtL;
  float _upan, _lpan;
};
///////////////////////////////////////////////////////////////////////////////
struct BAL_AMP_DATA : public DspBlockData {
  BAL_AMP_DATA(std::string name);
  dspblk_ptr_t createInstance() const override;
};
struct BAL_AMP : public DspBlock {
  using dataclass_t = BAL_AMP_DATA;
  BAL_AMP(const DspBlockData* dbd);
  void compute(DspBuffer& dspbuf) final;
  void doKeyOn(const KeyOnInfo& koi) final;
  float _filt;
};
///////////////////////////////////////////////////////////////////////////////
struct AMP_MOD_OSC_DATA : public DspBlockData {
  AMP_MOD_OSC_DATA(std::string name);
  dspblk_ptr_t createInstance() const override;
};
struct AMP_MOD_OSC : public DspBlock {
  using dataclass_t = AMP_MOD_OSC_DATA;
  AMP_MOD_OSC(const DspBlockData* dbd);
  void compute(DspBuffer& dspbuf) final;
  void doKeyOn(const KeyOnInfo& koi) final;
  float _filt;
};
///////////////////////////////////////////////////////////////////////////////
struct XGAIN_DATA : public DspBlockData {
  XGAIN_DATA(std::string name);
  dspblk_ptr_t createInstance() const override;
};
struct XGAIN : public DspBlock {
  using dataclass_t = XGAIN_DATA;
  XGAIN(const DspBlockData* dbd);
  void compute(DspBuffer& dspbuf) final;
  void doKeyOn(const KeyOnInfo& koi) final;
  float _filt;
};
///////////////////////////////////////////////////////////////////////////////
struct XFADE_DATA : public DspBlockData {
  XFADE_DATA(std::string name);
  dspblk_ptr_t createInstance() const override;
};
struct XFADE : public DspBlock {
  using dataclass_t = XFADE_DATA;
  XFADE(const DspBlockData* dbd);
  void compute(DspBuffer& dspbuf) final;
  void doKeyOn(const KeyOnInfo& koi) final;
  float _pumix, _plmix; // for smoothing
};
///////////////////////////////////////////////////////////////////////////////
struct PANNER_DATA : public DspBlockData {
  PANNER_DATA(std::string name);
  dspblk_ptr_t createInstance() const override;
};
struct PANNER : public DspBlock {
  using dataclass_t = PANNER_DATA;
  PANNER(const DspBlockData* dbd);
  void compute(DspBuffer& dspbuf) final;
  void doKeyOn(const KeyOnInfo& koi) final;
  float _plmix, _prmix; // for smoothing
};
///////////////////////////////////////////////////////////////////////////////

} // namespace ork::audio::singularity
