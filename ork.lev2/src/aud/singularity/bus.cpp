////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <string>
#include <assert.h>
#include <unistd.h>
#include <math.h>

#include <ork/lev2/aud/singularity/synthdata.h>
#include <ork/lev2/aud/singularity/synth.h>
#include <ork/lev2/aud/singularity/hud.h>
#include <ork/lev2/aud/singularity/krzobjects.h>
#include <ork/lev2/aud/singularity/dspblocks.h>
#include <ork/lev2/aud/singularity/fxgen.h>
#include <ork/util/logger.h>

namespace ork::audio::singularity {
///////////////////////////////////////////////////////////////////////////////
void OutputBus::resize(int numframes) {
  _buffer.resize(numframes);
  if (_dsplayer) {
    _dsplayer->resize(numframes);
  }
}
///////////////////////////////////////////////////////////////////////////////
layer_ptr_t OutputBus::prepareBusDSP(lyrdata_ptr_t ld, synth* syn) {

  assert(ld->_algdata != nullptr);

  // off-RT: the whole cost lives here — layer alloc, dsp-block graph
  // construction and convolver IR FFT precomputation. Reads only stable synth
  // state (_curprogrambus, _outputBusses) and mutates nothing on the bus. The
  // synth is passed in rather than synth::instance() so this runs safely from
  // within the synth constructor (singleton not yet published).
  auto l               = std::make_shared<Layer>();
  l->_is_bus_processor = true;
  syn->_keyOnLayer(l, 0, 0, ld); // outbus layer always keyed on...
  return l;
}

void OutputBus::commitBusDSP(layer_ptr_t prebuilt, lyrdata_ptr_t ld) {

  // audio-thread: dispose the superseded layer (cheap — its alg, holding the
  // convolver buffers, is returned to the voice cache rather than freed) then
  // swap in the pre-warmed layer.
  if(_dsplayer){
    _dsplayer->keyOff();
    _dsplayer->endCompute();
    _dsplayer->_alg->_algdata.returnAlgInst(_dsplayer->_alg);
    _dsplayer->_alg = nullptr;
  }

  _dsplayer     = nullptr;
  _dsplayerdata = ld;
  _dsplayer     = prebuilt;
}

void OutputBus::setBusDSP(lyrdata_ptr_t ld) {
  commitBusDSP(prepareBusDSP(ld, synth::instance().get()), ld);
}
///////////////////////////////////////////////////////////////////////////////
layer_ptr_t InsertGroup::prepareBranch(lyrdata_ptr_t ld, synth* syn) {

  // off-RT (see synth.h): one keyed-on bus-processor layer per branch. the
  // branch layer IS the fork buffer — it owns the DspBuffer its branch reads
  // and writes — so building it here is what makes computeInserts
  // allocation-free.
  OrkAssertI(ld != nullptr, "insert group branch has null layerdata");
  OrkAssertIFMT(
      ld->_algdata != nullptr, //
      "insert group branch<%s> has no algdata",
      ld->_name.c_str());
  auto l               = std::make_shared<Layer>();
  l->_is_bus_processor = true;
  syn->_keyOnLayer(l, 0, 0, ld);
  l->_outbus = nullptr; // a branch never mixes; computeInserts moves its output
  return l;
}
///////////////////////////////////////////////////////////////////////////////
void InsertGroup::disposeBranch(layer_ptr_t l) {

  // audio-thread half of the install (see synth.h): cheap — the alg, which
  // holds the branch's delay lines and convolver buffers, goes back to its
  // voice cache instead of being freed.
  l->keyOff();
  l->endCompute();
  if (l->_alg) {
    l->_alg->_algdata.returnAlgInst(l->_alg);
    l->_alg = nullptr;
  }
}
///////////////////////////////////////////////////////////////////////////////
void InsertGroup::prepare(synth* syn) {

  // every branch is rebuilt, so _layers is always 1:1 with _layerdatas and no
  // branch can carry state from a superseded config.
  _layers.clear();
  for (auto ld : _layerdatas) {
    _layers.push_back(prepareBranch(ld, syn));
  }
}
///////////////////////////////////////////////////////////////////////////////
void InsertGroup::disposeLayers() {
  for (auto l : _layers) {
    disposeBranch(l);
  }
  _layers.clear();
}
///////////////////////////////////////////////////////////////////////////////
scopesource_ptr_t OutputBus::createScopeSource() {
  _scopesource = std::make_shared<ScopeSource>();
  return _scopesource;
}

} //namespace ork::audio::singularity {