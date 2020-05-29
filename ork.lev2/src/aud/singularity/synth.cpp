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
#include <GLFW/glfw3.h>

#include <ork/lev2/aud/singularity/synthdata.h>
#include <ork/lev2/aud/singularity/synth.h>
#include <ork/lev2/aud/singularity/hud.h>
#include <ork/lev2/aud/singularity/krzobjects.h>

namespace ork::audio::singularity {
///////////////////////////////////////////////////////////////////////////////
synth_ptr_t synth::instance() {
  static auto the_syn = std::make_shared<synth>();
  return the_syn;
}

///////////////////////////////////////////////////////////////////////////////

synth::synth()
    : _sampleRate(0.0f)
    , _dt(0.0f)
    , _soloLayer(-1)
    , _timeaccum(0.0f)
    , _hudpage(0)
    , _oswidth(1500)     //
    , _ostriglev(0.0f) { //

  for (int i = 0; i < 256; i++) {
    auto l = new Layer();
    _allVoices.insert(l);
    _freeVoices.insert(l);
  }

  for (int i = 0; i < 256; i++) {
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
    if (it != _eventmap.end() && it->first <= _timeaccum) {
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
}

///////////////////////////////////////////////////////////////////////////////

programInst* synth::keyOn(int note, prgdata_constptr_t pdata) {
  assert(pdata);
  auto it = _freeProgInst.begin();
  assert(it != _freeProgInst.end());
  auto pi = *it;
  // printf("syn allocProgInst<%p>\n", pi);
  pi->_progdata = pdata;
  _freeProgInst.erase(it);
  pi->keyOn(note, pdata);
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
    _ibuf.resize(numframes);
    _obuf.resize(numframes);
    for (auto lay : _allVoices) {
      lay->resize(numframes);
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
  // clear synth output mix buffer
  /////////////////////////////

  for (int i = 0; i < inumframes; i++) {
    master_left[i]  = 0.0f;
    master_right[i] = 0.0f;
  }

  /////////////////////////////
  // synth update tick
  /////////////////////////////

  float tick = float(inumframes) * _dt;
  _lnoteframe++;
  _lnotetime += tick;
  this->tick(tick);

  /////////////////////////////
  // compute/accumulate layer instances
  /////////////////////////////

  int inumv = 0;
  for (auto l : _activeVoices) {
    l->compute(_obuf, inumframes);
    inumv++;
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

void programInst::keyOn(int note, prgdata_constptr_t pd) {
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

    int vel = 96;

    // printf( "lovel<%d>\n", ld->_loVel );
    // printf( "hivel<%d>\n", ld->_hiVel );

    if (vel < ld->_loVel || vel > ld->_hiVel)
      continue;

    // printf("KEYON L%d\n", ilayer);

    auto l      = syn->allocLayer();
    l->_ldindex = ilayer - 1;

    assert(l != nullptr);

    assert(ld != nullptr);

    l->keyOn(note, vel, ld);
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

  if (syn->_hudLayer)
    syn->_hudbuf.push(syn->_hudLayer->_HKF);
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
