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
/// ScreenOutputCompositingNode : OutputCompositingNode responsible for output
///   direct to the screen. implies mono rendering..
///////////////////////////////////////////////////////////////////////////////

class ScreenOutputCompositingNode : public OutputCompositingNode {
  DeclareConcreteX(ScreenOutputCompositingNode, OutputCompositingNode);

public:
  ScreenOutputCompositingNode();
  ~ScreenOutputCompositingNode();

  std::string _layername;
  int supersample() const {
    return _supersample;
  }
  void setSuperSample(int ss);

  EBufferFormat _format;
  bool _monoviewer = false;


private:
  void gpuInit(lev2::Context* pTARG, int w, int h) final;
  void beginAssemble(CompositorDrawData& drawdata) final;
  void endAssemble(CompositorDrawData& drawdata) final;
  void composite(CompositorDrawData& drawdata) final;

  svar256_t _impl;
  rtgroup_ptr_t _downsamplebuffer;
  int _supersample;
};

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
