////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/aud/singularity/keyon_prof.h>
#include <cstdio>
#include <cstdlib>

namespace ork::audio::singularity {

///////////////////////////////////////////////////////////////////////////////

namespace {

struct KeyOnProfSlot {
  std::atomic<uint64_t> _nanos{0};
  std::atomic<uint64_t> _count{0};
  std::atomic<uint64_t> _maxnanos{0};
  std::atomic<const char*> _maxlabel{nullptr};
};

// BSS, not heap: the probes run on the audio thread and this table must exist
//  before the first callback with no construction order to lose.
KeyOnProfSlot _slots[KOP_COUNT];

const char* const _phasenames[KOP_COUNT] = {
    "eventdrain",   //
    ". alloclayer", //
    ". layerkeyon",
    ".   ctrlblock",
    ".   algcreate",
    ".     algcold",
    ".   alggrid",
    ".   algkeyon",
    ".     blkparam",
    ".     blkdokeyon",
    ".     stageclear",
    ".     blkcreate",
    ". delayalloc",
    "voiceactivate",
    "voicedeact",
    "begincompute",
    "endcompute",
    "pass.voices",
    ".  pv.ctrl",
    ".  pv.dsp",
    ".    pv.one",
    ".      blkcompute",
    ".  pv.scopes",
    ".  pv.sends",
    "pass.busmix",
    "pass.fx",
    "pass.master"};

} // namespace

///////////////////////////////////////////////////////////////////////////////

bool keyonProfEnabledFlag = (getenv("ORKID_KEYON_PROF") != nullptr);

///////////////////////////////////////////////////////////////////////////////

void keyonProfAccum(int phase, uint64_t nanos, const char* label) {
  auto& slot = _slots[phase];
  slot._nanos.fetch_add(nanos, std::memory_order_relaxed);
  slot._count.fetch_add(1, std::memory_order_relaxed);
  uint64_t prevmax = slot._maxnanos.load(std::memory_order_relaxed);
  bool tookmax     = false;
  while (nanos > prevmax and                                //
         not(tookmax = slot._maxnanos.compare_exchange_weak(prevmax, //
                                                           nanos,   //
                                                           std::memory_order_relaxed))) {
  }
  if (tookmax and label)
    slot._maxlabel.store(label, std::memory_order_relaxed);
}

///////////////////////////////////////////////////////////////////////////////

void keyonProfReport() {
  if (not keyonProfEnabled())
    return;
  for (int i = 0; i < KOP_COUNT; i++) {
    auto& slot     = _slots[i];
    uint64_t nanos = slot._nanos.exchange(0, std::memory_order_relaxed);
    uint64_t count = slot._count.exchange(0, std::memory_order_relaxed);
    uint64_t mx    = slot._maxnanos.exchange(0, std::memory_order_relaxed);
    const char* lbl = slot._maxlabel.exchange(nullptr, std::memory_order_relaxed);
    if (0 == count)
      continue;
    printf(
        "[KEYON_PROF] %-14s total<%.1fus> count<%lu> avg<%.2fus> max<%.1fus> worst<%s>\n",
        _phasenames[i],
        double(nanos) * 1e-3,
        (unsigned long)count,
        double(nanos) * 1e-3 / double(count),
        double(mx) * 1e-3,
        lbl ? lbl : "-");
  }
}

} // namespace ork::audio::singularity
