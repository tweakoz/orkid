////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include "NodeCompositor.h"
#include <ork/lev2/gfx/material_freestyle.h>
#include <ork/lev2/vr/vr.h> // DistortionRect, distortion_lambda_t

namespace ork::lev2 {

///////////////////////////////////////////////////////////////////////////////
/// VrOutputNode : OutputCompositingNode responsible for output to a VR device
///   implies stereo rendering..
///////////////////////////////////////////////////////////////////////////////

class VrOutputNode final : public OutputCompositingNode {
  DeclareConcreteX(VrOutputNode, OutputCompositingNode);

public:
  VrOutputNode();
  ~VrOutputNode() final ;

  void setDistortionLambda(distortion_lambda_t l){
    _distortion_lambda = l;
  }
  int supersample() const {
    return _supersample;
  }
  void setSuperSample(int ss) {
    _supersample = ss;
  }

  bool _monoviewer = false;
  EBufferFormat _format = EBufferFormat::RGBA8;

private:
  void gpuInit(lev2::Context* pTARG, int w, int h) final;
  void beginAssemble(CompositorDrawData& drawdata) final;
  void endAssemble(CompositorDrawData& drawdata) final;
  void composite(CompositorDrawData& drawdata) final;

  svar256_t _impl;
  distortion_lambda_t _distortion_lambda;
  int _supersample = 0;
};

///////////////////////////////////////////////////////////////////////////////
/// SinglePassStereoVrOutputNode : SPVR — THE VR output node: both eyes in ONE
///   layered pass. This node pushes ONE CPD carrying both eye cameras with
///   SinglePassStereo set, so the forward render node runs its whole pass chain
///   once against a 2-layer multiview RtGroup. Everything downstream of the scene
///   pass (postfx, downsample, HUD, the XR handoff, the desktop mirror) stays PER
///   EYE: each layer is extracted into a per-eye RtGroup, which is what
///   downsampledEyeRtGroup(bool) hands back. Requires multiview; a device without
///   it is a hard error, not a degrade.
///////////////////////////////////////////////////////////////////////////////

class SinglePassStereoVrOutputNode final : public OutputCompositingNode {
  DeclareConcreteX(SinglePassStereoVrOutputNode, OutputCompositingNode);

public:

  compdrawdata_fn_t createAssembler(nodecompositortechnique_ptr_t tek);

  SinglePassStereoVrOutputNode();
  ~SinglePassStereoVrOutputNode() final ;

  void setDistortionLambda(distortion_lambda_t l){
    _distortion_lambda = l;
  }
  int supersample() const {
    return _supersample;
  }
  void setSuperSample(int ss) {
    _supersample = ss;
  }

  // Per-eye final DOWNSAMPLED buffer — the exact RtGroup handed to the XR runtime
  //  and the desktop mirror blit (post-postfx, post-downsample). Populated during
  //  assemble; content is valid from onEndAssemble through composite of the same
  //  frame. Additive READ accessor for headless per-eye capture — never touches the
  //  render/composite path. Null before the first assemble.
  lev2::rtgroup_ptr_t downsampledEyeRtGroup(bool left_eye);

  EBufferFormat _format = EBufferFormat::RGBA8;

private:
  void gpuInit(lev2::Context* pTARG, int w, int h) final;
  void onGpuUpdate(CompositorDrawData& drawdata) final;
  void composite(CompositorDrawData& drawdata) final;

  svar256_t _impl;
  distortion_lambda_t _distortion_lambda;
  int _supersample = 0;
};

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
