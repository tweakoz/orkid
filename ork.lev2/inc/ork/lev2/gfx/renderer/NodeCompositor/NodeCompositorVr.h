////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include "NodeCompositor.h"
#include <ork/lev2/gfx/material_freestyle.h>

namespace ork::lev2 {

///////////////////////////////////////////////////////////////////////////////
/// VrCompositingNode : OutputCompositingNode responsible for output to a VR device
///   implies stereo rendering..
///////////////////////////////////////////////////////////////////////////////

using distortion_lambda_t = std::function<void(rcfd_ptr_t RCFD,Texture*lrtexture)>;

class VrCompositingNode final : public OutputCompositingNode {
  DeclareConcreteX(VrCompositingNode, OutputCompositingNode);

public:
  VrCompositingNode();
  ~VrCompositingNode() final ;

  void setDistortionLambda(distortion_lambda_t l){
    _distorion_lambda = l;
  }

  EBufferFormat _format = EBufferFormat::RGBA8;

private:
  void gpuInit(lev2::Context* pTARG, int w, int h) final;
  void beginAssemble(CompositorDrawData& drawdata) final;
  void endAssemble(CompositorDrawData& drawdata) final;
  void composite(CompositorDrawData& drawdata) final;

  svar256_t _impl;
  distortion_lambda_t _distorion_lambda;
};

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
