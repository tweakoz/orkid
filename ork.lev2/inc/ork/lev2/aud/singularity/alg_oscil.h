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
// oscils
///////////////////////////////////////////////////////////////////////////////
struct PITCH_DATA : public DspBlockData {
  PITCH_DATA(std::string name);
  dspblk_ptr_t createInstance() const override;
};

struct PITCH : public DspBlock {
  using dataclass_t = PITCH_DATA;
  PITCH(const DspBlockData* dbd);
  void compute(DspBuffer& dspbuf) final;
  void doKeyOn(const KeyOnInfo& koi) final;
};

struct SWPLUSSHP_DATA : public DspBlockData {
  SWPLUSSHP_DATA(std::string name);
  dspblk_ptr_t createInstance() const override;
};
struct SWPLUSSHP : public DspBlock {
  using dataclass_t = SWPLUSSHP_DATA;
  SWPLUSSHP(const DspBlockData* dbd);
  void compute(DspBuffer& dspbuf) final;
  PolyBLEP _pblep;
  void doKeyOn(const KeyOnInfo& koi) final;
};
struct SAWPLUS_DATA : public DspBlockData {
  SAWPLUS_DATA(std::string name);
  dspblk_ptr_t createInstance() const override;
};
struct SAWPLUS : public DspBlock {
  using dataclass_t = SAWPLUS_DATA;
  SAWPLUS(const DspBlockData* dbd);
  void compute(DspBuffer& dspbuf) final;
  PolyBLEP _pblep;
  void doKeyOn(const KeyOnInfo& koi) final;
};
struct SINE_DATA : public DspBlockData {
  SINE_DATA(std::string name);
  dspblk_ptr_t createInstance() const override;
};
struct SINE : public DspBlock {
  using dataclass_t = SINE_DATA;
  SINE(const DspBlockData* dbd);
  void compute(DspBuffer& dspbuf) final;
  PolyBLEP _pblep;
  void doKeyOn(const KeyOnInfo& koi) final;
};
struct SAW_DATA : public DspBlockData {
  SAW_DATA(std::string name);
  dspblk_ptr_t createInstance() const override;
};
struct SAW : public DspBlock {
  using dataclass_t = SAW_DATA;
  SAW(const DspBlockData* dbd);
  void compute(DspBuffer& dspbuf) final;
  PolyBLEP _pblep;
  void doKeyOn(const KeyOnInfo& koi) final;
};
struct SQUARE_DATA : public DspBlockData {
  SQUARE_DATA(std::string name);
  dspblk_ptr_t createInstance() const override;
};
struct SQUARE : public DspBlock {
  using dataclass_t = SQUARE_DATA;
  SQUARE(const DspBlockData* dbd);
  void compute(DspBuffer& dspbuf) final;
  PolyBLEP _pblep;
  void doKeyOn(const KeyOnInfo& koi) final;
};
struct SINEPLUS_DATA : public DspBlockData {
  SINEPLUS_DATA(std::string name);
  dspblk_ptr_t createInstance() const override;
};
struct SINEPLUS : public DspBlock {
  using dataclass_t = SINEPLUS_DATA;
  SINEPLUS(const DspBlockData* dbd);
  void compute(DspBuffer& dspbuf) final;
  PolyBLEP _pblep;
  void doKeyOn(const KeyOnInfo& koi) final;
};
struct SHAPEMODOSC_DATA : public DspBlockData {
  SHAPEMODOSC_DATA(std::string name);
  dspblk_ptr_t createInstance() const override;
};
struct SHAPEMODOSC : public DspBlock {
  using dataclass_t = SHAPEMODOSC_DATA;
  SHAPEMODOSC(const DspBlockData* dbd);
  void compute(DspBuffer& dspbuf) final;
  PolyBLEP _pblep;
  void doKeyOn(const KeyOnInfo& koi) final;
};
struct PLUSSHAPEMODOSC_DATA : public DspBlockData {
  PLUSSHAPEMODOSC_DATA(std::string name);
  dspblk_ptr_t createInstance() const override;
};
struct PLUSSHAPEMODOSC : public DspBlock {
  using dataclass_t = PLUSSHAPEMODOSC_DATA;
  PLUSSHAPEMODOSC(const DspBlockData* dbd);
  void compute(DspBuffer& dspbuf) final;
  PolyBLEP _pblep;
  void doKeyOn(const KeyOnInfo& koi) final;
};
struct SYNCM_DATA : public DspBlockData {
  SYNCM_DATA(std::string name);
  dspblk_ptr_t createInstance() const override;
};
struct SYNCM : public DspBlock {
  using dataclass_t = SYNCM_DATA;
  SYNCM(const DspBlockData* dbd);
  void compute(DspBuffer& dspbuf) final;
  float _phase;
  float _phaseInc;
  void doKeyOn(const KeyOnInfo& koi) final;
};
struct SYNCS_DATA : public DspBlockData {
  SYNCS_DATA(std::string name);
  dspblk_ptr_t createInstance() const override;
};
struct SYNCS : public DspBlock {
  using dataclass_t = SYNCS_DATA;
  SYNCS(const DspBlockData* dbd);
  void compute(DspBuffer& dspbuf) final;
  PolyBLEP _pblep;
  float _prvmaster;
  void doKeyOn(const KeyOnInfo& koi) final;
};
struct PWM_DATA : public DspBlockData {
  PWM_DATA(std::string name);
  dspblk_ptr_t createInstance() const override;
};
struct PWM : public DspBlock {
  using dataclass_t = PWM_DATA;
  PWM(const DspBlockData* dbd);
  void compute(DspBuffer& dspbuf) final;
  PolyBLEP _pblep;
  void doKeyOn(const KeyOnInfo& koi) final;
};

struct NOISE_DATA : public DspBlockData {
  NOISE_DATA(std::string name);
  dspblk_ptr_t createInstance() const override;
};
struct NOISE : public DspBlock {
  using dataclass_t = NOISE_DATA;
  NOISE(const DspBlockData* dbd);
  void compute(DspBuffer& dspbuf) final;
  void doKeyOn(const KeyOnInfo& koi) final;
  void doKeyOff() final;
};

} // namespace ork::audio::singularity
