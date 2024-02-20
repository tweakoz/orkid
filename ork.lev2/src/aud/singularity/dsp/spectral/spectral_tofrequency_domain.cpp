////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/aud/singularity/synth.h>
#include <assert.h>
#include <ork/lev2/aud/singularity/filters.h>
#include <ork/lev2/aud/singularity/dsp_mix.h>
#include <ork/lev2/aud/singularity/modulation.h>
#include <ork/lev2/aud/singularity/spectral.h>
#include <ork/lev2/aud/singularity/fft.h>

ImplementReflectionX(ork::audio::singularity::ToFrequencyDomainData, "DspToFrequencyDomain");

namespace ork::audio::singularity {

void ToFrequencyDomainData::describeX(class_t* clazz) {
}

TimeToFrequencyDomain::TimeToFrequencyDomain(size_t length)
    : _length(length) {
  _fft.init(_length);
  _input.resize(_length);
  for (int i = 0; i < _length; i++) {
    float fj     = float(i) / float(_length - 1);
    float window = 0.54 - 0.46 * cos(PI2 * fj);
    _window.push_back(window);
  }
}
TimeToFrequencyDomain::~TimeToFrequencyDomain() {
}
void TimeToFrequencyDomain::resize(size_t length) {
  _length = length;
  _fft.init(_length);
  _input.resize(_length);
  for( auto& item : _input )
    item = 0.0f;
  _window.clear();
  for (int i = 0; i < _length; i++) {
    float fj     = float(i) / float(_length - 1);
    float window = 0.54 - 0.46 * cos(PI2 * fj);
    _window.push_back(window);
  }
}
bool TimeToFrequencyDomain::compute(
    const float* inp,  //
    floatvect_t& real, //
    floatvect_t& imag, //
    int inumframes) {  //
  bool didFFT = false;
  OrkAssert(_length % inumframes == 0);
  size_t complex_size = audiofft::AudioFFT::ComplexSize(_length);
  //
  if (real.size() != _length) {
    real.resize(complex_size);
    imag.resize(complex_size);
  }

  // input the time domain data for .666mSec chunk
  for (int i = 0; i < inumframes; i++) {
    int j     = _frames_in + i;
    _input[j] = inp[i]; //*_window[j];
  }
  _frames_in += inumframes;

  // we have enough data to run the fft
  if (_frames_in >= _length) {
    _fft.fft(_input.data(), real.data(), imag.data());
    // overlap add 50%
    std::copy(_input.begin() + _length / 2, _input.end(), _input.begin());
    _frames_in = _length / 2;
    didFFT     = true;
    if (0) {
      printf("TimeToFrequencyDomain::compute::fft triggered\n");
      size_t complex_size = audiofft::AudioFFT::ComplexSize(_length);
      for (int i = 0; i < complex_size; i++) {
        float fr = real[i];
        float fi = imag[i];
        printf("idx<%d> real: %g imag: %g\n", i, fr, fi);
      }
    }
  }
  return didFFT;
}

///////////////////////////////////////////////////////////////////////////////

ToFrequencyDomainData::ToFrequencyDomainData(std::string name, float fb)
    : DspBlockData(name) {
  _blocktype       = "ToFrequencyDomain";
  auto mix_param   = addParam();
  auto pitch_param = addParam();

  mix_param->useDefaultEvaluator();
  pitch_param->useDefaultEvaluator();
}
///////////////////////////////////////////////////////////////////////////////

dspblk_ptr_t ToFrequencyDomainData::createInstance() const { // override
  return std::make_shared<ToFrequencyDomain>(this);
}

///////////////////////////////////////////////////////////////////////////////

ToFrequencyDomain::ToFrequencyDomain(const ToFrequencyDomainData* dbd)
    : DspBlock(dbd) {
  _mydata = dbd;

  // auto syni = synth::instance();
  auto impl     = _impl[0].makeShared<TimeToFrequencyDomain>();
  impl->_length = _mydata->_length;
}
ToFrequencyDomain::~ToFrequencyDomain() {
}

///////////////////////////////////////////////////////////////////////////////

void ToFrequencyDomain::compute(DspBuffer& dspbuf) // final
{
  int inumframes = _layer->_dspwritecount;
  int ibase      = _layer->_dspwritebase;
  auto impl      = _impl[0].getShared<TimeToFrequencyDomain>();
  auto ibuf      = this->getInpBuf(dspbuf, 0) + ibase;
  dspbuf._didFFT = impl->compute(ibuf, dspbuf._real, dspbuf._imag, inumframes);
}

///////////////////////////////////////////////////////////////////////////////

void ToFrequencyDomain::doKeyOn(const KeyOnInfo& koi) // final
{
  auto impl = _impl[0].getShared<TimeToFrequencyDomain>();
  // impl->clear();
}
} // namespace ork::audio::singularity
