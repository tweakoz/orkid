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

void ToTimeDomainData::describeX(class_t* clazz) {}

struct TO_TD_IMPL {
    audiofft::AudioFFT _fft; // FFT object for performing IFFT
    std::vector<float> _output; // Buffer to hold IFFT output
    std::vector<float> _overlapBuffer; // Buffer to hold the overlapped portion from the previous frame
    size_t _frames_out = 0; // Counter to track output frames, reset after processing each block

    TO_TD_IMPL() {
        _fft.init(kSPECTRALSIZE); // Initialize FFT object with spectral size
        _output.resize(kSPECTRALSIZE); // Resize output buffer to match FFT size
        _overlapBuffer.resize(kSPECTRALSIZE / 2, 0); // Resize overlap buffer for 50% overlap, initialized to 0
    }

    void compute(ToTimeDomain* ttd, DspBuffer& dspbuf, int ibase, int inumframes) {
        auto obufL = ttd->getOutBuf(dspbuf, 0) + ibase; // Left channel output buffer
        auto obufR = ttd->getOutBuf(dspbuf, 1) + ibase; // Right channel output buffer

        // Ensure the DSP buffer sizes match expected complex size
        size_t complex_size = audiofft::AudioFFT::ComplexSize(kSPECTRALSIZE);
        OrkAssert(dspbuf._real.size() == complex_size);
        OrkAssert(dspbuf._imag.size() == complex_size);

        // Check if FFT has been performed for the current frame
        if(dspbuf._didFFT){

            // Perform the IFFT
            _fft.ifft(_output.data(), dspbuf._real.data(), dspbuf._imag.data());

            // Apply window and overlap-add for the current frame
            for(int i = 0; i < kSPECTRALSIZE; i++){
                // Apply Hanning window to the output
                float window = 0.5 * (1 - cos(2 * M_PI * i / (kSPECTRALSIZE - 1)));
                _output[i] *= window;

                // For the first half of the frame, add it to the overlap buffer from the previous frame
                if(i < kSPECTRALSIZE / 2){
                    _output[i] += _overlapBuffer[i];
                }

                // Update overlap buffer with the second half of the current frame
                // This part will be overlapped with the next frame's first half
                if(i >= kSPECTRALSIZE / 2){
                    _overlapBuffer[i - kSPECTRALSIZE / 2] = _output[i];
                }
            }

            // Reset the frames out counter since we're starting fresh after IFFT
            _frames_out = 0;
        }

        // Output the time-domain data to both left and right channels
        // Note: This loop now only needs to handle inumframes since we've already managed the output
        for(int i = 0; i < inumframes; i++){
            obufL[i] = _output[_frames_out + i];
            obufR[i] = _output[_frames_out + i];
        }

        // Update frames_out to manage the windowed output correctly
        _frames_out += inumframes;
    }
};

///////////////////////////////////////////////////////////////////////////////

ToTimeDomainData::ToTimeDomainData(std::string name, float fb)
    : DspBlockData(name) {
  _blocktype       = "ToTimeDomain";
  auto mix_param   = addParam();
  auto pitch_param = addParam();

  mix_param->useDefaultEvaluator();
  pitch_param->useDefaultEvaluator();
}
///////////////////////////////////////////////////////////////////////////////

dspblk_ptr_t ToTimeDomainData::createInstance() const { // override
  return std::make_shared<ToTimeDomain>(this);
}

///////////////////////////////////////////////////////////////////////////////

ToTimeDomain::ToTimeDomain(const ToTimeDomainData* dbd)
    : DspBlock(dbd) {
  _mydata = dbd;

  auto syni = synth::instance();
  auto impl = _impl[0].makeShared<TO_TD_IMPL>();

}
ToTimeDomain::~ToTimeDomain(){

}

///////////////////////////////////////////////////////////////////////////////

void ToTimeDomain::compute(DspBuffer& dspbuf) // final
{
  int inumframes = _layer->_dspwritecount;
  int ibase      = _layer->_dspwritebase;
  auto impl = _impl[0].getShared<TO_TD_IMPL>();
  impl->compute(this, dspbuf, ibase, inumframes);

}

///////////////////////////////////////////////////////////////////////////////

void ToTimeDomain::doKeyOn(const KeyOnInfo& koi) // final
{
  auto impl = _impl[0].getShared<TO_TD_IMPL>();
  //impl->clear();
}
} // namespace ork::audio::singularity
