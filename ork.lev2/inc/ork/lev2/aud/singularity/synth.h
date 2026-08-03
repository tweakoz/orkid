////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/orktypes.h>
#include <ork/lev2/lev2_types.h>
#include <ork/math/audiomath.h>
#include "krztypes.h"
#include "synthdata.h"
#include "layer.h"
#include <ork/kernel/concurrent_queue.h>
#include <ork/kernel/timer.h>
#include <ork/kernel/svariant.h>
#include <ork/lev2/aud/singularity/seq.h>
#include <ork/lev2/aud/singularity/filters.h>
#include <ork/lev2/aud/singularity/audiojobpool.h>
#include <ork/math/cmatrix4.h>
#include <array>

namespace ork::audio::singularity {

///////////////////////////////////////////////////////////////////////////////
// RtPoolSet: membership set over a CLOSED, preallocated population.
//  every item carries a stable _poolIndex in [0,N), so the set is a dense array
//  plus an index map: O(1) insert/erase/contains and NO node is ever allocated.
//  the voice pool's sets are mutated by the audio thread at every control pass
//  (allocLayer / activateVoices / deactivateVoices), where reaching the
//  allocator is not permitted - std::set cannot be used there.
//  insert de-duplicates, matching the std::set semantics the programInst
//  free-list relies on (liveKeyOff can re-insert an already-free instance).
//  iteration order is insertion order with an erased slot back-filled from the
//  end: deterministic, but NOT stable across an erase.
///////////////////////////////////////////////////////////////////////////////

template <typename T, size_t N> struct RtPoolSet {

  RtPoolSet() {
    _slots.fill(-1);
  }

  // only members of the pool this set was built over may be presented - a
  //  Layer built outside it (bus-DSP layers, insert-group layers) carries
  //  _poolIndex -1 and has no business here.
  static void _validate(const T& item) {
    assert(item->_poolIndex >= 0 and item->_poolIndex < int(N));
  }

  bool contains(const T& item) const {
    _validate(item);
    return _slots[item->_poolIndex] >= 0;
  }
  bool insert(const T& item) {
    _validate(item);
    int pidx = item->_poolIndex;
    if (_slots[pidx] >= 0)
      return false;
    _slots[pidx]     = _count;
    _dense[_count++] = item;
    return true;
  }
  bool erase(const T& item) {
    _validate(item);
    int pidx = item->_poolIndex;
    int slot = _slots[pidx];
    if (slot < 0)
      return false;
    int last = --_count;
    if (slot != last) {
      _dense[slot]                     = _dense[last];
      _slots[_dense[slot]->_poolIndex] = slot;
    }
    _dense[last] = T();
    _slots[pidx] = -1;
    return true;
  }
  // pop an arbitrary member (the free-lists have no ordering requirement)
  T takeAny() {
    if (0 == _count)
      return T();
    T item = _dense[_count - 1];
    erase(item);
    return item;
  }
  void clear() {
    for (int i = 0; i < _count; i++) {
      _slots[_dense[i]->_poolIndex] = -1;
      _dense[i]                     = T();
    }
    _count = 0;
  }
  size_t size() const {
    return size_t(_count);
  }
  bool empty() const {
    return 0 == _count;
  }
  T& operator[](int index) {
    return _dense[index];
  }
  T* begin() {
    return _dense.data();
  }
  T* end() {
    return _dense.data() + _count;
  }

  std::array<T, N> _dense{}; // value-initialized: slots past _count read as null
  std::array<int, N> _slots;
  int _count = 0;
};

///////////////////////////////////////////////////////////////////////////////
// voice stealing policy (A0b). PRIORITY is the composite the spec names:
//  (component priority, already-releasing, audibility, age).
///////////////////////////////////////////////////////////////////////////////

enum class VoiceStealPolicy : int {
  OFF      = 0, // no stealing - pool exhaustion is a hard error (legacy behavior)
  OLDEST   = 1,
  QUIETEST = 2,
  PRIORITY = 3,
};

///////////////////////////////////////////////////////////////////////////////

struct programInst {
  programInst();
  ~programInst();

  void keyOn(int note, int velocity, prgdata_constptr_t pd, keyonmod_ptr_t kmod = nullptr);
  void keyOff();

  prgdata_constptr_t _progdata;
  keyonmod_ptr_t _keymods;
  std::vector<layer_ptr_t> _layers;
  int _note = 0;
  int _velocity = 0;
  int _poolIndex = -1; // stable slot in the synth's programInst pool
  fmtx4 _emitter_matrix;
  float _gain = 0.0f;
  float _fadeGainLinear = 1.0f;
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

///////////////////////////////////////////////////////////////////////////////
// InsertGroup: N branches fed the same input, their outputs summed (fork/join)
//  - Single branch = serial insert
//  - Multiple branches = parallel insert (fork input, join outputs)
//  Branches compute SERIALLY inside the owning bus's audio job — the fork is in
//  the signal flow, not in the threading (nesting kickAndJoin inside a pool job
//  is a reentrancy question nobody needs to answer for a handful of branches).
///////////////////////////////////////////////////////////////////////////////

struct InsertGroup {
  std::vector<lyrdata_ptr_t> _layerdatas;   // branch data (config)
  std::vector<layer_ptr_t> _layers;          // runtime branch layers, 1:1 with _layerdatas
  float _mixGain = 1.0f;                     // gain applied to the joined branch sum

  bool isParallel() const { return _layerdatas.size() > 1; }
  size_t numLayers() const { return _layerdatas.size(); }

  // prepare/dispose mirror the bus-DSP install split (prepareBusDSP):
  //  prepare* runs on the CALLING thread and does every allocation (branch
  //  layers, alg graphs, dsp blocks, convolver IR FFTs) so that computeInserts
  //  allocates nothing; dispose* runs on the audio thread and only returns the
  //  superseded branch algs to their voice caches.
  //  a branch-list edit passes its prebuilt branch to the audio thread and does
  //  the list edit THERE — the caller never reads a live group, which is what
  //  keeps back-to-back edits from racing (or silently overwriting each other).
  static layer_ptr_t prepareBranch(lyrdata_ptr_t ld, synth* syn);
  static void disposeBranch(layer_ptr_t l);
  void prepare(synth* syn);
  void disposeLayers();
};
using insertgroup_ptr_t = std::shared_ptr<InsertGroup>;

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
  // Split of setBusDSP for off-RT effect installation:
  //  prepareBusDSP builds+warms the bus-DSP layer (alg graph, dsp blocks,
  //  convolver IR FFTs) on the CALLING thread and touches no live bus state;
  //  commitBusDSP performs only the audio-thread pointer swap and disposes
  //  the superseded layer (cheap: its alg is returned to the voice cache).
  //  The synth is passed explicitly (not fetched via synth::instance()) so the
  //  synchronous build is safe from inside the synth constructor's own
  //  setEffect(mainbus,"none") — the singleton is not yet published there.
  layer_ptr_t prepareBusDSP(lyrdata_ptr_t ld, synth* syn);
  void commitBusDSP(layer_ptr_t prebuilt, lyrdata_ptr_t ld);

  lyrdata_ptr_t _dsplayerdata;
  layer_ptr_t _dsplayer = nullptr;
  scopesource_ptr_t _scopesource;
  std::string _fxname;
  prgdata_constptr_t _uiprogram;
  float _prog_gain = 0.0f;
  std::vector<outbus_ptr_t> _children;
  fxpresetmap_t::iterator _fxcurpreset;
  layer_vect_t _exec_layers;

  /////////////////////////
  // DAW channel strip controls
  /////////////////////////

  bool _mute = false;
  bool _solo = false;
  float _pan = 0.0f;  // -1.0 (left) to +1.0 (right)

  /////////////////////////
  // Insert effects chain
  // Signal flow: Voices → _insertGroups[0..n] → _dsplayerdata → Output
  /////////////////////////

  std::vector<InsertGroup> _insertGroups;
  void computeInserts(int inumframes, int base, int count);
  /////////////////////////
};

///////////////////////////////////////////////////////////////////////////////

struct AudioThreadHandler{
  using audiohandler_t = std::function<void(synth*)>;
  audiohandler_t _handler = [](synth*){};
};

using audiothreadhandler_ptr_t = std::shared_ptr<AudioThreadHandler>;

struct ProgramChannel{
    prgdata_constptr_t _currentProgram;
    std::unordered_set<programInst*> _monoprogs;
    int _monokeycount = 0;
    std::vector<int> _mononotes;
    std::mutex _mutex;
};
using programchannel_ptr_t = std::shared_ptr<ProgramChannel>;

struct synth {
  synth();
  ~synth();
  void deinit();

  void disableMasterEq();
  void enableMasterEq();
  void setMasterEqBand(int band, float frqHZ, float widthHZ, float gainDB);

  using eventmap_t = std::multimap<float, void_lambda_t>;

  static synth_ptr_t instance();
  static void tearDown();

  typedef std::vector<hudsample> hudsamples_t;

  void setSampleRate(float sr);
  inline float sampleRate() const {
    return _sampleRate;
  }
  std::string statusString() const;
  outbus_ptr_t createOutputBus(std::string named);
  outbus_ptr_t outputBus(std::string named) const;

  void compute(int inumframes, const void* inputbuffer);

  programInst* keyOn(int note, int velocity, prgdata_constptr_t pd, keyonmod_ptr_t kmod = nullptr);
  void keyOff(programInst* p);

  void _keyOnLayer(layer_ptr_t l, int note, int velocity, lyrdata_ptr_t ld, keyonmod_ptr_t kmod = nullptr);
  void _keyOffLayer(layer_ptr_t l);
  void _cleanupKeyOnModifiers();


  programInst* liveKeyOn(int note, int velocity, prgdata_constptr_t pd, keyonmod_ptr_t kmod = nullptr);
  void liveKeyOff(programInst* p,int note, int velocity);

  layer_ptr_t allocLayer();
  void releaseLayer(layer_ptr_t l);
  void deactivateVoices();
  void activateVoices(int ifrpending);

  // voice stealing (A0b) - audio thread only, reached from allocLayer
  layer_ptr_t _selectStealVictim(bool sustaining_only);
  void _reclaimVoice(layer_ptr_t l);

  void resetFenables();

  void addEvent(float time, void_lambda_t ev);
  void _tick(eventmap_t& emap, float dt);
  float _timeaccum;

  void nextEffect(outbus_ptr_t bus); // temporary
  void prevEffect(outbus_ptr_t bus); // temporary
  void setEffect(outbus_ptr_t bus, std::string name); // temporary
  bool mainThreadHandler(); // returns true if any posted events were drained

  // audio job pool lifecycle — owned by the audio subsystem: brought up in
  // OrkEzApp::_audioInit (before the device starts) and torn down in
  // _audioExit (after the device stops). worker count from
  // ORKID_AUDIO_JOB_THREADS (default 3, 0 = compute inline/serial).
  void startupAudioJobPool();
  void shutdownAudioJobPool();
  
  fxpresetmap_t _fxpresets;

  std::map<std::string, outbus_ptr_t> _outputBusses;
  std::vector<onkey_t> _onkey_subscribers;
  onprofframe_t _onprofilerframe = nullptr;
  outbus_ptr_t _tempbus;

  bool _enableMasterEq = false;
  ParaOne _peqL[8];
  ParaOne _peqR[8];

  outputBuffer _ibuf;
  outputBuffer _obuf;
  float _sampleRate;
  float _dt;
  float _system_tempo = 120.0f;
  programchannel_ptr_t _prgchannel;

  using keyonmodvect_t = std::vector<keyonmod_ptr_t>;
  using proginstset_t = RtPoolSet<programInst*, kmaxlayerspersynth>;
  using voiceset_t    = RtPoolSet<layer_ptr_t, kmaxlayerspersynth>;

  std::set<layer_ptr_t> _allVoices;
  std::set<programInst*> _allProgInsts;

  // the voice pool. RT-thread-only and unlocked - the single-thread invariant
  //  is what makes these safe without a lock, so nothing off the audio thread
  //  may touch them. deactivation is flagged on the Layer (Layer::_wantsDeactivate)
  //  rather than queued, so a stolen voice can never carry a stale queue entry
  //  into its next keyOn.
  voiceset_t _freeVoices;
  voiceset_t _activeVoices;
  voiceset_t _pendactVoices;
  LockedResource<proginstset_t> _freeProgInst;
  LockedResource<proginstset_t> _activeProgInst;
  std::map<std::string, hudsamples_t> _hudsample_map;
  LockedResource<keyonmodvect_t> _CCIVALS;
  LockedResource<eventmap_t> _eventmap;
  std::vector<audiothreadhandler_ptr_t> _audiothreadhandlers;

  // delay line pool. freeDelayLine is reachable from the audio thread (dsp
  //  block dtors), where the 1MB DelayContext::clear() and the pool mutex are
  //  both illegal - it only hands the line to _delaydisposalq (lock-free,
  //  allocation-free) and drainDelayDisposal does the work off-RT (main pump).
  //  the queue holds the whole pool, so try_push can never see backpressure.
  static constexpr size_t kNumDelayContexts = 1024;
  using delaydequeue_t = std::deque<delaycontext_ptr_t>;
  LockedResource<delaydequeue_t> _delayspool;
  ork::MpMcBoundedQueue<delaycontext_ptr_t, kNumDelayContexts> _delaydisposalq;

  delaycontext_ptr_t allocDelayLine();
  void freeDelayLine(delaycontext_ptr_t);
  size_t drainDelayDisposal();

  void resize(int numframes);

  // pin every audio-hot pool to one NUMA node and take it out of the automatic
  //  balancer's scan set (see lev2/aud/audio_numa.h). one shot, at the end of
  //  construction, and only when a real device is driving.
  void bindAudioPoolsForRealtime();

  prgdata_constptr_t _globalprog;
  bankdata_ptr_t _globalbank;

  std::map<int, prgdata_ptr_t>::iterator _globalprgit;
  void nextProgram();
  void prevProgram();
  void enqueueHudEvent(hudevent_ptr_t hev);
  void registerSinkForHudEvent(uint32_t eventID, hudeventsink_ptr_t sink);
  void panic();
  
  int _soloLayer       = -1;
  size_t _numActiveVoices = 0;
  size_t _numActiveDspStages = 0;
  size_t _numActiveDspBlocks = 0;
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
  // voice-stealing knobs (A0b). read on the audio thread every allocLayer -
  //  mutate them through addEvent so the change lands at a control-pass
  //  boundary, exactly like _masterGain.
  VoiceStealPolicy _stealPolicy = VoiceStealPolicy::PRIORITY;
  int _voiceHeadroom            = 0;
  std::atomic<int> _stealCounter{0};
  int _dspwritebase             = 0;
  int _dspwritecount            = 0;
  int64_t _samplesuntilnexttick = 0;
  bool _lock_compute            = true;
  float _cpuload                = 0.0f;
  float _velcurvepower          = 0.5f;
  fmtx4 _listener_matrix;
  fmtx4 _inv_listener_matrix;

  // THE listener writer: every positional-audio producer publishes the listener
  //  through here, handing in the rig/walker VIEW matrix (world->rig). When a VR
  //  device is publishing a head pose the listener becomes the CENTER head view
  //  (orkidvr::composeHeadViewMatrix — the same rig*base*hmd the VR output nodes
  //  render between the eyes), so the ears track the head, not the rig. With no
  //  VR device the listener IS the matrix handed in.
  void setListenerFromRigView(const fmtx4& rig_view_matrix);
  std::atomic<int> _lifecycle_state;
  std::atomic<int> _num_soloed{0};  // Count of soloed buses for O(1) check

  void waitUntilReady() const;

  outbus_ptr_t _curprogrambus;

  layer_ptr_t _hudLayer   = nullptr;
  bool _clearhuddata = true;
  int _numFrames     = 0;
  std::atomic<int> _numactivevoices;
  ork::MpMcBoundedQueue<ork::svar1024_t> _hudbuf;

  HudFrameControl _curhud_kframe;
  hudvp_ptr_t _hudvp;
  hudeventrouter_ptr_t _hudEventRouter;

  // per-bus fork/join job slots for the RT compute fan-outs (mix, effects).
  // slot arrays are reused each control pass; each kickAndJoin completes
  // before the next begins, so one set suffices for both fan-outs.
  static constexpr int kMaxAudioJobs = 64;
  struct BusJobContext {
    synth* _synth      = nullptr;
    OutputBus* _bus    = nullptr;
    int _inumframes    = 0;
  };
  AudioJobPool _audioJobPool;
  BusJobContext _busJobContexts[kMaxAudioJobs];
  AudioJob _busJobs[kMaxAudioJobs];

  std::vector<keyonmod_ptr_t> _kmod_exec_list;
  std::vector<size_t> _kmod_rem_list;
  sequencer_ptr_t _sequencer;
};

} // namespace ork::audio::singularity
