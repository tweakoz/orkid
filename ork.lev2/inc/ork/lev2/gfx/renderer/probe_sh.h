////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
#pragma once
////////////////////////////////////////////////////////////////
// ProbeSHProjector — SKYLIGHT lane C step 2. Projects a light probe's captured
// cubemap onto the L2 spherical-harmonic basis (9 coefficients x RGB) entirely on
// the GPU, into a probe SH SSBO the shading side will later read (lane C step 3).
//
// The backend has no compute imageStore (bindImage is a stub) so, exactly like
// HZBBuilder, every output is an SSBO. Two kernels, one shader FILE each (a kernel
// that binds a SUBSET of a file's storage_interfaces gets a sparse descriptor set,
// which the Metal pipeline compiler rejects):
//   cs_probe_sh_accum  : samplerCube -> per-task partial sums (solid-angle weighted)
//   cs_probe_sh_reduce : sum the partials -> one probe slot of the SH SSBO
//
// Layout of the SH SSBO (hand-mirrored C++<->fxv2, trouble point T1 — this is its
// OWN block, the 64-light arrays are untouched): float SH[], vec4 stride per
// coefficient so a later shading-side bind can read it as vec4[].
//   float index = slot*kProbeSHFloatsPerProbe + coeff*4 + channel   (channel 3 = 0)
// Coefficient order is the standard Ramamoorthi L2 basis over the WORLD direction
// d: 0=Y00, 1=Y1-1(y), 2=Y10(z), 3=Y11(x), 4=Y2-2(xy), 5=Y2-1(yz), 6=Y20(3zz-1),
// 7=Y21(xz), 8=Y22(xx-yy). Values are RADIANCE integrals (L_i = sum L(d) Y_i(d) dw),
// NOT convolved with the cosine lobe — irradiance reconstruction applies the
// Ahat_l bands at read time.
////////////////////////////////////////////////////////////////
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/math/cvector3.h>
#include <memory>

namespace ork::lev2 {

struct Image;

struct ProbeSHProjector;
using probeshprojector_ptr_t = std::shared_ptr<ProbeSHProjector>;

static constexpr int kProbeSHCoeffs         = 9;
static constexpr int kProbeSHFloatsPerProbe = kProbeSHCoeffs * 4; // vec4 stride
// L2 is band-limited to the point where capture resolution barely matters; probes
// that never had a dim assigned project at this size rather than failing.
static constexpr int kProbeSHDefaultDim = 32;
// The SH SSBO is allocated ONCE at this capacity (144 bytes/slot — 36KB total).
// Growing it would mean reallocating a buffer that already holds every OTHER
// probe's baked coefficients, so it does not grow: overflowing the capacity is a
// loud failure instead of silent data loss.
static constexpr int kProbeSHMaxSlots = 256;

struct ProbeSHProjector {

  // project `cubetex` (a cubemap RTT of face size `face_dim`) into `slot` of the SH
  // SSBO. Runs its own dispatch phases, so the result is complete (and readable)
  // when this returns. CALLER CONTRACT: `cubetex` must have been rendered in an
  // EARLIER context frame — a dispatch phase submits and waits its own command
  // buffer ahead of the current frame's graphics submission, so a cube rendered in
  // this frame reads back as pure black (measured). ForwardPbrNodeImpl honours this
  // by projecting the previous frame's capture (LightProbe::_shPendingProject).
  void project(Context* ctx, texture_ptr_t cubetex, int face_dim, int slot);

  // EQUIRECT source (the procedural sky's IBL snapshot). Same integral, same
  // slot layout, same one-frame caller contract as project() above; the only
  // differences are the sphere parameterization and `decode`.
  //
  // `decode` is the capture PRE-SCALE's inverse (CommonStuff::envCaptureScaleInv):
  // the snapshot is written at a gain so a night sky clears the fp16 minimum
  // normal, and it is divided back out HERE — once — so the coefficients this
  // writes carry DECODED radiance and no reader may decode again.
  //
  // The direction for texel (u,v) is the world direction sky.fxv2
  // ps_sky_equirect wrote there, MIRROR INCLUDED (it writes rd.x = -rd.x so the
  // shared specular samplers read the bearing the baked .xir assets use), so
  // these coefficients are in WORLD space with nothing left to undo.
  void projectEquirect(Context* ctx, texture_ptr_t equitex, int width, int height, int slot, float decode);

  // CPU copy of one slot's 9 coefficients (staged readback — gates/debug only,
  // never the per-frame render path).
  bool readback(Context* ctx, int slot, fvec3 out_coeffs[kProbeSHCoeffs]);

  FxShaderStorageBuffer* shBuffer() const {
    return _ssbo_sh;
  }

private:
  void _ensure(Context* ctx);
  // stage B, shared by both accumulators: sum the partials into `slot`.
  void _reduceInto(Context* ctx, int slot);
  void _debugDump(Context* ctx, int slot, const char* what, int a, int b);

  Context* _ctx                         = nullptr;
  const FxComputeShader* _cs_accum      = nullptr;
  const FxComputeShader* _cs_equi       = nullptr;
  const FxComputeShader* _cs_reduce     = nullptr;
  FxShaderStorageBuffer* _params_accum  = nullptr; // {dim, ntasks, pad, pad}
  FxShaderStorageBuffer* _params_equi   = nullptr; // {w, h, ntasks, decode}
  FxShaderStorageBuffer* _params_reduce = nullptr; // {ntasks, slot, pad, pad}
  FxShaderStorageBuffer* _ssbo_partials = nullptr; // kProbeSHTasks * 36 floats
  FxShaderStorageBuffer* _ssbo_sh       = nullptr; // kProbeSHMaxSlots * 36 floats
};

////////////////////////////////////////////////////////////////
// CPU projection of an AUTHORED equirect radiance image (W4-S9). Same basis,
// same coefficient order and the same RADIANCE-integral units as
// ProbeSHProjector::projectEquirect above — this is the sibling for a map that
// arrives as PIXELS rather than as a resident texture.
//
// It exists because a baked scene's sky reaches the engine only as a packaged
// .xir: there is no raw equirect to bind and no snapshot to render, so the
// GPU kernel has nothing to sample. What both publish paths DO hold at the
// same point is the CPU image of specular roughness level 0 — the prefilter's
// identity level, i.e. the authored radiance resampled at the array's extent.
// Projecting that costs no context, no dispatch phase and no readback, which
// is what makes it legal on a loader thread.
//
// THE DIRECTION FOR TEXEL (u,v) is the one the sky snapshot carries there
// (skytools skyEquirectUV2Dir with sky.fxv2 ps_sky_equirect's rd.x mirror
// folded in): row 0 is the +Y pole, v sweeps to -Y, u sweeps azimuth.
// Coefficients come out in WORLD space with nothing left to undo, exactly as
// the GPU kernel's — measured against it at rel=0.005 by
// test_env_ambient_pipeline_gate, which is what pins this convention.
//
// AN AUTHORED .xir IS PROJECTED AT THE SAME BEARING, and its ambient lands on
// the right hemisphere but ~59 degrees off the dipole of the sky a viewer sees
// (test_env_diffuse_bakedmap_azimuth_gate, which the retired prefiltered
// lookup scored 17.8 on). That gap is NOT an axis convention: all eight sign
// patterns of this direction were measured against that gate and every one of
// them scores worse than this, so the .xir's specular level 0 does not carry
// the bearing its own skybox is drawn at. FILED, not papered over.
//
// UNITS ARE THE IMAGE'S: no decode is applied. A map written at a capture
// pre-scale yields coefficients at that same gain, so the ONE decode seam
// stays where its maps' other measurements decode (CommonStuff::envSHCoeffs /
// availableLightLuminance).
//
// false = the image carries nothing projectable (no data, degenerate extent,
// or a format with no color) — the caller must leave its SH invalid rather
// than publish zeros as a black sky.
bool projectEquirectImageSH(const Image& img, fvec3 out[kProbeSHCoeffs]);

} // namespace ork::lev2
