////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include "NodeCompositor.h"
#include "pbr_common.h"

namespace ork::lev2::pbr {

///////////////////////////////////////////////////////////////////////////////

struct ForwardNode : public RenderCompositingNode {
  DeclareConcreteX(ForwardNode, RenderCompositingNode);

public:
  ForwardNode();
  ~ForwardNode();

  void doGpuInit(lev2::Context* pTARG, int w, int h) final;
  void DoRender(CompositorDrawData& drawdata) final;

  lev2::rtbuffer_ptr_t GetOutput() const final;
  lev2::rtgroup_ptr_t GetOutputGroup() const final;

  svar256_t _impl;
  pbr::commonstuff_ptr_t _pbrcommon;

};

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
