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

class ScaleBiasCompositingNode : public PostCompositingNode {
  DeclareConcreteX(ScaleBiasCompositingNode, PostCompositingNode);

public:
  ScaleBiasCompositingNode();
  ~ScaleBiasCompositingNode();

private:
  void DoInit(lev2::Context* pTARG, int w, int h) final;                          // virtual
  void DoRender(CompositorDrawData& drawdata) final; // virtual

  lev2::RtBuffer* GetOutput() const final;
  svar256_t _impl;
};

///////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2 {
