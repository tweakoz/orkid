////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include "NodeCompositor.h"

namespace ork::lev2 {

///////////////////////////////////////////////////////////////////////////////
/// VrCompositingNode : OutputCompositingNode responsible for output to a VR device
///   implies stereo rendering..
///////////////////////////////////////////////////////////////////////////////

class VrCompositingNode : public OutputCompositingNode {
  DeclareConcreteX(VrCompositingNode, OutputCompositingNode);

public:
  VrCompositingNode();
  ~VrCompositingNode();

private:
  void gpuInit(lev2::Context* pTARG, int w, int h) final;
  void beginAssemble(CompositorDrawData& drawdata) final;
  void endAssemble(CompositorDrawData& drawdata) final;
  void composite(CompositorDrawData& drawdata) final;

  svar256_t _impl;
};

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
