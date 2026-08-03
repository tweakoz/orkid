// ==================================================================================
// Copyright (c) 2017 HiFi-LoFi
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is furnished
// to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
// FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
// COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
// IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
// ==================================================================================

#ifndef _FFTCONVOLVER_FFTCONVOLVER_H
#define _FFTCONVOLVER_FFTCONVOLVER_H

#include "fft.h"
#include "fft_utilities.h"
#include <vector>

namespace ork::audio::singularity {

namespace fftconvolver
{ 

/**
* @class FFTConvolver
* @brief Implementation of a partitioned FFT convolution algorithm with uniform block size
*
* Some notes on how to use it:
*
* - After initialization with an impulse response, subsequent data portions of
*   arbitrary length can be convolved. The convolver internally can handle
*   this by using appropriate buffering.
*
* - The convolver works without "latency" (except for the required
*   processing time, of course), i.e. the output always is the convolved
*   input for each processing call.
*
* - The convolver is suitable for real-time processing which means that no
*   "unpredictable" operations like allocations, locking, API calls, etc. are
*   performed during processing (all necessary allocations and preparations take
*   place during initialization).
*/
class FFTConvolver
{  
public:
  FFTConvolver();  
  virtual ~FFTConvolver();
  
  /**
  * @brief Initializes the convolver
  * @param blockSize Block size internally used by the convolver (partition size)
  * @param ir The impulse response
  * @param irLen Length of the impulse response
  * @return true: Success - false: Failed
  */
  bool init(size_t blockSize, const Sample* ir, size_t irLen);

  /**
  * @brief Convolves the the given input samples and immediately outputs the result
  * @param input The input samples
  * @param output The convolution result
  * @param len Number of input/output samples
  */
  void process(const Sample* input, Sample* output, size_t len);

  /**
  * @brief Resets the convolver and discards the set impulse response
  */
  void reset();
  
private:
  size_t _blockSize;
  size_t _segSize;
  size_t _segCount;
  size_t _fftComplexSize;
  std::vector<SplitComplex*> _segments;
  std::vector<SplitComplex*> _segmentsIR;
  SampleBuffer _fftBuffer;
  audiofft::AudioFFT _fft;

  std::vector<splitcomplex_ptr_t> _conQ;
  SplitComplex _preMultiplied;
  SplitComplex _conv;
  SampleBuffer _overlap;
  size_t _current;
  SampleBuffer _inputBuffer;
  size_t _inputBufferFill;

  // Prevent uncontrolled usage
  FFTConvolver(const FFTConvolver&);
  FFTConvolver& operator=(const FFTConvolver&);
};

/**
* @class TwoStageFFTConvolver
* @brief FFT convolver using two different block sizes
*
* The 2-stage convolver consists internally of two convolvers:
*
* - A head convolver, which processes the only the begin of the impulse response.
*
* - A tail convolver, which processes the rest and major amount of the impulse response.
*
* Using a short block size for the head convolver and a long block size for
* the tail convolver results in much less CPU usage, while keeping the
* calculation time of each processing call short.
*
* Furthermore, this convolver class provides virtual methods which provide the
* possibility to move the tail convolution into the background (e.g. by using
* multithreading, see startBackgroundProcessing()/waitForBackgroundProcessing()).
*
* As well as the basic FFTConvolver class, the 2-stage convolver is suitable
* for real-time processing which means that no "unpredictable" operations like
* allocations, locking, API calls, etc. are performed during processing (all
* necessary allocations and preparations take place during initialization).
*/
class TwoStageFFTConvolver
{
public:
  TwoStageFFTConvolver();
  virtual ~TwoStageFFTConvolver();

  /**
  * @brief Initialization of the convolver
  * @param headBlockSize The head block size
  * @param tailBlockSize the tail block size
  * @param ir The impulse response
  * @param irLen Length of the impulse response in samples
  * @return true: Success - false: Failed
  */
  bool init(size_t headBlockSize, size_t tailBlockSize, const Sample* ir, size_t irLen);

  /**
  * @brief Convolves the the given input samples and immediately outputs the result
  * @param input The input samples
  * @param output The convolution result
  * @param len Number of input/output samples
  */
  void process(const Sample* input, Sample* output, size_t len);

  /**
  * @brief Resets the convolver and discards the set impulse response
  */
  void reset();

protected:
  /**
  * @brief Method to start the background processing of the tail convolution
  *
  * The default implementation just performs the tail convolution synchronously,
  * on the calling thread. Overwrite this method (together with
  * waitForBackgroundProcessing()) to move the tail convolution to a background
  * thread.
  */
  virtual void startBackgroundProcessing();

  /**
  * @brief Method to wait for the background processing of the tail convolution
  *
  * The default implementation does nothing (as the default
  * startBackgroundProcessing() performs the tail convolution synchronously).
  */
  virtual void waitForBackgroundProcessing();

  /**
  * @brief Performs the tail convolution (to be called by startBackgroundProcessing())
  */
  void doBackgroundProcessing();

private:
  size_t _headBlockSize;
  size_t _tailBlockSize;
  FFTConvolver _headConvolver;
  FFTConvolver _tailConvolver0;
  SampleBuffer _tailOutput0;
  SampleBuffer _tailPrecalculated0;
  FFTConvolver _tailConvolver;
  SampleBuffer _tailOutput;
  SampleBuffer _tailPrecalculated;
  SampleBuffer _tailInput;
  SampleBuffer _backgroundProcessingInput;
  size_t _tailInputFill;
  size_t _precalculatedPos;

  // Prevent uncontrolled usage
  TwoStageFFTConvolver(const TwoStageFFTConvolver&);
  TwoStageFFTConvolver& operator=(const TwoStageFFTConvolver&);
};

} // End of namespace fftconvolver
} // End of namespace ork::audio::singularity

#endif // Header guard
