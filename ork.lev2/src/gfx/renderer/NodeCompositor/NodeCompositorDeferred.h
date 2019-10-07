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

class DeferredCompositingNode : public RenderCompositingNode {
  DeclareConcreteX(DeferredCompositingNode, RenderCompositingNode);

public:
  DeferredCompositingNode();
  ~DeferredCompositingNode();
  fvec4 _clearColor;
  fvec4 _fogColor;

private:
  void DoInit(lev2::GfxTarget* pTARG, int w, int h) final;
  void DoRender(CompositorDrawData& drawdata) final;

  lev2::RtBuffer* GetOutput() const final;
  svar256_t _impl;
};

class DeferredCompositingNodeNvMs : public RenderCompositingNode {
  DeclareConcreteX(DeferredCompositingNodeNvMs, RenderCompositingNode);

public:
  DeferredCompositingNodeNvMs();
  ~DeferredCompositingNodeNvMs();
  fvec4 _clearColor;
  fvec4 _fogColor;

private:
  void DoInit(lev2::GfxTarget* pTARG, int w, int h) final;
  void DoRender(CompositorDrawData& drawdata) final;

  lev2::RtBuffer* GetOutput() const final;
  svar256_t _impl;
};

///////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2 {
