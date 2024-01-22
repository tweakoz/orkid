////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include "dspblocks.h"
#include "dsp_mix.h"

namespace ork::audio::singularity {
///////////////////////////////////////////////////////////////////////////////
// amp blocks
///////////////////////////////////////////////////////////////////////////////
struct AMP_ADAPTIVE_DATA : public DspBlockData {
  DeclareConcreteX(AMP_ADAPTIVE_DATA,DspBlockData);
  AMP_ADAPTIVE_DATA(std::string name="DspAmpAdaptive");
  dspblk_ptr_t createInstance() const override;
};
struct AMP_ADAPTIVE : public DspBlock {
  using dataclass_t = AMP_ADAPTIVE_DATA;
  AMP_ADAPTIVE(const DspBlockData* dbd);
  void compute(DspBuffer& dspbuf) final;
  void doKeyOn(const KeyOnInfo& koi) final;
};
struct AMP_MONOIO_DATA : public DspBlockData {
  DeclareConcreteX(AMP_MONOIO_DATA,DspBlockData);
  AMP_MONOIO_DATA(std::string name="DspAmpMono");
  dspblk_ptr_t createInstance() const override;
};
struct AMP_MONOIO : public DspBlock {
  using dataclass_t = AMP_MONOIO_DATA;
  AMP_MONOIO(const DspBlockData* dbd);
  void compute(DspBuffer& dspbuf) final;
  void doKeyOn(const KeyOnInfo& koi) final;
};
///////////////////////////////////////////////////////////////////////////////
struct PLUSAMP_DATA : public DspBlockData {
  DeclareConcreteX(PLUSAMP_DATA,DspBlockData);
  PLUSAMP_DATA(std::string name="DspAmpPlus");
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
  DeclareConcreteX(XAMP_DATA,DspBlockData);
  XAMP_DATA(std::string name="DspAmpX");
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
struct STEREO_GAIN_DATA : public DspBlockData {
  DeclareConcreteX(STEREO_GAIN_DATA,DspBlockData);
  STEREO_GAIN_DATA(std::string name="DspAmpStereoGain");
  dspblk_ptr_t createInstance() const override;
};
struct STEREO_GAIN : public DspBlock {
  using dataclass_t = STEREO_GAIN_DATA;
  STEREO_GAIN(const DspBlockData* dbd);
  void compute(DspBuffer& dspbuf) final;
  float _filt;
};
///////////////////////////////////////////////////////////////////////////////
struct GAIN_DATA : public DspBlockData {
  DeclareConcreteX(GAIN_DATA,DspBlockData);
  GAIN_DATA(std::string name="DspAmpMonoGain");
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
  DeclareConcreteX(BANGAMP_DATA,DspBlockData);
  BANGAMP_DATA(std::string name="DspAmpBang");
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
  DeclareConcreteX(AMPU_AMPL_DATA,DspBlockData);
  AMPU_AMPL_DATA(std::string name="DspAmpUL");
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
  DeclareConcreteX(BAL_AMP_DATA,DspBlockData);
  BAL_AMP_DATA(std::string name="DspAmpBalance");
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
  DeclareConcreteX(AMP_MOD_OSC_DATA,DspBlockData);
  AMP_MOD_OSC_DATA(std::string name="DspAmpModOsc");
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
  DeclareConcreteX(XGAIN_DATA,DspBlockData);
  XGAIN_DATA(std::string name="DspAmpXGain");
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
  DeclareConcreteX(XFADE_DATA,DspBlockData);
  XFADE_DATA(std::string name="DspAmpXFade");
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
  DeclareConcreteX(PANNER_DATA,DspBlockData);
  PANNER_DATA(std::string name="DspAmpPanner");
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
struct PANNER2D_DATA : public DspBlockData {
  DeclareConcreteX(PANNER2D_DATA,DspBlockData);
  PANNER2D_DATA(std::string name="DspAmpPanner2D");
  dspblk_ptr_t createInstance() const override;
};
struct PANNER2D : public DspBlock {
  using dataclass_t = PANNER2D_DATA;
  PANNER2D(const DspBlockData* dbd);
  void compute(DspBuffer& dspbuf) final;
  void doKeyOn(const KeyOnInfo& koi) final;
  float _mixL, _mixR; // for smoothing
  delaycontext_ptr_t _delayL, _delayR;
  TrapSVF _filter1L, _filter1R;
  TrapSVF _fbLP;
  TrapSVF _dcBLOCK;
  TrapAllpass _allpassA, _allpassB, _allpassC;
  SimpleAllpass _ap2A, _ap2B, _ap2C;
  float _feedback = 0.995f;
  float _a0 = 1.0f;
  float _a1 = 1.0f;
  float _a2 = 1.0f;
  float _ap2 = 0.0f;
};
///////////////////////////////////////////////////////////////////////////////

} // namespace ork::audio::singularity
