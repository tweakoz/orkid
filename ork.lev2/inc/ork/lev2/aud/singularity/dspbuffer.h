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

  int _maxframes;
  int _numframes;

  size_t _spectrum_size = 0;
  bool _didFFT = false;
  std::vector<float> _real;
  std::vector<float> _imag;

  std::vector<float> _channels[kmaxdspblocksperstage];
};


} //namespace ork::audio::singularity {
