////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include "reflection.h"
#include "dspblocks.h"
#include <ork/math/cmatrix4.h>
#include <ork/math/cmatrix3.h>
#include <ork/math/cvector4.h>
#include <ork/math/cvector2.h>
#include "shelveeq.h"
#include "delays.h"
#include "fft_convolver.h"

namespace ork::audio::singularity {

static constexpr size_t kDEFAULT_SPECTRALSIZE = 4096;
///////////////////////////////////////////////////////////////////////////////
struct ToFrequencyDomainData : public DspBlockData {
  DeclareConcreteX(ToFrequencyDomainData, DspBlockData);
  ToFrequencyDomainData(std::string name = "X", float feedback = 0.0f);
  dspblk_ptr_t createInstance() const override;
  size_t _length = kDEFAULT_SPECTRALSIZE;
};
struct ToFrequencyDomain : public DspBlock {
  using dataclass_t = ToFrequencyDomainData;
  ToFrequencyDomain(const dataclass_t* dbd);
  ~ToFrequencyDomain();
  void compute(DspBuffer& dspbuf) final;
  void doKeyOn(const KeyOnInfo& koi) final;

  const dataclass_t* _mydata;
};
///////////////////////////////////////////////////////////////////////////////
struct SpectralShiftData : public DspBlockData {
  DeclareConcreteX(SpectralShiftData, DspBlockData);
  SpectralShiftData(std::string name = "X", float feedback = 0.0f);
  dspblk_ptr_t createInstance() const override;
  size_t _length = kDEFAULT_SPECTRALSIZE;
};
struct SpectralShift : public DspBlock {
  using dataclass_t = SpectralShiftData;
  SpectralShift(const dataclass_t* dbd);
  ~SpectralShift();
  void compute(DspBuffer& dspbuf) final;
  void doKeyOn(const KeyOnInfo& koi) final;

  const dataclass_t* _mydata;
};
///////////////////////////////////////////////////////////////////////////////
struct SpectralScaleData : public DspBlockData {
  DeclareConcreteX(SpectralScaleData, DspBlockData);
  SpectralScaleData(std::string name = "X", float feedback = 0.0f);
  dspblk_ptr_t createInstance() const override;
  size_t _length = kDEFAULT_SPECTRALSIZE;
};
struct SpectralScale : public DspBlock {
  using dataclass_t = SpectralScaleData;
  SpectralScale(const dataclass_t* dbd);
  ~SpectralScale();
  void compute(DspBuffer& dspbuf) final;
  void doKeyOn(const KeyOnInfo& koi) final;

  const dataclass_t* _mydata;
};
///////////////////////////////////////////////////////////////////////////////

using floatvect_t = std::vector<float>;
struct SpectralImpulseResponse {

  SpectralImpulseResponse(size_t length=kDEFAULT_SPECTRALSIZE);

  SpectralImpulseResponse(
      floatvect_t& impulseL, //
      floatvect_t& impulseR);

  void loadAudioFile(const std::string& path);
  void loadAudioFileX(const std::string& path, size_t truncate = 0);

  void combFilter(
      float frequency, //
      float top);
  void lowShelf(
      float frequency, //
      float gain);
  void highShelf(
      float frequency, //
      float gain);
  void lowRolloff(
      float frequency, //
      float slope);
  void highRolloff(
      float frequency, //
      float slope);

  void parametricEQ4(
      fvec4 frequencies, //
      fvec4 gains,       //
      fvec4 qvals);

  void vowelFormant(char vowel, float strength);
  void violinFormant(float strength);

  void
  set(floatvect_t& impulseL, //
      floatvect_t& impulseR);

  void setX(
      floatvect_t& impulseL, //
      floatvect_t& impulseR);

  void t2f();

  void setFromFrequencyBins(             //
      const floatvect_t& frequencyBinsL, // gainDB per frequency bin (left)
      const floatvect_t& frequencyBinsR, // gainDB per frequency bin (right)
      float samplerate);                 // sample rata of the frequency bin data

  void setFromComplexBins(               //
      const floatvect_t& realBinsL,      // complex real bin (left)
      const floatvect_t& realBinsR,      // complex real bin (right)
      const floatvect_t& imagBinsL,      // complex imaginary bin (left)
      const floatvect_t& imagBinsR);     // complex imaginary bin (right)

  void applyToStream(         //
      floatvect_t& realBinsL, // streamed frequency bin data (left)
      floatvect_t& realBinsR, // streamed frequency bin data (right)
      floatvect_t& imagBinsL, // streamed imaginary bin data (left)
      floatvect_t& imagBinsR, // streamed imaginary bin data (right)
      float mix) const;       // mix amount (0.0 to 1.0)

  void mirror();

  void blend(
      const SpectralImpulseResponse& A, //
      const SpectralImpulseResponse& B, //
      float index);

  floatvect_t _impulseL;
  floatvect_t _impulseR;
  floatvect_t _realL;
  floatvect_t _realR;
  floatvect_t _imagL;
  floatvect_t _imagR;
  svar64_t _impl;
  size_t _length = 0;
};

using spectralimpulseresponse_ptr_t = std::shared_ptr<SpectralImpulseResponse>;

struct SpectralImpulseResponseDataSet {
  std::vector<spectralimpulseresponse_ptr_t> _impulses;
};

using spectralimpulseresponsedataset_ptr_t = std::shared_ptr<SpectralImpulseResponseDataSet>;

struct SpectralConvolveData : public DspBlockData {
  DeclareConcreteX(SpectralConvolveData, DspBlockData);
  SpectralConvolveData(std::string name = "X", float feedback = 0.0f);
  dspblk_ptr_t createInstance() const override;
  spectralimpulseresponsedataset_ptr_t _impulse_dataset;
  size_t _length = kDEFAULT_SPECTRALSIZE;
};

struct SpectralConvolve : public DspBlock {
  using dataclass_t = SpectralConvolveData;
  SpectralConvolve(const dataclass_t* dbd);
  ~SpectralConvolve();
  void compute(DspBuffer& dspbuf) final;
  void doKeyOn(const KeyOnInfo& koi) final;

  const dataclass_t* _mydata;
  floatvect_t _impulseL;
  floatvect_t _impulseR;
  floatvect_t _realL;
  floatvect_t _realR;
  floatvect_t _imagL;
  floatvect_t _imagR;
};

using spectralconvolvedata_ptr_t = std::shared_ptr<SpectralConvolveData>;

///////////////////////////////////////////////////////////////////////////////
struct SpectralConvolveTDData : public DspBlockData {
  DeclareConcreteX(SpectralConvolveTDData, DspBlockData);
  SpectralConvolveTDData(std::string name = "X", float feedback = 0.0f);
  dspblk_ptr_t createInstance() const override;
  spectralimpulseresponsedataset_ptr_t _impulse_dataset;
};
using spectralconvolveTDdata_ptr_t = std::shared_ptr<SpectralConvolveTDData>;
struct SpectralConvolveTD : public DspBlock {
  using dataclass_t = SpectralConvolveTDData;
  SpectralConvolveTD(const dataclass_t* dbd);
  ~SpectralConvolveTD();
  void compute(DspBuffer& dspbuf) final;
  void doKeyOn(const KeyOnInfo& koi) final;

  const dataclass_t* _mydata;
  floatvect_t _inpqL;
  floatvect_t _inpqR;
  floatvect_t _outqL;
  floatvect_t _outqR;
  floatvect_t _impulseL;
  floatvect_t _impulseR;
  fftconvolver::FFTConvolver _convolverL;
  fftconvolver::FFTConvolver _convolverR;
};

using spectralconvolveTDdata_ptr_t = std::shared_ptr<SpectralConvolveTDData>;

///////////////////////////////////////////////////////////////////////////////
struct SpectralTestData : public DspBlockData {
  DeclareConcreteX(SpectralTestData, DspBlockData);
  SpectralTestData(std::string name = "X", float feedback = 0.0f);
  dspblk_ptr_t createInstance() const override;
  size_t _length = kDEFAULT_SPECTRALSIZE;
};
struct SpectralTest : public DspBlock {
  using dataclass_t = SpectralTestData;
  SpectralTest(const dataclass_t* dbd);
  ~SpectralTest();
  void compute(DspBuffer& dspbuf) final;
  void doKeyOn(const KeyOnInfo& koi) final;

  const dataclass_t* _mydata;
};
///////////////////////////////////////////////////////////////////////////////
struct ToTimeDomainData : public DspBlockData {
  DeclareConcreteX(ToTimeDomainData, DspBlockData);
  ToTimeDomainData(std::string name = "X", float feedback = 0.0f);
  dspblk_ptr_t createInstance() const override;
  size_t _length = kDEFAULT_SPECTRALSIZE;
};
struct ToTimeDomain : public DspBlock {
  using dataclass_t = ToTimeDomainData;
  ToTimeDomain(const dataclass_t* dbd);
  ~ToTimeDomain();
  void compute(DspBuffer& dspbuf) final;
  void doKeyOn(const KeyOnInfo& koi) final;

  const dataclass_t* _mydata;
};

///////////////////////////////////////////////////////////////////////////////

struct TimeToFrequencyDomain {
  TimeToFrequencyDomain(size_t length = kDEFAULT_SPECTRALSIZE);
  ~TimeToFrequencyDomain();
  void resize(size_t length);
  bool compute(const float* inp, floatvect_t& real, floatvect_t& imag, int inumframes);
  audiofft::AudioFFT _fft;
  std::vector<float> _input;
  std::vector<float> _window;
  size_t _frames_in = 0;
  size_t _length;
};
struct FrequencyToTimeDomain {
  FrequencyToTimeDomain(size_t length = kDEFAULT_SPECTRALSIZE);
  ~FrequencyToTimeDomain();
  void resize(size_t length);
  void compute(const floatvect_t& real, const floatvect_t& imag, int inumframes);
  audiofft::AudioFFT _fft;           // FFT object for performing IFFT
  std::vector<float> _output;        // Buffer to hold IFFT output
  std::vector<float> _overlapBuffer; // Buffer to hold the overlapped portion from the previous frame
  size_t _frames_out = 0;            // Counter to track output frames, reset after processing each block
  size_t _length;
};

} // namespace ork::audio::singularity
