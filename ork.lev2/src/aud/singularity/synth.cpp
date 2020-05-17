//#include <audiofile.h>
#include <string>
#include <assert.h>
#include <unistd.h>
#include <math.h>
#include <GLFW/glfw3.h>

#include <ork/lev2/aud/singularity/synthdata.h>
#include <ork/lev2/aud/singularity/synth.h>
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

programInst* synth::keyOn(int note, const ProgramData* pdata) {
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

  auto ilb = _ibuf._leftBuffer;
  auto lb  = _obuf._leftBuffer;
  auto rb  = _obuf._rightBuffer;

  if (input)
    for (int i = 0; i < inumframes; i++) {
      ilb[i] = input[i];
    }

  for (int i = 0; i < inumframes; i++) {
    lb[i] = 0.0f;
    rb[i] = 0.0f;
  }

  /////////////////////////////
  // compute prgs/layers
  /////////////////////////////

  float tick = float(inumframes) * _dt;

  _lnoteframe++;
  _lnotetime += tick;

  this->tick(tick);

  // printf("synth::Compute inumframes<%d> tick<%f>\n", inumframes, tick);

  // for( auto pi : _activeProgInst )
  //	pi->compute();

  int inumv = 0;
  for (auto l : _activeVoices) {
    l->compute(_obuf);
    inumv++;
  }

  if (0) //_testtone )
  {
    for (int i = 0; i < inumframes; i++) {
      float samp = sinf(_testtoneph) * _testtoneamp * .6;
      samp += sinf(_testtoneph * 2) * _testtoneamp * .3;
      samp += sinf(_testtoneph * 4) * _testtoneamp * .1;
      lb[i] += samp;
      rb[i] += samp;

      _testtoneph += _testtonepi;

      _testtoneamp *= _testtoneampps;
    }
  }

  deactivateVoices();

  /////////////////////////////
  // mixdown
  /////////////////////////////

  for (int i = 0; i < inumframes; i++) {
    lb[i] = clip_float(lb[i] * 0.1f, -1, 1);
    rb[i] = clip_float(rb[i] * 0.1f, -1, 1);
  }
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

void programInst::keyOn(int note, const ProgramData* pd) {
  int ilayer = 0;

  auto syn = synth::instance();

  for (const auto& ld : pd->_LayerDatas) {
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
