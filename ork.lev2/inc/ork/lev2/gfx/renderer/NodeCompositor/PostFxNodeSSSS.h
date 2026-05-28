////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include "NodeCompositor.h"

namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////
// PBR2 Phase 3 (P3.D) — Separable Subsurface Scattering post-fx node.
//
// Consumes the forward pass's MRT output (target0 = composed lit color,
// target1 = diffuse irradiance with sss_mask in .alpha). Runs three
// passes: horizontal blur of target1, vertical blur, composite back to a
// new RTG (= target0 with the diffuse term replaced by its blurred
// version, weighted by the per-pixel mask). Specular passes through
// untouched.
//
// _blurfactor is the screen-space sample radius in pixels (e.g. 4.0
// = subtle skin, 16.0 = wax/marble). For VR consider scaling by
// resolution. Performance: 3 fullscreen passes; early-out on mask=0 makes
// the cost ~0 for non-subsurface pixels.
///////////////////////////////////////////////////////////////////////////////

class PostFxNodeSSSS : public PostCompositingNode {
  DeclareConcreteX(PostFxNodeSSSS, PostCompositingNode);

public:
  PostFxNodeSSSS();
  ~PostFxNodeSSSS();

  float _blurfactor    = 12.0f;            // screen-space radius in pixels
                                            // (kernel far taps reach ±2.0× this;
                                            // keep small so samples stay inside
                                            // the surface — otherwise silhouette
                                            // pixels bleed BG into the surface)
  float _strength      = 3.0f;             // amplify the bleed contribution above physical;
                                            // strength*tinted_blurred replaces raw_diffuse in
                                            // the composite. 1.0 = energy-conserving; >1 = make
                                            // the effect visually dominant (subsurface bleed is
                                            // subtle physically — almost always cranked in games)
  fvec3 _subsurface_tint = fvec3(1, 1, 1); // color of the blurred-diffuse contribution
                                            // (per-frame override of the dominant subsurface
                                            // material's color; v1 uniform — proper per-pixel
                                            // tint via target2 channel deferred to P3.E)
  // Debug viz modes (composite output override):
  //   0 = normal composite (lit + delta)
  //   1 = mask only (target1.alpha visualized as grayscale)
  //   2 = raw diffuse only (target1.rgb)
  //   3 = blurred diffuse only (post v-pass; reveals kernel reach)
  //   4 = delta only (strength*tinted_blurred - raw) × mask — the actual SSSS contribution
  int _debug_mode = 0;

  void doGpuInit(lev2::Context* pTARG, int w, int h) final;
  void DoRender(CompositorDrawData& drawdata) final;

  lev2::rtbuffer_ptr_t GetOutput() const final;
  lev2::rtgroup_ptr_t GetOutputGroup() const final;

  svar256_t _impl;
};

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
