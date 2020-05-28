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

  void keyOn(int note, const ProgramData* pd);
  void keyOff();

  const ProgramData* _progdata;

  std::vector<Layer*> _layers;
};

///////////////////////////////////////////////////////////////////////////////

struct SynthData;

struct hudsample {
  float _time;
  float _value;
};
using synth_ptr_t = std::shared_ptr<synth>;

struct synth {
  synth();
  ~synth();

  static synth_ptr_t instance();

  typedef std::vector<hudsample> hudsamples_t;

  void setSampleRate(float sr);
  inline float sampleRate() const {
    return _sampleRate;
  }

  void compute(int inumframes, const void* inputbuffer);

  programInst* keyOn(int note, const ProgramData* pd);
  void keyOff(programInst* p);

  Layer* allocLayer();
  void freeLayer(Layer* l);
  void deactivateVoices();

  void resetFenables();

  void addEvent(float time, void_lambda_t ev);
  void tick(float dt);
  float _timeaccum;

  outputBuffer _ibuf;
  outputBuffer _obuf;
  float _sampleRate;
  float _dt;

  std::set<Layer*> _allVoices;
  std::set<programInst*> _allProgInsts;

  std::set<Layer*> _freeVoices;
  std::set<Layer*> _activeVoices;
  std::queue<Layer*> _deactiveateVoiceQ;
  std::set<programInst*> _freeProgInst;
  std::set<programInst*> _activeProgInst;
  std::map<std::string, hudsamples_t> _hudsample_map;

  std::multimap<float, void_lambda_t> _eventmap;

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
  int _genmode      = 0;
  float _ostriglev  = 0;
  bool _ostrigdir   = false;
  int64_t _oswidth  = 0;
  bool _bypassDSP   = false;
  bool _doModWheel  = false;
  bool _doPressure  = false;
  bool _doInput     = false;
  float _masterGain = 1.0;

  Layer* _hudLayer   = nullptr;
  bool _clearhuddata = true;
  int _numFrames     = 0;

  ork::MpMcBoundedQueue<ork::svar1024_t> _hudbuf;

  HudFrameControl _curhud_kframe;
  hudvp_ptr_t _hudvp;
};

} // namespace ork::audio::singularity
