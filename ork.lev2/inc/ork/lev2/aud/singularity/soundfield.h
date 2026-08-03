////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include "krztypes.h"
#include <ork/math/cvector3.h>
#include <ork/math/cmatrix4.h>
#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace ork::audio::singularity {

///////////////////////////////////////////////////////////////////////////////
// AMBISONIC CONVENTIONS (JUL25 soundfield spec §3) - internal, non-negotiable:
//  FOA, ACN channel order (0=W 1=Y 2=Z 3=X), SN3D normalization, 48kHz, float.
//  ambisonic axes are X=front, Y=LEFT, Z=up; azimuth is measured from front
//  toward +Y (so +90deg is the listener's left), elevation up from horizontal.
//  a plane wave of amplitude s from (az,el) encodes as
//      W = s/sqrt(2), X = s*cos(az)*cos(el), Y = s*sin(az)*cos(el), Z = s*sin(el)
//
//  the ENGINE's listener space is X=right, Y=up, Z=BACK (the view matrix comes
//  from glm::lookAtRH - cmatrix4.hpp:1131 - so the camera looks down -Z, and
//  +X is the listener's right: PANNER2D turns -atan2(x,z) into a pan whose x
//  component is +x/|r| -> hard right, panner.cpp:146..175).
//  the two frames therefore differ by the fixed permutation
//      ambi(v) = ( -v.z, -v.x, v.y )
//  which is applied in both directions when the listener rotation is folded
//  into the field (SoundField::_publishRotation).
///////////////////////////////////////////////////////////////////////////////

enum class FoaChannel : int { W = 0, Y = 1, Z = 2, X = 3 };

///////////////////////////////////////////////////////////////////////////////
// FOA -> stereo decoder. an INTERFACE from day one: the A4 HRTF tier lands as
//  another implementation of exactly this and nothing above it may assume
//  Gerzon. decode() runs on the audio thread - it may not allocate or lock.
///////////////////////////////////////////////////////////////////////////////

struct FoaDecoder {
  virtual ~FoaDecoder() = default;
  // foa: kfoanumchannels planar pointers (ACN/SN3D), count frames each.
  // outL/outR are OVERWRITTEN, not accumulated.
  virtual void decode(const float* const* foa, float* outL, float* outR, int count) = 0;
  virtual void reset() = 0;
};

///////////////////////////////////////////////////////////////////////////////
// Gerzon dual-band psychoacoustic decode to a stereo pair of coincident
//  cardioids aimed at +/-45deg azimuth:
//      L = 0.5*( sqrt(2)*W + D(cos(+45)*X + sin(+45)*Y) )
//      R = 0.5*( sqrt(2)*W + D(cos(-45)*X + sin(-45)*Y) )
//  D is the dual-band shelf on the DIRECTIONAL sum only (W is common to both
//  bands, so it is bit-transparent and needs no filter): the low band keeps
//  full directivity (velocity/Makita decode, correct interaural time cues
//  below the crossover) while the high band is scaled by the 2D first-order
//  max-rE factor cos(pi/4)=0.7071 (energy decode). the split is a complementary
//  one-pole at kGerzonCrossoverHz: hf == x - lf EXACTLY, so the two bands sum
//  back to unity gain when the shelf is flat.
//  Z (elevation) is DISCARDED - a horizontal 2-speaker decode has nowhere to
//  put it; elevation becomes audible at the A4 HRTF tier.
//
//  the resulting steady-state ratios for a horizontal plane wave at azimuth az
//  (low band, D=1) are  L/R = (1+cos(az-45)) / (1+cos(az+45)) :
//      az    0 -> 1.000      az  +45 -> 2.000      az  +90 -> 5.828
//                            az  -45 -> 0.500      az  -90 -> 0.1716
///////////////////////////////////////////////////////////////////////////////

static constexpr float kGerzonCrossoverHz = 700.0f;

struct GerzonStereoDecoder final : public FoaDecoder {
  GerzonStereoDecoder();
  void decode(const float* const* foa, float* outL, float* outR, int count) final;
  void reset() final;

  float _lfDirectionalGain = 1.0f;
  float _hfDirectionalGain = 0.70710678f; // 2D first-order max-rE
  float _alpha;                           // one-pole coefficient at the crossover
  float _lpL = 0.0f;
  float _lpR = 0.0f;
};

///////////////////////////////////////////////////////////////////////////////
// per-probe authored parameters (RADIAL mode; ZONE arrives at SF3).
///////////////////////////////////////////////////////////////////////////////

struct SoundFieldProbeParams {
  fvec3 _position;
  float _refDistance = 5.0f;  // full weight at or inside this radius
  float _maxDistance = 50.0f; // zero weight at or beyond it
  float _rolloff     = 1.0f;  // exponent on the smoothstep falloff
  float _gain        = 1.0f;  // authored linear gain
};

///////////////////////////////////////////////////////////////////////////////
// update-thread-owned probe record. never touched by the audio thread.
///////////////////////////////////////////////////////////////////////////////

struct SoundFieldProbe {
  int _id = -1;
  std::string _path;
  bool _loop = true;
  SoundFieldProbeParams _params;
  float _rawWeight    = 0.0f; // this frame's falloff solve
  float _slewWeight   = 0.0f; // after the hysteresis slew
  float _mixWeight    = 0.0f; // after tier normalization
  int _slot           = -1;   // stream slot, or -1 when parked
};

///////////////////////////////////////////////////////////////////////////////
// a probe's libsndfile handle + feeder scratch. defined in soundfield.cpp so
//  sndfile.h stays out of every translation unit that sees a synth.
///////////////////////////////////////////////////////////////////////////////

struct ProbeStreamFile;

///////////////////////////////////////////////////////////////////////////////
// one streaming slot. the ring is allocated ONCE (at SoundField construction)
//  and is a single-producer/single-consumer window: the feeder thread owns
//  _wframes, the audio thread owns _rframes, and neither ever takes a lock.
//
//  slot lifecycle (each transition has exactly one owner):
//     FREE      -- update  --> LOADING     (path/loop written before the store)
//     LOADING   -- feeder  --> READY       (file open + ring primed)
//     READY     -- update  --> PARKING     (dropped out of the nearest-K set)
//     PARKING   -- audio   --> RELEASING   (its ramped gain reached zero)
//     RELEASING -- feeder  --> FREE        (file closed, counters reset)
//  a park racing the feeder's prime is why LOADING->READY is a CAS.
///////////////////////////////////////////////////////////////////////////////

enum class ProbeSlotState : int {
  FREE      = 0,
  LOADING   = 1,
  READY     = 2,
  PARKING   = 3,
  RELEASING = 4,
};

struct ProbeStreamSlot {
  // 32768 frames == 0.68s at 48k; 512KB per slot, 4MB for the whole cap.
  static constexpr size_t kRingFrames = 1 << 15;
  // the update thread will not let a slot go audible until the ring holds this
  //  much, which is what makes an offline render byte-deterministic: a probe's
  //  first audible sample never depends on how far the feeder happened to get.
  static constexpr size_t kPrimeFrames = kRingFrames / 2;

  ProbeStreamSlot();
  ~ProbeStreamSlot();

  std::atomic<int> _state{int(ProbeSlotState::FREE)};
  std::atomic<uint64_t> _wframes{0}; // feeder-owned write cursor
  std::atomic<uint64_t> _rframes{0}; // audio-owned read cursor
  std::atomic<float> _targetGain{0.0f};
  std::atomic<int> _underruns{0};

  float _curGain = 0.0f;      // audio thread only
  std::vector<float> _ring;   // kRingFrames * kfoanumchannels, interleaved
  std::string _path;          // written by update while FREE, read by feeder
  bool _loop = true;
  std::unique_ptr<ProbeStreamFile> _file; // feeder thread only
};

///////////////////////////////////////////////////////////////////////////////
// SoundField: the one B-format mix point.
//   probes (weighted, summed in B-format) -> listener inverse rotation
//   -> FoaDecoder -> the dedicated "soundfield" OutputBus.
//
//  it does not exist until something asks for it: scenes with no probes never
//  create the bus, so their renders stay byte-identical.
///////////////////////////////////////////////////////////////////////////////

static constexpr const char* kSoundFieldBusName = "soundfield";

struct SoundField {

  SoundField();
  ~SoundField();

  // off-RT owner. first call creates the field, its bus and its feeder thread.
  static soundfield_ptr_t instance();
  // audio-thread view: null until instance() has published, null again after
  //  tearDown(). a plain atomic load - the audio thread never takes the mutex.
  static SoundField* rtInstance();
  static void tearDown();

  /////////////////////////////////////////////////////////////////////
  // update thread
  /////////////////////////////////////////////////////////////////////

  // fail-loud: throws if the file is missing or is not 4-channel.
  int createProbe(const std::string& ambixPath, bool loop);
  void destroyProbe(int probeID);
  void setProbeParams(int probeID, const SoundFieldProbeParams& params);
  // weight solve + hysteresis + tier normalization + slot scheduling.
  //  reads the listener straight off synth::_listener_matrix.
  void update(float dt);

  float probeWeight(int probeID) const;
  int probeSlot(int probeID) const;
  size_t numProbes() const;
  int numActiveSlots() const;
  int underrunCount() const;

  float _masterGain = 1.0f;
  float _slewTau    = 0.25f; // ~250ms hysteresis on every probe weight

  /////////////////////////////////////////////////////////////////////
  // audio thread
  /////////////////////////////////////////////////////////////////////

  void computeIntoBus(int base, int count);

  // SF2 live encode: a voice's mono sum, weighted by its four encode gains,
  //  summed into the live B-format accumulator. ramps g0 -> g1 across the pass
  //  so a moving source's per-pass gain update cannot zipper. called from the
  //  serial per-voice send pass, so it is on the audio thread and strictly
  //  before this pass's computeIntoBus - arithmetic only, no allocation, no
  //  lock, and (unlike the probes) no listener rotation: the azimuth it was
  //  given is already listener-relative.
  void accumulateLive(
      const float* srcL,
      const float* srcR,
      int count,
      const float (&g0)[kfoanumchannels],
      const float (&g1)[kfoanumchannels]);

  foadecoder_ptr_t _decoder;

private:
  void _feederLoop();
  void _publishRotation();
  void _scheduleSlots();
  void _waitForPrime();
  SoundFieldProbe* _findProbe(int probeID);
  const SoundFieldProbe* _findProbe(int probeID) const;

  outbus_ptr_t _bus;
  OutputBus* _busRT = nullptr; // audio-thread view of the same bus

  std::vector<SoundFieldProbe> _probes; // update thread only
  int _nextProbeID = 1;

  ProbeStreamSlot _slots[kmaxActiveProbes];

  // listener rotation, published double-buffered: the writer fills the
  //  inactive 3x3 then flips the index. the audio thread copies the published
  //  nine floats once per control pass. NOT a seqlock (that upgrade rides A4);
  //  a lock here would be an RT lock, which is forbidden.
  float _rot[2][9];
  std::atomic<int> _rotIndex{0};

  // audio-thread scratch, sized to one control pass and never reallocated.
  float _accum[kfoanumchannels][frames_per_controlpass];
  // the live-encode accumulator: a SECOND persistent scratch rather than a
  //  begin/end split of computeIntoBus, so no caller has to pair two calls.
  //  zeroed by the pass's first accumulateLive and consumed (flag cleared) by
  //  computeIntoBus. audio thread only.
  float _liveAccum[kfoanumchannels][frames_per_controlpass];
  bool _liveActive = false;
  float _decodeL[frames_per_controlpass];
  float _decodeR[frames_per_controlpass];

  std::thread _feeder;
  std::mutex _feedmtx;
  std::condition_variable _feedcv;
  std::atomic<bool> _feedrun{false};
};

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::audio::singularity
