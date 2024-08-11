////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
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

  void gpuInit(lev2::Context* pTARG, int w, int h) final;
  void beginAssemble(CompositorDrawData& drawdata) final;
  void endAssemble(CompositorDrawData& drawdata) final;
  void composite(CompositorDrawData& drawdata) final;

  svar256_t _impl;

};

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
