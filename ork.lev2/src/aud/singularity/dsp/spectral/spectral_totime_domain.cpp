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

ImplementReflectionX(ork::audio::singularity::ToTimeDomainData, "DspToTimeDomain");

namespace ork::audio::singularity {

void ToTimeDomainData::describeX(class_t* clazz) {
}

FrequencyToTimeDomain::FrequencyToTimeDomain() {
  _fft.init(kSPECTRALSIZE);                    // Initialize FFT object with spectral size
  _output.resize(kSPECTRALSIZE);               // Resize output buffer to match FFT size
  _overlapBuffer.resize(kSPECTRALSIZE / 2, 0); // Resize overlap buffer for 50% overlap, initialized to 0
}
FrequencyToTimeDomain::~FrequencyToTimeDomain() {
}
void FrequencyToTimeDomain::compute(
    const floatvect_t& real, //
    const floatvect_t& imag, //
    int inumframes) {        //

  // Ensure the DSP buffer sizes match expected complex size
  size_t complex_size = audiofft::AudioFFT::ComplexSize(kSPECTRALSIZE);
  OrkAssert(real.size() == complex_size);
  OrkAssert(imag.size() == complex_size);

  // Perform the IFFT
  _fft.ifft(_output.data(), real.data(), imag.data());

  // Apply window and overlap-add for the current frame
  for (int i = 0; i < kSPECTRALSIZE; i++) {
    // Apply Hanning window to the output
    float window = 0.5 * (1 - cos(2 * M_PI * i / (kSPECTRALSIZE - 1)));
    _output[i] *= window;

    // For the first half of the frame, add it to the overlap buffer from the previous frame
    if (i < kSPECTRALSIZE / 2) {
      _output[i] += _overlapBuffer[i];
    }

    // Update overlap buffer with the second half of the current frame
    // This part will be overlapped with the next frame's first half
    if (i >= kSPECTRALSIZE / 2) {
      _overlapBuffer[i - kSPECTRALSIZE / 2] = _output[i];
    }

    // Reset the frames out counter since we're starting fresh after IFFT
  }

  // Update frames_out to manage the windowed output correctly
  _frames_out = 0;
}

///////////////////////////////////////////////////////////////////////////////

ToTimeDomainData::ToTimeDomainData(std::string name, float fb)
    : DspBlockData(name) {
  _blocktype      = "ToTimeDomain";
  auto gain_param = addParam("gain", "dB");
  gain_param->useAmplitudeEvaluator();
}
///////////////////////////////////////////////////////////////////////////////

dspblk_ptr_t ToTimeDomainData::createInstance() const { // override
  return std::make_shared<ToTimeDomain>(this);
}

///////////////////////////////////////////////////////////////////////////////

ToTimeDomain::ToTimeDomain(const ToTimeDomainData* dbd)
    : DspBlock(dbd) {
  _mydata = dbd;

  // auto syni = synth::instance();
  auto impl = _impl[0].makeShared<FrequencyToTimeDomain>();
}
ToTimeDomain::~ToTimeDomain() {
}

///////////////////////////////////////////////////////////////////////////////

void ToTimeDomain::compute(DspBuffer& dspbuf) { // final
  int inumframes = _layer->_dspwritecount;
  int ibase      = _layer->_dspwritebase;
  float gain     = this->_param[0].eval();
  float gain_lin = decibel_to_linear_amp_ratio(gain);
  auto obufL     = this->getOutBuf(dspbuf, 0) + ibase; // Left channel output buffer
  auto obufR     = this->getOutBuf(dspbuf, 1) + ibase; // Right channel output buffer

  auto impl = _impl[0].getShared<FrequencyToTimeDomain>();

  if (dspbuf._didFFT) {
    impl->compute(dspbuf._real, dspbuf._imag, inumframes);
  }
  // Output the time-domain data to both left and right channels
  // Note: This loop now only needs to handle inumframes since we've already managed the output
  int ifout = impl->_frames_out;
  for (int i = 0; i < inumframes; i++) {
    obufL[i] = gain_lin * impl->_output[ifout + i];
    obufR[i] = gain_lin * impl->_output[ifout + i];
  }

  // Update frames_out to manage the windowed output correctly
  impl->_frames_out += inumframes;
}

///////////////////////////////////////////////////////////////////////////////

void ToTimeDomain::doKeyOn(const KeyOnInfo& koi) // final
{
  auto impl = _impl[0].getShared<FrequencyToTimeDomain>();
  // impl->clear();
}
} // namespace ork::audio::singularity
