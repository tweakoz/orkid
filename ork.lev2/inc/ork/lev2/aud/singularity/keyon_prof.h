////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>

namespace ork::audio::singularity {

///////////////////////////////////////////////////////////////////////////////
// note-on phase attribution, measured where the cost lands: on the audio
//  thread, inside the device callback. the phases nest (EVENTDRAIN contains
//  LAYERKEYON contains ALGGRID ...) so the table is read as a tree - a child's
//  total subtracted from its parent's is that parent's own time.
// RT contract: fixed slots in BSS, relaxed atomics, no allocation and no
//  locking; the per-probe cost is one monotonic clock read (vDSO).
// OFF unless ORKID_KEYON_PROF is set BEFORE the engine library loads
//  (ork::genviron snapshots environ at load - a later putenv is invisible).
///////////////////////////////////////////////////////////////////////////////

enum KeyOnPhase : int {
  KOP_EVENTDRAIN = 0, // synth::_tick - the whole deferred-event drain
  KOP_ALLOCLAYER,     //   voice allocation + steal ladder
  KOP_LAYERKEYON,     //   Layer::keyOn
  KOP_CTRLBLOCK,      //     controller instantiation + controller keyOn
  KOP_ALGCREATE,      //     AlgData::createAlgInst (counted misses == cold)
  KOP_ALGCOLD,        //       ... its cache-miss branch only
  KOP_ALGGRID,        //     dsp block grid instantiation
  KOP_ALGKEYON,       //     dsp block keyOn (param bind + doKeyOn)
  KOP_BLKPARAM,       //       ... DspBlock param binding
  KOP_BLKDOKEYON,     //       ... DspBlock::doKeyOn
  KOP_STAGECLEAR,     //       ... previous note's blocks released
  KOP_BLKCREATE,      //       ... DspBlockData::createInstance
  KOP_DELAYALLOC,     //   synth::allocDelayLine (dsp block ctors)
  KOP_VOICEACTIVATE,  // activateVoices
  KOP_VOICEDEACT,     // deactivateVoices (voice reclaim, alg return)
  KOP_BEGINCOMPUTE,   // per-callback: Layer::beginCompute over active voices
  KOP_ENDCOMPUTE,     // per-callback: Layer::endCompute over active voices
  KOP_PASSVOICES,     // per control pass: controllers + layer dsp
  KOP_PVCTRL,         //   ... Layer::updateControllers over active voices
  KOP_PVDSP,          //   ... Layer::compute over active voices
  KOP_PVONE,          //     ... ONE voice's dsp pass
  KOP_BLKCOMPUTE,     //       ... ONE dsp block's compute (labelled)
  KOP_PVSCOPES,       //   ... Layer::updateScopes over active voices
  KOP_PVSENDS,        //   ... Layer::mixToSendBus over active voices
  KOP_PASSBUSMIX,     // per control pass: per-bus mix fork/join
  KOP_PASSFX,         // per control pass: bus effects fork/join
  KOP_PASSMASTER,     // per control pass: bus->master accumulate + master eq
  KOP_COUNT
};

// read directly (not through a function-local static): the probes sit in the
//  per-block inner loop, where a guard-variable check per entry is not free.
extern bool keyonProfEnabledFlag;

inline bool keyonProfEnabled() {
  return keyonProfEnabledFlag;
}

void keyonProfAccum(int phase, uint64_t nanos, const char* label = nullptr);

// prints the accumulated table and clears it. diag-only, and called from the
//  same audio-thread window boundary that prints the PA_DIAG headroom line.
void keyonProfReport();

struct KeyOnProfScope {
  inline KeyOnProfScope(int phase, const char* label = nullptr)
      : _phase(phase)
      , _label(label)
      , _enabled(keyonProfEnabled()) {
    if (_enabled)
      _t0 = std::chrono::steady_clock::now();
  }
  inline ~KeyOnProfScope() {
    if (_enabled) {
      auto dt = std::chrono::steady_clock::now() - _t0;
      keyonProfAccum(_phase, uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(dt).count()), _label);
    }
  }
  int _phase;
  const char* _label;
  bool _enabled;
  std::chrono::steady_clock::time_point _t0;
};

} // namespace ork::audio::singularity
