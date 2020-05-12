#pragma once

#include "dspblocks.h"

namespace ork::audio::singularity {

///////////////////////////////////////////////////////////////////////////////
// oscils
///////////////////////////////////////////////////////////////////////////////

struct SAMPLEPB : public DspBlock {
  SAMPLEPB(dspblkdata_constptr_t dbd);
  void compute(DspBuffer& dspbuf) final;

  void doKeyOn(const DspKeyOnInfo& koi) final;
  void doKeyOff() final;
  // bool playbackDone() const;
  sampleOsc _spOsc;
  // bool _playbackDone;
  float _filtp;

  static void initBlock(dspblkdata_ptr_t blockdata);
};

struct SWPLUSSHP : public DspBlock {
  SWPLUSSHP(dspblkdata_constptr_t dbd);
  void compute(DspBuffer& dspbuf) final;
  PolyBLEP _pblep;
  void doKeyOn(const DspKeyOnInfo& koi) final;
};
struct SAWPLUS : public DspBlock {
  SAWPLUS(dspblkdata_constptr_t dbd);
  void compute(DspBuffer& dspbuf) final;
  PolyBLEP _pblep;
  void doKeyOn(const DspKeyOnInfo& koi) final;
};
struct SINE : public DspBlock {
  SINE(dspblkdata_constptr_t dbd);
  void compute(DspBuffer& dspbuf) final;
  PolyBLEP _pblep;
  void doKeyOn(const DspKeyOnInfo& koi) final;
};
struct SAW : public DspBlock {
  SAW(dspblkdata_constptr_t dbd);
  void compute(DspBuffer& dspbuf) final;
  PolyBLEP _pblep;
  void doKeyOn(const DspKeyOnInfo& koi) final;
};
struct SQUARE : public DspBlock {
  SQUARE(dspblkdata_constptr_t dbd);
  void compute(DspBuffer& dspbuf) final;
  PolyBLEP _pblep;
  void doKeyOn(const DspKeyOnInfo& koi) final;
};
struct SINEPLUS : public DspBlock {
  SINEPLUS(dspblkdata_constptr_t dbd);
  void compute(DspBuffer& dspbuf) final;
  PolyBLEP _pblep;
  void doKeyOn(const DspKeyOnInfo& koi) final;
};
struct SHAPEMODOSC : public DspBlock {
  SHAPEMODOSC(dspblkdata_constptr_t dbd);
  void compute(DspBuffer& dspbuf) final;
  PolyBLEP _pblep;
  void doKeyOn(const DspKeyOnInfo& koi) final;
};
struct PLUSSHAPEMODOSC : public DspBlock {
  PLUSSHAPEMODOSC(dspblkdata_constptr_t dbd);
  void compute(DspBuffer& dspbuf) final;
  PolyBLEP _pblep;
  void doKeyOn(const DspKeyOnInfo& koi) final;
};
struct SYNCM : public DspBlock {
  SYNCM(dspblkdata_constptr_t dbd);
  void compute(DspBuffer& dspbuf) final;
  float _phase;
  float _phaseInc;
  void doKeyOn(const DspKeyOnInfo& koi) final;
};
struct SYNCS : public DspBlock {
  SYNCS(dspblkdata_constptr_t dbd);
  void compute(DspBuffer& dspbuf) final;
  PolyBLEP _pblep;
  float _prvmaster;
  void doKeyOn(const DspKeyOnInfo& koi) final;
};
struct PWM : public DspBlock {
  PWM(dspblkdata_constptr_t dbd);
  void compute(DspBuffer& dspbuf) final;
  PolyBLEP _pblep;
  void doKeyOn(const DspKeyOnInfo& koi) final;
};

struct fm4syn;

struct FM4 : public DspBlock {
  FM4(dspblkdata_constptr_t dbd);
  void compute(DspBuffer& dspbuf) final;
  void doKeyOn(const DspKeyOnInfo& koi) final;
  void doKeyOff() final;
  fm4syn* _fm4;
  static void initBlock(dspblkdata_ptr_t blockdata);
};

struct NOISE : public DspBlock {
  NOISE(dspblkdata_constptr_t dbd);
  void compute(DspBuffer& dspbuf) final;
  void doKeyOn(const DspKeyOnInfo& koi) final;
  void doKeyOff() final;
};

} // namespace ork::audio::singularity
