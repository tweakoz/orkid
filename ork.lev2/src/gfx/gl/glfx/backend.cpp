////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include "../gl.h"
#include "glslfxi.h"
#include "glslfxi_parser.h"
#include <ork/kernel/string/string.h>

namespace ork::lev2::glslfx::shaderbuilder {

BackEnd::BackEnd(const ContainerNode* cnode, Container* c) {
  _cnode     = cnode;
  _container = c;
}

bool BackEnd::generate() {

  bool ok = true;
  try {
    _cnode->generate(*this);
  }
  catch(...) {
    ok = false;
  }
  return ok;
}

}