////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

namespace ork::audio::singularity {

///////////////////////////////////////////////////////////////////////////////

struct DspBuffer final {
  DspBuffer();
  void resize(int inumframes);

  float* channel(int ich);

  // Helper methods for insert processing
  void clear(int base, int count);                              // Zero specified range
  void copyFrom(const DspBuffer& src, int base, int count);     // Copy from source
  void mixIn(const DspBuffer& src, int base, int count, float gain); // Accumulate with gain

  int _maxframes;
  int _numframes;

  size_t _spectrum_size = 0;
  bool _didFFT = false;
  std::vector<float> _real;
  std::vector<float> _imag;

  std::vector<float> _channels[kmaxdspblocksperstage];
};


} //namespace ork::audio::singularity {
