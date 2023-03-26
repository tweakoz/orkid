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
