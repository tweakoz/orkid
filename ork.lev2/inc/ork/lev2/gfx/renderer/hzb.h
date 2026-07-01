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

  // outputs consumed by Scene::preRender's RCFD stamp + the cull occlusion test:
  FxShaderStorageBuffer* _ssbo = nullptr; // the packed mip pyramid (float HZB[]; max-depth)
  int _baseW = 0;                    // mip0 width  (= depthW/2)
  int _baseH = 0;                    // mip0 height (= depthH/2)
  int _mips  = 0;                    // number of mip levels (mip0 .. coarsest 1x1)
  bool _valid = false;               // true once a pyramid has been built this run

  // mip-offset table (in floats) into _ssbo; _offsets[i] = start of mip i, size _mips+1 (last = total)
  std::vector<uint32_t> _offsets;

private:
  void _ensure(Context* ctx, int depthW, int depthH); // (re)create shaders + SSBO on first build / resize
  Context* _ctx = nullptr;
  const FxComputeShader* _cs_mip0   = nullptr;
  const FxComputeShader* _cs_reduce = nullptr;
  FxShaderStorageBuffer* _params = nullptr; // per-dispatch {dstoff,dstw,dsth,srcoff,srcw,srch,depthw,depthh}
  int _depthW = 0;
  int _depthH = 0;
};

} // namespace ork::lev2
