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

struct BANDPASS_FILT : public DspBlock {
  BANDPASS_FILT(const DspBlockData* dbd);
  TrapSVF _filter;
  BiQuad _biquad;
  void compute(DspBuffer& dspbuf) final;
  void doKeyOn(const KeyOnInfo& koi) final;
};
struct BAND2 : public DspBlock {
  BAND2(const DspBlockData* dbd);
  TrapSVF _filter;
  void compute(DspBuffer& dspbuf) final;
  void doKeyOn(const KeyOnInfo& koi) final;
};
struct NOTCH_FILT : public DspBlock {
  NOTCH_FILT(const DspBlockData* dbd);
  TrapSVF _filter;
  void compute(DspBuffer& dspbuf) final;
  void doKeyOn(const KeyOnInfo& koi) final;
};
struct NOTCH2 : public DspBlock {
  NOTCH2(const DspBlockData* dbd);
  TrapSVF _filter1;
  void compute(DspBuffer& dspbuf) final;
  void doKeyOn(const KeyOnInfo& koi) final;
};
struct DOUBLE_NOTCH_W_SEP : public DspBlock {
  DOUBLE_NOTCH_W_SEP(const DspBlockData* dbd);
  TrapSVF _filter1;
  TrapSVF _filter2;
  void compute(DspBuffer& dspbuf) final;
  void doKeyOn(const KeyOnInfo& koi) final;
};
struct LOPAS2 : public DspBlock {
  LOPAS2(const DspBlockData* dbd);
  TrapSVF _filter;
  void compute(DspBuffer& dspbuf) final;
  void doKeyOn(const KeyOnInfo& koi) final;
};
struct LP2RES : public DspBlock {
  LP2RES(const DspBlockData* dbd);
  TrapSVF _filter;
  void compute(DspBuffer& dspbuf) final;
  void doKeyOn(const KeyOnInfo& koi) final;
};
struct LPGATE : public DspBlock {
  LPGATE(const DspBlockData* dbd);
  TrapSVF _filter;
  void compute(DspBuffer& dspbuf) final;
  void doKeyOn(const KeyOnInfo& koi) final;
};
struct FOURPOLE_HIPASS_W_SEP : public DspBlock {
  FOURPOLE_HIPASS_W_SEP(const DspBlockData* dbd);
  TrapSVF _filter1;
  TrapSVF _filter2;
  float _filtFC;
  void compute(DspBuffer& dspbuf) final;
  void doKeyOn(const KeyOnInfo& koi) final;
};
struct LPCLIP : public DspBlock {
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
