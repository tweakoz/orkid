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

  // P3.D — depth-aware sample rejection threshold, **meters**.
  // Samples whose linear eye-space depth differs from the center pixel's
  // by more than this get zero weight in the blur kernel.
  //   physical skin SSS: 1–2 mm scattering depth → threshold ~0.05 m
  //   demo / large props: 1–10 m (matches feature size)
  // Default 10 m is calibrated for the mtl_subsurface scene (4 m spheres
  // at ~10 m camera distance); production-scale skin scenes should bring
  // this way down or modulate by the material's _subsurface_radius.
  float _depth_reject_threshold = 10.0f;
  float _blurfactor    = 12.0f;            // screen-space radius in pixels
                                            // (kernel far taps reach ±2.0× this;
                                            // keep small so samples stay inside
                                            // the surface — otherwise silhouette
                                            // pixels bleed BG into the surface)
  float _strength      = 1.0f;             // gain on the tinted-blur contribution.
                                            // 1.0 = energy-conserving (physically plausible);
                                            // >1 = visually dominant (subsurface bleed is
                                            // subtle physically — games often crank to 1.5–3
                                            // for "dramatic" skin). Set via DSL per-scene
                                            // when a particular look needs it.
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
