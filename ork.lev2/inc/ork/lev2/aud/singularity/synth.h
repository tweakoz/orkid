////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/orktypes.h>
#include <ork/math/audiomath.h>
#include "krztypes.h"
#include "synthdata.h"
#include "layer.h"
#include <ork/kernel/concurrent_queue.h>
#include <ork/kernel/svariant.h>
#include <ork/lev2/aud/singularity/seq.h>

namespace ork::audio::singularity {

///////////////////////////////////////////////////////////////////////////////

struct programInst {
  programInst();
  ~programInst();

  void keyOn(int note, int velocity, prgdata_constptr_t pd, keyonmod_ptr_t kmod = nullptr);
  void keyOff();

  prgdata_constptr_t _progdata;
  keyonmod_ptr_t _keymods;
  std::vector<layer_ptr_t> _layers;
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
  layer_ptr_t _dsplayer = nullptr;
  scopesource_ptr_t _scopesource;
  std::string _fxname;
  prgdata_constptr_t _uiprogram;
  float _prog_gain = 0.0f;
  std::vector<outbus_ptr_t> _children;
  fxpresetmap_t::iterator _fxcurpreset;
  /////////////////////////
};

///////////////////////////////////////////////////////////////////////////////

struct AudioThreadHandler{
  using audiohandler_t = std::function<void(synth*)>;
  audiohandler_t _handler = [](synth*){};
};

using audiothreadhandler_ptr_t = std::shared_ptr<AudioThreadHandler>;

struct synth {
  synth();
  ~synth();


  using eventmap_t = std::multimap<float, void_lambda_t>;

  static synth_ptr_t _instance;
  static synth_ptr_t instance();
  static void tearDown();
  static void bringUp();

  typedef std::vector<hudsample> hudsamples_t;

  void setSampleRate(float sr);
  inline float sampleRate() const {
    return _sampleRate;
  }

  outbus_ptr_t createOutputBus(std::string named);
  outbus_ptr_t outputBus(std::string named) const;

  void compute(int inumframes, const void* inputbuffer);

  programInst* keyOn(int note, int velocity, prgdata_constptr_t pd, keyonmod_ptr_t kmod = nullptr);
  void keyOff(programInst* p);

  void _keyOnLayer(layer_ptr_t l, int note, int velocity, lyrdata_ptr_t ld, keyonmod_ptr_t kmod = nullptr);
  void _keyOffLayer(layer_ptr_t l);
  void _cleanupKeyOnModifiers();


  programInst* liveKeyOn(int note, int velocity, prgdata_constptr_t pd, keyonmod_ptr_t kmod = nullptr);
  void liveKeyOff(programInst* p);

  layer_ptr_t allocLayer();
  void releaseLayer(layer_ptr_t l);
  void deactivateVoices();
  void activateVoices(int ifrpending);

  void resetFenables();

  void addEvent(float time, void_lambda_t ev);
  void _tick(eventmap_t& emap, float dt);
  float _timeaccum;

  void nextEffect(outbus_ptr_t bus); // temporary
  void prevEffect(outbus_ptr_t bus); // temporary
  void setEffect(outbus_ptr_t bus, std::string name); // temporary
  void mainThreadHandler();
  
  fxpresetmap_t _fxpresets;

  std::map<std::string, outbus_ptr_t> _outputBusses;
  std::vector<onkey_t> _onkey_subscribers;
  onprofframe_t _onprofilerframe = nullptr;
  outbus_ptr_t _tempbus;

  outputBuffer _ibuf;
  outputBuffer _obuf;
  float _sampleRate;
  float _dt;
  float _system_tempo = 120.0f;

  using keyonmodvect_t = std::vector<keyonmod_ptr_t>;
  using proginstset_t = std::set<programInst*>;

  std::set<layer_ptr_t> _allVoices;
  std::set<programInst*> _allProgInsts;

  std::set<layer_ptr_t> _freeVoices;
  std::set<layer_ptr_t> _activeVoices;
  std::set<layer_ptr_t> _pendactVoices;
  std::queue<layer_ptr_t> _deactiveateVoiceQ;
  LockedResource<proginstset_t> _freeProgInst;
  LockedResource<proginstset_t> _activeProgInst;
  std::map<std::string, hudsamples_t> _hudsample_map;
  LockedResource<keyonmodvect_t> _CCIVALS;
  LockedResource<eventmap_t> _eventmap;
  std::vector<audiothreadhandler_ptr_t> _audiothreadhandlers;

  void resize(int numframes);

  prgdata_constptr_t _globalprog;
  bankdata_ptr_t _globalbank;

  std::map<int, prgdata_ptr_t>::iterator _globalprgit;
  void nextProgram();
  void prevProgram();

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

  outbus_ptr_t _curprogrambus;

  layer_ptr_t _hudLayer   = nullptr;
  bool _clearhuddata = true;
  int _numFrames     = 0;
  std::atomic<int> _numactivevoices;
  ork::MpMcBoundedQueue<ork::svar1024_t> _hudbuf;

  HudFrameControl _curhud_kframe;
  hudvp_ptr_t _hudvp;

  std::vector<keyonmod_ptr_t> _kmod_exec_list;
  std::vector<size_t> _kmod_rem_list;
  sequencer_ptr_t _sequencer;
};

} // namespace ork::audio::singularity
