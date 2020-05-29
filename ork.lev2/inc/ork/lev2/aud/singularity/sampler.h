#pragma once
#include <ork/lev2/aud/singularity/synthdata.h>
#include <ork/lev2/aud/singularity/dspblocks.h>
#include <ork/lev2/aud/singularity/envelope.h>

namespace ork::audio::singularity {

///////////////////////////////////////////////////////////////////////////////

enum struct eLoopMode {
  NOTSET = -1,
  NONE   = 0,
  FWD,
  BIDIR,
  FROMKM,
};

///////////////////////////////////////////////////////////////////////////////

struct natenvseg {
  float _slope;
  float _time;
};

///////////////////////////////////////////////////////////////////////////////

struct sample {
  sample();

  std::string _name;
  const s16* _sampleBlock;

  int _blk_start;
  int _blk_alt;

  int _blk_loopstart;
  int _blk_loopend;

  int _blk_end;

  int _loopPoint;
  int _subid;
  float _sampleRate;
  float _linGain;
  int _rootKey;
  int _highestPitch;

  eLoopMode _loopMode = eLoopMode::NONE;
  std::vector<natenvseg> _natenv;

  int _pitchAdjust = 0;
};

///////////////////////////////////////////////////////////////////////////////

struct multisample {
  std::string _name;
  int _objid;
  std::map<int, sample*> _samples;
};

///////////////////////////////////////////////////////////////////////////////

struct kmregion {
  int _lokey = 0, _hikey = 0;
  int _lovel = 0, _hivel = 127;
  int _tuning                 = 0;
  eLoopMode _loopModeOverride = eLoopMode::NOTSET;
  float _volAdj               = 0.0f;
  float _linGain              = 1.0f;
  int _multsampID = -1, _sampID = -1;
  std::string _sampleName;
  const multisample* _multiSample = nullptr;
  const sample* _sample           = nullptr;
};

///////////////////////////////////////////////////////////////////////////////

struct KeyMap {
  std::string _name;
  std::vector<kmregion*> _regions;
  int _kmID;

  kmregion* getRegion(int note, int vel) const;
};

///////////////////////////////////////////////////////////////////////////////

struct KmpBlockData {
  keymap_constptr_t _keymap;
  int _transpose   = 0;
  float _keyTrack  = 100.0f;
  float _velTrack  = 0.0f;
  int _timbreShift = 0;
  std::string _pbMode;
};

///////////////////////////////////////////////////////////////////////////////

struct NatEnv {
  NatEnv();
  void keyOn(const KeyOnInfo& KOI, const sample* s);
  void keyOff();
  const natenvseg& getCurSeg() const;
  float compute();
  void initSeg(int iseg);

  std::vector<natenvseg> _natenvseg;
  Layer* _layer;
  int _curseg;
  int _prvseg;
  int _numseg;
  int _framesrem;
  float _segtime;
  float _curamp;
  float _slopePerSecond;
  float _slopePerSample;
  float _SR;
  bool _ignoreRelease;
  int _state;
  envadjust_method_t _envadjust;
};

///////////////////////////////////////////////////////////////////////////////

struct sampleOsc {
  sampleOsc();

  void keyOn(const DspKeyOnInfo& koi);
  void keyOff();

  void findRegion(const DspKeyOnInfo& koi);

  void updateFreqRatio();
  void setSrRatio(float r);
  void compute(int inumfr);
  float playNoLoop();
  float playLoopFwd();
  float playLoopBid();
  // bool playbackDone() const;

  // typedef float(sampleOsc::*pbfunc_t)();
  // pbfunc_t _pbFunc = nullptr;

  float (sampleOsc::*_pbFunc)() = nullptr;

  //
  int64_t _blk_start;
  int64_t _blk_alt;
  int64_t _blk_loopstart;
  int64_t _blk_loopend;
  int64_t _blk_end;

  static constexpr float kinv64k = 1.0f / 65536.0f;
  static constexpr float kinv32k = 1.0f / 32768.0f;

  const sample* _sample;

  int _sampselnote;
  int _sampleRoot;

  float _playbackRate;

  int64_t _pbindex;
  int64_t _pbindexNext;

  float _keyoncents;
  int64_t _pbincrem;

  float _dt;
  float _synsr;
  // bool _isLooped;
  bool _enableNatEnv;
  eLoopMode _loopMode;
  bool _active;
  bool _forwarddir;
  int _loopCounter;
  float _curSampSRratio;
  float _NATENV[1024];
  float _OUTPUT[1024];
  int _keydiff;
  int _kmcents;
  int _pchcents;
  int _curpitchadjx;
  int _curpitchadj;
  int _curcents;
  float _baseCents;
  float _preDSPGAIN;
  int _samppbnote;

  float _curratio;

  NatEnv _natAmpEnv;

  Layer* _lyr;
  bool _released;
  const kmregion* _kmregion;
};

///////////////////////////////////////////////////////////////////////////////

struct SAMPLER final : public DspBlock {
  SAMPLER(const DspBlockData* dbd);
  void compute(DspBuffer& dspbuf);

  void doKeyOn(const DspKeyOnInfo& koi);
  void doKeyOff();
  sampleOsc _spOsc;
  float _filtp;

  static void initBlock(dspblkdata_ptr_t blockdata);
};

} // namespace ork::audio::singularity
