////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
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

namespace ork::audio::singularity {
///////////////////////////////////////////////////////////////////////////////
void OutputBus::resize(int numframes) {
  _buffer.resize(numframes);
  if (_dsplayer) {
    _dsplayer->resize(numframes);
  }
}
///////////////////////////////////////////////////////////////////////////////
void OutputBus::setBusDSP(lyrdata_ptr_t ld) {
  _dsplayerdata        = ld;
  auto l               = new Layer;
  l->_is_bus_processor = true;
  l->keyOn(0, 0, _dsplayerdata); // outbus layer always keyed on...
  _dsplayer = l;
}
scopesource_ptr_t OutputBus::createScopeSource() {
  _scopesource = std::make_shared<ScopeSource>();
  return _scopesource;
}

///////////////////////////////////////////////////////////////////////////////
synth_ptr_t synth::instance() {
  static auto the_syn = std::make_shared<synth>();
  return the_syn;
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
    : _sampleRate(0.0f)
    , _dt(0.0f)
    , _soloLayer(-1)
    , _timeaccum(0.0f)
    , _hudpage(0)
    , _oswidth(0.0333333f * getSampleRate()) //
    , _ostriglev(0.05f) {                    //

  _tempbus         = std::make_shared<OutputBus>();
  _tempbus->_name  = "temp-dsp";
  _numactivevoices = 0;
  createOutputBus("main");

  for (int i = 0; i < kmaxlayerspersynth; i++) {
    auto l = new Layer();
    _allVoices.insert(l);
    _freeVoices.insert(l);
  }

  for (int i = 0; i < kmaxlayerspersynth; i++) {
    auto pi = new programInst();
    _freeProgInst.insert(pi);
    _allProgInsts.insert(pi);
  }

  _hudvp = std::make_shared<HudViewport>();

  resize(frames_per_dsppass);
}

void synth::setSampleRate(float sr) {
  _sampleRate = sr;
  _dt         = 1.0f / sr;
}

///////////////////////////////////////////////////////////////////////////////

synth::~synth() {
  for (auto v : _allVoices)
    delete v;
  for (auto pi : _allProgInsts)
    delete pi;
}

///////////////////////////////////////////////////////////////////////////////
void synth::addEvent(float time, void_lambda_t ev) {
  _eventmap.insert(std::make_pair(time, ev));
}

///////////////////////////////////////////////////////////////////////////////

void synth::tick(float dt) {
  bool done = false;
  while (false == done) {
    done    = true;
    auto it = _eventmap.begin();
    if (it != _eventmap.end() and //
        it->first <= _timeaccum) {
      auto& event = it->second;
      event();
      done = false;
      _eventmap.erase(it);
    }
  }
  _timeaccum += dt;
}

///////////////////////////////////////////////////////////////////////////////

Layer* synth::allocLayer() {
  auto it = _freeVoices.begin();
  assert(it != _freeVoices.end());
  auto l = *it;
  // printf( "syn alloclayer<%p>\n", l );
  _freeVoices.erase(it);
  it = _activeVoices.find(l);
  assert(it == _activeVoices.end());
  _activeVoices.insert(l);
  return l;
}

///////////////////////////////////////////////////////////////////////////////

void synth::freeLayer(Layer* l) {
  _deactiveateVoiceQ.push(l);
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

    it = _freeVoices.find(l);
    assert(it == _freeVoices.end());
    _freeVoices.insert(l);

    done = (_deactiveateVoiceQ.size() == 0);
  }
  _numactivevoices = _activeVoices.size();
}

///////////////////////////////////////////////////////////////////////////////

programInst* synth::keyOn(int note, int velocity, prgdata_constptr_t pdata) {
  assert(pdata);
  auto it = _freeProgInst.begin();
  assert(it != _freeProgInst.end());
  auto pi = *it;
  // printf("syn allocProgInst<%p>\n", pi);
  pi->_progdata = pdata;
  _freeProgInst.erase(it);

  int clampn = std::clamp(note, 0, 127);
  int clampv = std::clamp(velocity, 0, 127);

  pi->keyOn(clampn, clampv, pdata);
  _activeProgInst.insert(pi);
  _lnoteframe   = 0;
  _lnotetime    = 0.0f;
  _clearhuddata = true;
  if (0) //_testtone )
  {
    float frq      = midi_note_to_frequency(note);
    _testtonepi    = pi2 * frq / _sampleRate;
    _testtoneph    = 0.0f;
    _testtoneamp   = 1.0f;
    _testtoneampps = slopeDBPerSample(-6, _sampleRate);
  }
  return pi;
}

///////////////////////////////////////////////////////////////////////////////

void synth::keyOff(programInst* pinst) {
  pinst->keyOff();
  auto it = _activeProgInst.find(pinst);
  assert(it != _activeProgInst.end());
  _activeProgInst.erase(it);
  _freeProgInst.insert(pinst);
  if (0) // _testtone )
  {
    _testtoneampps = slopeDBPerSample(-18, _sampleRate);
  }
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
  resize(inumframes);

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
  // synth update tick
  /////////////////////////////

  float tick = float(inumframes) * _dt;
  _lnoteframe++;
  _lnotetime += tick;
  this->tick(tick);

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

  int inumv = 0;
  for (auto l : _activeVoices) {
    l->compute(inumframes);
    inumv++;
  }

  /////////////////////////////
  // clear synth main output mix buffer
  /////////////////////////////

  for (int i = 0; i < inumframes; i++) {
    master_left[i]  = 0.0f;
    master_right[i] = 0.0f;
  }

  /////////////////////////////
  // compute DSP for output busses
  /////////////////////////////

  for (auto busitem : _outputBusses) {
    auto bus = busitem.second;
    if (bus->_dsplayer) {
      auto& bus_buf = bus->_buffer;
      auto dsp_buf  = bus->_dsplayer->_dspbuffer;
      dsp_buf->resize(inumframes);
      float* bus_left  = bus_buf._leftBuffer;
      float* bus_right = bus_buf._rightBuffer;
      float* dsp_left  = dsp_buf->channel(0);
      float* dsp_right = dsp_buf->channel(1);
      //////////////////////////////////////////
      // bus -> dsp buf input
      //////////////////////////////////////////
      for (int i = 0; i < inumframes; i++) {
        dsp_left[i]  = bus_left[i];
        dsp_right[i] = bus_right[i];
      }
      //////////////////////////////////////////
      // compute dsp -> tempbus
      //////////////////////////////////////////
      bus->_dsplayer->_outbus = _tempbus;
      bus->_dsplayer->compute(inumframes);
      //////////////////////////////////////////
      // tempbus -> bus out
      //////////////////////////////////////////
      float* tmp_left  = _tempbus->_buffer._leftBuffer;
      float* tmp_right = _tempbus->_buffer._rightBuffer;
      for (int i = 0; i < inumframes; i++) {
        bus_left[i]  = tmp_left[i];
        bus_right[i] = tmp_right[i];
      }
      //////////////////////////////////////////
    }
  }

  /////////////////////////////
  // compute/accumulate output busses
  //  (into main output)
  /////////////////////////////

  for (auto busitem : _outputBusses) {
    auto bus               = busitem.second;
    auto& obuf             = bus->_buffer;
    const float* bus_left  = obuf._leftBuffer;
    const float* bus_right = obuf._rightBuffer;
    for (int i = 0; i < inumframes; i++) {
      master_left[i] += bus_left[i];
      master_right[i] += bus_right[i];
    }
    //////////////////////////////////////
    // SignalScope
    //////////////////////////////////////
    if (bus->_scopesource) {
      bus->_scopesource->updateStereo(inumframes, bus_left, bus_right);
    }
  }

  /////////////////////////////
  // test tone ?
  /////////////////////////////

  if (0) {
    for (int i = 0; i < inumframes; i++) {
      double phase = 60.0 * pi2 * double(_testtoneph) / getSampleRate();
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
    master_left[i]  = clip_float(master_left[i], -1, 1);
    master_right[i] = clip_float(master_right[i], -1, 1);
  }

  /////////////////////////////

  deactivateVoices();
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

    l->keyOn(note, velocity, ld);
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
    l->keyOff();
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
