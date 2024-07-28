////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once
#include <ork/lev2/aud/singularity/synthdata.h>
#include <ork/lev2/aud/singularity/dspblocks.h>
#include <ork/lev2/aud/singularity/envelope.h>
#include <ork/lev2/aud/singularity/filters.h>

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

struct WaveformData{
  std::vector<s16> _sampledata;
};

///////////////////////////////////////////////////////////////////////////////

struct SampleData : public ork::Object {

  DeclareConcreteX(SampleData, ork::Object);

  SampleData();

  void loadFromAudioFile(const std::string& filename, bool normalize=true);

  std::string _name;
  const s16* _sampleBlock = nullptr;

  int _blk_start = 0;
  int _blk_alt = 0;
  int _blk_end = 0;

  int _blk_loopstart = 0;
  int _blk_loopend = 0;
  int _numChannels = 1;
  int _loopPoint = 0;
  int _subid = 0;
  float _sampleRate = 0.0f;
  float _linGain = 1.0f;
  int _rootKey = 0;
  int _highestPitch = 0;
  int _interpMethod = 0;
  float _originalPitch = 0.0f;
  
  svar64_t _user;

  eLoopMode _loopMode = eLoopMode::NOTSET;
  natenvwrapperdata_ptr_t _naturalEnvelope;
  int _pitchAdjust = 0;
};

///////////////////////////////////////////////////////////////////////////////

struct MultiSampleData : public ork::Object {

  DeclareConcreteX(MultiSampleData, ork::Object);

  std::string _name;
  int _objid;
  std::map<int, sample_ptr_t> _samples;
  std::map<std::string, sample_ptr_t> _samplesByName;
};

///////////////////////////////////////////////////////////////////////////////

struct KmRegionData : public ork::Object {

  DeclareConcreteX(KmRegionData, ork::Object);

  kmregion_ptr_t clone() const;
  int _lokey = 0, _hikey = 0;
  int _lovel = 0, _hivel = 127;
  int _tuning                 = 0;
  eLoopMode _loopModeOverride = eLoopMode::NOTSET;
  float _volAdj               = 0.0f;
  float _linGain              = 1.0f;
  int _multsampID = -1, _sampID = -1;
  std::string _sampleName;
  multisample_constptr_t _multiSample = nullptr;
  sample_constptr_t _sample           = nullptr;
};

///////////////////////////////////////////////////////////////////////////////

struct KeyMapData : public ork::Object {

  DeclareConcreteX(KeyMapData, ork::Object);

  keymap_ptr_t clone() const;

  std::string _name;
  std::vector<kmregion_ptr_t> _regions;
  int _kmID = -1;

  kmregion_ptr_t getRegion(int note, int vel) const;
};

///////////////////////////////////////////////////////////////////////////////

struct KmpBlockData {

  kmpblockdata_ptr_t clone() const;

  keymap_constptr_t _keymap;
  int _transpose   = 0;
  float _keyTrack  = 100.0f;
  float _velTrack  = 0.0f;
  int _timbreShift = 0;
  std::string _pbMode;
};

struct RegionSearch{
  int _sampselnote = -1;
  int _sampleRoot = -1;
  int _keydiff = -1;
  float _baseCents = 0.0f;
  float _preDSPGAIN = 1.0f;
  int _curpitchadjx = 0;
  int _curpitchadj = 0;
  int _kmcents = 0;
  int _pchcents = 0;
  sample_constptr_t _sample = nullptr;
  kmregion_constptr_t _kmregion = nullptr;
};
///////////////////////////////////////////////////////////////////////////////

struct NatEnv {
  NatEnv();
  void keyOn(const KeyOnInfo& KOI, sample_constptr_t s);
  void keyOff();
  const natenvseg& getCurSeg() const;
  float compute();
  void initSeg(int iseg);

  std::vector<natenvseg> _natenvseg;
  layer_ptr_t _layer;
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

class SamplerLowPassFilter {
public:
    SamplerLowPassFilter(size_t order) : _coeffs(order, 1.0f / order), _buffer(order, 0.0f), _order(order) {}

    float process(float input) {
        // Shift buffer contents
        for (size_t i = _order - 1; i > 0; --i) {
            _buffer[i] = _buffer[i - 1];
        }
        _buffer[0] = input;

        // Apply filter
        float output = 0.0f;
        for (size_t i = 0; i < _order; ++i) {
            output += _buffer[i] * _coeffs[i];
        }
        return output;
    }

private:
    std::vector<float> _coeffs;
    std::vector<float> _buffer;
    size_t _order;
};

///////////////////////////////////////////////////////////////////////////////

struct SampleOscillator {
  SampleOscillator(const SAMPLER_DATA* data);

  void keyOn(const KeyOnInfo& koi);
  void keyOff();

  void updateFreqRatio();
  void setSrRatio(float r);
  void compute(int inumfr);
  float playNoLoop();
  float playLoopFwd();
  float playLoopBid();
  // bool playbackDone() const;

  // typedef float(SampleOscillator::*pbfunc_t)();
  // pbfunc_t _pbFunc = nullptr;

  float (SampleOscillator::*_pbFunc)() = nullptr;

  const SAMPLER_DATA* _sampler_data;
  layer_ptr_t _lyr;
  bool _active;
  int64_t _pbindex;
  int64_t _pbindexNext;
  int64_t _pbincrem;
  float _curratio;
  eLoopMode _loopMode;
  int _loopCounter;
  //
  int64_t _blk_start;
  int64_t _blk_alt;
  int64_t _blk_loopstart;
  int64_t _blk_loopend;
  int64_t _blk_end;
  int _curcents = 0;
  NatEnvWrapperInst* _natenvwrapperinst = nullptr;


  float _playbackRate;


  float _keyoncents;

  float _dt;
  float _synsr;
  // bool _isLooped;
  bool _enableNatEnv;
  bool _forwarddir;
  float _curSampSRratio;
  float _NATENV[1024];
  float _OUTPUT[1024];
  int _samppbnote;

  RegionSearch _regionsearch;

  natenv_ptr_t _natAmpEnv;

  bool _released;
  SamplerLowPassFilter _lpFilter; 
  OnePoleLoPass _lpFilter2A; 
  OnePoleLoPass _lpFilter2B; 
  BiQuad _bq[4];
};

///////////////////////////////////////////////////////////////////////////////

struct SAMPLER_DATA : public DspBlockData {

  DeclareConcreteX(SAMPLER_DATA, DspBlockData);

  SAMPLER_DATA(std::string name="");
  dspblk_ptr_t createInstance() const override;
  RegionSearch findRegion(lyrdata_constptr_t ld, const KeyOnInfo& koi) const;
  float _lowpassfrq;
};

///////////////////////////////////////////////////////////////////////////////

struct SAMPLER final : public DspBlock {
  using dataclass_t = SAMPLER_DATA;
  SAMPLER(const DspBlockData* dbd);
  void compute(DspBuffer& dspbuf);

  void doKeyOn(const KeyOnInfo& koi);
  void doKeyOff();
  SampleOscillator* _spOsc = nullptr;
  natenvwrapperdata_ptr_t _natenvwrapperdata;
  float _filtp;
};

} // namespace ork::audio::singularity
