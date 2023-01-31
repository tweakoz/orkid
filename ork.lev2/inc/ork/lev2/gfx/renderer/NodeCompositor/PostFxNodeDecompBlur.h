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
  svar256_t _impl;

};

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
