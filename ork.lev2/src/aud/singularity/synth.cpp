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

#include <ork/lev2/aud/singularity/synthdata.h>
#include <ork/lev2/aud/singularity/synth.h>
#include <ork/lev2/aud/singularity/hud.h>
#include <ork/lev2/aud/singularity/krzobjects.h>
#include <ork/lev2/aud/singularity/dspblocks.h>
#include <ork/lev2/aud/singularity/fxgen.h>
#include <ork/util/logger.h>

namespace ork::audio::singularity {
static logchannel_ptr_t logchan_synth = logger()->createChannel("singul.syn", fvec3(1, 0.6, .8), true);
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
      logchan_synth->log("switched to effect<%s>", bus->_fxname.c_str());
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
      logchan_synth->log("switched to effect<%s>", bus->_fxname.c_str());
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
    _eventmap.atomicOp([=](eventmap_t& unlocked) { //
      float timestamp         = 0.0f;              // now
      auto deferred_operation = [=]() {
        assert(nextpreset->_algdata != nullptr); // did you add presets ?
        bus->setBusDSP(nextpreset);
        bus->_fxname      = name;
        bus->_fxcurpreset = it;
        logchan_synth->log("switched to effect<%s>", name.c_str());
      };
      unlocked.insert(std::make_pair(timestamp, deferred_operation));
    });
  }
}

///////////////////////////////////////////////////////////////////////////////
synth_ptr_t synth::_instance;
void synth::bringUp() {
  _instance = std::make_shared<synth>();
}
void synth::tearDown() {
  _instance = nullptr;
}
synth_ptr_t synth::instance() {
  return _instance;
}
///////////////////////////////////////////////////////////////////////////////
outbus_ptr_t synth::createOutputBus(std::string named) {
  auto bus             = std::make_shared<OutputBus>();
  bus->_name           = named;
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
delaycontext_ptr_t synth::allocDelayLine(){
  delaycontext_ptr_t rval;
  _delayspool.atomicOp([&](delaydequeue_t& unlocked){
    rval = unlocked.back();
    unlocked.pop_back();
  });
  return rval;
}
void synth::freeDelayLine(delaycontext_ptr_t delay){
  auto op = [this,delay](){
    _delayspool.atomicOp([&](delaydequeue_t& unlocked){
      unlocked.push_front(delay);
    });
  };

}
///////////////////////////////////////////////////////////////////////////////
synth::synth()
    : _timeaccum(0.0f)
    , _sampleRate(0.0f)
    , _dt(0.0f)
    , _soloLayer(-1)
    , _hudpage(0)
    , _masterGain(1.0f) { //

  for( int i=0; i<256; i++){
    _delayspool.atomicOp([&](delaydequeue_t& unlocked){
      auto delay = std::make_shared<DelayContext>();
      delay->clear();
      unlocked.push_back(delay);
    });
  }

  _sequencer  = std::make_shared<Sequencer>(this);
  _prgchannel = std::make_shared<ProgramChannel>();

  _tempbus              = std::make_shared<OutputBus>();
  _tempbus->_name       = "temp-dsp";
  _numactivevoices      = 0;
  auto mainbus          = createOutputBus("main");
  _curprogrambus        = mainbus;
  mainbus->_fxcurpreset = _fxpresets.rbegin().base();

  // TODO - synth::instance(); is creating chicken and egg problems
  loadAllFxPresets(this);

  setEffect(mainbus, "none");

  for (int i = 0; i < kmaxlayerspersynth; i++) {
    auto l = std::make_shared<Layer>();
    _allVoices.insert(l);
    _freeVoices.insert(l);
  }

  for (int i = 0; i < kmaxlayerspersynth; i++) {
    auto pi = new programInst();
    _freeProgInst.atomicOp([&pi](proginstset_t& piset) { piset.insert(pi); });
    _allProgInsts.insert(pi);
  }

  resize(1);

  _lock_compute = false;
}

void synth::setSampleRate(float sr) {
  _sampleRate = sr;
  _dt         = 1.0f / sr;
}

///////////////////////////////////////////////////////////////////////////////

synth::~synth() {

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

layer_ptr_t synth::allocLayer() {
  auto it = _freeVoices.begin();
  assert(it != _freeVoices.end());
  auto l = *it;
  // printf( "syn alloclayer<%p>\n", l );
  _freeVoices.erase(it);
  it = _activeVoices.find(l);
  assert(it == _activeVoices.end());
  _pendactVoices.insert(l);
  return l;
}

///////////////////////////////////////////////////////////////////////////////

void synth::releaseLayer(layer_ptr_t l) {
  if (l->_keepalive <= 0) {
    return;
  } else if ((--l->_keepalive) == 0) {
    // printf("LAYER<%p> DONE\n", this);
    _deactiveateVoiceQ.push(l);
  }
  assert(l->_keepalive >= 0);
  // printf( "layer<%p> release cnt<%d>\n", this, _keepalive );
}

///////////////////////////////////////////////////////////////////////////////

void synth::deactivateVoices() {
  bool done = (_deactiveateVoiceQ.size() == 0);

  while (false == done) {
    auto l = _deactiveateVoiceQ.front();
    _deactiveateVoiceQ.pop();

    if (l == _hudLayer) {
      _hudLayer = nullptr;
    }

    auto it = _activeVoices.find(l);
    assert(it != _activeVoices.end());
    _activeVoices.erase(it);

    int inumv = _activeVoices.size();

    // printf("syn freeLayer<%p> curnumvoices<%d>\n", l, inumv);

    l->endCompute();
    if (l->_keymods != nullptr) {
      l->_keymods->_dangling = true;
    }
    it = _freeVoices.find(l);
    assert(it == _freeVoices.end());
    _freeVoices.insert(l);
    if (l->_alg) {
      l->_alg->_algdata.returnAlgInst(l->_alg);
      l->_alg = nullptr;
    }

    /////////////////////////////////////

    done = (_deactiveateVoiceQ.size() == 0);
  }
  _numactivevoices = _activeVoices.size();
}

///////////////////////////////////////////////////////////////////////////////

void synth::activateVoices(int ifrpending) {
  for (auto v : _pendactVoices) {
    v->beginCompute(ifrpending - frames_per_controlpass);
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

  bool needs_new_trigger = true;

  if (pdata->_monophonic) {
    _prgchannel->_monokeycount++;
    _prgchannel->_mononotes.push_back(note);
    for (auto monopi : _prgchannel->_monoprogs) {
      if (monopi->_progdata == pdata) {
        pi                = monopi;
        needs_new_trigger = false;
        addEvent(0.0f, [note, velocity, monopi]() {
          for (auto l : monopi->_layers) {
            l->reTriggerMono(note, velocity);
          }
        });
      }
    }
  }

  if (needs_new_trigger) {
    _freeProgInst.atomicOp([&pi](proginstset_t& piset) {
      auto it = piset.begin();
      assert(it != piset.end());
      pi = *it;
      piset.erase(it);
    });
    pi->_progdata = pdata;
    if (pdata->_monophonic) {
      _prgchannel->_monokeycount = 1;
      _prgchannel->_mononotes.clear();
      _prgchannel->_mononotes.push_back(note);
    }
    addEvent(0.0f, [note, velocity, pdata, this, pi, kmods]() {
      logchan_synth->log("liveKeyOn note<%d>", note);

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
  auto pdata      = pinst->_progdata;
  bool do_key_off = true;
  if (pdata->_monophonic) {
    _prgchannel->_monokeycount--;
    do_key_off = (_prgchannel->_monokeycount == 0);
    if (not do_key_off) {
      int count = _prgchannel->_mononotes.size();
      for(int i=count-1; i>=0; i-- ){
        if( _prgchannel->_mononotes[i] == note ){
          auto it = _prgchannel->_mononotes.begin()+i;
          _prgchannel->_mononotes.erase(it);
          count = _prgchannel->_mononotes.size();
          if(count){
            int prev = _prgchannel->_mononotes[count-1];
            addEvent(0.0f, [prev, pinst]() {
              for (auto l : pinst->_layers) {
                l->reTriggerMono(prev, 0);
              }
            });
          }
        }
      }
    }
  }

  if (do_key_off) {
    auto it = _prgchannel->_monoprogs.find(pinst);
    if (it != _prgchannel->_monoprogs.end()) {
      _prgchannel->_monoprogs.erase(it);
    }
    addEvent(0.0f, [pinst, this]() {
      pinst->keyOff();
      _activeProgInst.atomicOp([pinst](proginstset_t& piset) { //
        auto it = piset.find(pinst);
        assert(it != piset.end());
        piset.erase(it);
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

void synth::mainThreadHandler() {

  /////////////////////////////////
  // execute external audio thread handlers
  /////////////////////////////////

  for (auto h : _audiothreadhandlers) {
    h->_handler(this);
  }

  /////////////////////////////////
  // process sequencer
  /////////////////////////////////

  _sequencer->process();

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
}

programInst* synth::keyOn(int note, int velocity, prgdata_constptr_t pdata, keyonmod_ptr_t kmods) {
  assert(pdata);
  programInst* pi = nullptr;

  _freeProgInst.atomicOp([&pi](proginstset_t& piset) {
    auto it = piset.begin();
    assert(it != piset.end());
    pi = *it;
    piset.erase(it);
  });
  pi->_progdata = pdata;
  // printf("syn KEYON<%d>\n", note);

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
  pinst->keyOff();
  _activeProgInst.atomicOp([pinst](proginstset_t& piset) { //
    auto it = piset.find(pinst);
    assert(it != piset.end());
    piset.erase(it);
  });
  _freeProgInst.atomicOp([pinst](proginstset_t& piset) { //
    piset.insert(pinst);
  });
}

///////////////////////////////////////////////////////////////////////////////

void synth::resize(int numframes) {
  if (numframes > _numFrames) {
    logchan_synth->log("RESIZE NUMFRAMES<%d>", numframes);
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

void synth::compute(int inumframes, const void* inputBuffer) {

  // if (_lock_compute)
  // return;

  resize(inumframes);

  auto master_left  = _obuf._leftBuffer;
  auto master_right = _obuf._rightBuffer;
  auto input        = (const float*)inputBuffer;
  auto input_left   = _ibuf._leftBuffer;

  /////////////////////////////
  // test tone ?
  /////////////////////////////
  if (0) {
    double frq = midi_note_to_frequency(GNOTE);
    // printf("GNOTE<%d> frq<%g>\n", GNOTE, frq);
    static const float kinvsr = getInverseSampleRate();
    for (int i = 0; i < inumframes; i++) {
      double phase = frq * pi2 * double(_testtoneph) * kinvsr;
      float samp   = sinf(phase);
      // printf("i<%d> samp<%g>\n", i, samp);
      master_left[i]  = samp;
      master_right[i] = samp;
      _testtoneph++;
    }
    constexpr int k_samples_per_tick = 128;
    float elapsed_this_tick          = float(k_samples_per_tick) * getInverseSampleRate();
    auto& eventmap                   = _eventmap.LockForWrite();
    this->_tick(eventmap, elapsed_this_tick);
    _eventmap.UnLock();
  }
  /////////////////////////////
  // real output ?
  /////////////////////////////
  else {

    ////////////////////////////

    if (_onprofilerframe) {
      SynthProfilerFrame frame;
      frame._samplerate  = getSampleRate();
      frame._controlrate = controlRate();
      frame._cpuload     = _cpuload;
      frame._numlayers   = _activeVoices.size();

      int numdspblocks = 0;
      for (auto v : _activeVoices) {
        auto ld = v->_layerdata;
        numdspblocks += ld->numDspBlocks();
      }

      frame._numdspblocks = numdspblocks;
      _onprofilerframe(frame);
    }

    /////////////////////////////
    // clear output buffer
    /////////////////////////////

    /////////////////////////////
    // route to synth input
    /////////////////////////////

    if (input)
      for (int i = 0; i < inumframes; i++) {
        input_left[i] = input[i];
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
    //////////////////////////////////
    for (auto l : _activeVoices)
      l->beginCompute(inumframes);
    //////////////////////////////////
    int ifrpending = inumframes;
    _dspwritecount = frames_per_controlpass;
    _dspwritebase  = 0;
    //////////////////////////////////
    while (ifrpending > 0) {
      // printf("_dspwritecount<%d> _dspwritebase<%d>\n", _dspwritecount, _dspwritebase);
      ////////////////////////////////
      // update controllers
      ////////////////////////////////
      for (auto l : _activeVoices)
        l->updateControllers();
      ////////////////////////////////
      // update dsp modules
      ////////////////////////////////
      for (auto l : _activeVoices)
        l->compute(_dspwritebase, _dspwritecount);
      /////////////////////////////
      // synth update tick
      /////////////////////////////
      _samplesuntilnexttick -= frames_per_controlpass;
      if (_samplesuntilnexttick < 0) {
        float elapsed_this_tick = float(k_samples_per_tick) * getInverseSampleRate();
        _lnoteframe++;
        _lnotetime += elapsed_this_tick;
        auto& eventmap = _eventmap.LockForWrite();
        this->_tick(eventmap, elapsed_this_tick);
        _eventmap.UnLock();
        _samplesuntilnexttick += k_samples_per_tick;
        ////////////////////////////////////////////
        activateVoices(ifrpending);
        deactivateVoices();
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
      for (auto l : _activeVoices) {
        l->mixToBus(_dspwritebase, _dspwritecount);
        l->updateScopes(_dspwritebase, _dspwritecount);
      }
      /////////////////////////////
      // compute/accumulate output busses
      //  (into main output)
      /////////////////////////////
      for (auto busitem : _outputBusses) {
        auto bus         = busitem.second;
        auto& bus_buf    = bus->_buffer;
        float* bus_left  = bus_buf._leftBuffer;
        float* bus_right = bus_buf._rightBuffer;
        //////////////////////////////////////////
        // bus DSP fx
        //////////////////////////////////////////
        if (1) {
          auto busdsplayer = bus->_dsplayer;
          if (busdsplayer) {
            auto dsp_buf = busdsplayer->_dspbuffer;
            dsp_buf->resize(inumframes);
            float* dsp_left  = dsp_buf->channel(0);
            float* dsp_right = dsp_buf->channel(1);
            //////////////////////////////////////////
            // bus -> dsp buf input
            //////////////////////////////////////////
            for (int i = 0; i < _dspwritecount; i++) {
              int j        = _dspwritebase + i;
              dsp_left[i]  = bus_left[j];
              dsp_right[i] = bus_right[j];
            }
            //////////////////////////////////////////
            // compute dsp -> tempbus
            //////////////////////////////////////////
            busdsplayer->_outbus = nullptr;
            busdsplayer->beginCompute(_dspwritecount);
            busdsplayer->updateControllers();
            busdsplayer->compute(0, _dspwritecount);
            busdsplayer->endCompute();
            //////////////////////////////////////////
            // tempbus -> bus out
            //////////////////////////////////////////
            const float* fxlyroutl = busdsplayer->_dspbuffer->channel(0);
            const float* fxlyroutr = busdsplayer->_dspbuffer->channel(1);
            for (int i = 0; i < _dspwritecount; i++) {
              int j        = _dspwritebase + i;
              bus_left[j]  = fxlyroutl[i];
              bus_right[j] = fxlyroutr[i];
            }
            //////////////////////////////////////////
          }
        }
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
      // update indices
      ////////////////////////////////
      _dspwritebase += frames_per_controlpass;
      ifrpending -= frames_per_controlpass;
      /////////////////////////////
    }
    //////////////////////////////////
    for (auto l : _activeVoices)
      l->endCompute();
  }
  /////////////////////////////
  // final clamping
  /////////////////////////////
  for (int i = 0; i < inumframes; i++) {
    float L = clip_float(master_left[i] * _masterGain, -1.0f, 1.0f);
    float R = clip_float(master_right[i] * _masterGain, -1.0f, 1.0f);
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
}

///////////////////////////////////////////////////////////////////////////////

void synth::_keyOffLayer(layer_ptr_t l) {
  l->_lyrPhase = 1;
  this->releaseLayer(l);
  l->keyOff();
}

///////////////////////////////////////////////////////////////////////////////

void programInst::keyOn(int note, int velocity, prgdata_constptr_t pd, keyonmod_ptr_t kmod) {
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

    l->_ldindex = ilayer - 1;
    l->_keymods = _keymods;
    l->_name    = ld->_name;

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
  }
  _numframes = inumframes;
}
} // namespace ork::audio::singularity
