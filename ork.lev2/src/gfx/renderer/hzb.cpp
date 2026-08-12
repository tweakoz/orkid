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
  // LAYERED mip0 only — the eye->center registration model (HZBBuilder::StereoReg) the
  // stereo reduction widens itself by. Parametric, never baked: the rig (IPD, per-eye
  // frustum, near/far) and the pyramid extent all move these.
  float    c0, c1;               // RESIDUAL window(z) in mip0 texels = c0 + c1/z_view
  float    za, zb;               // 1/z_view = za*d + zb  (d = stored window depth)
  uint32_t maxrad;               // hard cap on the gather radius, in mip0 texels
  // per-eye at-infinity NDC map (HZBBuilder::StereoReg): applied, not searched over.
  // Row-major 3x3 homography on (ndc_center, 1) — projective because a canted eye is not an
  // affine reparameterization of the center image.
  float    h0[9];                // left  eye
  float    h1[9];                // right eye
};

// Cost ceiling on the layered reduction's horizontal gather. A texel whose own depth needs a
// wider window than this cannot be registered against the center camera at all, so it is
// published as FAR (occludes nothing) rather than approximated — see cs_hzb_mip0_layered.
static constexpr uint32_t kHzbStereoMaxRadius = 16;

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

///////////////////////////////////////////////////////////////////////////////
// The layered kernel lives in its OWN shader module, not appended to the text above.
// Declaration ORDER decides descriptor binding numbers in this backend, so adding a
// sampler_set/interface to the shared text renumbers the MONO kernels' bindings — which
// silently re-points cs_hzb_mip0's params/pyramid buffers and poisons the dual-pass pyramid
// (observed: mono hypermesh rejection 73% -> 100%). Two modules, two independent orders.
///////////////////////////////////////////////////////////////////////////////

static std::string _hzb_text_layered() {
  return R"S(
fxconfig fxcfg_default {}
sampler_set sset_hzb_layered (descriptor_set 0) { sampler2DArray u_depth_layered; }
storage_interface hif_par (descriptor_set 0) { buffer layout(std430) hpar {
  uint p_dstoff; uint p_dstw; uint p_dsth; uint p_srcoff; uint p_srcw; uint p_srch; uint p_depthw; uint p_depthh;
  float p_c0; float p_c1; float p_za; float p_zb; uint p_maxrad;
  float p_h0_0; float p_h0_1; float p_h0_2; float p_h0_3; float p_h0_4;
  float p_h0_5; float p_h0_6; float p_h0_7; float p_h0_8;
  float p_h1_0; float p_h1_1; float p_h1_2; float p_h1_3; float p_h1_4;
  float p_h1_5; float p_h1_6; float p_h1_7; float p_h1_8; }; }
storage_interface hif_hzb (descriptor_set 0) { buffer layout(std430) hhzb { float HZB[]; }; }
compute_interface hiface_mip0L : sset_hzb_layered { storage { hif_par hif_hzb }
                                           inputs { layout(local_size_x = 8, local_size_y = 8); } }
////////////////////////////////////////
// mip0 (LAYERED — single-pass stereo). Same destination footprint as cs_hzb_mip0, but the
// source is the 2-layer ARRAY depth one multiview pass wrote — and, unlike the mono kernel,
// the source and the CONSUMER do not share a camera.
//
// THE COMBINE DIRECTION IS DERIVED FROM THE CONSUMER, not from the operator's name.
// hzb_box_occluded (lib_hzb, hmdflow_render.cpp) ends in `return znear_obj > occ` under
// standard-Z (smaller = nearer), so a LARGER occ value occludes LESS. max() is therefore the
// under-occlude direction, and it is why widening the reduction is always safe: a max over a
// SUPERSET of samples can only grow, and growing only ever removes occlusion.
//
// REGISTRATION IS THE WHOLE PROBLEM. The consumer indexes this pyramid with the CENTER
// camera, so texel t must hold the farthest surface the CENTER camera sees at t. The stored
// depth is what the EYES saw: the same world point sits at t +/- disparity(z) in an eye's
// image, so a same-texel max silently answers for the wrong point near a silhouette — it can
// report NEARER than the truth, which is the over-cull direction, and it moves with the head
// (the reported VR flicker). So the reduction gathers over the disparity window instead:
//
//   t_eye        = the at-infinity NDC map of t through THAT eye's projection (p_h*, a homography)
//   seed         = the both-eyes 2x2 max, each eye read at ITS OWN t_eye
//   rad(seed)    = c0 + c1/z(seed), in mip0 texels — the residual PARALLAX at that depth
//   result       = max over the +/-rad horizontal window around each eye's t_eye
//
// THE MAP IS APPLIED, NOT SEARCHED, AND IT IS PROJECTIVE. Only the parallax term shrinks with
// depth; the view-projection difference between an eye and the (averaged) center does not, and
// on a measured headset it is ~65 mip0 texels — larger than any affordable window, so folding
// it into rad made every texel exceed the cap and publish far. It has to be the EXACT map, too:
// an affine (scale+offset per axis) map covers asymmetric FOV but not the ~3.8 degrees of cant
// a real headset gives each display, and the ~20 texels that left over at infinity were alone
// past the cap — every texel published far, in the headset only, while every parallel-eye rig
// (novr, offscreen) solved the identity map and culled normally.
//
// WHY rad SIZED FROM THE SEED IS SUFFICIENT, not a heuristic: only the FARTHEST surface at t
// can decide the max, and disparity shrinks monotonically with depth. Any surface farther
// than the seed therefore has a SMALLER displacement than rad(seed) and is inside the window
// by construction; anything the window sweeps in that is nearer cannot lower a max. Samples
// pulled in from neighbours are never wrong to include — only missing a far one is.
//
// The gather is horizontal because eye separation is horizontal; the map and the residual are
// both fitted from the ACTUAL eye and center projections (HZBBuilder::computeStereoReg), so an
// asymmetric per-eye frustum is registered exactly rather than assumed away or swept over.
//
// A texel whose own depth needs a window wider than p_maxrad is published as FAR: at that
// point the pyramid cannot answer for the center camera at all, and "occludes nothing" is the
// only honest value. That is a bounded cost (content nearer than the cap's depth stops
// occluding under stereo), never an over-cull.
compute_shader cs_hzb_mip0_layered : hiface_mip0L {
  uint dx = gl_GlobalInvocationID.x;
  uint dy = gl_GlobalInvocationID.y;
  if (dx >= p_dstw) { return; }
  if (dy >= p_dsth) { return; }
  ivec2 mxd = ivec2(int(p_depthw) - 1, int(p_depthh) - 1);
  // this destination texel as CENTER-camera NDC, then through each eye's at-infinity map to
  // the depth texel that eye rasterized the same world direction into.
  float cnx = ((float(dx) + 0.5) / float(p_dstw)) * 2.0 - 1.0;
  float cny = ((float(dy) + 0.5) / float(p_dsth)) * 2.0 - 1.0;
  // the at-infinity map is PROJECTIVE (a canted display is not an affine reparameterization
  // of the center image), so it divides. A degenerate w means this texel's direction is on
  // or behind that eye's horizon: publish far, which occludes nothing.
  float lw = p_h0_6 * cnx + p_h0_7 * cny + p_h0_8;
  float rw = p_h1_6 * cnx + p_h1_7 * cny + p_h1_8;
  if (abs(lw) < 1e-9 || abs(rw) < 1e-9) {
    HZB[p_dstoff + dy * p_dstw + dx] = 1.0;
    return;
  }
  float lnx = (p_h0_0 * cnx + p_h0_1 * cny + p_h0_2) / lw;
  float lny = (p_h0_3 * cnx + p_h0_4 * cny + p_h0_5) / lw;
  float rnx = (p_h1_0 * cnx + p_h1_1 * cny + p_h1_2) / rw;
  float rny = (p_h1_3 * cnx + p_h1_4 * cny + p_h1_5) / rw;
  int   lx  = int(floor((lnx * 0.5 + 0.5) * float(p_depthw)));
  int   ly  = int(floor((lny * 0.5 + 0.5) * float(p_depthh)));
  int   rx  = int(floor((rnx * 0.5 + 0.5) * float(p_depthw)));
  int   ry  = int(floor((rny * 0.5 + 0.5) * float(p_depthh)));
  int   lx0 = clamp(lx,     0, mxd.x); int lx1 = clamp(lx + 1, 0, mxd.x);
  int   ly0 = clamp(ly,     0, mxd.y); int ly1 = clamp(ly + 1, 0, mxd.y);
  int   rx0 = clamp(rx,     0, mxd.x); int rx1 = clamp(rx + 1, 0, mxd.x);
  int   ry0 = clamp(ry,     0, mxd.y); int ry1 = clamp(ry + 1, 0, mxd.y);
  float l0 = texelFetch(u_depth_layered, ivec3(lx0, ly0, 0), 0).r;
  float l1 = texelFetch(u_depth_layered, ivec3(lx1, ly0, 0), 0).r;
  float l2 = texelFetch(u_depth_layered, ivec3(lx0, ly1, 0), 0).r;
  float l3 = texelFetch(u_depth_layered, ivec3(lx1, ly1, 0), 0).r;
  float r0 = texelFetch(u_depth_layered, ivec3(rx0, ry0, 1), 0).r;
  float r1 = texelFetch(u_depth_layered, ivec3(rx1, ry0, 1), 0).r;
  float r2 = texelFetch(u_depth_layered, ivec3(rx0, ry1, 1), 0).r;
  float r3 = texelFetch(u_depth_layered, ivec3(rx1, ry1, 1), 0).r;
  float seed = max(max(max(l0, l1), max(l2, l3)), max(max(r0, r1), max(r2, r3)));
  // residual parallax at the seed's depth, in mip0 texels. invz is clamped positive: a
  // cleared-to-far texel maps to 1/z ~ 0 and needs no window at all.
  float invz = max(p_za * seed + p_zb, 0.0);
  int   rad  = int(ceil(p_c0 + p_c1 * invz));
  if (rad < 0) { rad = 0; }
  if (rad > int(p_maxrad)) {
    HZB[p_dstoff + dy * p_dstw + dx] = 1.0; // unregisterable this close -> occlude nothing
    return;
  }
  float acc = seed;
  for (int o = 1; o <= rad; o++) {
    int lm0 = clamp(lx - o * 2,     0, mxd.x);
    int lm1 = clamp(lx - o * 2 + 1, 0, mxd.x);
    int lp0 = clamp(lx + o * 2,     0, mxd.x);
    int lp1 = clamp(lx + o * 2 + 1, 0, mxd.x);
    acc = max(acc, texelFetch(u_depth_layered, ivec3(lm0, ly0, 0), 0).r);
    acc = max(acc, texelFetch(u_depth_layered, ivec3(lm1, ly0, 0), 0).r);
    acc = max(acc, texelFetch(u_depth_layered, ivec3(lp0, ly0, 0), 0).r);
    acc = max(acc, texelFetch(u_depth_layered, ivec3(lp1, ly0, 0), 0).r);
    acc = max(acc, texelFetch(u_depth_layered, ivec3(lm0, ly1, 0), 0).r);
    acc = max(acc, texelFetch(u_depth_layered, ivec3(lm1, ly1, 0), 0).r);
    acc = max(acc, texelFetch(u_depth_layered, ivec3(lp0, ly1, 0), 0).r);
    acc = max(acc, texelFetch(u_depth_layered, ivec3(lp1, ly1, 0), 0).r);
    int rm0 = clamp(rx - o * 2,     0, mxd.x);
    int rm1 = clamp(rx - o * 2 + 1, 0, mxd.x);
    int rp0 = clamp(rx + o * 2,     0, mxd.x);
    int rp1 = clamp(rx + o * 2 + 1, 0, mxd.x);
    acc = max(acc, texelFetch(u_depth_layered, ivec3(rm0, ry0, 1), 0).r);
    acc = max(acc, texelFetch(u_depth_layered, ivec3(rm1, ry0, 1), 0).r);
    acc = max(acc, texelFetch(u_depth_layered, ivec3(rp0, ry0, 1), 0).r);
    acc = max(acc, texelFetch(u_depth_layered, ivec3(rp1, ry0, 1), 0).r);
    acc = max(acc, texelFetch(u_depth_layered, ivec3(rm0, ry1, 1), 0).r);
    acc = max(acc, texelFetch(u_depth_layered, ivec3(rm1, ry1, 1), 0).r);
    acc = max(acc, texelFetch(u_depth_layered, ivec3(rp0, ry1, 1), 0).r);
    acc = max(acc, texelFetch(u_depth_layered, ivec3(rp1, ry1, 1), 0).r);
  }
  HZB[p_dstoff + dy * p_dstw + dx] = acc;
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

void HZBBuilder::_ensureLayered(Context* ctx, int dW, int dH) {
  _ensure(ctx, dW, dH);
  if (not _cs_mip0_layered) {
    auto fxi = ctx->FXI();
    // compiled under its OWN module name so _ensure's two lookups stay exactly what they
    // were; the text is shared, so the layered entry can never drift from its mono twin.
    auto sh          = fxi->shaderFromShaderText("hzb_build_layered", _hzb_text_layered());
    _cs_mip0_layered = fxi->computeShader(sh, "cs_hzb_mip0_layered");
    OrkAssertI(
        _cs_mip0_layered != nullptr,
        "hzb: cs_hzb_mip0_layered did not resolve — the single-pass-stereo pyramid would fall "
        "back to a kernel that binds the 2-layer array depth as a plain sampler2D");
  }
}

///////////////////////////////////////////////////////////////////////////////
// EYE -> CENTER REGISTRATION MODEL. The stereo mip0 kernel needs to know how far a world
// point can move between an eye's image (where the depth was rasterized) and the center
// camera's image (which the cull indexes with). That displacement is affine in 1/z for ANY
// pair of perspective projections — the 1/z term is the eye offset (half IPD), the constant
// is whatever per-eye frustum asymmetry the rig has — so two probe depths pin it exactly.
//
// The probes are the frustum extremes: the near plane and (effectively) infinity. The true
// per-point displacement is a max of affine functions of 1/z, hence convex, so a chord fitted
// between the endpoints lies ABOVE it everywhere in between — the fit over-estimates inside
// the range, never under, and the range covers every depth a sample can have.
///////////////////////////////////////////////////////////////////////////////

HZBBuilder::StereoReg HZBBuilder::computeStereoReg(
    const CameraMatrices& eyeL,
    const CameraMatrices& eyeR,
    const CameraMatrices& center,
    int                   baseW,
    int                   baseH) {

  StereoReg reg;
  const fmtx4& pmtx = center.GetPMatrix();
  const fmtx4& ivp  = center.GetIVPMatrix();
  const fmtx4& vpc  = center.GetVPMatrix();
  const fmtx4& vpl  = eyeL.GetVPMatrix();
  const fmtx4& vpr  = eyeR.GetVPMatrix();

  // window depth of a view-space distance, through the camera's own projection (whatever
  // near/far/handedness convention it carries — nothing here assumes one).
  auto depth_of = [&](float z) -> float {
    fvec4 clip = fvec4(0.0f, 0.0f, -z, 1.0f).transform(pmtx);
    return (fabsf(clip.w) > 1e-12f) ? (clip.z / clip.w) : 1.0f;
  };
  // the near/far the projection actually encodes, recovered by bisection-free inversion of
  // the two probe depths below.
  float znear = 0.0f;
  {
    // pull the near distance out of the projection by walking the inverse-VP through the
    // near-plane NDC z. Cheaper and convention-proof: probe.
    fvec4 nearpt = fvec4(0.0f, 0.0f, 0.0f, 1.0f).transform(ivp); // ndc(0,0,0) -> world
    fvec4 vpt    = nearpt.transform(center.GetVMatrix());
    znear        = fabsf(vpt.z / ((fabsf(vpt.w) > 1e-12f) ? vpt.w : 1.0f));
  }
  if (not(znear > 1e-4f))
    return reg; // no usable projection -> caller must not publish a pyramid

  const float z1 = znear;          // nearest a rasterized sample can be
  const float z2 = znear * 100000.0f; // effectively infinity
  const float d1 = depth_of(z1);
  const float d2 = depth_of(z2);
  if (fabsf(d1 - d2) < 1e-9f)
    return reg;
  // 1/z = za*d + zb, exact for a perspective projection (window depth is affine in 1/z).
  reg._za = ((1.0f / z1) - (1.0f / z2)) / (d1 - d2);
  reg._zb = (1.0f / z1) - reg._za * d1;

  // where an eye puts the world point the CENTER camera puts at center-NDC (nx,ny,dnorm).
  //  null w => the point is behind that camera; the caller's grid never probes there.
  auto eye_ndc = [&](int e, float nx, float ny, float dnorm, float& ex, float& ey) -> bool {
    fvec4 wp = fvec4(nx, ny, dnorm, 1.0f).transform(ivp);
    if (fabsf(wp.w) < 1e-12f)
      return false;
    wp = wp * (1.0f / wp.w);
    fvec4 ec = fvec4(wp.x, wp.y, wp.z, 1.0f).transform(e ? vpr : vpl);
    if (ec.w <= 1e-6f)
      return false;
    ex = ec.x / ec.w;
    ey = ec.y / ec.w;
    return true;
  };

  ////////////////////////////////////////////////////////////////////////////
  // PART 1 — the at-infinity map, per eye. Two cameras see the same DIRECTIONS at infinity,
  //  so what is left there is purely the difference between the two view-projections: a
  //  direction is a projective function of center NDC, and its eye NDC is a projective
  //  function of the direction, so the composition is a HOMOGRAPHY — exactly, for any rig,
  //  including one whose displays are canted. Four correspondences determine a homography,
  //  and the map IS one, so the four NDC corners SOLVE it rather than fit it: nothing is
  //  left at infinity for the window below to carry.
  //
  //  This is a map and not part of the window because it does not shrink with depth: a
  //  runtime's per-eye asymmetric FOV alone measured 65 mip0 texels on a headset. An AFFINE
  //  map (the shape this used to solve) covers that asymmetry but cannot express cant, and
  //  the ~20 texels a measured headset's 3.8-degree cant left over were by themselves past
  //  the reduction's cap — every texel published far, in the headset only.
  //
  //  Falls back to identity for any eye whose corner probes do not resolve, and the residual
  //  fit then charges the full displacement to the window (where the assert below sees it).
  ////////////////////////////////////////////////////////////////////////////
  const float d_inf = depth_of(z2);
  const float cx[4] = {-1.0f, +1.0f, +1.0f, -1.0f};
  const float cy[4] = {-1.0f, -1.0f, +1.0f, +1.0f};
  for (int e = 0; e < 2; e++) {
    // rows: x = h0 u + h1 v + h2 - h6 u x - h7 v x ; y = h3 u + h4 v + h5 - h6 u y - h7 v y
    double A[8][9] = {};
    bool   ok      = true;
    for (int i = 0; i < 4 and ok; i++) {
      float ex, ey;
      ok = eye_ndc(e, cx[i], cy[i], d_inf, ex, ey);
      if (not ok)
        break;
      double u = cx[i], v = cy[i];
      double* rx = A[i * 2 + 0];
      double* ry = A[i * 2 + 1];
      rx[0] = u; rx[1] = v; rx[2] = 1.0; rx[6] = -u * ex; rx[7] = -v * ex; rx[8] = ex;
      ry[3] = u; ry[4] = v; ry[5] = 1.0; ry[6] = -u * ey; ry[7] = -v * ey; ry[8] = ey;
    }
    if (not ok)
      continue; // identity map stands
    // gaussian elimination with partial pivoting; the corner basis is well conditioned, so a
    // singular system means the probes are degenerate and identity is the honest answer.
    for (int c = 0; c < 8 and ok; c++) {
      int piv = c;
      for (int r = c + 1; r < 8; r++)
        if (fabs(A[r][c]) > fabs(A[piv][c]))
          piv = r;
      if (fabs(A[piv][c]) < 1e-12) {
        ok = false;
        break;
      }
      if (piv != c)
        for (int k = c; k < 9; k++)
          std::swap(A[c][k], A[piv][k]);
      for (int r = 0; r < 8; r++) {
        if (r == c)
          continue;
        double f = A[r][c] / A[c][c];
        for (int k = c; k < 9; k++)
          A[r][k] -= f * A[c][k];
      }
    }
    if (not ok)
      continue; // identity map stands
    for (int k = 0; k < 8; k++)
      reg._regH[e][k] = float(A[k][8] / A[k][k]);
    reg._regH[e][8] = 1.0f;
  }
  // the map, as the residual fit and the kernel both apply it
  auto apply_map = [&](int e, float nx, float ny, float& px, float& py) -> bool {
    const float* h = reg._regH[e];
    float        w = h[6] * nx + h[7] * ny + h[8];
    if (fabsf(w) < 1e-9f)
      return false;
    px = (h[0] * nx + h[1] * ny + h[2]) / w;
    py = (h[3] * nx + h[4] * ny + h[5]) / w;
    return true;
  };

  ////////////////////////////////////////////////////////////////////////////
  // PART 2 — what the map does NOT cover, as a window. Worst leftover displacement at each
  //  probe depth over a grid spanning the center camera's whole NDC square (the pyramid
  //  covers exactly that). At infinity this is ~0 for an axis-aligned rig and small for a
  //  canted one; near the eye it is the IPD parallax, which is what the window is for.
  ////////////////////////////////////////////////////////////////////////////
  auto worst_at = [&](float z) -> float {
    float dnorm = depth_of(z);
    float worst = 0.0f;
    for (int iy = 0; iy <= 8; iy++) {
      for (int ix = 0; ix <= 8; ix++) {
        float nx = -1.0f + 2.0f * (float(ix) / 8.0f);
        float ny = -1.0f + 2.0f * (float(iy) / 8.0f);
        for (int e = 0; e < 2; e++) {
          float ex, ey, px, py;
          if (not eye_ndc(e, nx, ny, dnorm, ex, ey))
            continue;
          if (not apply_map(e, nx, ny, px, py))
            continue;
          // NDC -> mip0 texels (uv = ndc*0.5+0.5, uv -> texel = *base)
          float tx = fabsf(ex - px) * 0.5f * float(baseW);
          float ty = fabsf(ey - py) * 0.5f * float(baseH);
          worst    = std::max(worst, std::max(tx, ty));
        }
      }
    }
    return worst;
  };
  const float w1 = worst_at(z1);
  const float w2 = worst_at(z2);
  // chord through (1/z1,w1) and (1/z2,w2)
  const float iz1 = 1.0f / z1, iz2 = 1.0f / z2;
  reg._c1 = (w1 - w2) / (iz1 - iz2);
  reg._c0 = w1 - reg._c1 * iz1;
  if (reg._c1 < 0.0f)
    reg._c1 = 0.0f;
  if (reg._c0 < 0.0f)
    reg._c0 = 0.0f;
  reg._ok = true;
  return reg;
}

///////////////////////////////////////////////////////////////////////////////
// SINGLE-PASS STEREO. Structurally identical to build() — mip0 from the depth image, then
// the same SSBO reduce chain — with the ONE difference that mip0 samples the 2-layer array
// depth and combines the two eyes REGISTERED to the center camera (cs_hzb_mip0_layered).
//
// A pyramid whose registration cannot be computed is not published at all: _valid stays
// false, every consumer reads "no occlusion", and the caller says so out loud. Publishing an
// unregistered stereo pyramid is what removes geometry the eyes can see.
///////////////////////////////////////////////////////////////////////////////

void HZBBuilder::buildLayered(
    Context*              ctx,
    texture_ptr_t         depth,
    int                   num_layers,
    const CameraMatrices* eyeL,
    const CameraMatrices* eyeR,
    const CameraMatrices* center) {
  if (not depth)
    return;
  OrkAssertI(
      eyeL and eyeR and center,
      "hzb: buildLayered needs the eye AND center cameras — the stereo reduction is registered "
      "against the camera the cull indexes the pyramid with, and cannot be guessed");
  auto reg = computeStereoReg(*eyeL, *eyeR, *center, std::max(1, depth->_width / 2), std::max(1, depth->_height / 2));
  if (not reg._ok) {
    _valid = false;
    // OWNER LAW (aug11): no quiet inert modes. An unsolvable registration in a live
    // stereo frame means the caller handed us cameras the cull cannot be registered
    // against — that is a defect at the call site, not a condition to ride out as
    // frustum-only. Assert with the state so the failing run names its own cause.
    OrkAssertI(
        false,
        "hzb: stereo registration UNSOLVABLE from the supplied cameras — the layered pyramid "
        "would be inert (in==out). This is a caller defect: the eye/center cameras handed to "
        "buildLayered do not admit a projective registration. Inspect the [SPVR:HZB] lines "
        "above for the camera state that led here.");
    return;
  }
  // OWNER LAW: no quiet inert modes. _c0 is what the at-infinity map did NOT absorb, and the
  // kernel publishes FAR for any texel whose window exceeds the cap — so a _c0 at or past the
  // cap is not a degraded pyramid, it is a uniformly-1.0 one: occlusion inert (in==out) on
  // every texel, every frame, with no leg failing and no consumer able to tell. This fired for
  // real: an affine at-infinity map left ~20 texels against a cap of 16 on a canted headset
  // rig, and the run reported a healthy BUILT line the whole time.
  if (not(reg._c0 < float(kHzbStereoMaxRadius))) {
    printf("[hzb] stereo registration residual c0<%.3f> texels vs cap<%u> — the at-infinity map "
           "left more than the reduction can gather over, so EVERY mip0 texel publishes far\n",
           reg._c0,
           kHzbStereoMaxRadius);
    fflush(stdout);
    OrkAssertI(
        false,
        "hzb: the eye->center at-infinity registration leaves a residual at or past the "
        "reduction's gather cap — every texel would overflow and publish far, making the "
        "layered pyramid uniformly 1.0 and stereo occlusion inert (in==out) with no leg "
        "failing. What survives at infinity belongs in the MAP, not the window; the printf "
        "above names the residual.");
  }
  // The kernel reads layer 0 and layer 1 by literal index. A group with any other layer count
  //  would silently reduce over the wrong set (or out of range), so this is a precondition,
  //  not a preference — single-pass stereo is two views by construction.
  OrkAssertI(
      num_layers == 2,
      "hzb: buildLayered called with a depth image that does not have exactly 2 array layers — "
      "cs_hzb_mip0_layered combines layer 0 and layer 1 by literal index");
  int dW = depth->_width;
  int dH = depth->_height;
  if (dW < 2 or dH < 2)
    return;
  _ensureLayered(ctx, dW, dH);
  if (not _ssbo)
    return;

  auto fxi   = ctx->FXI();
  auto ci    = ctx->CI();
  auto write = [&](const HParams& p) {
    auto m = fxi->mapStorageBuffer(_params, 0, sizeof(HParams), BufferMapAccess::WRITE_ONLY);
    std::memcpy(m->_mappedaddr, &p, sizeof(HParams));
    fxi->unmapStorageBuffer(m.get());
  };

  // mip0 — sample BOTH eye layers of the array depth and combine, registered (both-eyes-agree)
  {
    HParams p{};
    p.dstoff = _offsets[0];
    p.dstw   = uint32_t(_baseW);
    p.dsth   = uint32_t(_baseH);
    p.depthw = uint32_t(dW);
    p.depthh = uint32_t(dH);
    p.c0     = reg._c0;
    p.c1     = reg._c1;
    p.za     = reg._za;
    p.zb     = reg._zb;
    p.maxrad = kHzbStereoMaxRadius;
    for (int k = 0; k < 9; k++) {
      p.h0[k] = reg._regH[0][k];
      p.h1[k] = reg._regH[1][k];
    }
    _stereoReg = reg;
    write(p);
    ci->beginDispatchPhase("hzb:build");
    ci->bindStorageBuffer(_cs_mip0_layered, 0, _params);
    ci->bindStorageBuffer(_cs_mip0_layered, 1, _ssbo);
    ci->bindSampler(_cs_mip0_layered, 2, depth.get());
    ci->dispatchCompute(_cs_mip0_layered, (uint32_t(_baseW) + 7) / 8, (uint32_t(_baseH) + 7) / 8, 1);
    ci->endDispatchPhase();
  }

  // reduce mips 1..N-1 — layer-agnostic once mip0 has combined, so the mono kernel verbatim.
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
    ci->beginDispatchPhase("hzb:build");
    ci->bindStorageBuffer(_cs_reduce, 0, _params);
    ci->bindStorageBuffer(_cs_reduce, 1, _ssbo);
    ci->dispatchCompute(_cs_reduce, (uint32_t(w) + 7) / 8, (uint32_t(h) + 7) / 8, 1);
    ci->endDispatchPhase();
  }
  _valid = true;
  _debugReport(ctx, "hzb:layered", num_layers);
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
    ci->beginDispatchPhase("hzb:build");
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
    ci->beginDispatchPhase("hzb:build");
    ci->bindStorageBuffer(_cs_reduce, 0, _params);
    ci->bindStorageBuffer(_cs_reduce, 1, _ssbo);
    ci->dispatchCompute(_cs_reduce, (uint32_t(w) + 7) / 8, (uint32_t(h) + 7) / 8, 1);
    ci->endDispatchPhase();
  }
  _valid = true;
  _debugReport(ctx, "hzb", 1);
}

///////////////////////////////////////////////////////////////////////////////

void HZBBuilder::_debugReport(Context* ctx, const char* tag, int num_layers) {
  auto fxi = ctx->FXI();
  // ORKID_DEBUG_HZB=1 — throttled sanity readback (the phases waited): confirm sane depth in [0,1],
  // mip0_center ~ the scene's center-pixel depth, coarsest_max ~ the farthest visible surface.
  // The readback is a full pyramid copyToHost — measurably expensive (~35 FPS on a heavy scene),
  // so no frame-rate number is ever taken from a run with this armed.
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
    printf("[%s] base<%dx%d> mips<%d> layers<%d> mip0_center<%.5f> mip0_min<%.5f> near_frac<%.3f> coarsest_max<%.5f>\n",
           tag, _baseW, _baseH, _mips, num_layers, mip0_center, mip0min, float(nnear) / float(mip0n), coarsest);
    for (int i = 0; i < _mips; i++) {
      int w = std::max(1, _baseW >> i), h = std::max(1, _baseH >> i);
      float mn = 1e9f, mx = -1e9f; uint32_t nfar = 0;
      for (int k = 0; k < w * h; k++) {
        float d = buf[_offsets[i] + uint32_t(k)];
        mn = std::min(mn, d); mx = std::max(mx, d);
        if (d >= 0.99999f) nfar++;
      }
      printf("[hzbmip] %d <%dx%d> min<%.6f> max<%.6f> far_frac<%.3f>\n", i, w, h, mn, mx, float(nfar) / float(w * h));
    }
    fflush(stdout);
  }
  // ORKID_HZB_DUMP=<path> — one-shot raw pyramid dump for the offline cull oracle.
  static const char* s_dumppath = getenv("ORKID_HZB_DUMP");
  static bool        s_dumped   = false;
  static int         s_dumpctr  = 0;
  if (s_dumppath and not s_dumped and _valid and (++s_dumpctr > 400)) {
    s_dumped       = true;
    uint32_t total = _offsets[_mips];
    std::vector<float> buf(total);
    auto m = fxi->mapStorageBuffer(_ssbo, 0, size_t(total) * sizeof(float), BufferMapAccess::READ_ONLY);
    std::memcpy(buf.data(), m->_mappedaddr, size_t(total) * sizeof(float));
    fxi->unmapStorageBuffer(m.get());
    FILE* f = fopen(s_dumppath, "wb");
    if (f) {
      int32_t hdr[3] = {_baseW, _baseH, _mips};
      fwrite(hdr, sizeof(int32_t), 3, f);
      fwrite(_offsets.data(), sizeof(uint32_t), _offsets.size(), f);
      fwrite(buf.data(), sizeof(float), total, f);
      fclose(f);
      printf("[hzb] DUMPED pyramid -> %s (base %dx%d mips %d total %u)\n", s_dumppath, _baseW, _baseH, _mips, total);
      fflush(stdout);
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
