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

class ScreenOutputCompositingNode : public OutputCompositingNode {
  DeclareConcreteX(ScreenOutputCompositingNode, OutputCompositingNode);

public:
  ScreenOutputCompositingNode();
  ~ScreenOutputCompositingNode();

  PoolString _layername;
  int supersample() const {
    return _supersample;
  }
  void setSuperSample(int ss) {
    _supersample = ss;
  }

private:
  void gpuInit(lev2::Context* pTARG, int w, int h) final;
  void beginAssemble(CompositorDrawData& drawdata) final;
  void endAssemble(CompositorDrawData& drawdata) final;
  void composite(CompositorDrawData& drawdata) final;

  svar256_t _impl;
  int _supersample;
};

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
