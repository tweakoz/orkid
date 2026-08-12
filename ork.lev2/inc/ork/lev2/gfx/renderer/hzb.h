////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
#pragma once
////////////////////////////////////////////////////////////////
// HZBBuilder — a hierarchical-Z (max-depth mip pyramid) built each frame from the scene depth, for
// 1-phase GPU occlusion culling. The backend has no compute imageStore (see reference_vk_compute_no
// _imagestore), so the pyramid lives in an SSBO, not a texture: cs_hzb_mip0 SAMPLES the depth texture
// and max-reduces a 2x2 block into mip0; cs_hzb_reduce max-reduces SSBO mip i -> mip i+1. The per-view
// culls (MeshInstCull / terrain) READ this SSBO (which they already do for everything else) and reject
// instances/chunks whose nearest depth is behind the HZB max over their screen footprint.
//
// Lifecycle: the ForwardNode calls build() at frame-end (depth valid) with THIS frame's depth; the
// result is stored on the Scene and stamped into the RCFD in Scene::preRender for NEXT frame's cull
// (1 frame late — the standard 1-phase scheme). Frame 0 / no depth = cleared-to-far -> HZB all-far ->
// nothing occluded (safe, never over-culls).
////////////////////////////////////////////////////////////////
#include <ork/lev2/gfx/renderer/compute_drawable.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <memory>
#include <vector>

namespace ork::lev2 {

struct HZBBuilder;
using hzbbuilder_ptr_t = std::shared_ptr<HZBBuilder>;

struct HZBBuilder {

  // build (or rebuild on resize) the max-depth pyramid from `depth` (the scene's Z32F depth texture).
  void build(Context* ctx, texture_ptr_t depth);

  // SINGLE-PASS STEREO sibling. `depth` is the 2-layer ARRAY image one multiview pass wrote;
  // mip0 reduces over BOTH eye layers so a texel occludes only what BOTH eyes agree is
  // occluded (see cs_hzb_mip0_layered for why that is max(), derived from the consumer
  // compare). Everything from mip1 down is layer-agnostic and reuses cs_hzb_reduce verbatim.
  // A sibling rather than a branch inside build(): the dual-pass path is the shipping mono
  // chain and stays byte-identical.
  //
  // The eye and center cameras are REQUIRED, not decorative: the consumer indexes this
  // pyramid with the CENTER camera's projection while the stored depth was rasterized with
  // the EYE projections, and the kernel needs the eye->center screen displacement to widen
  // its reduction enough to cover it (StereoReg below). Passing nothing registered would be
  // the misregistered shape this class refuses to publish.
  void buildLayered(
      Context*              ctx,
      texture_ptr_t         depth,
      int                   num_layers,
      const CameraMatrices* eyeL,
      const CameraMatrices* eyeR,
      const CameraMatrices* center);

  // eye->center screen registration, solved from the ACTUAL eye and center projections.
  //
  // TWO PARTS, because they have completely different magnitudes on a real headset. The part
  // that SURVIVES AT INFINITY is not parallax at all — it is the difference between an eye's
  // view-projection and the center one (a runtime hands out per-eye asymmetric FOVs AND cants
  // each display outward), and it is a fixed NDC map. Two perspective cameras see the same
  // DIRECTIONS at infinity, so that map is the composition of two projective transforms —
  // a HOMOGRAPHY, exactly, for any rig:
  //
  //    (ndc_eye, 1) ~ _regH * (ndc_center, 1)    (per eye, row-major 3x3; [0]=left [1]=right)
  //
  // It is solved, not fitted: four corner correspondences determine a homography exactly, and
  // the true map IS one, so nothing is left over for a canted rig. An AFFINE map (s,t per
  // axis) can express asymmetric FOV but NOT cant, and a measured headset cants ~3.8 degrees
  // per eye — which left ~20 mip0 texels at infinity, past the kernel's gather cap, so every
  // texel published far and stereo occlusion went inert in the headset while every parallel-eye
  // rig (novr, offscreen) solved the identity map and never saw it.
  //
  // The kernel APPLIES the map, it does not gather over it: as a search window it would cost
  // hundreds of fetches per texel per eye and overflow any sane cap.
  //
  // What is LEFT after the map is the actual parallax, which does shrink with depth and does
  // need a window:  residual(z) = _c0 + _c1 / z_view, in mip0 texels, with view depth
  // recovered from a stored window depth d by  1/z = _za*d + _zb.  With the map exact, _c0
  // (the part that survives at infinity) is ~0 by construction, and a _c0 at or past the cap
  // means the pyramid could only ever be uniformly far — asserted, never ridden.
  struct StereoReg {
    float _regH[2][9] = {
        {1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f},
        {1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f}};
    float _c0 = 0.0f;
    float _c1 = 0.0f;
    float _za = 0.0f;
    float _zb = 0.0f;
    bool  _ok = false;
  };
  // the model the last layered build dispatched with (reported at the point of use; the
  // stereo gate asserts it rather than trusting the kernel's name).
  StereoReg _stereoReg;

  static StereoReg computeStereoReg(
      const CameraMatrices& eyeL,
      const CameraMatrices& eyeR,
      const CameraMatrices& center,
      int                   baseW,
      int                   baseH);

  // outputs consumed by Scene::preRender's RCFD stamp + the cull occlusion test:
  FxShaderStorageBuffer* _ssbo = nullptr; // the packed mip pyramid (float HZB[]; max-depth)
  int _baseW = 0;                    // mip0 width  (= depthW/2)
  int _baseH = 0;                    // mip0 height (= depthH/2)
  int _mips  = 0;                    // number of mip levels (mip0 .. coarsest 1x1)
  bool _valid = false;               // true once a pyramid has been built this run
  // PROVENANCE: the frame whose depth passes last wrote the image this pyramid was built
  //  from (-1 before the first build). The 1-phase scheme requires it to be STRICTLY
  //  EARLIER than the frame consuming the pyramid; equality means same-frame depth reached
  //  the build, which is the exact hazard the ForwardNode's seed guard exists to forbid.
  //  Written by the builder's caller, read by cull oracles — never a guard input.
  int _sourceDepthFrame = -1;

  // mip-offset table (in floats) into _ssbo; _offsets[i] = start of mip i, size _mips+1 (last = total)
  std::vector<uint32_t> _offsets;

private:
  void _ensure(Context* ctx, int depthW, int depthH); // (re)create shaders + SSBO on first build / resize
  // SPVR: _ensure plus the layered mip0 kernel. Separate so _ensure stays exactly what the
  // dual-pass path uses; the cost is one extra JIT of the same shader text, once per run.
  void _ensureLayered(Context* ctx, int depthW, int depthH);
  // ORKID_DEBUG_HZB / ORKID_HZB_DUMP standing debug instrument, shared by both builders:
  // throttled pyramid readback (per-mip min/max/far-fraction) + the one-shot raw dump the
  // offline cull oracle reads. Costs nothing unless one of the two env vars is set.
  void _debugReport(Context* ctx, const char* tag, int num_layers);
  Context* _ctx = nullptr;
  const FxComputeShader* _cs_mip0   = nullptr;
  const FxComputeShader* _cs_reduce = nullptr;
  const FxComputeShader* _cs_mip0_layered = nullptr; // SPVR: samples the 2-layer array depth
  FxShaderStorageBuffer* _params = nullptr; // per-dispatch {dstoff,dstw,dsth,srcoff,srcw,srch,depthw,depthh}
  int _depthW = 0;
  int _depthH = 0;
};

} // namespace ork::lev2
