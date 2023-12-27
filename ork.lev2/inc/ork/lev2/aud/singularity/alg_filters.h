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
// filter blocks
///////////////////////////////////////////////////////////////////////////////

struct BANDPASS_FILT_DATA : public DspBlockData {
  BANDPASS_FILT_DATA(std::string name);
  dspblk_ptr_t createInstance() const override;
};

struct BANDPASS_FILT : public DspBlock {
  using dataclass_t = BANDPASS_FILT_DATA;
  BANDPASS_FILT(const DspBlockData* dbd);
  TrapSVF _filter;
  BiQuad _biquad;
  void compute(DspBuffer& dspbuf) final;
  void doKeyOn(const KeyOnInfo& koi) final;
};
struct BAND2_DATA : public DspBlockData {
  BAND2_DATA(std::string name);
  dspblk_ptr_t createInstance() const override;
};
struct BAND2 : public DspBlock {
  using dataclass_t = BAND2_DATA;
  BAND2(const DspBlockData* dbd);
  TrapSVF _filter;
  void compute(DspBuffer& dspbuf) final;
  void doKeyOn(const KeyOnInfo& koi) final;
};
struct NOTCH_FILT_DATA : public DspBlockData {
  NOTCH_FILT_DATA(std::string name);
  dspblk_ptr_t createInstance() const override;
};
struct NOTCH_FILT : public DspBlock {
  using dataclass_t = NOTCH_FILT_DATA;
  NOTCH_FILT(const DspBlockData* dbd);
  TrapSVF _filter;
  void compute(DspBuffer& dspbuf) final;
  void doKeyOn(const KeyOnInfo& koi) final;
};
struct NOTCH2_DATA : public DspBlockData {
  NOTCH2_DATA(std::string name);
  dspblk_ptr_t createInstance() const override;
};
struct NOTCH2 : public DspBlock {
  using dataclass_t = NOTCH2_DATA;
  NOTCH2(const DspBlockData* dbd);
  TrapSVF _filter1;
  void compute(DspBuffer& dspbuf) final;
  void doKeyOn(const KeyOnInfo& koi) final;
};
struct DOUBLE_NOTCH_W_SEP_DATA : public DspBlockData {
  DOUBLE_NOTCH_W_SEP_DATA(std::string name);
  dspblk_ptr_t createInstance() const override;
};
struct DOUBLE_NOTCH_W_SEP : public DspBlock {
  using dataclass_t = DOUBLE_NOTCH_W_SEP_DATA;
  DOUBLE_NOTCH_W_SEP(const DspBlockData* dbd);
  TrapSVF _filter1;
  TrapSVF _filter2;
  void compute(DspBuffer& dspbuf) final;
  void doKeyOn(const KeyOnInfo& koi) final;
};
struct LOPAS2_DATA : public DspBlockData {
  LOPAS2_DATA(std::string name);
  dspblk_ptr_t createInstance() const override;
};
struct LOPAS2 : public DspBlock {
  using dataclass_t = LOPAS2_DATA;
  LOPAS2(const DspBlockData* dbd);
  TrapSVF _filter;
  void compute(DspBuffer& dspbuf) final;
  void doKeyOn(const KeyOnInfo& koi) final;
};
struct LP2RES_DATA : public DspBlockData {
  LP2RES_DATA(std::string name);
  dspblk_ptr_t createInstance() const override;
};
struct LP2RES : public DspBlock {
  using dataclass_t = LP2RES_DATA;
  LP2RES(const DspBlockData* dbd);
  TrapSVF _filter;
  void compute(DspBuffer& dspbuf) final;
  void doKeyOn(const KeyOnInfo& koi) final;
};
struct LPGATE_DATA : public DspBlockData {
  LPGATE_DATA(std::string name);
  dspblk_ptr_t createInstance() const override;
};
struct LPGATE : public DspBlock {
  using dataclass_t = LPGATE_DATA;
  LPGATE(const DspBlockData* dbd);
  TrapSVF _filter;
  void compute(DspBuffer& dspbuf) final;
  void doKeyOn(const KeyOnInfo& koi) final;
};
struct FOURPOLE_HIPASS_W_SEP_DATA : public DspBlockData {
  FOURPOLE_HIPASS_W_SEP_DATA(std::string name);
  dspblk_ptr_t createInstance() const override;
};
struct FOURPOLE_HIPASS_W_SEP : public DspBlock {
  using dataclass_t = FOURPOLE_HIPASS_W_SEP_DATA;
  FOURPOLE_HIPASS_W_SEP(const DspBlockData* dbd);
  TrapSVF _filter1;
  TrapSVF _filter2;
  float _filtFC;
  void compute(DspBuffer& dspbuf) final;
  void doKeyOn(const KeyOnInfo& koi) final;
};
struct LPCLIP_DATA : public DspBlockData {
  LPCLIP_DATA(std::string name);
  dspblk_ptr_t createInstance() const override;
};
struct LPCLIP : public DspBlock {
  using dataclass_t = LPCLIP_DATA;
  LPCLIP(const DspBlockData* dbd);
  OnePoleLoPass _lpf;
  void compute(DspBuffer& dspbuf) final;
  void doKeyOn(const KeyOnInfo& koi) final;
};
/////////////////
// newstyle
/////////////////
struct LowPassData : public DspBlockData {
  LowPassData(std::string name);
  dspblk_ptr_t createInstance() const override;
};
struct LowPass : public DspBlock {
  // krzname: LOPASS
  using dataclass_t = LowPassData;
  LowPass(const LowPassData* dbd);
  OnePoleLoPass _lpf;
  void compute(DspBuffer& dspbuf) final;
  void doKeyOn(const KeyOnInfo& koi) final;
};
struct HighPassData : public DspBlockData {
  HighPassData(std::string name);
  dspblk_ptr_t createInstance() const override;
};
struct HighPass : public DspBlock {
  // krzname: HighPass
  using dataclass_t = HighPassData;
  HighPass(const HighPassData* dbd);
  OnePoleHiPass _hpf;
  void compute(DspBuffer& dspbuf) final;
  void doKeyOn(const KeyOnInfo& koi) final;
};
struct AllPassData : public DspBlockData {
  AllPassData(std::string name);
  dspblk_ptr_t createInstance() const override;
};
struct AllPass : public DspBlock {
  // krzname: ALPASS
  using dataclass_t = AllPassData;
  AllPass(const AllPassData* dbd);
  TrapAllpass _filter;
  void compute(DspBuffer& dspbuf) final;
  void doKeyOn(const KeyOnInfo& koi) final;
};
struct HighFreqStimulatorData : public DspBlockData {
  HighFreqStimulatorData(std::string name);
  dspblk_ptr_t createInstance() const override;
};
struct HighFreqStimulator : public DspBlock {
  // krzname: HIFREQ_STIMULATOR
  using dataclass_t = HighFreqStimulatorData;
  HighFreqStimulator(const HighFreqStimulatorData* dbd);
  TrapSVF _filter1;
  TrapSVF _filter2;
  float _smoothFC;
  void compute(DspBuffer& dspbuf) final;
  void doKeyOn(const KeyOnInfo& koi) final;
};
struct TwoPoleLowPassData : public DspBlockData {
  // krzname: "2POLE LOWPASS"
  TwoPoleLowPassData(std::string name);
  dspblk_ptr_t createInstance() const override;
};
struct TwoPoleLowPass : public DspBlock {
  using dataclass_t = TwoPoleLowPassData;
  TwoPoleLowPass(const DspBlockData* dbd);
  TrapSVF _filter;
  float _smoothFC;
  void compute(DspBuffer& dspbuf) final;
  void doKeyOn(const KeyOnInfo& koi) final;
};

struct TwoPoleAllPassData : public DspBlockData {
  // krzname: "2POLE ALLPASS"
  TwoPoleAllPassData(std::string name);
  dspblk_ptr_t createInstance() const override;
};
struct TwoPoleAllPass : public DspBlock {
  using dataclass_t = TwoPoleAllPassData;
  TwoPoleAllPass(const DspBlockData* dbd);
  TrapAllpass _filterL;
  TrapAllpass _filterH;
  void compute(DspBuffer& dspbuf) final;
  void doKeyOn(const KeyOnInfo& koi) final;
};

struct FourPoleLowPassWithSepData : public DspBlockData {
  // krzname: "4POLE LOPASS W/SEP"
  FourPoleLowPassWithSepData(std::string name);
  dspblk_ptr_t createInstance() const override;
};
struct FourPoleLowPassWithSep : public DspBlock {
  using dataclass_t = FourPoleLowPassWithSepData;
  FourPoleLowPassWithSep(const DspBlockData* dbd);
  TrapSVF _filter1;
  TrapSVF _filter2;
  float _filtFC;
  void compute(DspBuffer& dspbuf) final;
  void doKeyOn(const KeyOnInfo& koi) final;
};

} // namespace ork::audio::singularity
