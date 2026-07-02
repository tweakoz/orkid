////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
//
// hzb.cpp — build a max-depth hierarchical-Z pyramid in an SSBO from the scene depth, for 1-phase GPU
// occlusion culling. See hzb.h. The backend has no compute imageStore, so the pyramid is an SSBO:
//   cs_hzb_mip0   : SAMPLE the depth texture, max a 2x2 block -> SSBO mip0 (half depth res)
//   cs_hzb_reduce : max a 2x2 block of SSBO mip i-1 -> mip i, down to 1x1
// The per-view culls READ this SSBO and reject anything whose nearest depth is behind the HZB max
// over its screen footprint. Each mip is its own dispatch phase (params change per mip; correctness
// over a single-submit pyramid — optimize later).
//
////////////////////////////////////////////////////////////////
#include <ork/lev2/gfx/renderer/hzb.h>
#include <ork/lev2/gfx/ci.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace ork::lev2 {

///////////////////////////////////////////////////////////////////////////////

struct HParams {
  uint32_t dstoff, dstw, dsth;   // destination mip: offset (floats) + dims
  uint32_t srcoff, srcw, srch;   // source mip (reduce only): offset + dims
  uint32_t depthw, depthh;       // depth texture dims (mip0 only)
};

///////////////////////////////////////////////////////////////////////////////

static std::string _hzb_text() {
  return R"S(
fxconfig fxcfg_default {}
sampler_set sset_hzb (descriptor_set 0) { sampler2D u_depth; }
storage_interface hif_par (descriptor_set 0) { buffer layout(std430) hpar {
  uint p_dstoff; uint p_dstw; uint p_dsth; uint p_srcoff; uint p_srcw; uint p_srch; uint p_depthw; uint p_depthh; }; }
storage_interface hif_hzb (descriptor_set 0) { buffer layout(std430) hhzb { float HZB[]; }; }
compute_interface hiface_mip0 : sset_hzb { storage { hif_par hif_hzb }
                                           inputs { layout(local_size_x = 8, local_size_y = 8); } }
compute_interface hiface_red                { storage { hif_par hif_hzb }
                                           inputs { layout(local_size_x = 8, local_size_y = 8); } }
////////////////////////////////////////
// mip0 — sample a 2x2 block of the depth texture, store the MAX (farthest) into SSBO mip0.
compute_shader cs_hzb_mip0 : hiface_mip0 {
  uint dx = gl_GlobalInvocationID.x;
  uint dy = gl_GlobalInvocationID.y;
  if (dx >= p_dstw) { return; }
  if (dy >= p_dsth) { return; }
  ivec2 mxd = ivec2(int(p_depthw) - 1, int(p_depthh) - 1);
  ivec2 dc  = ivec2(int(dx) * 2, int(dy) * 2);
  float a = texelFetch(u_depth, min(dc,                mxd), 0).r;
  float b = texelFetch(u_depth, min(dc + ivec2(1, 0),  mxd), 0).r;
  float c = texelFetch(u_depth, min(dc + ivec2(0, 1),  mxd), 0).r;
  float d = texelFetch(u_depth, min(dc + ivec2(1, 1),  mxd), 0).r;
  HZB[p_dstoff + dy * p_dstw + dx] = max(max(a, b), max(c, d));
}
////////////////////////////////////////
// reduce — max a 2x2 block of SSBO mip (src) into mip (dst).
compute_shader cs_hzb_reduce : hiface_red {
  uint dx = gl_GlobalInvocationID.x;
  uint dy = gl_GlobalInvocationID.y;
  if (dx >= p_dstw) { return; }
  if (dy >= p_dsth) { return; }
  uint mxx = p_srcw - 1u;
  uint mxy = p_srch - 1u;
  uint sx0 = min(dx * 2u,       mxx);
  uint sx1 = min(dx * 2u + 1u,  mxx);
  uint sy0 = min(dy * 2u,       mxy);
  uint sy1 = min(dy * 2u + 1u,  mxy);
  float a = HZB[p_srcoff + sy0 * p_srcw + sx0];
  float b = HZB[p_srcoff + sy0 * p_srcw + sx1];
  float c = HZB[p_srcoff + sy1 * p_srcw + sx0];
  float d = HZB[p_srcoff + sy1 * p_srcw + sx1];
  HZB[p_dstoff + dy * p_dstw + dx] = max(max(a, b), max(c, d));
}
)S";
}

///////////////////////////////////////////////////////////////////////////////

void HZBBuilder::_ensure(Context* ctx, int dW, int dH) {
  _ctx     = ctx;
  auto fxi = ctx->FXI();
  if (not _cs_mip0) {
    auto sh    = fxi->shaderFromShaderText("hzb_build", _hzb_text());
    _cs_mip0   = fxi->computeShader(sh, "cs_hzb_mip0");
    _cs_reduce = fxi->computeShader(sh, "cs_hzb_reduce");
    // BAR: tiny, CPU-written per frame (forward memcpy), GPU-read per dispatch.
    _params    = fxi->createStorageBuffer(sizeof(HParams), StorageBufferUsage::DEFAULT, BufferResidency::BAR);
  }
  if (dW == _depthW and dH == _depthH and _ssbo)
    return; // dims unchanged — reuse the pyramid SSBO
  _depthW = dW;
  _depthH = dH;
  _baseW  = std::max(1, dW / 2);
  _baseH  = std::max(1, dH / 2);
  // pack the mip pyramid: _offsets[i] = first float of mip i; _offsets[_mips] = total float count.
  _offsets.clear();
  uint32_t off = 0;
  int w = _baseW, h = _baseH, mips = 0;
  while (true) {
    _offsets.push_back(off);
    off += uint32_t(w) * uint32_t(h);
    mips++;
    if ((w == 1 and h == 1) or mips > 24)
      break;
    w = std::max(1, w >> 1);
    h = std::max(1, h >> 1);
  }
  _offsets.push_back(off); // total at [_mips]
  _mips = mips;
  // NOTE: on resize this leaks the prior _ssbo (raw FXI buffer; resize is rare — P1).
  // DEVICE: the pyramid is GPU-written (build computes) and GPU-read (cull shaders) every
  // frame — in sysram it crossed PCIe BOTH ways per frame. The only CPU read is the
  // ORKID_DEBUG_HZB throttled sanity readback, which routes through the staged copyToHost.
  _ssbo = fxi->createStorageBuffer(size_t(off) * sizeof(float), StorageBufferUsage::DEFAULT, BufferResidency::DEVICE);
}

///////////////////////////////////////////////////////////////////////////////

void HZBBuilder::build(Context* ctx, texture_ptr_t depth) {
  if (not depth)
    return;
  int dW = depth->_width;
  int dH = depth->_height;
  if (dW < 2 or dH < 2)
    return;
  _ensure(ctx, dW, dH);
  if (not _ssbo)
    return;

  auto fxi   = ctx->FXI();
  auto ci    = ctx->CI();
  auto write = [&](const HParams& p) {
    auto m = fxi->mapStorageBuffer(_params, 0, sizeof(HParams), BufferMapAccess::WRITE_ONLY);
    std::memcpy(m->_mappedaddr, &p, sizeof(HParams));
    fxi->unmapStorageBuffer(m.get());
  };

  // mip0 — sample depth
  {
    HParams p{};
    p.dstoff = _offsets[0];
    p.dstw   = uint32_t(_baseW);
    p.dsth   = uint32_t(_baseH);
    p.depthw = uint32_t(dW);
    p.depthh = uint32_t(dH);
    write(p);
    ci->beginDispatchPhase();
    ci->bindStorageBuffer(_cs_mip0, 0, _params);
    ci->bindStorageBuffer(_cs_mip0, 1, _ssbo);
    ci->bindSampler(_cs_mip0, 2, depth.get());
    ci->dispatchCompute(_cs_mip0, (uint32_t(_baseW) + 7) / 8, (uint32_t(_baseH) + 7) / 8, 1);
    ci->endDispatchPhase();
  }

  // reduce mips 1..N-1 (SSBO -> SSBO)
  for (int i = 1; i < _mips; i++) {
    int w  = std::max(1, _baseW >> i);
    int h  = std::max(1, _baseH >> i);
    int pw = std::max(1, _baseW >> (i - 1));
    int ph = std::max(1, _baseH >> (i - 1));
    HParams p{};
    p.dstoff = _offsets[i];
    p.dstw   = uint32_t(w);
    p.dsth   = uint32_t(h);
    p.srcoff = _offsets[i - 1];
    p.srcw   = uint32_t(pw);
    p.srch   = uint32_t(ph);
    write(p);
    ci->beginDispatchPhase();
    ci->bindStorageBuffer(_cs_reduce, 0, _params);
    ci->bindStorageBuffer(_cs_reduce, 1, _ssbo);
    ci->dispatchCompute(_cs_reduce, (uint32_t(w) + 7) / 8, (uint32_t(h) + 7) / 8, 1);
    ci->endDispatchPhase();
  }
  _valid = true;

  // ORKID_DEBUG_HZB=1 — throttled sanity readback (the phases waited): confirm sane depth in [0,1],
  // mip0_center ~ the scene's center-pixel depth, coarsest_max ~ the farthest visible surface.
  static const bool s_dbg = (getenv("ORKID_DEBUG_HZB") != nullptr);
  static int s_ctr        = 0;
  if (s_dbg and ((s_ctr++ & 63) == 0)) {
    uint32_t total = _offsets[_mips];
    std::vector<float> buf(total);
    auto m = fxi->mapStorageBuffer(_ssbo, 0, size_t(total) * sizeof(float), BufferMapAccess::READ_ONLY);
    std::memcpy(buf.data(), m->_mappedaddr, size_t(total) * sizeof(float));
    fxi->unmapStorageBuffer(m.get());
    uint32_t cx          = uint32_t(_baseW) / 2;
    uint32_t cy          = uint32_t(_baseH) / 2;
    float    mip0_center = buf[_offsets[0] + cy * uint32_t(_baseW) + cx];
    float    coarsest    = buf[_offsets[_mips - 1]]; // smallest mip's max-depth over the whole screen
    // min + near-content fraction over all of mip0 — definitively separates "uniform
    // clear (1.0 everywhere)" from "real varied depth (some near surfaces < 0.99)".
    uint32_t mip0n   = uint32_t(_baseW) * uint32_t(_baseH);
    float    mip0min = 1.0f;
    uint32_t nnear   = 0;
    for (uint32_t i = 0; i < mip0n; i++) {
      float d = buf[_offsets[0] + i];
      if (d < mip0min) mip0min = d;
      if (d < 0.99f) nnear++;
    }
    printf("[hzb] base<%dx%d> mips<%d> mip0_center<%.5f> mip0_min<%.5f> near_frac<%.3f> coarsest_max<%.5f>\n",
           _baseW, _baseH, _mips, mip0_center, mip0min, float(nnear) / float(mip0n), coarsest);
  }
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
