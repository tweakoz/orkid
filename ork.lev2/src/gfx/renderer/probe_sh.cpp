////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
//
// probe_sh.cpp — GPU L2 spherical-harmonic projection of a probe cubemap. See probe_sh.h
// for the SSBO layout and coefficient order.
//
// The integral is sum over all 6*dim*dim cube texels of L(d) * Y_i(d) * dw, with dw the
// exact texel solid angle (area-element form) so the result is resolution-independent
// and the weights sum to 4*pi. Reduction is two-stage rather than shared-memory/atomic:
// stage A gives each of kProbeSHTasks invocations a strided slice of the texels and has
// it write its own 9 partial sums; stage B sums the partials serially into the probe's
// slot. Serial stage B keeps the sum order fixed, so a given cubemap always projects to
// bit-identical coefficients.
//
// Directions are generated with the GL cube-face convention purely as a sphere
// parameterization — the cube is sampled BY DIRECTION (textureLod, mandatory: compute
// has no implicit-LOD derivatives), so the actual face layout of the RTT cannot affect
// the result.
//
////////////////////////////////////////////////////////////////
#include <ork/lev2/gfx/renderer/probe_sh.h>
#include <ork/lev2/gfx/ci.h>
#include <ork/lev2/gfx/image.h>
#include <ork/lev2/gfx/shadman.h>
#include <cmath>
#include <cstring>
#include <vector>

namespace ork::lev2 {

///////////////////////////////////////////////////////////////////////////////

static constexpr int kProbeSHTasks     = 1024; // parallel accumulators (16 groups of 64)
static constexpr int kProbeSHTaskGroup = 64;

struct AccumParams {
  uint32_t dim, ntasks, pad0, pad1;
};
struct EquiParams {
  uint32_t w, h, ntasks;
  float decode;
};
struct ReduceParams {
  uint32_t ntasks, slot, pad0, pad1;
};

///////////////////////////////////////////////////////////////////////////////

static std::string _accum_text() {
  return R"S(
fxconfig fxcfg_default {}
sampler_set sset_psh (descriptor_set 0) { samplerCube u_cube; }
storage_interface pif_par  (descriptor_set 0) { buffer layout(std430) ppar {
  uint p_dim; uint p_ntasks; uint p_pad0; uint p_pad1; }; }
storage_interface pif_part (descriptor_set 0) { buffer layout(std430) ppart { float PART[]; }; }
compute_interface piface_accum : sset_psh { storage { pif_par pif_part }
                                            inputs { layout(local_size_x = 64); } }
////////////////////////////////////////
compute_shader cs_probe_sh_accum : piface_accum {
  uint t = gl_GlobalInvocationID.x;
  if (t >= p_ntasks) { return; }
  uint dim   = p_dim;
  uint fsz   = dim * dim;
  uint total = 6u * fsz;
  float invdim = 1.0 / float(dim);

  vec3 a0 = vec3(0.0, 0.0, 0.0);
  vec3 a1 = vec3(0.0, 0.0, 0.0);
  vec3 a2 = vec3(0.0, 0.0, 0.0);
  vec3 a3 = vec3(0.0, 0.0, 0.0);
  vec3 a4 = vec3(0.0, 0.0, 0.0);
  vec3 a5 = vec3(0.0, 0.0, 0.0);
  vec3 a6 = vec3(0.0, 0.0, 0.0);
  vec3 a7 = vec3(0.0, 0.0, 0.0);
  vec3 a8 = vec3(0.0, 0.0, 0.0);

  for (uint i = t; i < total; i = i + p_ntasks) {
    uint f  = i / fsz;
    uint r  = i - f * fsz;
    uint py = r / dim;
    uint px = r - py * dim;
    float u = (float(px) + 0.5) * 2.0 * invdim - 1.0;
    float v = (float(py) + 0.5) * 2.0 * invdim - 1.0;
    vec3 d;
    if (f == 0u) { d = vec3( 1.0,  -v,  -u); }
    else if (f == 1u) { d = vec3(-1.0,  -v,   u); }
    else if (f == 2u) { d = vec3(   u, 1.0,   v); }
    else if (f == 3u) { d = vec3(   u,-1.0,  -v); }
    else if (f == 4u) { d = vec3(   u,  -v, 1.0); }
    else { d = vec3(  -u,  -v,-1.0); }
    d = normalize(d);
    // exact texel solid angle (area-element differences over the texel's [-1,1] footprint)
    float x0 = u - invdim;
    float x1 = u + invdim;
    float y0 = v - invdim;
    float y1 = v + invdim;
    float dw = atan(x0 * y0, sqrt(x0 * x0 + y0 * y0 + 1.0))
             - atan(x0 * y1, sqrt(x0 * x0 + y1 * y1 + 1.0))
             - atan(x1 * y0, sqrt(x1 * x1 + y0 * y0 + 1.0))
             + atan(x1 * y1, sqrt(x1 * x1 + y1 * y1 + 1.0));
    vec3 L = textureLod(u_cube, d, 0.0).rgb * dw;
    a0 = a0 + L * 0.2820948;
    a1 = a1 + L * (0.4886025 * d.y);
    a2 = a2 + L * (0.4886025 * d.z);
    a3 = a3 + L * (0.4886025 * d.x);
    a4 = a4 + L * (1.0925484 * d.x * d.y);
    a5 = a5 + L * (1.0925484 * d.y * d.z);
    a6 = a6 + L * (0.3153916 * (3.0 * d.z * d.z - 1.0));
    a7 = a7 + L * (1.0925484 * d.x * d.z);
    a8 = a8 + L * (0.5462742 * (d.x * d.x - d.y * d.y));
  }

  uint o = t * 36u;
  PART[o +  0u] = a0.x; PART[o +  1u] = a0.y; PART[o +  2u] = a0.z; PART[o +  3u] = 0.0;
  PART[o +  4u] = a1.x; PART[o +  5u] = a1.y; PART[o +  6u] = a1.z; PART[o +  7u] = 0.0;
  PART[o +  8u] = a2.x; PART[o +  9u] = a2.y; PART[o + 10u] = a2.z; PART[o + 11u] = 0.0;
  PART[o + 12u] = a3.x; PART[o + 13u] = a3.y; PART[o + 14u] = a3.z; PART[o + 15u] = 0.0;
  PART[o + 16u] = a4.x; PART[o + 17u] = a4.y; PART[o + 18u] = a4.z; PART[o + 19u] = 0.0;
  PART[o + 20u] = a5.x; PART[o + 21u] = a5.y; PART[o + 22u] = a5.z; PART[o + 23u] = 0.0;
  PART[o + 24u] = a6.x; PART[o + 25u] = a6.y; PART[o + 26u] = a6.z; PART[o + 27u] = 0.0;
  PART[o + 28u] = a7.x; PART[o + 29u] = a7.y; PART[o + 30u] = a7.z; PART[o + 31u] = 0.0;
  PART[o + 32u] = a8.x; PART[o + 33u] = a8.y; PART[o + 34u] = a8.z; PART[o + 35u] = 0.0;
}
)S";
}

///////////////////////////////////////////////////////////////////////////////
// EQUIRECT accumulator. Its own shader FILE, not a second kernel in the cube
// file: a kernel that binds a SUBSET of a file's storage_interfaces gets a
// sparse descriptor set and the Metal pipeline compiler rejects it.
//
// The integral is over the SOURCE grid exactly — one sample per snapshot texel,
// no resampling — because the sun disc occupies a handful of texels and a
// coarser sphere grid would either miss it or count it many times over. Texel
// solid angle is sin(theta) dtheta dphi, which sums to 4*pi.
//
// The direction is the one ps_sky_equirect WROTE at that texel, including its
// x mirror, so the coefficients come out in world space.
///////////////////////////////////////////////////////////////////////////////

static std::string _equi_text() {
  return R"S(
fxconfig fxcfg_default {}
sampler_set sset_pshe (descriptor_set 0) { sampler2D u_equi; }
storage_interface eif_par  (descriptor_set 0) { buffer layout(std430) epar {
  uint e_w; uint e_h; uint e_ntasks; float e_decode; }; }
storage_interface eif_part (descriptor_set 0) { buffer layout(std430) epart { float PART[]; }; }
compute_interface eiface_accum : sset_pshe { storage { eif_par eif_part }
                                             inputs { layout(local_size_x = 64); } }
////////////////////////////////////////
compute_shader cs_probe_sh_equi : eiface_accum {
  uint t = gl_GlobalInvocationID.x;
  if (t >= e_ntasks) { return; }
  uint w     = e_w;
  uint h     = e_h;
  uint total = w * h;
  float invw = 1.0 / float(w);
  float invh = 1.0 / float(h);
  // dtheta * dphi, constant over the grid; sin(theta) is the per-row factor
  float dsolid = (3.14159265 * invh) * (6.28318531 * invw);

  vec3 a0 = vec3(0.0, 0.0, 0.0);
  vec3 a1 = vec3(0.0, 0.0, 0.0);
  vec3 a2 = vec3(0.0, 0.0, 0.0);
  vec3 a3 = vec3(0.0, 0.0, 0.0);
  vec3 a4 = vec3(0.0, 0.0, 0.0);
  vec3 a5 = vec3(0.0, 0.0, 0.0);
  vec3 a6 = vec3(0.0, 0.0, 0.0);
  vec3 a7 = vec3(0.0, 0.0, 0.0);
  vec3 a8 = vec3(0.0, 0.0, 0.0);

  for (uint i = t; i < total; i = i + e_ntasks) {
    uint py = i / w;
    uint px = i - py * w;
    float u = (float(px) + 0.5) * invw;
    float v = (float(py) + 0.5) * invh;
    float theta = v * 3.14159265;
    float phi   = u * 6.28318531 - 3.14159265;
    float st    = sin(theta);
    float ct    = cos(theta);
    // ps_sky_equirect: skyEquirectUV2Dir(uv) then rd.x = -rd.x
    vec3 d = vec3(-(st * cos(phi)), ct, st * sin(phi));
    float dw = st * dsolid;
    vec3 L = textureLod(u_equi, vec2(u, v), 0.0).rgb * (dw * e_decode);
    a0 = a0 + L * 0.2820948;
    a1 = a1 + L * (0.4886025 * d.y);
    a2 = a2 + L * (0.4886025 * d.z);
    a3 = a3 + L * (0.4886025 * d.x);
    a4 = a4 + L * (1.0925484 * d.x * d.y);
    a5 = a5 + L * (1.0925484 * d.y * d.z);
    a6 = a6 + L * (0.3153916 * (3.0 * d.z * d.z - 1.0));
    a7 = a7 + L * (1.0925484 * d.x * d.z);
    a8 = a8 + L * (0.5462742 * (d.x * d.x - d.y * d.y));
  }

  uint o = t * 36u;
  PART[o +  0u] = a0.x; PART[o +  1u] = a0.y; PART[o +  2u] = a0.z; PART[o +  3u] = 0.0;
  PART[o +  4u] = a1.x; PART[o +  5u] = a1.y; PART[o +  6u] = a1.z; PART[o +  7u] = 0.0;
  PART[o +  8u] = a2.x; PART[o +  9u] = a2.y; PART[o + 10u] = a2.z; PART[o + 11u] = 0.0;
  PART[o + 12u] = a3.x; PART[o + 13u] = a3.y; PART[o + 14u] = a3.z; PART[o + 15u] = 0.0;
  PART[o + 16u] = a4.x; PART[o + 17u] = a4.y; PART[o + 18u] = a4.z; PART[o + 19u] = 0.0;
  PART[o + 20u] = a5.x; PART[o + 21u] = a5.y; PART[o + 22u] = a5.z; PART[o + 23u] = 0.0;
  PART[o + 24u] = a6.x; PART[o + 25u] = a6.y; PART[o + 26u] = a6.z; PART[o + 27u] = 0.0;
  PART[o + 28u] = a7.x; PART[o + 29u] = a7.y; PART[o + 30u] = a7.z; PART[o + 31u] = 0.0;
  PART[o + 32u] = a8.x; PART[o + 33u] = a8.y; PART[o + 34u] = a8.z; PART[o + 35u] = 0.0;
}
)S";
}

///////////////////////////////////////////////////////////////////////////////

static std::string _reduce_text() {
  return R"S(
fxconfig fxcfg_default {}
storage_interface rif_par  (descriptor_set 0) { buffer layout(std430) rpar {
  uint r_ntasks; uint r_slot; uint r_pad0; uint r_pad1; }; }
storage_interface rif_part (descriptor_set 0) { buffer layout(std430) rpart { float PART[]; }; }
storage_interface rif_sh   (descriptor_set 0) { buffer layout(std430) rsh   { float SH[];   }; }
compute_interface riface_reduce { storage { rif_par rif_part rif_sh }
                                  inputs { layout(local_size_x = 36); } }
////////////////////////////////////////
// one invocation per (coefficient,channel) float of the destination slot; the
// unused w lane of each vec4 is zeroed so the block reads cleanly as vec4[].
compute_shader cs_probe_sh_reduce : riface_reduce {
  uint k = gl_GlobalInvocationID.x;
  if (k >= 36u) { return; }
  uint c  = k / 4u;
  uint ch = k - c * 4u;
  float s = 0.0;
  if (ch < 3u) {
    for (uint t = 0u; t < r_ntasks; t++) {
      s = s + PART[t * 36u + k];
    }
  }
  SH[r_slot * 36u + k] = s;
}
)S";
}

///////////////////////////////////////////////////////////////////////////////

void ProbeSHProjector::_ensure(Context* ctx) {
  _ctx     = ctx;
  auto fxi = ctx->FXI();
  if (not _cs_accum) {
    auto sh_a = fxi->shaderFromShaderText("probe_sh_accum", _accum_text());
    _cs_accum = fxi->computeShader(sh_a, "cs_probe_sh_accum");
    OrkAssertI(_cs_accum, "probe SH: cs_probe_sh_accum failed to compile (samplerCube in compute)");
    auto sh_e = fxi->shaderFromShaderText("probe_sh_equi", _equi_text());
    _cs_equi  = fxi->computeShader(sh_e, "cs_probe_sh_equi");
    OrkAssertI(_cs_equi, "probe SH: cs_probe_sh_equi failed to compile (sampler2D in compute)");
    auto sh_r  = fxi->shaderFromShaderText("probe_sh_reduce", _reduce_text());
    _cs_reduce = fxi->computeShader(sh_r, "cs_probe_sh_reduce");
    OrkAssertI(_cs_reduce, "probe SH: cs_probe_sh_reduce failed to compile");
    // BAR: tiny, CPU-written per dispatch, GPU-read once.
    _params_accum  = fxi->createStorageBuffer(sizeof(AccumParams), StorageBufferUsage::DEFAULT, BufferResidency::BAR);
    _params_equi   = fxi->createStorageBuffer(sizeof(EquiParams), StorageBufferUsage::DEFAULT, BufferResidency::BAR);
    _params_reduce = fxi->createStorageBuffer(sizeof(ReduceParams), StorageBufferUsage::DEFAULT, BufferResidency::BAR);
    // DEVICE: GPU-written then GPU-read within the same projection, never on the CPU.
    _ssbo_partials = fxi->createStorageBuffer(
        size_t(kProbeSHTasks) * size_t(kProbeSHFloatsPerProbe) * sizeof(float),
        StorageBufferUsage::DEFAULT,
        BufferResidency::DEVICE);
    // DEVICE, allocated once at full capacity: it accumulates every probe's baked
    // coefficients across frames, so a reallocation would discard them all.
    size_t sh_bytes = size_t(kProbeSHMaxSlots) * size_t(kProbeSHFloatsPerProbe) * sizeof(float);
    _ssbo_sh        = fxi->createStorageBuffer(sh_bytes, StorageBufferUsage::DEFAULT, BufferResidency::DEVICE);
    // zero the whole capacity once: a slot that has been claimed but not yet
    // captured must read as "no radiance", never as VRAM garbage.
    auto m = fxi->mapStorageBuffer(_ssbo_sh, 0, sh_bytes, BufferMapAccess::WRITE_ONLY);
    std::memset(m->_mappedaddr, 0, sh_bytes);
    fxi->unmapStorageBuffer(m.get());
  }
}

///////////////////////////////////////////////////////////////////////////////

void ProbeSHProjector::project(Context* ctx, texture_ptr_t cubetex, int face_dim, int slot) {
  OrkAssertI(cubetex, "probe SH: projection requested with no cubemap texture");
  OrkAssertI(slot >= 0, "probe SH: projection requested with an unassigned probe slot");
  OrkAssertI(slot < kProbeSHMaxSlots, "probe SH: probe slot exceeds kProbeSHMaxSlots");
  if (face_dim <= 0)
    face_dim = kProbeSHDefaultDim;
  _ensure(ctx);

  auto fxi = ctx->FXI();
  auto ci  = ctx->CI();

  {
    AccumParams p{};
    p.dim    = uint32_t(face_dim);
    p.ntasks = uint32_t(kProbeSHTasks);
    auto m   = fxi->mapStorageBuffer(_params_accum, 0, sizeof(AccumParams), BufferMapAccess::WRITE_ONLY);
    std::memcpy(m->_mappedaddr, &p, sizeof(AccumParams));
    fxi->unmapStorageBuffer(m.get());

    ci->beginDispatchPhase();
    ci->bindStorageBuffer(_cs_accum, 0, _params_accum);
    ci->bindStorageBuffer(_cs_accum, 1, _ssbo_partials);
    ci->bindSampler(_cs_accum, 2, cubetex.get());
    ci->dispatchCompute(_cs_accum, uint32_t(kProbeSHTasks / kProbeSHTaskGroup), 1, 1);
    ci->endDispatchPhase();
  }

  _reduceInto(ctx, slot);
  _debugDump(ctx, slot, "cube", face_dim, face_dim);
}

///////////////////////////////////////////////////////////////////////////////

void ProbeSHProjector::projectEquirect(
    Context* ctx,            //
    texture_ptr_t equitex,   //
    int width,               //
    int height,              //
    int slot,                //
    float decode) {          //

  OrkAssertI(equitex, "probe SH: equirect projection requested with no source texture");
  OrkAssertIFMT(
      width >= 2 and height >= 2,
      "probe SH: equirect projection source is %dx%d - too small to carry a sphere",
      width,
      height);
  OrkAssertI(slot >= 0, "probe SH: equirect projection requested with an unassigned probe slot");
  OrkAssertI(slot < kProbeSHMaxSlots, "probe SH: probe slot exceeds kProbeSHMaxSlots");
  // A zero decode would silently zero every coefficient — the exact failure the
  // capture pre-scale exists to prevent.
  OrkAssertIFMT(decode > 0.0f, "probe SH: equirect projection given a non-positive capture decode (%g)", double(decode));
  _ensure(ctx);

  auto fxi = ctx->FXI();
  auto ci  = ctx->CI();

  {
    EquiParams p{};
    p.w      = uint32_t(width);
    p.h      = uint32_t(height);
    p.ntasks = uint32_t(kProbeSHTasks);
    p.decode = decode;
    auto m   = fxi->mapStorageBuffer(_params_equi, 0, sizeof(EquiParams), BufferMapAccess::WRITE_ONLY);
    std::memcpy(m->_mappedaddr, &p, sizeof(EquiParams));
    fxi->unmapStorageBuffer(m.get());

    ci->beginDispatchPhase();
    ci->bindStorageBuffer(_cs_equi, 0, _params_equi);
    ci->bindStorageBuffer(_cs_equi, 1, _ssbo_partials);
    ci->bindSampler(_cs_equi, 2, equitex.get());
    ci->dispatchCompute(_cs_equi, uint32_t(kProbeSHTasks / kProbeSHTaskGroup), 1, 1);
    ci->endDispatchPhase();
  }

  _reduceInto(ctx, slot);
  _debugDump(ctx, slot, "equi", width, height);
}

///////////////////////////////////////////////////////////////////////////////

void ProbeSHProjector::_reduceInto(Context* ctx, int slot) {
  auto fxi = ctx->FXI();
  auto ci  = ctx->CI();
  ReduceParams p{};
  p.ntasks = uint32_t(kProbeSHTasks);
  p.slot   = uint32_t(slot);
  auto m   = fxi->mapStorageBuffer(_params_reduce, 0, sizeof(ReduceParams), BufferMapAccess::WRITE_ONLY);
  std::memcpy(m->_mappedaddr, &p, sizeof(ReduceParams));
  fxi->unmapStorageBuffer(m.get());

  ci->beginDispatchPhase();
  ci->bindStorageBuffer(_cs_reduce, 0, _params_reduce);
  ci->bindStorageBuffer(_cs_reduce, 1, _ssbo_partials);
  ci->bindStorageBuffer(_cs_reduce, 2, _ssbo_sh);
  ci->dispatchCompute(_cs_reduce, 1, 1, 1);
  ci->endDispatchPhase();
}

///////////////////////////////////////////////////////////////////////////////

// ORKID_DEBUG_PROBE_SH=1 — the only in-engine window onto a captured probe's
// coefficients (the render path never reads them back). Costs a staged readback,
// so it stays behind the env var.
void ProbeSHProjector::_debugDump(Context* ctx, int slot, const char* what, int a, int b) {
  static const bool s_dbg = (getenv("ORKID_DEBUG_PROBE_SH") != nullptr);
  if (not s_dbg)
    return;
  fvec3 c[kProbeSHCoeffs];
  if (readback(ctx, slot, c)) {
    printf("[probesh] %s slot<%d> src<%dx%d>", what, slot, a, b);
    for (int i = 0; i < kProbeSHCoeffs; i++)
      printf(" c%d<%.6g,%.6g,%.6g>", i, c[i].x, c[i].y, c[i].z);
    printf("\n");
  }
}

///////////////////////////////////////////////////////////////////////////////

bool ProbeSHProjector::readback(Context* ctx, int slot, fvec3 out_coeffs[kProbeSHCoeffs]) {
  if (not _ssbo_sh)
    return false;
  if (slot < 0 or slot >= kProbeSHMaxSlots)
    return false;
  auto fxi           = ctx->FXI();
  size_t total_bytes = size_t(kProbeSHMaxSlots) * size_t(kProbeSHFloatsPerProbe) * sizeof(float);
  std::vector<float> buf(size_t(kProbeSHMaxSlots) * size_t(kProbeSHFloatsPerProbe));
  auto m = fxi->mapStorageBuffer(_ssbo_sh, 0, total_bytes, BufferMapAccess::READ_ONLY);
  if (not m)
    return false;
  std::memcpy(buf.data(), m->_mappedaddr, total_bytes);
  fxi->unmapStorageBuffer(m.get());
  const float* base = buf.data() + size_t(slot) * size_t(kProbeSHFloatsPerProbe);
  for (int i = 0; i < kProbeSHCoeffs; i++) {
    out_coeffs[i] = fvec3(base[i * 4 + 0], base[i * 4 + 1], base[i * 4 + 2]);
  }
  return true;
}

///////////////////////////////////////////////////////////////////////////////
// projectEquirectImageSH — the CPU twin of cs_probe_sh_equi above (see
// probe_sh.h for why an authored map cannot go through the kernel).
//
// Line for line the same integral: one sample per SOURCE texel, texel solid
// angle sin(theta) dtheta dphi, the same nine basis polynomials in the same
// order, and the same direction per texel. The only differences are that the
// source is host memory and that there is no decode to divide out.
///////////////////////////////////////////////////////////////////////////////

namespace {

// scalar, local: the shared Image converters fan onto the concurrent queue and
// JOIN, and this runs on a loader thread that must not be entangled with that
// pool.
inline float _shHalfToFloat(uint16_t h) {
  uint32_t sign     = uint32_t(h & 0x8000) << 16;
  uint32_t exponent = uint32_t((h & 0x7C00) >> 10);
  uint32_t mantissa = uint32_t(h & 0x03FF) << 13;
  uint32_t bits     = 0;
  if (0 == exponent) {
    if (0 == mantissa) {
      bits = sign; // +-0
    } else {       // subnormal half -> normal float
      exponent = 1;
      while (0 == (mantissa & 0x00800000)) {
        mantissa <<= 1;
        exponent--;
      }
      mantissa &= ~uint32_t(0x00800000);
      bits = sign | ((exponent + 127 - 15) << 23) | mantissa;
    }
  } else if (31 == exponent) {
    bits = sign | 0x7F800000 | mantissa; // inf / nan
  } else {
    bits = sign | ((exponent + 127 - 15) << 23) | mantissa;
  }
  float rval;
  memcpy(&rval, &bits, sizeof(float));
  return rval;
}

} // namespace

bool projectEquirectImageSH(const Image& img, fvec3 out[kProbeSHCoeffs]) {
  for (int i = 0; i < kProbeSHCoeffs; i++)
    out[i] = fvec3(0, 0, 0);
  if (nullptr == img._data)
    return false;
  const int w = int(img._width);
  const int h = int(img._height);
  if ((w < 2) or (h < 2))
    return false;
  const int ncomp = int(img._numcomponents);
  if (ncomp < 3)
    return false;
  const int bpc = int(img._bytesPerChannel);
  if ((bpc != 1) and (bpc != 2) and (bpc != 4))
    return false;

  const uint8_t* base = (const uint8_t*)img._data->data();
  if (nullptr == base)
    return false;

  const double invw   = 1.0 / double(w);
  const double invh   = 1.0 / double(h);
  const double dsolid = (M_PI * invh) * (2.0 * M_PI * invw);

  double acc[kProbeSHCoeffs][3] = {};

  for (int py = 0; py < h; py++) {
    const double v     = (double(py) + 0.5) * invh;
    const double theta = v * M_PI;
    const double st    = sin(theta);
    const double ct    = cos(theta);
    const double dw    = st * dsolid;
    for (int px = 0; px < w; px++) {
      const double u   = (double(px) + 0.5) * invw;
      const double phi = u * 2.0 * M_PI - M_PI;
      // the direction ps_sky_equirect writes at this texel, x mirror included
      const double dx = -(st * cos(phi));
      const double dy = ct;
      const double dz = st * sin(phi);
      double rgb[3]   = {0, 0, 0};
      const size_t e0 = (size_t(py) * size_t(w) + size_t(px)) * size_t(ncomp);
      for (int c = 0; c < 3; c++) {
        switch (bpc) {
          case 4:
            rgb[c] = double(((const float*)base)[e0 + c]);
            break;
          case 2:
            rgb[c] = double(_shHalfToFloat(((const uint16_t*)base)[e0 + c]));
            break;
          default:
            rgb[c] = double(base[e0 + c]) * (1.0 / 255.0);
            break;
        }
        rgb[c] *= dw;
      }
      const double basis[kProbeSHCoeffs] = {
          0.2820948,
          0.4886025 * dy,
          0.4886025 * dz,
          0.4886025 * dx,
          1.0925484 * dx * dy,
          1.0925484 * dy * dz,
          0.3153916 * (3.0 * dz * dz - 1.0),
          1.0925484 * dx * dz,
          0.5462742 * (dx * dx - dy * dy)};
      for (int i = 0; i < kProbeSHCoeffs; i++)
        for (int c = 0; c < 3; c++)
          acc[i][c] += rgb[c] * basis[i];
    }
  }

  for (int i = 0; i < kProbeSHCoeffs; i++)
    out[i] = fvec3(float(acc[i][0]), float(acc[i][1]), float(acc[i][2]));
  return true;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
