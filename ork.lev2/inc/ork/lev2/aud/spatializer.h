////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/aud/singularity/synth.h>
#include <ork/lev2/aud/singularity/alg_amp.h>

namespace ork::audio::singularity {

struct Spatializer;
using spatializer_ptr_t = std::shared_ptr<Spatializer>;

///////////////////////////////////////////////////////////////////////////////

struct SpatializerData : public ork::Object {
  DeclareAbstractX(SpatializerData, ork::Object);

public:
  virtual spatializer_ptr_t createInstance() const = 0;
};
using spatializerdata_ptr_t = std::shared_ptr<SpatializerData>;

///////////////////////////////////////////////////////////////////////////////

struct Spatializer {
  virtual ~Spatializer() = default;
  virtual void configureBus(outbus_ptr_t bus) = 0;
  virtual void updateSpatialParams(const fmtx4& listenerMatrix,
                                   const fvec3& emitterPos) = 0;
};

///////////////////////////////////////////////////////////////////////////////

struct PannerSpatializerData : public SpatializerData {
  DeclareConcreteX(PannerSpatializerData, SpatializerData);

public:
  PannerSpatializerData() = default;
  spatializer_ptr_t createInstance() const override;

  // Distance falloff (OpenAL inverse-distance-clamped model)
  float _refDistance  = 1.0f;    // distance at which gain = 1.0
  float _maxDistance  = 100.0f;  // beyond this, gain = _minGainDB
  float _rolloff      = 1.0f;    // attenuation rate multiplier
  float _minGainDB    = -60.0f;  // floor gain in dB

  // Directional / head shadow
  float _headShadowMix = 1.0f;    // 0=no IID filtering, 1=full
  float _iidBaseFreq   = 3000.0f; // LPF floor frequency (rear)
  float _iidMaxFreq    = 8000.0f; // LPF ceiling frequency (front)
};
using pannerspatializerdata_ptr_t = std::shared_ptr<PannerSpatializerData>;

///////////////////////////////////////////////////////////////////////////////

struct PannerSpatializer : public Spatializer {
  void configureBus(outbus_ptr_t bus) override;
  void updateSpatialParams(const fmtx4& listenerMatrix,
                           const fvec3& emitterPos) override;

private:
  dspblkdata_ptr_t _pannerBlock;
};

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::audio::singularity
