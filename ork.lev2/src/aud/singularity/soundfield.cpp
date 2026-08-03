////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <sndfile.h>

#include <ork/lev2/aud/singularity/soundfield.h>
#include <ork/lev2/aud/singularity/synth.h>
#include <ork/file/path.h>
#include <ork/kernel/string/string.h>
#include <ork/util/logger.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace ork::audio::singularity {

static logchannel_ptr_t logchan_sfield = logger()->configureChannel("SoundField", fvec3(0.4, 0.9, 1), true);

///////////////////////////////////////////////////////////////////////////////
// the field is reachable from the audio thread through a plain atomic; the
//  shared_ptr and its mutex belong to the off-RT side only.
///////////////////////////////////////////////////////////////////////////////

static std::mutex g_sfield_mutex;
static soundfield_ptr_t g_sfield;
static std::atomic<SoundField*> g_sfield_rt{nullptr};

///////////////////////////////////////////////////////////////////////////////
// GerzonStereoDecoder
///////////////////////////////////////////////////////////////////////////////

GerzonStereoDecoder::GerzonStereoDecoder() {
  _alpha = 1.0f - expf(-pi2 * kGerzonCrossoverHz * getInverseSampleRate());
}

void GerzonStereoDecoder::reset() {
  _lpL = 0.0f;
  _lpR = 0.0f;
}

void GerzonStereoDecoder::decode(const float* const* foa, float* outL, float* outR, int count) {
  const float* chW = foa[int(FoaChannel::W)];
  const float* chY = foa[int(FoaChannel::Y)];
  const float* chX = foa[int(FoaChannel::X)];
  // cos(+/-45) == sin(+45) == -sin(-45)
  constexpr float k = 0.70710678f;
  for (int i = 0; i < count; i++) {
    float w    = chW[i] * float(sqrt2);
    float dirL = k * (chX[i] + chY[i]);
    float dirR = k * (chX[i] - chY[i]);
    _lpL += (dirL - _lpL) * _alpha;
    _lpR += (dirR - _lpR) * _alpha;
    float shelfL = _lfDirectionalGain * _lpL + _hfDirectionalGain * (dirL - _lpL);
    float shelfR = _lfDirectionalGain * _lpR + _hfDirectionalGain * (dirR - _lpR);
    outL[i]      = 0.5f * (w + shelfL);
    outR[i]      = 0.5f * (w + shelfR);
  }
}

///////////////////////////////////////////////////////////////////////////////
// ProbeStreamFile / ProbeStreamSlot
///////////////////////////////////////////////////////////////////////////////

struct ProbeStreamFile {
  ~ProbeStreamFile() {
    if (_sf)
      sf_close(_sf);
  }
  SNDFILE* _sf = nullptr;
};

ProbeStreamSlot::ProbeStreamSlot() {
  _ring.resize(kRingFrames * kfoanumchannels, 0.0f);
}

ProbeStreamSlot::~ProbeStreamSlot() {
}

///////////////////////////////////////////////////////////////////////////////
// SoundField lifecycle
///////////////////////////////////////////////////////////////////////////////

SoundField::SoundField() {
  auto syn = synth::instance();
  _bus     = syn->outputBus(kSoundFieldBusName);
  if (not _bus) {
    _bus = syn->createOutputBus(kSoundFieldBusName);
  }
  _busRT   = _bus.get();
  _decoder = std::make_shared<GerzonStereoDecoder>();

  for (int i = 0; i < 2; i++) {
    for (int j = 0; j < 9; j++) {
      _rot[i][j] = (j % 4) == 0 ? 1.0f : 0.0f;
    }
  }
  memset(_accum, 0, sizeof(_accum));
  memset(_liveAccum, 0, sizeof(_liveAccum));
  memset(_decodeL, 0, sizeof(_decodeL));
  memset(_decodeR, 0, sizeof(_decodeR));

  _feedrun.store(true);
  _feeder = std::thread([this]() { this->_feederLoop(); });
}

SoundField::~SoundField() {
  _feedrun.store(false);
  _feedcv.notify_all();
  if (_feeder.joinable())
    _feeder.join();
}

soundfield_ptr_t SoundField::instance() {
  std::lock_guard<std::mutex> lock(g_sfield_mutex);
  if (not g_sfield) {
    g_sfield = std::make_shared<SoundField>();
    g_sfield_rt.store(g_sfield.get(), std::memory_order_release);
  }
  return g_sfield;
}

SoundField* SoundField::rtInstance() {
  return g_sfield_rt.load(std::memory_order_acquire);
}

void SoundField::tearDown() {
  // the last reference is held here so ~SoundField (which JOINS the feeder
  //  thread) runs after the mutex is released.
  soundfield_ptr_t doomed;
  {
    std::lock_guard<std::mutex> lock(g_sfield_mutex);
    // unpublish BEFORE dropping the reference: the audio thread must not be
    //  able to reach a field whose feeder thread is being joined.
    g_sfield_rt.store(nullptr, std::memory_order_release);
    doomed = g_sfield;
    g_sfield.reset();
  }
}

///////////////////////////////////////////////////////////////////////////////
// probe registry (update thread)
///////////////////////////////////////////////////////////////////////////////

SoundFieldProbe* SoundField::_findProbe(int probeID) {
  for (auto& p : _probes) {
    if (p._id == probeID)
      return &p;
  }
  return nullptr;
}

const SoundFieldProbe* SoundField::_findProbe(int probeID) const {
  for (auto& p : _probes) {
    if (p._id == probeID)
      return &p;
  }
  return nullptr;
}

int SoundField::createProbe(const std::string& ambixPath, bool loop) {
  auto abspath = file::Path::expandPathString(ambixPath);

  // validate up front so a bad asset fails at authoring time, on the thread
  //  that authored it - never as silence on the audio thread.
  SF_INFO sfinfo;
  memset(&sfinfo, 0, sizeof(sfinfo));
  SNDFILE* sf = sf_open(abspath.c_str(), SFM_READ, &sfinfo);
  if (nullptr == sf) {
    auto msg = FormatString("SoundField: cannot open ambix probe asset <%s> : %s", abspath.c_str(), sf_strerror(nullptr));
    logchan_sfield->log("%s", msg.c_str());
    throw std::runtime_error(msg);
  }
  int channels    = sfinfo.channels;
  int rate        = sfinfo.samplerate;
  int64_t nframes = int64_t(sfinfo.frames);
  sf_close(sf);

  // AmbiX WAV only this slice - FuMa/HOA conversion and resample-at-import
  //  are SF5. anything else is an authoring error, not a runtime condition.
  if (channels != kfoanumchannels) {
    auto msg = FormatString(
        "SoundField: ambix probe asset <%s> has %d channels, FOA AmbiX requires exactly %d",
        abspath.c_str(),
        channels,
        kfoanumchannels);
    logchan_sfield->log("%s", msg.c_str());
    throw std::runtime_error(msg);
  }
  // a zero-length loop would spin the feeder forever on seek-and-retry.
  if (nframes <= 0) {
    auto msg = FormatString("SoundField: ambix probe asset <%s> is empty", abspath.c_str());
    logchan_sfield->log("%s", msg.c_str());
    throw std::runtime_error(msg);
  }
  if (rate != int(getSampleRate())) {
    auto msg = FormatString(
        "SoundField: ambix probe asset <%s> is %dHz, the field runs at %dHz (resample-at-import is SF5)",
        abspath.c_str(),
        rate,
        int(getSampleRate()));
    logchan_sfield->log("%s", msg.c_str());
    throw std::runtime_error(msg);
  }

  SoundFieldProbe probe;
  probe._id   = _nextProbeID++;
  probe._path = abspath;
  probe._loop = loop;
  _probes.push_back(probe);
  return probe._id;
}

void SoundField::destroyProbe(int probeID) {
  for (size_t i = 0; i < _probes.size(); i++) {
    if (_probes[i]._id != probeID)
      continue;
    int slot = _probes[i]._slot;
    if (slot >= 0) {
      _slots[slot]._targetGain.store(0.0f);
      _slots[slot]._state.store(int(ProbeSlotState::PARKING));
    }
    _probes.erase(_probes.begin() + i);
    return;
  }
}

void SoundField::setProbeParams(int probeID, const SoundFieldProbeParams& params) {
  auto probe = _findProbe(probeID);
  if (nullptr == probe) {
    throw std::runtime_error(FormatString("SoundField: setProbeParams on unknown probe id<%d>", probeID));
  }
  probe->_params = params;
}

float SoundField::probeWeight(int probeID) const {
  auto probe = _findProbe(probeID);
  return probe ? probe->_mixWeight : 0.0f;
}

int SoundField::probeSlot(int probeID) const {
  auto probe = _findProbe(probeID);
  return probe ? probe->_slot : -1;
}

size_t SoundField::numProbes() const {
  return _probes.size();
}

int SoundField::numActiveSlots() const {
  int count = 0;
  for (int i = 0; i < kmaxActiveProbes; i++) {
    int st = _slots[i]._state.load();
    if (st == int(ProbeSlotState::LOADING) or st == int(ProbeSlotState::READY))
      count++;
  }
  return count;
}

int SoundField::underrunCount() const {
  int count = 0;
  for (int i = 0; i < kmaxActiveProbes; i++)
    count += _slots[i]._underruns.load();
  return count;
}

///////////////////////////////////////////////////////////////////////////////
// listener rotation
///////////////////////////////////////////////////////////////////////////////

void SoundField::_publishRotation() {
  auto syn = synth::instance();
  // world -> listener, engine axes (X=right Y=up Z=back).
  fmtx4 w2l = syn->_inv_listener_matrix;

  // columns of the ambisonic rotation are the images of the ambisonic basis
  //  vectors: take each ambisonic axis into engine space, rotate it, take it
  //  back. done this way rather than as a precomputed product so the two
  //  permutations stay legible and cannot drift apart.
  //  ambi(v) = (-v.z, -v.x, v.y);  ambi^-1(a) = (-a.y, a.z, -a.x)
  static const fvec3 kAmbiBasisInEngine[3] = {
      fvec3(0, 0, -1), // ambisonic X (front)
      fvec3(-1, 0, 0), // ambisonic Y (left)
      fvec3(0, 1, 0),  // ambisonic Z (up)
  };

  int next = 1 - _rotIndex.load(std::memory_order_relaxed);
  for (int j = 0; j < 3; j++) {
    auto v4 = fvec4(kAmbiBasisInEngine[j], 0.0f).transform(w2l);
    fvec3 v(v4.x, v4.y, v4.z);
    fvec3 a(-v.z, -v.x, v.y);
    _rot[next][0 * 3 + j] = a.x;
    _rot[next][1 * 3 + j] = a.y;
    _rot[next][2 * 3 + j] = a.z;
  }
  _rotIndex.store(next, std::memory_order_release);
}

///////////////////////////////////////////////////////////////////////////////
// weight solve + slot scheduling (update thread)
///////////////////////////////////////////////////////////////////////////////

void SoundField::_scheduleSlots() {
  // rank by mix weight; the nearest-K (by weight) hold the streaming slots and
  //  everyone else fades to zero and parks. the 250ms slew below is what keeps
  //  a probe hovering at the K/K+1 boundary from thrashing its slot.
  std::vector<int> order;
  order.reserve(_probes.size());
  for (size_t i = 0; i < _probes.size(); i++)
    order.push_back(int(i));
  std::sort(order.begin(), order.end(), [this](int a, int b) {
    if (_probes[a]._mixWeight != _probes[b]._mixWeight)
      return _probes[a]._mixWeight > _probes[b]._mixWeight;
    return _probes[a]._id < _probes[b]._id; // deterministic tiebreak
  });

  int admitted = 0;
  std::vector<int> wants;
  for (int idx : order) {
    auto& probe = _probes[idx];
    bool want   = (probe._mixWeight > 0.0f) and (admitted < kmaxActiveProbes);
    if (want) {
      admitted++;
      wants.push_back(idx);
    } else if (probe._slot >= 0) {
      auto& slot = _slots[probe._slot];
      slot._targetGain.store(0.0f);
      int st = slot._state.load();
      if (st == int(ProbeSlotState::LOADING) or st == int(ProbeSlotState::READY))
        slot._state.store(int(ProbeSlotState::PARKING));
      probe._slot = -1;
    }
  }

  for (int idx : wants) {
    auto& probe = _probes[idx];
    if (probe._slot < 0) {
      for (int s = 0; s < kmaxActiveProbes; s++) {
        if (_slots[s]._state.load() != int(ProbeSlotState::FREE))
          continue;
        auto& slot = _slots[s];
        slot._path = probe._path;
        slot._loop = probe._loop;
        slot._wframes.store(0);
        slot._rframes.store(0);
        slot._targetGain.store(0.0f);
        slot._curGain = 0.0f;
        slot._state.store(int(ProbeSlotState::LOADING), std::memory_order_release);
        probe._slot = s;
        break;
      }
    }
    if (probe._slot >= 0)
      _slots[probe._slot]._targetGain.store(probe._mixWeight);
  }
}

void SoundField::_waitForPrime() {
  // bounded: a probe that never primes must not wedge the update thread. it
  //  only ever waits when a probe STARTS - a running slot is topped up by the
  //  feeder far ahead of the audio thread.
  using clock_t_ = std::chrono::steady_clock;
  auto deadline  = clock_t_::now() + std::chrono::milliseconds(1000);
  while (true) {
    bool pending = false;
    for (int s = 0; s < kmaxActiveProbes; s++) {
      auto& slot = _slots[s];
      if (slot._state.load() != int(ProbeSlotState::LOADING))
        continue;
      pending = true;
    }
    if (not pending)
      return;
    if (clock_t_::now() >= deadline) {
      logchan_sfield->log("SoundField: probe prime timed out - the field will start with a gap");
      return;
    }
    _feedcv.notify_all();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
}

void SoundField::update(float dt) {

  _publishRotation();

  auto syn         = synth::instance();
  fvec3 listenerPos = syn->_listener_matrix.translation();

  // radial falloff: full inside refDistance, smoothstep out to maxDistance.
  for (auto& probe : _probes) {
    auto& params = probe._params;
    float d      = (params._position - listenerPos).magnitude();
    float raw    = 0.0f;
    if (d <= params._refDistance) {
      raw = 1.0f;
    } else if (d < params._maxDistance and params._maxDistance > params._refDistance) {
      float t = (d - params._refDistance) / (params._maxDistance - params._refDistance);
      float s = 1.0f - (t * t * (3.0f - 2.0f * t));
      raw     = powf(s, params._rolloff);
    }
    probe._rawWeight = raw;
    // ~250ms exponential hysteresis so walking a falloff edge never zippers.
    float k = (_slewTau > 0.0f) ? (1.0f - expf(-dt / _slewTau)) : 1.0f;
    probe._slewWeight += (raw - probe._slewWeight) * k;
  }

  // one tier this slice: normalize only when the sum exceeds unity (never
  //  boost quiet solitude). priority tiers + ducking are SF3.
  float sum = 0.0f;
  for (auto& probe : _probes)
    sum += probe._slewWeight;
  float norm = (sum > 1.0f) ? (1.0f / sum) : 1.0f;

  for (auto& probe : _probes)
    probe._mixWeight = probe._slewWeight * norm * probe._params._gain * _masterGain;

  _scheduleSlots();
  _feedcv.notify_all();
  _waitForPrime();
}

///////////////////////////////////////////////////////////////////////////////
// feeder thread — all disk IO and libsndfile decode happens HERE.
///////////////////////////////////////////////////////////////////////////////

void SoundField::_feederLoop() {

  static constexpr size_t kChunkFrames = 2048;
  std::vector<float> chunk(kChunkFrames * kfoanumchannels, 0.0f);

  while (_feedrun.load()) {

    bool didwork = false;

    for (int s = 0; s < kmaxActiveProbes; s++) {
      auto& slot = _slots[s];
      int st     = slot._state.load(std::memory_order_acquire);

      if (st == int(ProbeSlotState::RELEASING)) {
        slot._file.reset();
        slot._wframes.store(0);
        slot._rframes.store(0);
        slot._path.clear();
        slot._state.store(int(ProbeSlotState::FREE), std::memory_order_release);
        didwork = true;
        continue;
      }

      if (st == int(ProbeSlotState::FREE))
        continue;

      if (nullptr == slot._file) {
        SF_INFO sfinfo;
        memset(&sfinfo, 0, sizeof(sfinfo));
        SNDFILE* sf = sf_open(slot._path.c_str(), SFM_READ, &sfinfo);
        if (nullptr == sf) {
          // createProbe already proved this file openable; losing it mid-run
          //  is a real failure, not something to paper over with silence.
          logchan_sfield->log("SoundField: feeder cannot open <%s> : %s", slot._path.c_str(), sf_strerror(nullptr));
          slot._state.store(int(ProbeSlotState::RELEASING));
          continue;
        }
        auto file  = std::make_unique<ProbeStreamFile>();
        file->_sf  = sf;
        slot._file = std::move(file);
        didwork       = true;
      }

      // top the ring up to capacity; the audio thread only ever drains it.
      uint64_t w = slot._wframes.load(std::memory_order_relaxed);
      uint64_t r = slot._rframes.load(std::memory_order_acquire);
      while ((w - r) < ProbeStreamSlot::kRingFrames) {
        size_t space = size_t(ProbeStreamSlot::kRingFrames - (w - r));
        size_t want  = std::min(space, kChunkFrames);
        size_t got   = 0;
        int rewinds  = 0;
        while (got < want) {
          sf_count_t n = sf_readf_float(slot._file->_sf, chunk.data() + got * kfoanumchannels, sf_count_t(want - got));
          if (n <= 0) {
            // bounded per unproductive stretch: a loop shorter than the chunk
            //  rewinds many times legitimately (the counter resets on every
            //  successful read), but a file truncated to nothing under a live
            //  loop must not spin the feeder on seek-and-retry.
            if (slot._loop and rewinds++ < 2) {
              sf_seek(slot._file->_sf, 0, SEEK_SET);
              continue;
            }
            // one-shot exhausted: pad with silence so the read cursor stays
            //  continuous and the slot parks on its authored fade.
            memset(chunk.data() + got * kfoanumchannels, 0, (want - got) * kfoanumchannels * sizeof(float));
            got = want;
            break;
          }
          got += size_t(n);
          rewinds = 0;
        }
        size_t head = size_t(w % ProbeStreamSlot::kRingFrames);
        size_t run1 = std::min(got, ProbeStreamSlot::kRingFrames - head);
        memcpy(slot._ring.data() + head * kfoanumchannels, chunk.data(), run1 * kfoanumchannels * sizeof(float));
        if (run1 < got) {
          memcpy(
              slot._ring.data(),
              chunk.data() + run1 * kfoanumchannels,
              (got - run1) * kfoanumchannels * sizeof(float));
        }
        w += got;
        slot._wframes.store(w, std::memory_order_release);
        didwork = true;
      }

      if (st == int(ProbeSlotState::LOADING)) {
        if ((w - r) >= ProbeStreamSlot::kPrimeFrames) {
          int expected = int(ProbeSlotState::LOADING);
          // a park that landed while we were priming wins - do not resurrect it.
          slot._state.compare_exchange_strong(expected, int(ProbeSlotState::READY), std::memory_order_acq_rel);
        }
      }
    }

    if (not didwork) {
      std::unique_lock<std::mutex> lock(_feedmtx);
      _feedcv.wait_for(lock, std::chrono::milliseconds(4));
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
// audio thread
///////////////////////////////////////////////////////////////////////////////

void SoundField::accumulateLive(
    const float* srcL,
    const float* srcR,
    int count,
    const float (&g0)[kfoanumchannels],
    const float (&g1)[kfoanumchannels]) {

  if (count <= 0 or count > frames_per_controlpass)
    return;

  // first voice of the pass owns the clear: the accumulator is only ever read
  //  once, by the computeIntoBus that follows in this same pass.
  if (not _liveActive) {
    memset(_liveAccum, 0, sizeof(_liveAccum));
    _liveActive = true;
  }

  const float inv = 1.0f / float(count);
  for (int i = 0; i < count; i++) {
    // MONO SUM of the voice's own stereo dsp buffer. the direction is carried
    //  by the encode gains, so the panner's L/R weighting must not be applied
    //  a second time - but its distance attenuation, which is already baked
    //  into these samples, is exactly the falloff the send is specified to use.
    float s = 0.5f * (srcL[i] + srcR[i]);
    float t = float(i) * inv;
    for (int ch = 0; ch < kfoanumchannels; ch++)
      _liveAccum[ch][i] += (g0[ch] + (g1[ch] - g0[ch]) * t) * s;
  }
}

///////////////////////////////////////////////////////////////////////////////

void SoundField::computeIntoBus(int base, int count) {

  // consumed either way: a pass whose accumulator is never folded in must not
  //  leak its samples into the next one.
  const bool live = _liveActive;
  _liveActive     = false;

  if (nullptr == _busRT)
    return;
  if (count <= 0 or count > frames_per_controlpass)
    return;

  bool anyaudible = false;

  for (int ch = 0; ch < kfoanumchannels; ch++)
    memset(_accum[ch], 0, sizeof(float) * size_t(count));

  for (int s = 0; s < kmaxActiveProbes; s++) {
    auto& slot = _slots[s];
    int st     = slot._state.load(std::memory_order_acquire);

    if (st != int(ProbeSlotState::READY) and st != int(ProbeSlotState::PARKING)) {
      slot._curGain = 0.0f;
      continue;
    }

    float target = slot._targetGain.load(std::memory_order_relaxed);
    float g0     = slot._curGain;
    float dg     = (target - g0) / float(count);

    if (st == int(ProbeSlotState::PARKING) and g0 <= 0.0f and target <= 0.0f) {
      slot._curGain = 0.0f;
      slot._state.store(int(ProbeSlotState::RELEASING), std::memory_order_release);
      continue;
    }

    uint64_t r = slot._rframes.load(std::memory_order_relaxed);
    uint64_t w = slot._wframes.load(std::memory_order_acquire);
    int avail  = int(std::min<uint64_t>(w - r, uint64_t(count)));
    if (avail < count)
      slot._underruns.fetch_add(1, std::memory_order_relaxed);

    const float* ring = slot._ring.data();
    for (int i = 0; i < avail; i++) {
      size_t f    = size_t((r + uint64_t(i)) % ProbeStreamSlot::kRingFrames) * kfoanumchannels;
      float g     = g0 + dg * float(i);
      _accum[0][i] += ring[f + 0] * g;
      _accum[1][i] += ring[f + 1] * g;
      _accum[2][i] += ring[f + 2] * g;
      _accum[3][i] += ring[f + 3] * g;
    }
    slot._rframes.store(r + uint64_t(avail), std::memory_order_release);
    slot._curGain = target;
    if (target > 0.0f or g0 > 0.0f)
      anyaudible = true;
  }

  // a field with nothing audible leaves the bus buffer exactly as the clear
  //  pass left it - a scene that declares no probes costs a bus of silence.
  if (not anyaudible and not live)
    return;

  ///////////////////////////////////
  // listener inverse rotation: 3x3 on ACN 1..3, W passthrough.
  ///////////////////////////////////

  float rot[9];
  memcpy(rot, _rot[_rotIndex.load(std::memory_order_acquire)], sizeof(rot));

  float* chY = _accum[int(FoaChannel::Y)];
  float* chZ = _accum[int(FoaChannel::Z)];
  float* chX = _accum[int(FoaChannel::X)];
  for (int i = 0; i < count; i++) {
    float x = chX[i];
    float y = chY[i];
    float z = chZ[i];
    chX[i]  = rot[0] * x + rot[1] * y + rot[2] * z;
    chY[i]  = rot[3] * x + rot[4] * y + rot[5] * z;
    chZ[i]  = rot[6] * x + rot[7] * y + rot[8] * z;
  }

  ///////////////////////////////////
  // the live encode folds in AFTER the rotation, not before: a probe is
  //  authored in WORLD ambisonic axes and needs the world->listener rotation,
  //  while a live voice's azimuth came from the panner's ANGLE, which the
  //  emitter systems already computed in LISTENER space. rotating it again
  //  would turn the field twice for the same head movement.
  ///////////////////////////////////

  if (live) {
    for (int ch = 0; ch < kfoanumchannels; ch++) {
      const float* src = _liveAccum[ch];
      float* dst       = _accum[ch];
      for (int i = 0; i < count; i++)
        dst[i] += src[i];
    }
  }

  ///////////////////////////////////

  const float* foa[kfoanumchannels] = {_accum[0], _accum[1], _accum[2], _accum[3]};
  _decoder->decode(foa, _decodeL, _decodeR, count);

  float* busL = _busRT->_buffer._leftBuffer;
  float* busR = _busRT->_buffer._rightBuffer;
  for (int i = 0; i < count; i++) {
    busL[base + i] += _decodeL[i];
    busR[base + i] += _decodeR[i];
  }
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::audio::singularity
