////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/gfx/scenegraph/scenegraph.h>
#include <ork/lev2/ui/event.h>
#include <ork/application/application.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorScreen.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/OutputNodeRtGroup.h>
#include <ork/reflect/properties/registerX.inl>
#include <ork/kernel/opq.h>
#include <ork/util/logger.h>
///////////////////////////////////////////////////////////////////////////////
#include <ork/lev2/gfx/renderer/NodeCompositor/pbr_node_deferred.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/pbr_node_forward.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/unlit_node.h>

namespace ork::lev2::scenegraph {

///////////////////////////////////////////////////

Synchro::Synchro() {

  _updcount.store(0);
  _rencount.store(0);
}

///////////////////////////////////////////////////

Synchro::~Synchro() {
  terminate();
}

///////////////////////////////////////////////////

void Synchro::terminate() {
  printf("Synchro<%p> terminating...\n", this);
}

///////////////////////////////////////////////////

bool Synchro::beginUpdate() {
  return (_updcount.load() == _rencount.load());
}

///////////////////////////////////////////////////

void Synchro::endUpdate() {
  _updcount++;
}

///////////////////////////////////////////////////

bool Synchro::beginRender() {
  return ((_updcount.load() - 1) == _rencount.load());
}

///////////////////////////////////////////////////

void Synchro::endRender() {
  _rencount++;
}

} // namespace ork::lev2::scenegraph
