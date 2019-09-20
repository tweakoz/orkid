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

class VrCompositingNode : public OutputCompositingNode {
  DeclareConcreteX(VrCompositingNode, OutputCompositingNode);

public:
  VrCompositingNode();
  ~VrCompositingNode();

private:
  void gpuInit(lev2::GfxTarget* pTARG, int w, int h) final;
  void beginFrame(CompositorDrawData& drawdata, CompositingImpl* pCCI) final;
  void endFrame(CompositorDrawData& drawdata, CompositingImpl* pCCI,RtGroup* final) final;

  svar256_t _impl;

};

///////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2 {
