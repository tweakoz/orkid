////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include "NodeCompositor.h"
#include <ork/lev2/gfx/material_freestyle.h>
#include <ork/lev2/ez_secondary_win.h>
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

class DualMonoVrOutputNode final : public OutputCompositingNode {
  DeclareConcreteX(DualMonoVrOutputNode, OutputCompositingNode);

public:

  compdrawdata_fn_t createAssembler(nodecompositortechnique_ptr_t tek);

  DualMonoVrOutputNode();
  ~DualMonoVrOutputNode() final ;

  void setDistortionLambda(distortion_lambda_t l){
    _distortion_lambda = l;
  }
  int supersample() const {
    return _supersample;
  }
  void setSuperSample(int ss) {
    _supersample = ss;
  }

  ezsecondarywin_ptr_t createExternalViewer(orkezapp_ptr_t app, const EzSecondaryWinConfig& cfg, bool mono = true);
  void closeExternalViewer();

  EBufferFormat _format = EBufferFormat::RGBA8;

private:
  void gpuInit(lev2::Context* pTARG, int w, int h) final;
  void onGpuUpdate(CompositorDrawData& drawdata) final;
  void composite(CompositorDrawData& drawdata) final;

  svar256_t _impl;
  distortion_lambda_t _distortion_lambda;
  int _supersample = 0;
  ezsecondarywin_ptr_t _externalViewer;
};

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
