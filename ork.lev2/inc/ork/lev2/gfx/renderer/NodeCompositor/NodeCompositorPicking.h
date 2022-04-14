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
/// PickingCompositingNode : RenderCompositingNode for picking
///   shall select all pick variants of shaders throught the frame
///////////////////////////////////////////////////////////////////////////////

struct PickingCompositingNode : public RenderCompositingNode {
  DeclareConcreteX(PickingCompositingNode, RenderCompositingNode);

public:
  PickingCompositingNode();
  ~PickingCompositingNode();

  std::string _layername;
  fvec4 _clearColor;

  void resize(int w, int h);

  void doGpuInit(lev2::Context* pTARG, int w, int h) final;
  void DoRender(CompositorDrawData& drawdata) final;

  lev2::rtbuffer_ptr_t GetOutput() const final;
  lev2::rtgroup_ptr_t GetOutputGroup() const final;
  svar256_t _impl;
};

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
