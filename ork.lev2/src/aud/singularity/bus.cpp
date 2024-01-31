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
void OutputBus::setBusDSP(lyrdata_ptr_t ld) {

  assert(ld->_algdata != nullptr);

  if(_dsplayer){
    //_dsplayer->release();
    //synth::instance()->releaseLayer(_dsplayer);
    _dsplayer->keyOff();
    _dsplayer->endCompute();
    _dsplayer->_alg->_algdata.returnAlgInst(_dsplayer->_alg);
    _dsplayer->_alg = nullptr;

  }

  _dsplayer = nullptr;

  _dsplayerdata        = ld;
  auto l               = std::make_shared<Layer>();
  l->_is_bus_processor = true;
  synth::instance()->_keyOnLayer(l, 0, 0, _dsplayerdata); // outbus layer always keyed on...
  _dsplayer = l;
}
scopesource_ptr_t OutputBus::createScopeSource() {
  _scopesource = std::make_shared<ScopeSource>();
  return _scopesource;
}

} //namespace ork::audio::singularity {