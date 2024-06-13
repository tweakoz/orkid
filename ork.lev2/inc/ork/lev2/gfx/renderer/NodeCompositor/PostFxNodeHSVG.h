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

class PostFxNodeHSVG : public PostCompositingNode {
  DeclareConcreteX(PostFxNodeHSVG, PostCompositingNode);

public:
  PostFxNodeHSVG();
  ~PostFxNodeHSVG();

  float _hue = 0.0f;
  float _saturation = 1.0f;
  float _value = 1.0f;
  float _gamma = 1.0f;

  void doGpuInit(lev2::Context* pTARG, int w, int h) final; // virtual
  void DoRender(CompositorDrawData& drawdata) final;        // virtual

  lev2::rtbuffer_ptr_t GetOutput() const final;
  svar256_t _impl;

};

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
