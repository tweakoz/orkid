////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
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

namespace ork::audio::singularity {
///////////////////////////////////////////////////////////////////////////////
void synth::nextEffect() {
  _eventmap.atomicOp([this](eventmap_t& emap) { //
    emap.insert(std::make_pair(0.0f, [this]() {
      for (auto bus : _outputBusses) {

        ///////////////////////////////
        auto it = _fxcurpreset;
        if (it == _fxpresets.end()) {
          it = _fxpresets.begin();
        } else {
          it++;
        }
        if (it == _fxpresets.end()) {
          it = _fxpresets.begin();
        }
        ///////////////////////////////
        _fxcurpreset    = it;
        auto nextpreset = _fxcurpreset->second;
        assert(nextpreset->_algdata != nullptr); // did you add presets ?
        bus.second->setBusDSP(nextpreset);
        bus.second->_fxname = it->first;
      }
    }));
  });
}
///////////////////////////////////////////////////////////////////////////////
void OutputBus::resize(int numframes) {
  _buffer.resize(numframes);
  if (_dsplayer) {
    _dsplayer->resize(numframes);
  }
}
///////////////////////////////////////////////////////////////////////////////
void OutputBus::setBusDSP(lyrdata_ptr_t ld) {

  assert(ld->_algdata != nullptr);

  _dsplayer = nullptr;

  _dsplayerdata        = ld;
  auto l               = std::make_shared<Layer>();
  l->_is_bus_processor = true;
  synth::instance()->_keyOnLayer(l,0, 0, _dsplayerdata); // outbus layer always keyed on...
  _dsplayer = l;
}
scopesource_ptr_t OutputBus::createScopeSource() {
  _scopesource = std::make_shared<ScopeSource>();
  return _scopesource;
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
synth::synth()
    : _timeaccum(0.0f)
    , _sampleRate(0.0f)
    , _dt(0.0f)
    , _soloLayer(-1)
    , _hudpage(0)
    , _masterGain(1.0f) { //
  _fxcurpreset = _fxpresets.rbegin().base();

  _tempbus         = std::make_shared<OutputBus>();
  _tempbus->_name  = "temp-dsp";
  _numactivevoices = 0;
  createOutputBus("main");

  // TODO - synth::instance(); is creating chicken and egg problems
  loadAllFxPresets(this);
  nextEffect();

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

  _hudvp = std::make_shared<HudLayoutGroup>();

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
 // _deactiveateVoiceQ.clear();
  _freeProgInst.atomicOp([](proginstset_t& unlocked){
    unlocked.clear();
  });
  _activeProgInst.atomicOp([](proginstset_t& unlocked){
    unlocked.clear();
  });

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
      event();
      done = false;
      emap.erase(it);
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
  if ((--l->_keepalive) == 0) {
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

    it = _freeVoices.find(l);
    assert(it == _freeVoices.end());
    _freeVoices.insert(l);

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
programInst* synth::liveKeyOn(int note, int velocity, prgdata_constptr_t pdata) {
  if (not pdata)
    return nullptr;
  programInst* pi = nullptr;
  _freeProgInst.atomicOp([&pi](proginstset_t& piset) {
    auto it = piset.begin();
    assert(it != piset.end());
    pi = *it;
    piset.erase(it);
  });
  pi->_progdata = pdata;

  addEvent(0.0f, [note, velocity, pdata, this, pi]() {
    // printf("syn allocProgInst<%p>\n", pi);

    int clampn = std::clamp(note, 0, 127);
    int clampv = std::clamp(velocity, 0, 127);

    pi->keyOn(clampn, clampv, pdata);

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
  return pi;
}
///////////////////////////////////////////////////////////////////////////////
void synth::liveKeyOff(programInst* pinst) {
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
///////////////////////////////////////////////////////////////////////////////

programInst* synth::keyOn(int note, int velocity, prgdata_constptr_t pdata) {
  assert(pdata);
  programInst* pi = nullptr;

  _freeProgInst.atomicOp([&pi](proginstset_t& piset) {
    auto it = piset.begin();
    assert(it != piset.end());
    pi = *it;
    piset.erase(it);
  });
  pi->_progdata = pdata;
  // printf("syn allocProgInst<%p>\n", pi);

  int clampn = std::clamp(note, 0, 127);
  int clampv = std::clamp(velocity, 0, 127);

  pi->keyOn(clampn, clampv, pdata);

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

  if (_lock_compute)
    return;

  resize(inumframes);

  /////////////////////////////

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

  auto input = (const float*)inputBuffer;

  auto input_left   = _ibuf._leftBuffer;
  auto master_left  = _obuf._leftBuffer;
  auto master_right = _obuf._rightBuffer;

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
  auto& eventmap = _eventmap.LockForWrite();
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
      this->_tick(eventmap, elapsed_this_tick);
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
      //////////////////////////////////////////
      // accumulate bus to master
      //////////////////////////////////////////
      for (int i = 0; i < _dspwritecount; i++) {
        int j = _dspwritebase + i;
        master_left[j] += bus_left[j];
        master_right[j] += bus_right[j];
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
  _eventmap.UnLock();
  //////////////////////////////////
  for (auto l : _activeVoices)
    l->endCompute();
  /////////////////////////////
  // test tone ?
  /////////////////////////////
  if (0) {
    for (int i = 0; i < inumframes; i++) {
      double phase = 120.0f * pi2 * double(_testtoneph) * getInverseSampleRate();
      float samp   = sinf(phase) * .6;
      // printf("i<%d> samp<%g>\n", i, samp);
      master_left[i]  = samp;
      master_right[i] = samp;
      _testtoneph++;
    }
  }
  /////////////////////////////
  // final clamping
  /////////////////////////////
  for (int i = 0; i < inumframes; i++) {
    master_left[i]  = clip_float(master_left[i], -2, 2);
    master_right[i] = clip_float(master_right[i], -2, 2);
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

void synth::_keyOnLayer(layer_ptr_t l, int note, int velocity, lyrdata_ptr_t ld){
  std::lock_guard<std::mutex> lock(l->_mutex);

  l->reset();

  assert(ld != nullptr);

  l->_koi._layer = l;
  l->_koi._key       = note;
  l->_koi._vel       = velocity;
  l->_koi._layerdata = ld;

  l->_HKF._miscText   = "";
  l->_HKF._note       = note;
  l->_HKF._vel        = velocity;
  l->_HKF._layerdata  = ld;
  l->_HKF._layerIndex = l->_ldindex;
  l->_HKF._useFm4     = false;

  l->_layerBasePitch = clip_float(note * 100, -0, 12700);

  l->_ignoreRelease = ld->_ignRels;
  l->_curnote       = note;
  l->_layerdata     = ld;
  l->_outbus        = this->outputBus(ld->_outbus);
  l->_layerLinGain  = ld->_layerLinGain;

  l->_curvel = velocity;

  l->_layerTime = 0.0f;

  l->retain();

  /////////////////////////////////////////////
  // controllers
  /////////////////////////////////////////////

  if (ld->_ctrlBlock) {
    l->_ctrlBlock = std::make_shared<ControlBlockInst>();
    l->_ctrlBlock->keyOn(l->_koi, ld->_ctrlBlock);
  }

  ///////////////////////////////////////

  l->_alg = l->_layerdata->_algdata->createAlgInst();
  // assert(_alg);
  if (l->_alg) {
    l->_alg->keyOn(l->_koi);
  }

  l->_HKF._alg = l->_alg;

  ///////////////////////////////////////

  l->_lyrPhase = 0;
  l->_sinrepPH = 0.0f;

}
void synth::_keyOffLayer(layer_ptr_t l){
  l->_lyrPhase = 1;
  this->releaseLayer(l);

  ///////////////////////////////////////

  if (l->_ctrlBlock)
    l->_ctrlBlock->keyOff();

  ///////////////////////////////////////

  if (l->_ignoreRelease)
    return;

  ///////////////////////////////////////

  if (l->_alg)
    l->_alg->keyOff();

}

///////////////////////////////////////////////////////////////////////////////

void programInst::keyOn(int note, int velocity, prgdata_constptr_t pd) {
  int ilayer = 0;

  auto syn = synth::instance();

  for (const auto& ld : pd->_layerdatas) {
    ilayer++;

    if (note < ld->_loKey || note > ld->_hiKey)
      continue;

    if (syn->_soloLayer >= 0) {
      if (syn->_soloLayer != (ilayer - 1))
        continue;
    }

    // printf( "lovel<%d>\n", ld->_loVel );
    // printf( "hivel<%d>\n", ld->_hiVel );

    if (velocity < ld->_loVel || velocity > ld->_hiVel)
      continue;

    // printf("KEYON L%d\n", ilayer);

    auto l      = syn->allocLayer();
    l->_ldindex = ilayer - 1;

    assert(l != nullptr);

    assert(ld != nullptr);

    syn->_keyOnLayer(l,note,velocity,ld);

    _layers.push_back(l);
  }
  int inuml = _layers.size();
  int solol = syn->_soloLayer;

  if (solol >= 0 and solol < inuml) {
    syn->_hudLayer = _layers[solol];
  } else if (inuml > 0) {
    syn->_hudLayer = _layers[0];
  } else
    syn->_hudLayer = nullptr;

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
