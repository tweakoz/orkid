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
// SF2 live encode - the emitter-side send into the one B-format mix point.
//  authored on the SpatializerData (which both ECS sound emitters already hold
//  one of), so a live singularity voice reaches the SoundField with no new
//  component type: absent == the field never sees the voice.
//
//  SETTLED SEMANTICS (JUL29 adjudications - these are law, not preference):
//   _level  is dB, matching every other authored gain in the audio surface
//           (_gainOffsetDB / _masterGainDB / PannerSpatializerData::_minGainDB).
//   _spread is DIRECTIVITY INTERPOLATION, not a decorrelation bank: W is kept
//           whole and the three directional channels are scaled by (1-_spread),
//           so 0 is a point source and 1 is directionless. a true decorrelated
//           omni (per-voice allpass bank) is state, cost and a determinism
//           surface - it belongs to a later tier.
//   ELEVATION is absent by construction: nothing in the live path carries it
//           (PANNER2D has ANGLE and DISTANCE only), so the encode runs with
//           el=0. the math keeps the term (Layer::encodeToSoundField) so the
//           HRTF tier only has to supply el.
//   FALLOFF is the PANNER's OpenAL inverse-distance model and NOT the probe
//           smoothstep - the encode reads the voice's post-panner buffer, so
//           the dry and encoded copies of one voice cannot disagree about
//           distance. the field's own _masterGain is a PROBE-tier weight (the
//           update thread folds it into each probe's mix weight) and does not
//           touch a live send: a send's only level control is _level.
///////////////////////////////////////////////////////////////////////////////

struct SoundFieldSendData final : public ork::Object {
  DeclareConcreteX(SoundFieldSendData, ork::Object);

public:
  SoundFieldSendData() = default;

  float _level  = 0.0f; // dB
  float _spread = 0.0f; // 0=point source, 1=directionless
};
using soundfieldsenddata_ptr_t = std::shared_ptr<SoundFieldSendData>;

///////////////////////////////////////////////////////////////////////////////

struct SpatializerData : public ork::Object {
  DeclareAbstractX(SpatializerData, ork::Object);

public:
  virtual spatializer_ptr_t createInstance() const = 0;

  // null == no live encode. on the ABSTRACT base because every spatializer
  //  type, present and future, can route into the field.
  soundfieldsenddata_ptr_t _soundfieldSend;
};
using spatializerdata_ptr_t = std::shared_ptr<SpatializerData>;

///////////////////////////////////////////////////////////////////////////////
// the ONE place an authored send becomes voice-program state. both ECS sound
//  emitters call it from their program builders (control thread) with the
//  PANNER2D block they just appended; the encode then rides that block's ANGLE
//  data param, which the emitter systems already rewrite every frame for a
//  moving entity. no-op when the spatializer authors no send.
///////////////////////////////////////////////////////////////////////////////

void configureSoundFieldSend(
    lyrdata_ptr_t layer,
    spatializerdata_ptr_t spatializer,
    dspblkdata_ptr_t pannerBlock);

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
