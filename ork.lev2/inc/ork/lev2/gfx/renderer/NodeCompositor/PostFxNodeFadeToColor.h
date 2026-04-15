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

class PostFxNodeFadeToColor : public PostCompositingNode {
  DeclareConcreteX(PostFxNodeFadeToColor, PostCompositingNode);

public:
  PostFxNodeFadeToColor();
  ~PostFxNodeFadeToColor();

  // FadeAmount in [0,1]: 0 = passthrough, 1 = fully replaced by FadeColor.
  fvec4 _fadeColor  = fvec4(1, 1, 1, 1);
  float _fadeAmount = 0.0f;

  void doGpuInit(lev2::Context* pTARG, int w, int h) final; // virtual
  void DoRender(CompositorDrawData& drawdata) final;        // virtual

  lev2::rtbuffer_ptr_t GetOutput() const final;
  lev2::rtgroup_ptr_t GetOutputGroup() const final;
  svar256_t _impl;
};

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
