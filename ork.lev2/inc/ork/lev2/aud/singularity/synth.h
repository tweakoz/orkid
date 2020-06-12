#pragma once

#include <ork/orktypes.h>
#include <ork/math/audiomath.h>
#include "krztypes.h"
#include "synthdata.h"
#include "layer.h"
#include <ork/kernel/concurrent_queue.h>
#include <ork/kernel/svariant.h>
#include <ork/lev2/gfx/gfxenv.h>

namespace ork::audio::singularity {

///////////////////////////////////////////////////////////////////////////////

struct programInst {
  programInst();
  ~programInst();

  void keyOn(int note, int velocity, prgdata_constptr_t pd);
  void keyOff();

  prgdata_constptr_t _progdata;

  std::vector<Layer*> _layers;
};

using onkey_t = std::function<void(
    int note, //
    int velocity,
    programInst* pinst)>;

///////////////////////////////////////////////////////////////////////////////

struct SynthProfilerFrame {
  float _samplerate  = 0;
  float _controlrate = 0;
  int _buffersize    = 0;
  float _cpuload     = 0.0f;
  int _numlayers     = 0;
  int _numdspblocks  = 0;
};

using onprofframe_t = std::function<void(const SynthProfilerFrame& profframe)>;

///////////////////////////////////////////////////////////////////////////////

struct SynthData;

struct hudsample {
  float _time;
  float _value;
};
using synth_ptr_t = std::shared_ptr<synth>;

///////////////////////////////////////////////////////////////////////////////

struct OutputBus {
  void resize(int numframes);
  std::string _name;
  outputBuffer _buffer;
  scopesource_ptr_t createScopeSource();

  /////////////////////////
  // output bus DSP
  /////////////////////////

  void setBusDSP(lyrdata_ptr_t ld);

  lyrdata_ptr_t _dsplayerdata;
  Layer* _dsplayer = nullptr;
  scopesource_ptr_t _scopesource;

  /////////////////////////
};

///////////////////////////////////////////////////////////////////////////////

struct synth {
  synth();
  ~synth();

  using eventmap_t = std::multimap<float, void_lambda_t>;
  static synth_ptr_t instance();

  typedef std::vector<hudsample> hudsamples_t;

  void setSampleRate(float sr);
  inline float sampleRate() const {
    return _sampleRate;
  }

  outbus_ptr_t createOutputBus(std::string named);
  outbus_ptr_t outputBus(std::string named) const;

  void compute(int inumframes, const void* inputbuffer);

  programInst* keyOn(int note, int velocity, prgdata_constptr_t pd);
  void keyOff(programInst* p);

  Layer* allocLayer();
  void freeLayer(Layer* l);
  void deactivateVoices();
  void activateVoices(int ifrpending);

  void resetFenables();

  void addEvent(float time, void_lambda_t ev);
  void _tick(eventmap_t& emap, float dt);
  float _timeaccum;

  std::map<std::string, outbus_ptr_t> _outputBusses;
  std::vector<onkey_t> _onkey_subscribers;
  onprofframe_t _onprofilerframe = nullptr;

  outbus_ptr_t _tempbus;

  outputBuffer _ibuf;
  outputBuffer _obuf;
  float _sampleRate;
  float _dt;

  std::set<Layer*> _allVoices;
  std::set<programInst*> _allProgInsts;

  std::set<Layer*> _freeVoices;
  std::set<Layer*> _activeVoices;
  std::set<Layer*> _pendactVoices;
  std::queue<Layer*> _deactiveateVoiceQ;
  std::set<programInst*> _freeProgInst;
  std::set<programInst*> _activeProgInst;
  std::map<std::string, hudsamples_t> _hudsample_map;

  LockedResource<eventmap_t> _eventmap;

  void resize(int numframes);

  int _soloLayer       = -1;
  bool _stageEnable[5] = {true, true, true, true, true};
  int _lnoteframe;
  float _lnotetime;
  float _testtonepi;
  float _testtoneph;
  float _testtoneamp;
  float _testtoneampps;
  int _hudpage;
  int _genmode                  = 0;
  float _ostriglev              = 0;
  bool _ostrigdir               = false;
  int _osgainmode               = 3; // auto
  int64_t _oswidth              = 0;
  bool _bypassDSP               = false;
  bool _doModWheel              = false;
  bool _doPressure              = false;
  bool _doInput                 = false;
  float _masterGain             = 1.0;
  int _dspwritebase             = 0;
  int _dspwritecount            = 0;
  int64_t _samplesuntilnexttick = 0;
  bool _lock_compute            = true;
  float _cpuload                = 0.0f;

  Layer* _hudLayer   = nullptr;
  bool _clearhuddata = true;
  int _numFrames     = 0;
  std::atomic<int> _numactivevoices;
  ork::MpMcBoundedQueue<ork::svar1024_t> _hudbuf;

  HudFrameControl _curhud_kframe;
  hudvp_ptr_t _hudvp;
};

} // namespace ork::audio::singularity
