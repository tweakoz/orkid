////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/math/cvector2.h>

// 2D Perlin Noise
namespace ork::math {

///////////////////////////////////////////////////////////////////////////////

class NoiseCache2D {
private:
  const u32 mkSamplesPerSide; // Must be a power of two
  float* mCache;

  float SmoothT(float t);
  float& CacheAt(u32 x, u32 y);
  float AverageAt(u32 x, u32 y);
  float CosInterpAt(float x, float y);

public:
  NoiseCache2D(int seed = 0, u32 samplesPerSide = 64);
  void SmoothCache();
  ~NoiseCache2D();
  float ValueAt(const fvec2& pos, const fvec2& offset, float amplitude, float frequency);
};

///////////////////////////////////////////////////////////////////////////////

class PerlinNoiseGenerator {
private:
  struct Octave {
    NoiseCache2D* mNoise;
    float mFrequency;
    float mAmplitude;
    fvec2 mOffset;
  };

  orklist<Octave> mOctaves;

public:
  PerlinNoiseGenerator();
  ~PerlinNoiseGenerator();

  void AddOctave(float frequency, float amplitude, const fvec2& offset, int seed, u32 dimensions);

  float ValueAt(const fvec2& pos);
};

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::math
