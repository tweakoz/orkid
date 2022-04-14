////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include "NodeCompositor.h"

namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

class RtGroupOutputCompositingNode : public OutputCompositingNode {
  DeclareConcreteX(RtGroupOutputCompositingNode, OutputCompositingNode);

public:
  RtGroupOutputCompositingNode(rtgroup_ptr_t defaultrtg=nullptr);
  ~RtGroupOutputCompositingNode();

  std::string _layername;

  void resize(int w, int h);

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

  rtgroup_ptr_t _static_rtg;
  int _supersample;

};

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
