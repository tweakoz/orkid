////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include "NodeCompositor.h"
#include <ork/lev2/gfx/material_freestyle.h>

namespace ork::lev2 {

///////////////////////////////////////////////////////////////////////////////
/// VrCompositingNode : OutputCompositingNode responsible for output to a VR device
///   implies stereo rendering..
///////////////////////////////////////////////////////////////////////////////

using distortion_lambda_t = std::function<void(RenderContextFrameData& RCFD,Texture*lrtexture)>;

class VrCompositingNode final : public OutputCompositingNode {
  DeclareConcreteX(VrCompositingNode, OutputCompositingNode);

public:
  VrCompositingNode();
  ~VrCompositingNode() final ;

  void setDistortionLambda(distortion_lambda_t l){
    _distorion_lambda = l;
  }
  int supersample() const {
    return _supersample;
  }
  void setSuperSample(int ss) {
    _supersample = ss;
  }

private:
  void gpuInit(lev2::Context* pTARG, int w, int h) final;
  void beginAssemble(CompositorDrawData& drawdata) final;
  void endAssemble(CompositorDrawData& drawdata) final;
  void composite(CompositorDrawData& drawdata) final;

  svar256_t _impl;
  distortion_lambda_t _distorion_lambda;
  int _supersample;
};

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
