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

class PostFxNodeDecompBlur : public PostCompositingNode {
  DeclareConcreteX(PostFxNodeDecompBlur, PostCompositingNode);

public:
  PostFxNodeDecompBlur();
  ~PostFxNodeDecompBlur();

  float _amount = 0.5f;
  float _threshold = 0.5f;
  float _blurfactor = 1.0f/32.0f;
  int  _blurwidth = 16;

  void doGpuInit(lev2::Context* pTARG, int w, int h) final; // virtual
  void DoRender(CompositorDrawData& drawdata) final;        // virtual

  lev2::rtbuffer_ptr_t GetOutput() const final;
  lev2::rtgroup_ptr_t GetOutputGroup() const final;

  svar256_t _impl;

};

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
