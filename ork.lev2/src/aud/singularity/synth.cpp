////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <string>
#include <assert.h>
#include <unistd.h>
#include <math.h>

#include <ork/lev2/aud/audio_numa.h>
#include <ork/lev2/aud/singularity/synthdata.h>
#include <ork/lev2/aud/singularity/synth.h>
#include <ork/lev2/aud/singularity/hud.h>
#include <ork/lev2/aud/singularity/keyon_prof.h>
#include <ork/lev2/aud/singularity/spike_diag.h>
#include <ork/lev2/aud/singularity/krzobjects.h>
#include <ork/lev2/aud/singularity/dspblocks.h>
#include <ork/lev2/aud/singularity/fxgen.h>
#include <ork/lev2/vr/vr.h>
#include <ork/lev2/aud/singularity/soundfield.h>
#include <ork/util/logger.h>
#include <ork/kernel/opq.h>
#include <ork/kernel/profiler.h>

namespace ork::audio::singularity {
static logchannel_ptr_t logchan_synth = logger()->configureChannel("SingulSynth", fvec3(1, 0.6, .8), true);
///////////////////////////////////////////////////////////////////////////////
// OutputBus::computeInserts - process insert effect chain
// Signal flow: input (bus buffer) → insert groups → output (bus buffer)
// Groups run in serial; the branches of a group fork/join: every branch is fed
//  the group's input, and the branch outputs are summed and scaled by _mixGain.
//  A one-branch group is just the degenerate case (its output replaces the
//  input, weighted by _mixGain like any other group).
// RT contract: branch layers and their buffers are built at install time
//  (InsertGroup::prepare) — nothing here allocates.
///////////////////////////////////////////////////////////////////////////////
void OutputBus::computeInserts(int inumframes, int base, int count) {
  // Skip if no insert groups - this is the common case for existing code
  if (_insertGroups.empty()) {
    return;
  }

  auto& bus_buf    = _buffer;
  float* bus_left  = bus_buf._leftBuffer + base;
  float* bus_right = bus_buf._rightBuffer + base;

  // Process each insert group in serial order
  for (auto& group : _insertGroups) {
    size_t numbranches = group._layerdatas.size();
    if (0 == numbranches) {
      continue;
    }
    OrkAssertIFMT(
        group._layers.size() == numbranches, //
        "insert group has %zu branches but %zu runtime layers - install did not prepare()",
        numbranches,
        group._layers.size());

    //////////////////////////////////////////
    // fork: EVERY branch is loaded with the group input before ANY branch runs
    //  — the join below overwrites that input in place.
    //////////////////////////////////////////
    for (size_t ib = 0; ib < numbranches; ib++) {
      auto dsp_buf = group._layers[ib]->_dspbuffer;
      dsp_buf->resize(inumframes);
      float* dsp_left  = dsp_buf->channel(0);
      float* dsp_right = dsp_buf->channel(1);
      for (int i = 0; i < count; i++) {
        dsp_left[i]  = bus_left[i];
        dsp_right[i] = bus_right[i];
      }
    }

    //////////////////////////////////////////
    // compute: branches run serially inside this bus's job (see InsertGroup).
    //  each branch reads and writes its own layer's buffer, so they are
    //  independent regardless of ordering.
    //////////////////////////////////////////
    for (size_t ib = 0; ib < numbranches; ib++) {
      auto layer = group._layers[ib];
      layer->beginCompute(count);
      layer->updateControllers();
      layer->compute(0, count);
      layer->endCompute();
    }

    //////////////////////////////////////////
    // join: mixGain-weighted sum of the branch outputs → bus
    //////////////////////////////////////////
    float gain = group._mixGain;
    for (size_t ib = 0; ib < numbranches; ib++) {
      auto dsp_buf           = group._layers[ib]->_dspbuffer;
      const float* out_left  = dsp_buf->channel(0);
      const float* out_right = dsp_buf->channel(1);
      if (0 == ib) {
        for (int i = 0; i < count; i++) {
          bus_left[i]  = out_left[i] * gain;
          bus_right[i] = out_right[i] * gain;
        }
      } else {
        for (int i = 0; i < count; i++) {
          bus_left[i] += out_left[i] * gain;
          bus_right[i] += out_right[i] * gain;
        }
      }
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
void synth::nextEffect(outbus_ptr_t bus) {
  _eventmap.atomicOp([=](eventmap_t& emap) { //
    emap.insert(std::make_pair(0.0f, [=]() {
      ///////////////////////////////
      auto it = bus->_fxcurpreset;
      if (it == _fxpresets.end()) {
        it = _fxpresets.begin();
      } else {
        it++;
      }
      if (it == _fxpresets.end()) {
        it = _fxpresets.begin();
      }
      ///////////////////////////////
      bus->_fxcurpreset = it;
      auto nextpreset   = *(bus->_fxcurpreset);
      assert(nextpreset->_algdata != nullptr); // did you add presets ?
      bus->setBusDSP(nextpreset);
      bus->_fxname = nextpreset->_name;
      if(0)logchan_synth->log("switched to effect<%s>", bus->_fxname.c_str());
    }));
  });
}
///////////////////////////////////////////////////////////////////////////////
void synth::prevEffect(outbus_ptr_t bus) {
  _eventmap.atomicOp([=](eventmap_t& emap) { //
    emap.insert(std::make_pair(0.0f, [=]() {
      auto it = bus->_fxcurpreset;
      if (it != _fxpresets.end()) {
        if (it == _fxpresets.begin()) {
          // If it's the beginning, rotate to the end
          it = std::prev(_fxpresets.end());
        } else {
          // Otherwise, just decrement
          --it;
        }
      }
      ///////////////////////////////
      bus->_fxcurpreset = it;
      auto nextpreset   = *(bus->_fxcurpreset);
      assert(nextpreset->_algdata != nullptr); // did you add presets ?
      bus->setBusDSP(nextpreset);
      bus->_fxname = nextpreset->_name;
      if(0)logchan_synth->log("switched to effect<%s>", bus->_fxname.c_str());
    }));
  });
}
///////////////////////////////////////////////////////////////////////////////
void synth::setEffect(outbus_ptr_t bus, std::string name) {
  fxpresetmap_t::iterator it = _fxpresets.begin();
  for (; it != _fxpresets.end(); ++it) {
    if ((*it)->_name == name) {
      break;
    }
  }
  if (it != _fxpresets.end()) {
    auto nextpreset = (*it);
    assert(nextpreset->_algdata != nullptr); // did you add presets ?
    // Build+warm the bus-DSP layer here on the caller (setup) thread, OFF the
    // audio thread — this is where the convolver IR FFTs live. The deferred
    // event then only pointer-swaps the fully-warmed layer into the bus.
    // The layer is captured by shared_ptr so it stays alive until the swap.
    // `this` (not synth::instance()) keeps the synchronous build safe for the
    // constructor's own setEffect(mainbus,"none") — see prepareBusDSP.
    auto prebuilt = bus->prepareBusDSP(nextpreset, this);
    _eventmap.atomicOp([=](eventmap_t& unlocked) { //
      float timestamp         = 0.0f;              // now
      auto deferred_operation = [=]() {
        bus->commitBusDSP(prebuilt, nextpreset);
        bus->_fxname      = name;
        bus->_fxcurpreset = it;
        if(0)logchan_synth->log("switched to effect<%s>", name.c_str());
      };
      unlocked.insert(std::make_pair(timestamp, deferred_operation));
    });
  }
}

///////////////////////////////////////////////////////////////////////////////
void synth::tearDown() {
  instance()->deinit();
}
synth_ptr_t synth::instance() {
  static synth_ptr_t ginstance = std::make_shared<synth>();
  return ginstance;
}
///////////////////////////////////////////////////////////////////////////////
outbus_ptr_t synth::createOutputBus(std::string named) {
  auto bus   = std::make_shared<OutputBus>();
  bus->_name = named;
  // a bus created after the block size settled would otherwise never be sized
  //  (synth::resize only acts when numframes GROWS) and would hand out null
  //  buffers to the clear/mix/send passes.
  bus->resize(_numFrames);
  _outputBusses[named] = bus;
  return bus;
}
///////////////////////////////////////////////////////////////////////////////
outbus_ptr_t synth::outputBus(std::string named) const {
  auto it = _outputBusses.find(named);
  return (it != _outputBusses.end()) //
             ? it->second
             : nullptr;
}
///////////////////////////////////////////////////////////////////////////////
std::atomic<int> galloccount = 0;
delaycontext_ptr_t synth::allocDelayLine() {
  KeyOnProfScope prof(KOP_DELAYALLOC);
  delaycontext_ptr_t rval;
  // keep this lambda's capture at or under 16 bytes - atomicOp takes a
  //  std::function, and a larger capture heap-allocates on the audio thread.
  auto take = [&rval](delaydequeue_t& unlocked) {
    if (unlocked.empty())
      return;
    rval = unlocked.back();
    unlocked.pop_back();
    int count = galloccount.fetch_add(1);
    // printf("alloc<%d>\n", count );
  };
  _delayspool.atomicOp(take);
  if (nullptr == rval) {
    // pool empty: lines freed since the last off-RT drain are still pending.
    //  recover inline rather than hand back nothing - this pays clear() on the
    //  calling thread (possibly the audio thread), so it is a degrade path,
    //  not a design point: mainThreadHandler is the intended drain site.
    drainDelayDisposal();
    _delayspool.atomicOp(take);
  }
  OrkAssertIFMT(rval != nullptr, "delay line pool exhausted (all %zu contexts in use)", kNumDelayContexts);
  return rval;
}
///////////////////////////////////////////////////////////////////////////////
// freeDelayLine runs on the AUDIO THREAD (dsp block dtors, at voice keyOn and
//  bus-fx commit): no OPQ enqueue (copies a closure -> malloc), no pool mutex,
//  and no DelayContext::clear() (1MB memset) here. the line is handed to the
//  lock-free disposal queue and serviced by drainDelayDisposal off-RT.
///////////////////////////////////////////////////////////////////////////////
void synth::freeDelayLine(delaycontext_ptr_t delay) {
  bool pushed = _delaydisposalq.try_push(delay);
  // the queue holds the entire pool, so a full queue cannot mean backpressure -
  //  it means the same line was freed twice.
  OrkAssertI(pushed, "delay disposal queue full - delay line freed twice?");
}
///////////////////////////////////////////////////////////////////////////////
// off-RT service for lines freed on the audio thread. returns the number of
//  lines returned to the pool.
///////////////////////////////////////////////////////////////////////////////
size_t synth::drainDelayDisposal() {
  size_t numdrained = 0;
  delaycontext_ptr_t delay;
  while (_delaydisposalq.try_pop(delay)) {
    delay->clear();
    _delayspool.atomicOp([&delay](delaydequeue_t& unlocked) {
      unlocked.push_front(delay);
      int count = galloccount.fetch_add(-1);
      // printf("free<%d>\n", count );
    });
    numdrained++;
  }
  return numdrained;
}
///////////////////////////////////////////////////////////////////////////////
synth::synth()
    : _timeaccum(0.0f)
    , _sampleRate(0.0f)
    , _dt(0.0f)
    , _soloLayer(-1)
    , _hudpage(0)
    , _masterGain(1.0f) { //

  _lifecycle_state = 0;

  _hudEventRouter = std::make_shared<HudEventRouter>();

  //logchan_synth->log("clearing delay lines...");
  std::atomic<int> delayopcounter{kNumDelayContexts};
  for (size_t i = 0; i < kNumDelayContexts; i++) {
    auto op = [this, &delayopcounter]() {
      auto delay = std::make_shared<DelayContext>();
      delay->clear();
      _delayspool.atomicOp([&](delaydequeue_t& unlocked) {
        unlocked.push_back(delay);
      });
      delayopcounter.fetch_sub(1);
    };
    opq::concurrentQueue()->enqueue(op);
  }
  while (delayopcounter.load() > 0) {
    usleep(1000);
  }
  //logchan_synth->log("delay lines cleared.");

  _sequencer  = std::make_shared<Sequencer>(this);
  _prgchannel = std::make_shared<ProgramChannel>();

  _tempbus              = std::make_shared<OutputBus>();
  _tempbus->_name       = "temp-dsp";
  _numactivevoices      = 0;
  auto mainbus          = createOutputBus("main");
  _curprogrambus        = mainbus;
  mainbus->_fxcurpreset = _fxpresets.rbegin().base();

  // TODO - synth::instance(); is creating chicken and egg problems
  // Live constraint for anything reached from HERE: we run inside the Meyers singleton's
  // constructor, so synth::instance() re-entered from this call tree deadlocks on the
  // static-init guard. Both calls below take the synth explicitly (loadAllFxPresets(this);
  // setEffect -> prepareBusDSP(ld, this)) for exactly that reason - keep it that way.
  loadAllFxPresets(this);

  setEffect(mainbus, "none");

  // the pools are closed populations: _poolIndex is handed out here, once, and
  //  is what makes every membership set below allocation-free (see RtPoolSet).
  for (int i = 0; i < kmaxlayerspersynth; i++) {
    auto l        = std::make_shared<Layer>();
    l->_poolIndex = i;
    _allVoices.insert(l);
    _freeVoices.insert(l);
  }

  for (int i = 0; i < kmaxlayerspersynth; i++) {
    auto pi        = new programInst();
    pi->_poolIndex = i;
    _freeProgInst.atomicOp([&pi](proginstset_t& piset) { piset.insert(pi); });
    _allProgInsts.insert(pi);
  }

  resize(1);

  bindAudioPoolsForRealtime();

  _lock_compute = false;
  _lifecycle_state = 1;
}

///////////////////////////////////////////////////////////////////////////////
// the audio-hot allocations, enumerated once. everything reached by the dsp
//  inner loops is here: the delay pool (1024 x 1MB, built above on concurrent
//  workers and therefore scattered across every node), each voice's dsp grid
//  buffer (32 channels x 16384 frames), and the Layer objects themselves. The
//  binder coalesces before it calls the kernel, so the small pool bookkeeping
//  interleaved between these allocations is swept in with them - which is the
//  only way to cover it without splitting the heap into a vma per object.
///////////////////////////////////////////////////////////////////////////////
void synth::bindAudioPoolsForRealtime() {

  if (not lev2::audioNumaActive()) {
    return;
  }

  lev2::AudioNumaBinder binder;

  _delayspool.atomicOp([&binder](delaydequeue_t& unlocked) {
    for (auto delay : unlocked) {
      binder.add(delay.get(), sizeof(DelayContext));
      binder.add(delay->_buffer.data(), delay->_buffer.size() * sizeof(float));
    }
  });

  for (auto lay : _allVoices) {
    binder.add(lay.get(), sizeof(Layer));
    auto dspbuf = lay->_dspbuffer;
    if (dspbuf) {
      for (int i = 0; i < kmaxdspblocksperstage; i++) {
        binder.add(dspbuf->_channels[i].data(), dspbuf->_channels[i].size() * sizeof(float));
      }
    }
  }

  binder.commit("singularity-pools");
}

void synth::setSampleRate(float sr) {
  _sampleRate = sr;
  _dt         = 1.0f / sr;
}

///////////////////////////////////////////////////////////////////////////////
// listener publication. the rig view is what the game knows (walker camera);
//  the head pose is what the ears need, and only the VR device has it. keeping
//  the compose on this side of the seam means an emitter system never has to
//  know whether VR is live.
///////////////////////////////////////////////////////////////////////////////

void synth::setListenerFromRigView(const fmtx4& rig_view_matrix) {
  fmtx4 head_view       = lev2::orkidvr::composeHeadViewMatrix(rig_view_matrix);
  _inv_listener_matrix  = head_view;            // world -> listener
  _listener_matrix      = head_view.inverse();  // listener -> world
}

void synth::waitUntilReady() const {
  while (_lifecycle_state.load() != 1) {
    usleep(1000);
  }
}

///////////////////////////////////////////////////////////////////////////////

synth::~synth() {
  deinit();
}

void synth::deinit() {

  if(_lifecycle_state.exchange(2)==2)
    return;

  // the field holds a reference to one of _outputBusses and runs a feeder
  //  thread; it must be unpublished and joined before the busses go away.
  SoundField::tearDown();

  // Clear pending events first — prevent stale KOFF events from firing
  _eventmap.atomicOp([](eventmap_t& emap) { emap.clear(); });

  opq::concurrentQueue()->drain();

  _allVoices.clear();
  _freeVoices.clear();
  _activeVoices.clear();
  _pendactVoices.clear();
  _freeProgInst.atomicOp([](proginstset_t& unlocked) { unlocked.clear(); });
  _activeProgInst.atomicOp([](proginstset_t& unlocked) { unlocked.clear(); });

  _hudsample_map.clear();
  _fxpresets.clear();
  _outputBusses.clear();
  _onkey_subscribers.clear();

  for (auto pi : _allProgInsts)
    delete pi;
}


///////////////////////////////////////////////////////////////////////////////
void synth::addEvent(float time, void_lambda_t ev) {
  _eventmap.atomicOp([time, ev](eventmap_t& emap) { //
    emap.insert(std::make_pair(time, ev));
  });
}

///////////////////////////////////////////////////////////////////////////////

void synth::_tick(eventmap_t& emap, float elapsed_this_tick) {
  KeyOnProfScope prof(KOP_EVENTDRAIN);
  bool done = false;
  while (false == done) {
    done    = true;
    auto it = emap.begin();
    if (it != emap.end() and //
        it->first <= _timeaccum) {
      auto& event = it->second;

      // logchan_synth->log("event @ time<%g>", it->first);

      event();
      done = false;
      it   = emap.erase(it);
    }
  }
  _timeaccum += elapsed_this_tick;
}

///////////////////////////////////////////////////////////////////////////////

// _selectStealVictim: rank the active voices and return the worst one, or null
//  when nothing qualifies. cheap POD reads only (no controller-map walks, no
//  allocation) - an O(kmaxlayerspersynth) sweep of the dense active array fits
//  the control-pass budget many times over.
//  sustaining_only skips voices already in their release tail: those are the
//  headroom pass's non-candidates (keying them off again buys nothing).
///////////////////////////////////////////////////////////////////////////////

layer_ptr_t synth::_selectStealVictim(bool sustaining_only) {
  layer_ptr_t victim = nullptr;
  int best_prio      = 0;
  int best_rel       = 0;
  float best_gain    = 0.0f;
  float best_age     = 0.0f;
  for (auto& l : _activeVoices) {
    if (sustaining_only and l->_lyrPhase != 0)
      continue;
    // _keymods is null for any keyOn made without modifiers (default priority)
    int prio   = (l->_keymods != nullptr) ? l->_keymods->_priority : 0;
    int rel    = (l->_lyrPhase == 1) ? 0 : 1; // already releasing sorts first
    float gain = l->_ampenvgain;
    float age  = l->_layerTime;
    bool better = (victim == nullptr);
    if (not better) {
      switch (_stealPolicy) {
        case VoiceStealPolicy::OLDEST:
          better = (age > best_age);
          break;
        case VoiceStealPolicy::QUIETEST:
          better = (gain < best_gain);
          break;
        default: // PRIORITY: the spec's composite
          if (prio != best_prio)
            better = (prio < best_prio);
          else if (rel != best_rel)
            better = (rel < best_rel);
          else if (gain != best_gain)
            better = (gain < best_gain);
          else
            better = (age > best_age);
          break;
      }
    }
    if (better) {
      victim    = l;
      best_prio = prio;
      best_rel  = rel;
      best_gain = gain;
      best_age  = age;
    }
  }
  return victim;
}

///////////////////////////////////////////////////////////////////////////////
// _reclaimVoice: the ONE active->free transition, audio thread only (the
//  deactivateVoices sweep and the steal path in allocLayer).
//  detaching the layer from its owning note is load bearing: without it a later
//  programInst::keyOff would reach a layer that has since been re-keyed by
//  somebody else and release a live voice.
///////////////////////////////////////////////////////////////////////////////

void synth::_reclaimVoice(layer_ptr_t l) {
  if (l == _hudLayer) {
    _hudLayer = nullptr;
  }
  _activeVoices.erase(l);
  l->_wantsDeactivate = false;
  l->_keepalive       = 0;
  l->endCompute();
  if (l->_keymods != nullptr) {
    l->_keymods->_dangling = true;
  }
  auto pinst = l->_programinst;
  if (pinst) {
    auto& layers = pinst->_layers;
    for (auto it = layers.begin(); it != layers.end(); ++it) {
      if (*it == l) {
        layers.erase(it);
        break;
      }
    }
  }
  assert(not _freeVoices.contains(l));
  _freeVoices.insert(l);
  if (l->_alg) {
    l->_alg->_algdata.returnAlgInst(l->_alg);
    l->_alg = nullptr;
  }
}

///////////////////////////////////////////////////////////////////////////////
// allocLayer runs on the audio thread (keyOn events drained in _tick).
//
// THE EXHAUSTION BRIDGE: an envelope-safe steal is keyOff + natural drain, but
//  that frees the voice ASYNCHRONOUSLY while a caller needs a layer NOW. the
//  two knobs cover the gap between those:
//   - _voiceHeadroom keys off the worst-ranked SUSTAINING voice as soon as the
//     free pool falls to the headroom mark, so release tails are already
//     draining before the pool actually empties;
//   - on true exhaustion the worst-ranked voice is keyed off AND hard-reclaimed
//     in place. that hard cut is the design's only audible artifact: an abrupt
//     end to the least audible voice (lowest priority, already releasing,
//     quietest, oldest). a nonzero headroom makes that victim a voice that is
//     already deep in its release tail, which is what makes the cut inaudible.
///////////////////////////////////////////////////////////////////////////////

layer_ptr_t synth::allocLayer() {
  KeyOnProfScope prof(KOP_ALLOCLAYER);
  if (_stealPolicy != VoiceStealPolicy::OFF) {
    if (_voiceHeadroom > 0 and int(_freeVoices.size()) <= _voiceHeadroom) {
      auto early = _selectStealVictim(true);
      if (early) {
        _keyOffLayer(early);
      }
    }
    if (_freeVoices.empty()) {
      // steal candidates come from the ACTIVE set only, which also keeps the
      //  in-flight layers of the keyOn we are serving (still pending) off the
      //  victim list.
      auto victim = _selectStealVictim(false);
      if (victim) {
        auto pinst = victim->_programinst;
        _keyOffLayer(victim);
        _reclaimVoice(victim);
        // a stolen note that just lost its last layer is over: hand its
        //  programInst back too, or the note pool bleeds one instance per steal
        //  and starves in exactly the situation stealing exists to survive
        //  (nobody is left to keyOff a note that was taken away).
        //  the cost is the contract of stealing: a caller still holding this
        //  programInst* holds a recycled instance, so a keyOff through it is a
        //  no-op or reaches a foreign note.
        if (pinst and pinst->_layers.empty()) {
          _activeProgInst.atomicOp([pinst](proginstset_t& piset) { piset.erase(pinst); });
          _freeProgInst.atomicOp([pinst](proginstset_t& piset) { piset.insert(pinst); });
        }
        _stealCounter.fetch_add(1);
      }
    }
  }
  // backstop: with stealing on, only reachable if every voice was keyed on
  //  within this same control pass and none has been activated yet.
  OrkAssertIFMT(
      not _freeVoices.empty(), //
      "singularity voice pool exhausted (%d voices, stealPolicy<%d>)",
      kmaxlayerspersynth,
      int(_stealPolicy));
  auto l = _freeVoices.takeAny();
  // printf( "syn alloclayer<%p>\n", l );
  assert(not _activeVoices.contains(l));
  _pendactVoices.insert(l);
  return l;
}

///////////////////////////////////////////////////////////////////////////////

void synth::releaseLayer(layer_ptr_t l) {
  if (l->_keepalive <= 0) {
    return;
  } else if ((--l->_keepalive) == 0) {
    // printf("LAYER<%p> DONE\n", this);
    l->_wantsDeactivate = true;
  }
  assert(l->_keepalive >= 0);
  // printf( "layer<%p> release cnt<%d>\n", this, _keepalive );
}

///////////////////////////////////////////////////////////////////////////////

void synth::deactivateVoices() {
  KeyOnProfScope prof(KOP_VOICEDEACT);
  // sweep the dense active array in place; _reclaimVoice back-fills the erased
  //  slot from the end, so the index is only advanced when nothing was removed.
  int i = 0;
  while (i < int(_activeVoices.size())) {
    auto l = _activeVoices[i];
    if (l->_wantsDeactivate) {
      _reclaimVoice(l);
    } else {
      i++;
    }
  }
  _numactivevoices = _activeVoices.size();
}

///////////////////////////////////////////////////////////////////////////////

void synth::activateVoices(int ifrpending) {
  KeyOnProfScope prof(KOP_VOICEACTIVATE);
  for (auto& v : _pendactVoices) {
    // frames left AFTER the pass now in flight - by the pass's own width, so a
    //  clamped tail pass cannot hand a negative frame count to the new voice.
    v->beginCompute(ifrpending - _dspwritecount);
    v->updateControllers();
    v->compute(_dspwritebase, _dspwritecount);
    _activeVoices.insert(v);
  }
  _pendactVoices.clear();
}

///////////////////////////////////////////////////////////////////////////////
void synth::nextProgram() {
  if (_globalprgit == _globalbank->_programs.end()) {
    _globalprgit = _globalbank->_programs.begin();
  } else {
    _globalprgit++;
  }
  _globalprog = _globalprgit->second;
}
void synth::prevProgram() {
  if (_globalprgit == _globalbank->_programs.rend().base()) {
    _globalprgit = _globalbank->_programs.rend().base();
  } else {
    _globalprgit--;
  }
  _globalprog = _globalprgit->second;
}
///////////////////////////////////////////////////////////////////////////////
static int GNOTE = 0;
programInst* synth::liveKeyOn(int note, int velocity, prgdata_constptr_t pdata, keyonmod_ptr_t kmods) {
  GNOTE = note;
  if (not pdata)
    return nullptr;
  programInst* pi = nullptr;

  ///////////////////////////////////////
  // SEQUENCER RECORDING
  ///////////////////////////////////////

  if (_sequencer->_recording_clip) {
    auto rec_track = _sequencer->_recording_track;
    if (rec_track->_outbus == _curprogrambus) {
      auto as_evclip = std::dynamic_pointer_cast<EventClip>(_sequencer->_recording_clip);
      if (as_evclip) {
        float time                          = _timeaccum;
        auto pb0                            = _sequencer->_sequence_playbacks[0];
        auto ts_start                       = pb0->_sequence->_timebase->timeToTimeStamp(time);
        auto nonev                          = std::make_shared<NoteOnEvent>();
        nonev->_note                        = note;
        nonev->_velocity                    = velocity;
        nonev->_timestamp                   = ts_start;
        as_evclip->_rec_noteon_events[note] = nonev;
      }
    }
  }

  ///////////////////////////////////////

  bool needs_new_trigger = true;

  // Lock _prgchannel for monophonic state access, release before addEvent
  // to avoid deadlock with _eventmap lock held by audio thread.
  {
    std::lock_guard<std::mutex> lock(_prgchannel->_mutex);
    if (pdata->_monophonic) {
      _prgchannel->_monokeycount++;
      _prgchannel->_mononotes.push_back(note);
      for (auto monopi : _prgchannel->_monoprogs) {
        if (monopi->_progdata == pdata) {
          pi                = monopi;
          needs_new_trigger = false;
          break;
        }
      }
      if (needs_new_trigger) {
        // New monophonic voice — reset state
        _prgchannel->_monokeycount = 1;
        _prgchannel->_mononotes.clear();
        _prgchannel->_mononotes.push_back(note);
      }
    }
  } // unlock before addEvent

  if (!needs_new_trigger) {
    // Monophonic re-trigger of existing voice
    addEvent(0.0f, [note, velocity, pi]() {
      for (auto l : pi->_layers) {
        l->reTriggerMono(note, velocity);
      }
    });
  } else {
    _freeProgInst.atomicOp([&pi](proginstset_t& piset) { pi = piset.takeAny(); });
    OrkAssertIFMT(
        pi != nullptr, //
        "singularity programInst pool exhausted (%d instances) - unbalanced keyOn/keyOff ?",
        kmaxlayerspersynth);
    pi->_progdata = pdata;
    addEvent(0.0f, [note, velocity, pdata, this, pi, kmods]() {
      if(0)logchan_synth->log("liveKeyOn note<%d>", note);

      int clampn = std::clamp(note, 0, 127);
      int clampv = std::clamp(velocity, 0, 127);

      pi->keyOn(clampn, clampv, pdata, kmods);
      if (kmods) {
        _CCIVALS.atomicOp([kmods](keyonmodvect_t& unlocked) { unlocked.push_back(kmods); });
      }
      _activeProgInst.atomicOp([pi](proginstset_t& piset) { //
        piset.insert(pi);
      });

      _lnoteframe   = 0;
      _lnotetime    = 0.0f;
      _clearhuddata = true;

      for (auto h : _onkey_subscribers) {
        h(clampn, clampv, pi);
      }
    });
  }
  return pi;
}
///////////////////////////////////////////////////////////////////////////////
void synth::liveKeyOff(programInst* pinst, int note, int velocity) {

  ///////////////////////////////////////
  // SEQUENCER RECORDING
  ///////////////////////////////////////

  if (_sequencer->_recording_clip) {
    auto as_evclip = std::dynamic_pointer_cast<EventClip>(_sequencer->_recording_clip);
    if (as_evclip) {
      auto rec_track = _sequencer->_recording_track;
      if (rec_track->_outbus == _curprogrambus) {
        float time    = _timeaccum;
        auto pb0      = _sequencer->_sequence_playbacks[0];
        auto timebase = pb0->_sequence->_timebase;
        auto ts_end   = timebase->timeToTimeStamp(time);
        auto it       = as_evclip->_rec_noteon_events.find(note);
        if (it != as_evclip->_rec_noteon_events.end()) {
          auto nonev = it->second;

          as_evclip->_rec_noteon_events.erase(it);

          auto ts_start = nonev->_timestamp;

          TimeStampComparatorLessEqual compare;
          bool check = compare(ts_start, ts_end);
          timestamp_ptr_t ts_duration;
          if (check) {
            ts_duration = ts_end->sub(ts_start);
          } else {
            ts_start    = std::make_shared<TimeStamp>();
            ts_duration = ts_end->sub(ts_start);
          }
          as_evclip->createNoteEvent(ts_start, ts_duration, nonev->_note, nonev->_velocity);
        }
      }
    }
  }

  ///////////////////////////////////////

  auto pdata      = pinst->_progdata;
  bool do_key_off = true;
  int retrigger_note = -1;

  // Lock _prgchannel for monophonic state access, release before addEvent
  // to avoid deadlock with _eventmap lock held by audio thread.
  {
    std::lock_guard<std::mutex> lock(_prgchannel->_mutex);
    if (pdata->_monophonic) {
      _prgchannel->_monokeycount--;
      do_key_off = (_prgchannel->_monokeycount == 0);
      if (not do_key_off) {
        int count = _prgchannel->_mononotes.size();
        for (int i = count - 1; i >= 0; i--) {
          if (_prgchannel->_mononotes[i] == note) {
            auto it = _prgchannel->_mononotes.begin() + i;
            _prgchannel->_mononotes.erase(it);
            count = _prgchannel->_mononotes.size();
            if (count) {
              retrigger_note = _prgchannel->_mononotes[count - 1];
            }
          }
        }
      }
    }
    if (do_key_off) {
      _prgchannel->_monoprogs.erase(pinst);
    }
  } // unlock before addEvent

  if (!do_key_off && retrigger_note >= 0) {
    addEvent(0.0f, [retrigger_note, pinst]() {
      for (auto l : pinst->_layers) {
        l->reTriggerMono(retrigger_note, 0);
      }
    });
  }

  if (do_key_off) {
    addEvent(0.0f, [pinst, this]() {
      pinst->keyOff();
      _activeProgInst.atomicOp([pinst](proginstset_t& piset) { //
        piset.erase(pinst);
      });
      _freeProgInst.atomicOp([pinst](proginstset_t& piset) { //
        piset.insert(pinst);
      });
    });
  }
}
///////////////////////////////////////////////////////////////////////////////

template <typename T> void _remove_items(std::vector<T>& vec, const std::vector<size_t>& indices_to_remove) {
  std::vector<T> temp;
  temp.reserve(vec.size() - indices_to_remove.size());
  size_t curr_index = 0;
  for (size_t i = 0; i < vec.size(); ++i) {
    if (curr_index < indices_to_remove.size() && indices_to_remove[curr_index] == i) {
      ++curr_index; // Skip this element
    } else {
      temp.push_back(vec[i]); // Keep this element
    }
  }

  vec.swap(temp); // Replace the original vector with the temporary one
}

///////////////////////////////////////////////////////////////////////////////
// audio job pool lifecycle — see synth.h; called from OrkEzApp::_audioInit /
// _audioExit so subsystem (and legacy) bringup/teardown own the workers.
///////////////////////////////////////////////////////////////////////////////

void synth::startupAudioJobPool() {
  int numworkers = 3;
  if (const char* env = getenv("ORKID_AUDIO_JOB_THREADS")) {
    numworkers = atoi(env);
    numworkers = std::clamp(numworkers, 0, 16);
  }
  _audioJobPool.startup(numworkers);
  logchan_synth->log("audio job pool: %d workers (+audio thread)", numworkers);
}

void synth::shutdownAudioJobPool() {
  _audioJobPool.shutdown();
}

///////////////////////////////////////////////////////////////////////////////
// RT fan-out job bodies (run on AudioJobPool workers and/or the audio thread)
///////////////////////////////////////////////////////////////////////////////

static void _jobMixBusLayers(void* vctx) {
  auto ctx = (synth::BusJobContext*)vctx;
  auto s   = ctx->_synth;
  for (auto l : ctx->_bus->_exec_layers) {
    l->mixToBus(s->_dspwritebase, s->_dspwritecount);
  }
}

static void _jobComputeBusEffects(void* vctx) {
  auto ctx         = (synth::BusJobContext*)vctx;
  auto s           = ctx->_synth;
  auto bus         = ctx->_bus;
  int inumframes   = ctx->_inumframes;
  auto& bus_buf    = bus->_buffer;
  float* bus_left  = bus_buf._leftBuffer;
  float* bus_right = bus_buf._rightBuffer;
  //////////////////////////////////////////
  // Insert effects chain (runs BEFORE _dsplayer)
  // Signal flow: Voices → _insertGroups → _dsplayer → Output
  //////////////////////////////////////////
  bus->computeInserts(inumframes, s->_dspwritebase, s->_dspwritecount);
  //////////////////////////////////////////
  // bus DSP fx (main bus effect via setEffect)
  //////////////////////////////////////////
  auto busdsplayer = bus->_dsplayer;
  if (busdsplayer) {
    auto dsp_buf = busdsplayer->_dspbuffer;
    dsp_buf->resize(inumframes);
    float* dsp_left  = dsp_buf->channel(0);
    float* dsp_right = dsp_buf->channel(1);
    //////////////////////////////////////////
    // bus -> dsp buf input
    //////////////////////////////////////////
    for (int i = 0; i < s->_dspwritecount; i++) {
      int j        = s->_dspwritebase + i;
      dsp_left[i]  = bus_left[j];
      dsp_right[i] = bus_right[j];
    }
    //////////////////////////////////////////
    // compute dsp -> tempbus
    //////////////////////////////////////////
    busdsplayer->_outbus = nullptr;
    busdsplayer->beginCompute(s->_dspwritecount);
    busdsplayer->updateControllers();
    busdsplayer->compute(0, s->_dspwritecount);
    busdsplayer->endCompute();
    //////////////////////////////////////////
    // tempbus -> bus out
    //////////////////////////////////////////
    const float* fxlyroutl = busdsplayer->_dspbuffer->channel(0);
    const float* fxlyroutr = busdsplayer->_dspbuffer->channel(1);
    for (int i = 0; i < s->_dspwritecount; i++) {
      int j        = s->_dspwritebase + i;
      bus_left[j]  = fxlyroutl[i];
      bus_right[j] = fxlyroutr[i];
    }
    //////////////////////////////////////////
  }
}

///////////////////////////////////////////////////////////////////////////////

bool synth::mainThreadHandler() {
  /////////////////////////////////
  // execute external audio thread handlers
  /////////////////////////////////

  for (auto h : _audiothreadhandlers) {
    h->_handler(this);
  }

  /////////////////////////////////
  // drain sequencer event callbacks (posted from audio thread)
  /////////////////////////////////

  int num_drained = _sequencer->drainMainThreadEventCallbacks();

  /////////////////////////////////

  num_drained += _hudEventRouter->processEvents();

  /////////////////////////////////
  // service delay lines freed on the audio thread (clear + return to pool)
  /////////////////////////////////

  num_drained += int(drainDelayDisposal());

  /////////////////////////////////
  // in critical section,
  //  separate _CCIVALS into items to execute and items to remove
  /////////////////////////////////

  _kmod_exec_list.clear();
  _kmod_rem_list.clear();

  _CCIVALS.atomicOp([&](keyonmodvect_t& unlocked) {
    size_t index = 0;
    for (auto kmod : unlocked) {
      if (kmod->_dangling) {
        _kmod_rem_list.push_back(index);
      } else {
        _kmod_exec_list.push_back(kmod);
      }
      index++;
    }
    _remove_items(unlocked, _kmod_rem_list); // remove dangling items
  });

  /////////////////////////////////
  // execute remaining items
  /////////////////////////////////

  for (auto kmod : _kmod_exec_list) {
    for (auto item : kmod->_mods) {
      auto kmdata = item.second;
      if (kmdata->_generator) {
        kmdata->_currentValue = kmdata->_currentValue * 0.95 + kmdata->_generator() * 0.05;
      }
      if (kmdata->_subscriber) {
        kmdata->_subscriber(kmdata->_name, kmdata->_currentValue);
        kmdata->_evstrings.atomicOp([kmdata](std::vector<std::string>& unlocked) {
          for (auto item : unlocked) {
            kmdata->_subscriber(kmdata->_name, item);
          }
          unlocked.clear();
        });
      }
    }
  }

  return (num_drained > 0);
}

programInst* synth::keyOn(int note, int velocity, prgdata_constptr_t pdata, keyonmod_ptr_t kmods) {

  float fv = float(velocity) / 127.0f;
  fv       = powf(fv, _velcurvepower);
  velocity = int(fv * 127.0f);

  assert(pdata);
  programInst* pi = nullptr;

  _freeProgInst.atomicOp([&pi](proginstset_t& piset) { pi = piset.takeAny(); });
  OrkAssertIFMT(
      pi != nullptr, //
      "singularity programInst pool exhausted (%d instances) - unbalanced keyOn/keyOff ?",
      kmaxlayerspersynth);
  pi->_progdata = pdata;
  //printf("syn KEYON<%d>\n", note);

  int clampn = std::clamp(note, 0, 127);
  int clampv = std::clamp(velocity, 0, 127);

  pi->keyOn(clampn, clampv, pdata, kmods);

  _activeProgInst.atomicOp([pi](proginstset_t& piset) { //
    piset.insert(pi);
  });

  _lnoteframe   = 0;
  _lnotetime    = 0.0f;
  _clearhuddata = true;

  for (auto h : _onkey_subscribers) {
    h(clampn, clampv, pi);
  }

  return pi;
}

///////////////////////////////////////////////////////////////////////////////

void synth::keyOff(programInst* pinst) {
  //printf("syn keyOff pinst<%p>\n", pinst);
  pinst->keyOff();
  _activeProgInst.atomicOp([pinst](proginstset_t& piset) { //
    assert(piset.contains(pinst));
    piset.erase(pinst);
  });
  _freeProgInst.atomicOp([pinst](proginstset_t& piset) { //
    piset.insert(pinst);
  });
}

///////////////////////////////////////////////////////////////////////////////

void synth::resize(int numframes) {
  if (numframes > _numFrames) {
    if(0)logchan_synth->log("RESIZE NUMFRAMES<%d>", numframes);
    _tempbus->resize(numframes);
    _ibuf.resize(numframes);
    _obuf.resize(numframes);
    for (auto lay : _allVoices) {
      lay->resize(numframes);
    }
    for (auto bus : _outputBusses) {
      bus.second->resize(numframes);
    }
  }
  _numFrames = numframes;
}

///////////////////////////////////////////////////////////////////////////////

std::string synth::statusString() const {
  std::string rval;
  rval += FormatString("synth<%p> ", this);
  rval += FormatString( "samplerate<%g> ", _sampleRate);
  rval += FormatString( "numframes<%d> ", _numFrames);
  rval += FormatString( "numbusses<%zu> ", _outputBusses.size());
  rval += FormatString( "numvoices<%zu> ", _allVoices.size());
  rval += FormatString( "mastergain<%g> ", _masterGain);
  rval += FormatString( "activevoices<%zu> ", _numActiveVoices );
  rval += FormatString( "activedspblocks<%zu> ", _numActiveDspBlocks );
  rval += FormatString( "activedspstages<%zu> ", _numActiveDspStages );
  rval += FormatString( "cpuload<%g> ", _cpuload);
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

void synth::compute(int inumframes, const void* inputBuffer) {

  if (_lifecycle_state.load() >= 2)
    return; // shutting down — do not access any containers

  resize(inumframes);
  spikeDiagMark(); // mark 1: resize

  auto master_left  = _obuf._leftBuffer;
  auto master_right = _obuf._rightBuffer;
  auto input        = (const float*)inputBuffer;
  auto input_left   = _ibuf._leftBuffer;

  /////////////////////////////
  // test tone ?
  /////////////////////////////
  if (0) {
    double frq = 120.0f;
    // printf("GNOTE<%d> frq<%g>\n", GNOTE, frq);
    static const float kinvsr = getInverseSampleRate();
    for (int i = 0; i < inumframes; i++) {
      double phase = frq * pi2 * double(_testtoneph) * kinvsr;
      float samp   = sinf(phase)*0.33;
      // printf("i<%d> samp<%g>\n", i, samp);
      master_left[i]  = samp;
      master_right[i] = samp;
      _testtoneph++;
    }
    //constexpr int k_samples_per_tick = 128;
    //float elapsed_this_tick          = float(k_samples_per_tick) * getInverseSampleRate();
    //auto& eventmap                   = _eventmap.LockForWrite();
    //this->_tick(eventmap, elapsed_this_tick);
    //_eventmap.UnLock();
  }
  /////////////////////////////
  // real output ?
  /////////////////////////////
  else {

    ////////////////////////////
    OrkProfilerFrameBegin(CHANNEL_AUDIO, CpuProfilerChannel, {.capture_fps = true});

    /////////////////////////////
    // clear output buffer
    /////////////////////////////

    /////////////////////////////
    // route to synth input
    /////////////////////////////

    if (input){
      for (int i = 0; i < inumframes; i++) {
        float j = input[i];
        input_left[i] = j;
      }
    }

    /////////////////////////////
    // clear output busses
    /////////////////////////////

    for (auto busitem : _outputBusses) {
      auto bus         = busitem.second;
      auto& obuf       = bus->_buffer;
      float* bus_left  = obuf._leftBuffer;
      float* bus_right = obuf._rightBuffer;
      for (int i = 0; i < inumframes; i++) {
        bus_left[i]  = 0.0f;
        bus_right[i] = 0.0f;
      }
    }

    /////////////////////////////
    // compute/accumulate layer instances
    //  (into output busses)
    /////////////////////////////
    constexpr int k_samples_per_tick = 128;
    _numActiveVoices = _activeVoices.size();
    spikeDiagMark(); // mark 2: input route + bus clear
    //////////////////////////////////
    {
      KeyOnProfScope profbc(KOP_BEGINCOMPUTE);
      for (auto l : _activeVoices)
        l->beginCompute(inumframes);
    }
    spikeDiagMark(); // mark 3: beginCompute
    //////////////////////////////////
    int ifrpending = inumframes;
    _dspwritecount = frames_per_controlpass;
    _dspwritebase  = 0;
    //////////////////////////////////
    while (ifrpending > 0) {
      // every buffer in flight here (_obuf, the bus buffers, the layer dsp
      //  buffers) is sized to exactly inumframes, so the tail pass of a chunk
      //  whose size is not a multiple of frames_per_controlpass MUST be
      //  clamped - a full-width tail pass writes past the end of all of them.
      //  no-op for the multiple-of-32 chunk sizes the devices deliver; for a
      //  pathological chunk the tail simply runs the control rate a little
      //  early, which every consumer below tolerates because they are all
      //  driven by _dspwritebase/_dspwritecount rather than by the constant.
      _dspwritecount = std::min(frames_per_controlpass, ifrpending);
      // printf("_dspwritecount<%d> _dspwritebase<%d>\n", _dspwritecount, _dspwritebase);
      ////////////////////////////////
      // update controllers
      ////////////////////////////////
      { 
        KeyOnProfScope profpv(KOP_PASSVOICES);
        OrkProfilerSampleScope(CHANNEL_AUDIO, "voices");
        {
          KeyOnProfScope profc(KOP_PVCTRL);
          for (auto l : _activeVoices)
            l->updateControllers();
        }
        ////////////////////////////////
        // update dsp modules
        ////////////////////////////////
        {
          KeyOnProfScope profd(KOP_PVDSP);
          for (auto l : _activeVoices) {
            KeyOnProfScope profone(KOP_PVONE, l->_layerdata ? l->_layerdata->_name.c_str() : "?");
            l->compute(_dspwritebase, _dspwritecount);
          }
        }
        /////////////////////////////
        // clear synth main output mix buffer
        /////////////////////////////
        for (int i = 0; i < _dspwritecount; i++) {
          int j           = _dspwritebase + i;
          master_left[j]  = 0.0f;
          master_right[j] = 0.0f;
        }
        /////////////////////////////
        // accumulate layers into busses
        /////////////////////////////
        if (false) { // serial
          for (auto l : _activeVoices) {
            l->mixToBus(_dspwritebase, _dspwritecount);
            l->updateScopes(_dspwritebase, _dspwritecount);
          }
        } else { // parallel
          //////
          for (auto bitem : _outputBusses) {
            auto bus = bitem.second;
            bus->_exec_layers.clear();
          }
          //////
          for (auto l : _activeVoices) {
            auto bus = l->_outbus;
            bus->_exec_layers.push_back(l);
          }
          //////
          // fork/join across the audio job pool (RT-safe: preallocated
          // slots, no allocation/locks — see AudioJobPool)
          int njobs = 0;
          for (auto bitem : _outputBusses) {
            OrkAssert(njobs < kMaxAudioJobs);
            auto& ctx   = _busJobContexts[njobs];
            ctx._synth  = this;
            ctx._bus    = bitem.second.get();
            _busJobs[njobs] = {_jobMixBusLayers, &ctx};
            njobs++;
          }
          {
            KeyOnProfScope profbm(KOP_PASSBUSMIX);
            _audioJobPool.kickAndJoin(_busJobs, njobs);
          }
          //////
          {
            KeyOnProfScope profs(KOP_PVSCOPES);
            for (auto l : _activeVoices) {
              l->updateScopes(_dspwritebase, _dspwritecount);
            }
          }
          //////
        }
        /////////////////////////////
        // per-voice sends into their named send busses, and the per-voice
        //  ambisonic encode into the field. serial and AFTER the per-bus mix
        //  join: a send writes a bus other than the voice's own, which no
        //  per-bus job may do (see Layer::mixToSendBus). still ahead of the
        //  effects pass below, so a send bus's fx sees the sends.
        /////////////////////////////
        {
          KeyOnProfScope profsb(KOP_PVSENDS);
          for (auto l : _activeVoices) {
            l->mixToSendBus(_dspwritebase, _dspwritecount);
            l->encodeToSoundField(_dspwritebase, _dspwritecount);
          }
        }
        /////////////////////////////
        // the ambisonic probe field, rotated + decoded into its own bus.
        //  ahead of the effects pass so the "soundfield" bus's fx and gain
        //  apply to it exactly like any other bus. null (one atomic load) in
        //  every scene that never asked for a field.
        /////////////////////////////
        if (auto sfield = SoundField::rtInstance()) {
          sfield->computeIntoBus(_dspwritebase, _dspwritecount);
        }
      } // voices
      spikeDiagMark(); // per pass mark A: voices
      {
        OrkProfilerSampleScope(CHANNEL_AUDIO, "events");
        /////////////////////////////
        // synth update tick (events)
        /////////////////////////////
        // the pass width, not the constant: a clamped tail pass must not
        //  advance the event clock by more samples than it produced.
        _samplesuntilnexttick -= _dspwritecount;
        if (_samplesuntilnexttick < 0) {
          float elapsed_this_tick = float(k_samples_per_tick) * getInverseSampleRate();
          _lnoteframe++;
          _lnotetime += elapsed_this_tick;
          auto& eventmap = _eventmap.LockForWrite();
          this->_tick(eventmap, elapsed_this_tick);
          _eventmap.UnLock();
          _samplesuntilnexttick += k_samples_per_tick;
          ////////////////////////////////////////////
          // process sequencer from audio thread
          //  so MIDI timing is sample-accurate
          //  and independent of render frame rate
          ////////////////////////////////////////////
          _sequencer->process();
          ////////////////////////////////////////////
          activateVoices(ifrpending);
          deactivateVoices();
        }
      } // events
      spikeDiagMark(); // per pass mark B: events (tick/sequencer/voice churn)
      {
        KeyOnProfScope proffx(KOP_PASSFX);
        OrkProfilerSampleScope(CHANNEL_AUDIO, "effects");
        /////////////////////////////
        // compute/accumulate output busses
        //  (into main output)
        /////////////////////////////
        // fork/join across the audio job pool (RT-safe: preallocated
        // slots, no allocation/locks — see AudioJobPool). the joining
        // audio thread helps drain, which subsumes the old is_last_bus
        // run-inline special case.
        int njobs = 0;
        for (auto busitem : _outputBusses) {
          OrkAssert(njobs < kMaxAudioJobs);
          auto& ctx       = _busJobContexts[njobs];
          ctx._synth      = this;
          ctx._bus        = busitem.second.get();
          ctx._inumframes = inumframes;
          _busJobs[njobs] = {_jobComputeBusEffects, &ctx};
          njobs++;
        }
        _audioJobPool.kickAndJoin(_busJobs, njobs);
      } // effects
      spikeDiagMark(); // per pass mark C: bus effects
      {
        KeyOnProfScope profmx(KOP_PASSMASTER);
        OrkProfilerSampleScope(CHANNEL_AUDIO, "mixing");
        //////////////////////////////////////////
        // accumulate busses to master
        //////////////////////////////////////////
        bool any_soloed = _num_soloed.load() > 0;
        for (auto busitem : _outputBusses) {
          auto bus         = busitem.second;
          //////////////////////////////////////////
          // mute/solo logic
          //////////////////////////////////////////
          if (any_soloed && !bus->_solo) {
            continue;  // skip non-soloed buses when solo is active
          }
          if (!any_soloed && bus->_mute) {
            continue;  // skip muted buses when no solo active
          }
          //////////////////////////////////////////
          auto& bus_buf    = bus->_buffer;
          float* bus_left  = bus_buf._leftBuffer;
          float* bus_right = bus_buf._rightBuffer;
          //////////////////////////////////////////
          // accumulate bus to master
          //////////////////////////////////////////
          for (int i = 0; i < _dspwritecount; i++) {
            int j   = _dspwritebase + i;
            float L = bus_left[j];
            float R = bus_right[j];
            master_left[j] += L;
            master_right[j] += R;
          }
          //////////////////////////////////////
          // SignalScope
          //////////////////////////////////////
          if (bus->_scopesource) {
            bus->_scopesource->updateStereo(
                _dspwritecount, //
                bus_left + _dspwritebase,
                bus_right + _dspwritebase,
                true);
          }
        }
        ////////////////////////////////
        // master bus DSP
        ////////////////////////////////
        if(_enableMasterEq){
          for (int i = 0; i < _dspwritecount; i++) {
            int j   = _dspwritebase + i;
            float L = master_left[j];
            float R = master_right[j];
            for( int ifilt=0; ifilt<1; ifilt++ ){
              L = _peqL[ifilt].compute(L);
              R = _peqR[ifilt].compute(R);
            }
            master_left[j] = L;
            master_right[j] = R;
          }
        }
      } // mixing
      spikeDiagMark(); // per pass mark D: master accumulate
      ////////////////////////////////
      // update indices
      ////////////////////////////////
      _dspwritebase += _dspwritecount;
      ifrpending -= _dspwritecount;
      /////////////////////////////
    }
    //////////////////////////////////
    {
      KeyOnProfScope profec(KOP_ENDCOMPUTE);
      for (auto l : _activeVoices)
        l->endCompute();
    }
    spikeDiagMark(); // mark N-2: endCompute
    //////////////////////////////////
    OrkProfilerFrameEnd(CHANNEL_AUDIO);
    if (_onprofilerframe) {
      SynthProfilerFrame frame;
      frame._samplerate       = getSampleRate();
      frame._controlrate      = controlRate();
      frame._cpuload          = _cpuload;
      frame._numlayers        = _activeVoices.size();

      int numdspblocks = 0;
      int numdspstages = 0;
      for (auto v : _activeVoices) {
        auto ld = v->_layerdata;
        numdspblocks += ld->numDspBlocks();
        numdspstages += ld->numDspStages();
      }

      frame._numdspblocks = numdspblocks;
      _numActiveDspBlocks = numdspblocks;
      _numActiveDspStages = numdspstages;
      _onprofilerframe(frame);
    }
  }
  /////////////////////////////
  // final clamping
  /////////////////////////////
  float clamp = 2.0f;
  for (int i = 0; i < inumframes; i++) {
    // float L = clip_float(master_left[i] * _masterGain, -clamp, clamp);
    // float R = clip_float(master_right[i] * _masterGain, -clamp, clamp);
    float L = master_left[i] * _masterGain;
    float R = master_right[i] * _masterGain;
    if (isnan(L) or isinf(L)) {
      L = 0.0f;
    }
    if (isnan(R) or isinf(R)) {
      R = 0.0f;
    }
    master_left[i]  = L;
    master_right[i] = R;
  }
  /////////////////////////////
}

///////////////////////////////////////////////////////////////////////////////

void synth::resetFenables() {
  for (int i = 0; i < 5; i++)
    _stageEnable[i] = true;
}

///////////////////////////////////////////////////////////////////////////////

programInst::programInst()
    : _progdata(nullptr) {
}

///////////////////////////////////////////////////////////////////////////////

programInst::~programInst() {
}

///////////////////////////////////////////////////////////////////////////////

void synth::_keyOnLayer(layer_ptr_t l, int note, int velocity, lyrdata_ptr_t ld, keyonmod_ptr_t kmod) {

  std::lock_guard<std::mutex> lock(l->_mutex);

  assert(ld != nullptr);

  l->_koi._layer     = l;
  l->_koi._key       = note;
  l->_koi._vel       = velocity;
  l->_koi._layerdata = ld;

  outbus_ptr_t obus = _curprogrambus;

  if (ld->_outbus.size()) {
    obus = outputBus(ld->_outbus);
  }
  if (kmod and kmod->_outbus_override) {
    obus = kmod->_outbus_override;
  }

  l->keyOn(note, velocity, ld, obus);

  // per-voice send: resolved HERE (keyOn cleared it) because the bus map is
  //  ours, not the audio thread's — mixToSendBus must never do a lookup.
  //  a named-but-absent send bus is a data error, not a silent dry voice.
  if (ld->_sendbus.length()) {
    auto sendbus = outputBus(ld->_sendbus);
    OrkAssertIFMT(
        sendbus != nullptr, //
        "layer<%s> sends to bus<%s> which does not exist",
        ld->_name.c_str(),
        ld->_sendbus.c_str());
    l->_sendbus   = sendbus;
    l->_sendLevel = ld->_sendLevel;
  }

  // the soundfield send, resolved the same way and for the same reason: the
  //  encode pass may not look anything up. an authored send with no field is a
  //  data error, not a silent dry voice - SoundField::instance() is NOT called
  //  here (it creates a bus and spawns the feeder thread), so the scene must
  //  have declared a SoundFieldSystemData exactly as a probe scene must.
  const auto& sfsend = ld->_soundfieldSend;
  if (sfsend._enabled) {
    // layers are frequently unnamed, so name the program too - otherwise the
    //  loudest thing about this failure would be a "layer<>".
    const char* pname = ld->_programdata ? ld->_programdata->_name.c_str() : "?";
    auto sfield       = SoundField::rtInstance();
    OrkAssertIFMT(
        sfield != nullptr, //
        "program<%s> layer<%s> authors a soundfieldSend but no SoundField exists - the scene must declare a "
        "SoundFieldSystemData",
        pname,
        ld->_name.c_str());
    // the alg instance was built by l->keyOn above, and its grid mirrors the
    //  data's stage/block indices 1:1 (Alg::keyOn), so the authored structural
    //  coordinates land on THIS voice's own panner param.
    OrkAssertIFMT(
        l->_alg != nullptr and sfsend._angleStage >= 0 and sfsend._angleStage < kmaxdspstagesperlayer, //
        "layer<%s> soundfieldSend has no resolved panner stage",
        ld->_name.c_str());
    auto stage = l->_alg->_stageblock._stages[sfsend._angleStage];
    OrkAssertIFMT(
        stage != nullptr and sfsend._angleBlock >= 0 and sfsend._angleBlock < stage->_numblocks, //
        "layer<%s> soundfieldSend has no resolved panner block",
        ld->_name.c_str());
    auto blk = stage->_blocks[sfsend._angleBlock];
    OrkAssertIFMT(
        blk != nullptr and sfsend._angleParam >= 0 and sfsend._angleParam < blk->_numParams, //
        "layer<%s> soundfieldSend has no resolved panner ANGLE param",
        ld->_name.c_str());
    l->_sfield       = sfield;
    l->_sfAngleParam = &blk->_param[sfsend._angleParam];
    l->_sfLevelLin   = decibel_to_linear_amp_ratio(sfsend._levelDB);
    l->_sfSpread     = std::clamp(sfsend._spread, 0.0f, 1.0f);
    l->_sfPrimed     = false;
  }
}

///////////////////////////////////////////////////////////////////////////////

void synth::_keyOffLayer(layer_ptr_t l) {
  l->_lyrPhase = 1;
  this->releaseLayer(l);
  l->keyOff();
}

///////////////////////////////////////////////////////////////////////////////

void programInst::keyOn(int note, int velocity, prgdata_constptr_t pd, keyonmod_ptr_t kmod) {

  _note     = note;
  _velocity = velocity;

  _keymods = kmod;

  auto syn     = synth::instance();
  auto prgchan = syn->_prgchannel;

  size_t layer_mask = 0xffffffff;
  if (kmod) {
    layer_mask = kmod->_layermask;
  }
  // printf( "layer_mask<0x%08x>\n", layer_mask);
  size_t ilayer         = 0;
  size_t num_layerdatas = pd->_layerdatas.size();

  if (_progdata->_monophonic) {
    std::lock_guard<std::mutex> lock(prgchan->_mutex);
    prgchan->_monoprogs.insert(this);
  }

  for (size_t ilayer = 0; ilayer < num_layerdatas; ilayer++) {
    auto ld = pd->_layerdatas[ilayer];

    if ((layer_mask & (1 << ilayer)) == 0) {
      continue;
    }
    if (syn->_soloLayer >= 0) {
      if (syn->_soloLayer != ilayer)
        continue;
    }

    if (note < ld->_loKey || note > ld->_hiKey)
      continue;

    // printf( "lovel<%d>\n", ld->_loVel );
    // printf( "hivel<%d>\n", ld->_hiVel );

    if (velocity < ld->_loVel || velocity > ld->_hiVel)
      continue;

    auto l = syn->allocLayer();
    assert(l != nullptr);
    assert(ld != nullptr);

    l->_ldindex     = ilayer - 1;
    l->_keymods     = _keymods;
    l->_name        = ld->_name;
    l->_programinst = this;

    syn->_keyOnLayer(l, note, velocity, ld, kmod);

    _layers.push_back(l);

  } // for (size_t ilayer = 0; ilayer < num_layerdatas; ilayer++) {

  int inuml = _layers.size();
  int solol = syn->_soloLayer;

  if (solol >= 0 and solol < inuml) {
    syn->_hudLayer = _layers[solol];
  } else if (inuml > 0) {
    syn->_hudLayer = _layers[0];
  } else {
    syn->_hudLayer = nullptr;
  }
  // printf("KEYON L%d\n", ilayer);

  // if (syn->_hudLayer)
  // syn->_hudbuf.push(syn->_hudLayer->_HKF);
}

///////////////////////////////////////////////////////////////////////////////

void programInst::keyOff() {
  for (auto l : _layers)
    synth::instance()->_keyOffLayer(l);
  _layers.clear();
}

///////////////////////////////////////////////////////////////////////////////

outputBuffer::outputBuffer()
    : _leftBuffer(nullptr)
    , _rightBuffer(nullptr)
    , _maxframes(0)
    , _numframes(0) {
}

///////////////////////////////////////////////////////////////////////////////

void outputBuffer::resize(int inumframes) {
  if (inumframes > _maxframes) {
    if (_leftBuffer)
      delete[] _leftBuffer;
    if (_rightBuffer)
      delete[] _rightBuffer;
    _leftBuffer  = new float[inumframes];
    _rightBuffer = new float[inumframes];
    _maxframes   = inumframes;
    // the block size only settles once the device is open, so these arrive
    //  after the pool bind pass and have to be caught here or stay in the
    //  balancer's scan set (see lev2/aud/audio_numa.h).
    lev2::audioNumaBindRange(_leftBuffer, inumframes * sizeof(float));
    lev2::audioNumaBindRange(_rightBuffer, inumframes * sizeof(float));
  }
  _numframes = inumframes;
}

///////////////////////////////////////////////////////////////////////////////
void synth::registerSinkForHudEvent(uint32_t evID, hudeventsink_ptr_t sink) {
  _hudEventRouter->registerSinkForHudEvent(evID, sink);
}
///////////////////////////////////////////////////////////////////////////////
void synth::enqueueHudEvent(hudevent_ptr_t hev) {
  _hudEventRouter->_hudevents.push(hev);
  // wake the (possibly idle-waiting) main-thread pump so posted events are
  // drained immediately rather than on its idle-backstop timeout. RT variant:
  // this runs on the audio thread — must never take a lock.
  opq::mainSerialQueue()->mSemaphore.notify_rt();
}
///////////////////////////////////////////////////////////////////////////////

void synth::panic() {
  // flag only - the deactivateVoices sweep that follows this tick does the
  //  endCompute/alg-return/pool-return work for every flagged voice.
  addEvent(0.0f, [=]() {
    for (auto& l : _activeVoices) {
      l->_wantsDeactivate = true;
    }
  });
}

void synth::disableMasterEq(){
  _enableMasterEq = false;
}
void synth::enableMasterEq(){
  _enableMasterEq = true;
}
void synth::setMasterEqBand(int band, float frqHZ, float widthHZ, float gainDB){
  OrkAssert(band>=0);
  OrkAssert(band<8);
  _peqL[band].Clear();
  _peqR[band].Clear();
  _peqL[band].set2(frqHZ, widthHZ, gainDB);
  _peqR[band].set2(frqHZ, widthHZ, gainDB);

}

} // namespace ork::audio::singularity
