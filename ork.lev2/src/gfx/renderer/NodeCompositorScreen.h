////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include "NodeCompositor.h"

namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

class ScreenOutputCompositingNode : public OutputCompositingNode {
  DeclareConcreteX(ScreenOutputCompositingNode, OutputCompositingNode);

public:
  ScreenOutputCompositingNode();
  ~ScreenOutputCompositingNode();

  PoolString _layername;

private:
  void DoInit(lev2::GfxTarget* pTARG, int w, int h) final;                          // virtual
  void _produce(CompositorDrawData& drawdata, CompositingImpl* pCCI,innerl_t lambda) final; // virtual

  svar256_t _impl;

};

///////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2 {
