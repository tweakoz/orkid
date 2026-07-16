////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
#include "hfdflow_module.h"
#include <cmath>

ImplementReflectionX(ork::lev2::terrain::RelaxUvModuleData, "terrain::RelaxUvModuleData");

namespace ork::lev2::terrain {

///////////////////////////////////////////////////////////////////////////////
// RelaxUvModule — equal-area UV relaxation for terrain (the slope-stretch fix).
//
// GOAL: normalize texels per unit PHYSICAL (surface) area. The planar UV gives each
// texel a world footprint that scales by sqrt(1+|grad h|^2)=1/nrm.y (worse on slopes),
// so baked detail smears on cliffs. This module warps the UV so each texel covers ~equal
// surface area, KEEPING the unit-square boundary fixed (UVs stay in [0,1]).
//
// ALGORITHM (linearized optimal transport — fully Eulerian compute stencils, no global
// solve, no point advection; the same compute-pass-iteration idiom as erox/flow3d):
//   rho   = sqrt(1+|grad h|^2)   at FULL res              // surface-area density (>1 on slopes)
//   box-average rho -> the coarse relax grid              // area is an INTEGRAL: average the density,
//                                                         // never re-derive it from a smoothed height
//   solve  laplacian(psi) = (rho/mean - 1)  (Neumann)     // red-black SOR (in place, omega ~ 2/(1+sin(pi/N)))
//   uv    = planar + strength * grad(psi) / dim           // high-rho regions EXPAND -> more texels
// Neumann BC makes grad(psi) tangential at the border, so the boundary slides along the
// unit-square edges and the map stays in [0,1] for free.
// SOLVER NOTE: plain Jacobi CANNOT converge this — its lowest-mode error factor is
// 1 - pi^2/(2 N^2) per sweep (~60k sweeps at N=256); the shipped 1024 sweeps left the warp
// ~7% built (measured: +-5 texel warp, zero equal-area benefit). Red-black SOR at omega_opt
// converges the low mode ~1e-5 within the same ~1024-sweep budget.
//
// INPUT CONTRACT: the height this module receives is in NATURAL UNITS (true meters) — the
// frame normals/binormals and the surface-area density rho are computed from the honest meter
// amplitude directly (a per-texel height delta IS meters). No [0,1] stretch, no vertical scale:
// a slope quantity here matches the meter geometry the renderer draws.
//
// It ALSO precomputes the per-vertex tangent frame so the render VS goes tap-light:
//   normal   = geometric surface normal (parameterization-INVARIANT)         -> "Out".zw  (n.x,n.z; n.y=+sqrt)
//   binormal = dP/du of the RELAXED uv (the one frame axis the relax perturbs)-> "Binormal".xyz
//   relaxed_uv                                                               -> "Out".xy
// The VS reads these and derives tangent = cross(N,B); only P needs a live height tap.
//
// Outputs (multi-channel RGBA32F, baked to EXR; downsampled to render_dimension on load):
//   "Out"      RGBA = (relaxed_uv.x, relaxed_uv.y, normal.x, normal.z)
//   "Binormal" RGBA = (binormal.x,  binormal.y,   binormal.z, 1)
///////////////////////////////////////////////////////////////////////////////

// fixed-point scale for the atomic-uint density-mean sum. Accumulated over the COARSE grid
// (cdim^2 cells x rho up to ~16 x kSumFix) — 256 keeps the worst case ~1.1e9 at cdim=512,
// safely under uint32 (4096 was borderline even at cdim=256 once rho is computed at honest
// amplitude). Mean quantization bias ~0.2% at 256 — irrelevant (it divides out of rho/mean).
static constexpr double kSumFix = 256.0;

// COARSE relax-grid cap. A fold-free map redistributes area as a low-frequency deformation, so
// the Poisson/warp run on this capped grid and the uv upsamples. The cap bounds what the warp
// can equalize: the WITHIN-CELL demand it cannot reach has starvation-p99 ~1.31 at 256 vs
// ~1.19 at 512 (measured, erodeflow 2048) — 256 mathematically cannot hit the <=1.3 quality
// gate. 512 is safe now that the solver CONVERGES (the old 512 fold sweep ran the unconverged
// Jacobi); the per-vertex fold guard in cs_warp still backstops. HASHED into the cook key.
static constexpr int kRelaxCap = 512;

// 0) clear the density accumulator (single uint)
static std::string _reset_text() {
  return R"S(
fxconfig fxcfg_default {}
storage_interface si_sum (descriptor_set 0) { buffer layout(std430) sb { uint sdata[1]; }; }
compute_interface iface { storage { si_sum } inputs { layout(local_size_x = 1, local_size_y = 1, local_size_z = 1); } }
compute_shader cs_reset : iface { sdata[0] = 0u; }
)S";
}

// 0a) BOX-AVERAGE the full-res density (dim) -> the coarse relax grid (cdim), and atomic-sum the
//     coarse means for the global mean. Area is an INTEGRAL quantity: averaging rho preserves each
//     coarse cell's true surface-area demand, whereas the old height-downsample-then-gradient
//     UNDERESTIMATED thin canyon walls (gradients of a smoothed height). The relaxation still runs
//     coarse (a fold-free map redistributes area at low frequency; ~(dim/cdim)^2 less solver work).
static std::string _boxavg_text(int dim, int cdim) {
  std::string t = R"S(
fxconfig fxcfg_default {}
storage_interface si_rf  (descriptor_set 0) { buffer layout(std430) ib { float rin[%DIMSQ%]; }; }
storage_interface si_rc  (descriptor_set 0) { buffer layout(std430) ob { float rout[%CDIMSQ%]; }; }
storage_interface si_sum (descriptor_set 0) { buffer layout(std430) sb { uint  sdata[1]; }; }
compute_interface iface { storage { si_rf si_rc si_sum } inputs { layout(local_size_x = 8, local_size_y = 8, local_size_z = 1); } }
compute_shader cs_boxavg : iface {
  if (gl_GlobalInvocationID.x >= %CDIMU% || gl_GlobalInvocationID.y >= %CDIMU%) { return; }
  int cx = int(gl_GlobalInvocationID.x); int cz = int(gl_GlobalInvocationID.y);
  int D  = int(%DIMU%);
  float r = float(%DIMU%) / float(%CDIMU%);
  int x0 = int(floor(float(cx) * r)); int x1 = min(int(floor(float(cx + 1) * r)), D);
  int z0 = int(floor(float(cz) * r)); int z1 = min(int(floor(float(cz + 1) * r)), D);
  float acc = 0.0; int cnt = 0;
  for (int z = z0; z < z1; z++) {
    for (int x = x0; x < x1; x++) { acc += rin[uint(z) * %DIMU% + uint(x)]; cnt = cnt + 1; }
  }
  float m = (cnt > 0) ? (acc / float(cnt)) : 1.0;
  rout[uint(cz) * %CDIMU% + uint(cx)] = m;
  atomicAdd(sdata[0], uint(m * %SUMFIX%));
}
)S";
  _shadersub(t, "%DIMSQ%",  FormatString("%d", dim * dim));
  _shadersub(t, "%CDIMSQ%", FormatString("%d", cdim * cdim));
  _shadersub(t, "%DIMU%",   FormatString("%du", dim));
  _shadersub(t, "%CDIMU%",  FormatString("%du", cdim));
  _shadersub(t, "%SUMFIX%", FormatString("%g", kSumFix));
  return t;
}

// 0b) UPSAMPLE the coarse relaxed uv (cdim, vec2) -> the full bake dim (vec2) by bilinear interpolation.
//     uv is a normalized [0,1] parameterization (resolution-independent), so no rescale; bilinear of a
//     fold-free coarse field stays fold-free. The frame pass then runs at FULL res off this upsampled uv.
static std::string _upsample_text(int dim, int cdim) {
  std::string t = R"S(
fxconfig fxcfg_default {}
storage_interface si_cuv (descriptor_set 0) { buffer layout(std430) cb { float cuv[%CUVSQ%]; }; }
storage_interface si_uv  (descriptor_set 0) { buffer layout(std430) ub { float udata[%UVSQ%]; }; }
compute_interface iface { storage { si_cuv si_uv } inputs { layout(local_size_x = 8, local_size_y = 8, local_size_z = 1); } }
compute_shader cs_upsample : iface {
  if (gl_GlobalInvocationID.x >= %DIMU% || gl_GlobalInvocationID.y >= %DIMU%) { return; }
  int dx = int(gl_GlobalInvocationID.x); int dz = int(gl_GlobalInvocationID.y);
  int C  = int(%CDIMU%); uint Cu = %CDIMU%;
  float r  = float(%CDIMU%) / float(%DIMU%);
  float fx = (float(dx) + 0.5) * r - 0.5;          // coarse-space sample center
  float fz = (float(dz) + 0.5) * r - 0.5;
  int x0 = int(floor(fx)); int z0 = int(floor(fz));
  // BORDER: clamp the tap PAIR (not each tap) and let the weight run outside [0,1] -> linear
  // EXTRAPOLATION at the edges. Per-tap clamping made the outer ~dim/cdim/2 full-res texels
  // CONSTANT: a det=0 rim of zero-area bake triangles (uncovered atlas edge) + smeared border.
  int x0c = clamp(x0, 0, C-2); int x1c = x0c + 1;
  int z0c = clamp(z0, 0, C-2); int z1c = z0c + 1;
  float tx = fx - float(x0c); float tz = fz - float(z0c);
  vec2 c00 = vec2(cuv[2u*(uint(z0c)*Cu+uint(x0c))+0u], cuv[2u*(uint(z0c)*Cu+uint(x0c))+1u]);
  vec2 c10 = vec2(cuv[2u*(uint(z0c)*Cu+uint(x1c))+0u], cuv[2u*(uint(z0c)*Cu+uint(x1c))+1u]);
  vec2 c01 = vec2(cuv[2u*(uint(z1c)*Cu+uint(x0c))+0u], cuv[2u*(uint(z1c)*Cu+uint(x0c))+1u]);
  vec2 c11 = vec2(cuv[2u*(uint(z1c)*Cu+uint(x1c))+0u], cuv[2u*(uint(z1c)*Cu+uint(x1c))+1u]);
  vec2 uv  = clamp(mix(mix(c00,c10,tx), mix(c01,c11,tx), tz), vec2(0.0), vec2(1.0));
  uint i = uint(dz)*%DIMU% + uint(dx);
  udata[2u*i+0u] = uv.x;
  udata[2u*i+1u] = uv.y;
}
)S";
  _shadersub(t, "%CUVSQ%", FormatString("%d", 2 * cdim * cdim));
  _shadersub(t, "%UVSQ%",  FormatString("%d", 2 * dim * dim));
  _shadersub(t, "%DIMU%",  FormatString("%du", dim));
  _shadersub(t, "%CDIMU%", FormatString("%du", cdim));
  return t;
}

// 1) rho = sqrt(1+|grad h|^2)  (physical slope) at FULL res — cs_boxavg then averages it to the
//    coarse relax grid (and accumulates the mean there; no atomics needed here).
static std::string _density_text(int dim, float aspect) {
  std::string t = R"S(
fxconfig fxcfg_default {}
storage_interface si_h   (descriptor_set 0) { buffer layout(std430) hb { float hdata[%DIMSQ%]; }; }
storage_interface si_rho (descriptor_set 0) { buffer layout(std430) rb { float rdata[%DIMSQ%]; }; }
compute_interface iface { storage { si_h si_rho } inputs { layout(local_size_x = 8, local_size_y = 8, local_size_z = 1); } }
compute_shader cs_density : iface {
  if (gl_GlobalInvocationID.x >= %DIMU% || gl_GlobalInvocationID.y >= %DIMU%) { return; }
  int  xi = int(gl_GlobalInvocationID.x);
  int  yi = int(gl_GlobalInvocationID.y);
  int  W  = int(%DIMU%); uint Wu = %DIMU%; uint i = uint(yi)*Wu + uint(xi);
  float hl = hdata[(xi>0)   ? i-1u : i];
  float hr = hdata[(xi<W-1) ? i+1u : i];
  float hd = hdata[(yi>0)   ? i-Wu : i];
  float hu = hdata[(yi<W-1) ? i+Wu : i];
  float gx = (hr - hl) * 0.5 * float(%ASPECT%);   // physical slope d(h_m)/d(x_m)
  float gz = (hu - hd) * 0.5 * float(%ASPECT%);
  rdata[i] = sqrt(1.0 + gx*gx + gz*gz);           // surface-area density (>=1)
}
)S";
  _shadersub(t, "%DIMSQ%", FormatString("%d", dim * dim));
  _shadersub(t, "%DIMU%", FormatString("%du", dim));
  _shadersub(t, "%ASPECT%", FormatString("%g", aspect));
  return t;
}

// 2) f = rho - mean(rho)   (the zero-mean Poisson RHS, Neumann-solvable). Stored back into rdata.
static std::string _rhs_text(int dim) {
  std::string t = R"S(
fxconfig fxcfg_default {}
storage_interface si_rho (descriptor_set 0) { buffer layout(std430) rb { float rdata[%DIMSQ%]; }; }
storage_interface si_sum (descriptor_set 0) { buffer layout(std430) sb { uint  sdata[1]; }; }
compute_interface iface { storage { si_rho si_sum } inputs { layout(local_size_x = 8, local_size_y = 8, local_size_z = 1); } }
compute_shader cs_rhs : iface {
  if (gl_GlobalInvocationID.x >= %DIMU% || gl_GlobalInvocationID.y >= %DIMU%) { return; }
  uint i = gl_GlobalInvocationID.y*%DIMU% + gl_GlobalInvocationID.x;
  float mean = (float(sdata[0]) / %SUMFIX%) / float(%DIMSQ%);
  rdata[i]   = rdata[i] / max(mean, 1e-6) - 1.0;   // DIMENSIONLESS density anomaly (rho/mean - 1):
}                                                  // makes the warp density-independent (strength is now
                                                   // terrain-steepness-invariant; un-normalized overshoots)
)S";
  _shadersub(t, "%DIMSQ%", FormatString("%d", dim * dim));
  _shadersub(t, "%DIMU%", FormatString("%du", dim));
  _shadersub(t, "%SUMFIX%", FormatString("%g", kSumFix));
  return t;
}

// 3) one red-black SOR half-sweep of laplacian(psi)=f, Neumann (clamped neighbors), IN PLACE.
//    psi=slot0, f=slot1. Emitted TWICE (parity 0=red, 1=black); a full sweep = red then black
//    with a barrier between. In-place is race-free: a cell's 4 neighbors are all the OTHER
//    parity (the edge self-clamp reads the thread's OWN cell before it writes). omega is the
//    optimal over-relaxation 2/(1+sin(pi/N)) — this is what makes the low-frequency mode
//    (the actual warp) converge within ~O(N) sweeps where plain Jacobi needs ~O(N^2 ln).
static std::string _sor_text(int dim, int parity, double omega) {
  std::string t = R"S(
fxconfig fxcfg_default {}
storage_interface si_p (descriptor_set 0) { buffer layout(std430) pb { float pdata[%DIMSQ%]; }; }
storage_interface si_f (descriptor_set 0) { buffer layout(std430) fb { float fdata[%DIMSQ%]; }; }
compute_interface iface { storage { si_p si_f } inputs { layout(local_size_x = 8, local_size_y = 8, local_size_z = 1); } }
compute_shader cs_sor_p%PARITY% : iface {
  if (gl_GlobalInvocationID.x >= %DIMU% || gl_GlobalInvocationID.y >= %DIMU%) { return; }
  int  xi = int(gl_GlobalInvocationID.x);
  int  yi = int(gl_GlobalInvocationID.y);
  if (((uint(xi) + uint(yi)) & 1u) != %PARITY%u) { return; }
  int  W  = int(%DIMU%); uint Wu = %DIMU%; uint i = uint(yi)*Wu + uint(xi);
  float pl = pdata[(xi>0)   ? i-1u : i];   // Neumann: reflect the edge (no flux)
  float pr = pdata[(xi<W-1) ? i+1u : i];
  float pd = pdata[(yi>0)   ? i-Wu : i];
  float pu = pdata[(yi<W-1) ? i+Wu : i];
  float gs = (pl + pr + pd + pu - fdata[i]) * 0.25;   // Gauss-Seidel target (lap(psi)=f, unit spacing)
  pdata[i] = pdata[i] + float(%OMEGA%) * (gs - pdata[i]);
}
)S";
  _shadersub(t, "%DIMSQ%", FormatString("%d", dim * dim));
  _shadersub(t, "%DIMU%", FormatString("%du", dim));
  _shadersub(t, "%PARITY%", FormatString("%d", parity));
  _shadersub(t, "%OMEGA%", FormatString("%.9g", omega));
  return t;
}

// 4) warp: relaxed_uv = clamp(planar + strength*grad(psi)/dim, 0, 1).  uv=slot0 (vec2), psi=slot1.
static std::string _warp_text(int dim, float strength) {
  std::string t = R"S(
fxconfig fxcfg_default {}
storage_interface si_uv  (descriptor_set 0) { buffer layout(std430) ub { float udata[%UVSQ%]; }; }
storage_interface si_psi (descriptor_set 0) { buffer layout(std430) pb { float pdata[%DIMSQ%]; }; }
compute_interface iface { storage { si_uv si_psi } inputs { layout(local_size_x = 8, local_size_y = 8, local_size_z = 1); } }
compute_shader cs_warp : iface {
  if (gl_GlobalInvocationID.x >= %DIMU% || gl_GlobalInvocationID.y >= %DIMU%) { return; }
  int  xi = int(gl_GlobalInvocationID.x);
  int  yi = int(gl_GlobalInvocationID.y);
  int  W  = int(%DIMU%); uint Wu = %DIMU%; uint i = uint(yi)*Wu + uint(xi);
  int  xl = (xi>0)?xi-1:xi; int xr = (xi<W-1)?xi+1:xi;
  int  yd = (yi>0)?yi-1:yi; int yu = (yi<W-1)?yi+1:yi;
  float pc = pdata[i];
  float pl = pdata[uint(yi)*Wu+uint(xl)];
  float pr = pdata[uint(yi)*Wu+uint(xr)];
  float pd = pdata[uint(yd)*Wu+uint(xi)];
  float pu = pdata[uint(yu)*Wu+uint(xi)];
  float gx = (pr - pl) * 0.5;     // grad(psi) in texels
  float gz = (pu - pd) * 0.5;
  // FOLD-SAFE STEP. In TEXEL coordinates a vertex moves to  i + s*grad(psi)  (s = strength), so the map's
  // Jacobian is  J = I + s*H  (H = Hessian(psi) by central differences; the planar map is the identity in
  // texels). A foldover (non-bijective uv -> garbage atlas) is exactly det(J) <= 0 / trace(J) <= 0. Cap the
  // PER-VERTEX strength s to the largest value in [0, Smax] keeping  det(J) >= DMIN  AND  trace(J) >= TMIN
  // -> a bijective (fold-free) parameterization at ANY requested strength (steep regions still expand; flat
  // regions stop just short of collapsing). NB: operate on s (texel units), NOT k=s/dim (which is ~0).
  float Hxx = pr - 2.0*pc + pl;
  float Hyy = pu - 2.0*pc + pd;
  float pa  = pdata[uint(yu)*Wu+uint(xr)];  // (x+1,y+1)
  float pb  = pdata[uint(yu)*Wu+uint(xl)];  // (x-1,y+1)
  float pe  = pdata[uint(yd)*Wu+uint(xr)];  // (x+1,y-1)
  float pf  = pdata[uint(yd)*Wu+uint(xl)];  // (x-1,y-1)
  float Hxy = (pa - pb - pe + pf) * 0.25;
  float trH = Hxx + Hyy;
  float dtH = Hxx*Hyy - Hxy*Hxy;
  float s   = float(%STRENGTH%);                // the requested (texel-space) step
  // trace(J) = 2 + s*trH >= TMIN  (guards the 180-degree flip where det can stay > 0)
  const float TMIN = 0.30;
  if (trH < 0.0) s = min(s, (2.0 - TMIN) / (-trH));
  // det(J) = 1 + trH*s + dtH*s^2 >= DMIN : shrink s to just inside the first positive root if it dips below.
  const float DMIN = 0.15;
  float a = dtH, b = trH, c = 1.0 - DMIN;       // a*s^2 + b*s + c  (== det(J)-DMIN ; c>0 since det(J(0))=1)
  if (a*s*s + b*s + c < 0.0) {
    if (abs(a) < 1e-20) {
      if (b < 0.0) s = min(s, -c / b);
    } else {
      float disc = b*b - 4.0*a*c;
      if (disc >= 0.0) {
        float sq = sqrt(disc);
        float r1 = (-b - sq) / (2.0*a);
        float r2 = (-b + sq) / (2.0*a);
        float rmin = 1.0e30;
        if (r1 > 0.0) rmin = min(rmin, r1);
        if (r2 > 0.0) rmin = min(rmin, r2);
        if (rmin < 1.0e30) s = min(s, rmin * 0.97);
      }
    }
  }
  s = max(s, 0.0);
  float k    = s / float(%DIMU%);                   // texels -> uv (the fold-safe step)
  float pu_x = (float(xi) + 0.5) / float(%DIMU%);   // planar uv (matches gpu_chunk terr_pos)
  float pv_z = (float(yi) + 0.5) / float(%DIMU%);
  vec2  uv   = clamp(vec2(pu_x + gx*k, pv_z + gz*k), vec2(0.0), vec2(1.0));
  udata[2u*i + 0u] = uv.x;
  udata[2u*i + 1u] = uv.y;
}
)S";
  _shadersub(t, "%UVSQ%", FormatString("%d", 2 * dim * dim));
  _shadersub(t, "%DIMSQ%", FormatString("%d", dim * dim));
  _shadersub(t, "%DIMU%", FormatString("%du", dim));
  _shadersub(t, "%STRENGTH%", FormatString("%g", strength));
  return t;
}

// 5) frame: geometric normal + RELAXED binormal (dP/du). out=slot0 (RGBA), bn=slot1 (RGBA),
//    h=slot2, uv=slot3 (vec2). Out=(uv.x,uv.y,n.x,n.z) ; Binormal=(b.x,b.y,b.z,1).
static std::string _frame_text(int dim, float cell) {
  std::string t = R"S(
fxconfig fxcfg_default {}
storage_interface si_o  (descriptor_set 0) { buffer layout(std430) ob { float odata[%RGBASQ%]; }; }
storage_interface si_b  (descriptor_set 0) { buffer layout(std430) bb { float bdata[%RGBASQ%]; }; }
storage_interface si_h  (descriptor_set 0) { buffer layout(std430) hb { float hdata[%DIMSQ%]; }; }
storage_interface si_uv (descriptor_set 0) { buffer layout(std430) ub { float udata[%UVSQ%]; }; }
compute_interface iface { storage { si_o si_b si_h si_uv } inputs { layout(local_size_x = 8, local_size_y = 8, local_size_z = 1); } }
compute_shader cs_frame : iface {
  if (gl_GlobalInvocationID.x >= %DIMU% || gl_GlobalInvocationID.y >= %DIMU%) { return; }
  int  xi = int(gl_GlobalInvocationID.x);
  int  yi = int(gl_GlobalInvocationID.y);
  int  W  = int(%DIMU%); uint Wu = %DIMU%; uint i = uint(yi)*Wu + uint(xi);
  int  xl = (xi>0)?xi-1:xi; int xr = (xi<W-1)?xi+1:xi;
  int  yd = (yi>0)?yi-1:yi; int yu = (yi<W-1)?yi+1:yi;
  float C  = float(%CELL%);
  // world surface tangents (central diff). x-step = (xr-xl) cells; heights are METERS so the
  // y delta is already physical (no vertical scale).
  vec3 dPx = vec3(float(xr-xl)*C, (hdata[uint(yi)*Wu+uint(xr)] - hdata[uint(yi)*Wu+uint(xl)]), 0.0);
  vec3 dPz = vec3(0.0,            (hdata[uint(yu)*Wu+uint(xi)] - hdata[uint(yd)*Wu+uint(xi)]), float(yu-yd)*C);
  vec3 nrm = normalize(cross(dPz, dPx));
  if (nrm.y < 0.0) nrm = -nrm;                       // forced +y (single-valued heightfield)
  // relaxed-uv gradient -> inverse Jacobian -> dP/du (the relaxed binormal)
  vec2 uvl = vec2(udata[2u*(uint(yi)*Wu+uint(xl))+0u], udata[2u*(uint(yi)*Wu+uint(xl))+1u]);
  vec2 uvr = vec2(udata[2u*(uint(yi)*Wu+uint(xr))+0u], udata[2u*(uint(yi)*Wu+uint(xr))+1u]);
  vec2 uvd = vec2(udata[2u*(uint(yd)*Wu+uint(xi))+0u], udata[2u*(uint(yd)*Wu+uint(xi))+1u]);
  vec2 uvu = vec2(udata[2u*(uint(yu)*Wu+uint(xi))+0u], udata[2u*(uint(yu)*Wu+uint(xi))+1u]);
  vec2 duvdx = (uvr - uvl) * 0.5;   // (du/dx, dv/dx)
  vec2 duvdz = (uvu - uvd) * 0.5;   // (du/dz, dv/dz)
  float det  = duvdx.x*duvdz.y - duvdz.x*duvdx.y;
  vec3 binormal;
  if (abs(det) > 1e-12) {
    binormal = (dPx*duvdz.y - dPz*duvdx.y) / det;    // dP/du = dP/dx*(dx/du) + dP/dz*(dz/du)
  } else {
    binormal = dPx;                                  // degenerate -> planar tangent
  }
  binormal = binormal - dot(binormal, nrm)*nrm;      // orthonormalize against the normal
  binormal = normalize(binormal);
  uint b = 4u*i;
  odata[b+0u] = udata[2u*i+0u];   // relaxed uv.x
  odata[b+1u] = udata[2u*i+1u];   // relaxed uv.y
  odata[b+2u] = nrm.x;            // normal.x (n.y reconstructed +sqrt in the VS)
  odata[b+3u] = nrm.z;            // normal.z
  bdata[b+0u] = binormal.x;
  bdata[b+1u] = binormal.y;
  bdata[b+2u] = binormal.z;
  bdata[b+3u] = 1.0;
}
)S";
  _shadersub(t, "%RGBASQ%", FormatString("%d", 4 * dim * dim));
  _shadersub(t, "%UVSQ%", FormatString("%d", 2 * dim * dim));
  _shadersub(t, "%DIMSQ%", FormatString("%d", dim * dim));
  _shadersub(t, "%DIMU%", FormatString("%du", dim));
  _shadersub(t, "%CELL%", FormatString("%g", cell));
  return t;
}

///////////////////////////////////////////////////////////////////////////////

struct RelaxUvModuleInst : public TerrainComputeInst {
  RelaxUvModuleInst(const RelaxUvModuleData* d, dflow::GraphInst* g) : TerrainComputeInst(d, g), _d(d) {}

  void onLink(dflow::GraphInst*) final {
    _outUv = typedOutputNamed<HfImagePlugTraits>("Out");      // RGBA: relaxed_uv.xy + normal.x,z
    _outBn = typedOutputNamed<HfImagePlugTraits>("Binormal"); // RGBA: binormal.xyz + 1
    _input = typedInputNamed<HfImagePlugTraits>("In");        // height
  }
  void bakeAcquire(dflow::GraphInst* inst) final {
    auto env = inst->_impl.getShared<BakeEnv>();
    auto fxi = env->_ctx->FXI();
    int dim  = env->_w;
    size_t n = size_t(dim) * size_t(dim);
    // COARSE relaxation grid (see kRelaxCap). The FRAME (normal/binormal) still runs at full dim.
    int   cdim   = std::min(dim, kRelaxCap);
    _cdim = cdim;
    size_t nC    = size_t(cdim) * size_t(cdim);
    float cell   = (dim  > 0) ? (env->_extent_m / float(dim))  : 1.0f;  // full-res cell
    float aspect = 1.0f / cell; // per-texel height delta (METERS) -> d(h_m)/d(x_m) rise/run (FULL res)
    // optimal SOR over-relaxation for the 2D Poisson problem at grid size cdim
    double omega = 2.0 / (1.0 + std::sin(M_PI / double(cdim)));
    // outputs (RGBA32F) — at full bake dim
    _outUv->_value->_w = dim; _outUv->_value->_h = dim; _outUv->_value->_channels = 4;
    _outUv->_value->_ssbo = env->createStorageBuffer(n * 4 * sizeof(float));
    _outBn->_value->_w = dim; _outBn->_value->_h = dim; _outBn->_value->_channels = 4;
    _outBn->_value->_ssbo = env->createStorageBuffer(n * 4 * sizeof(float));
    // scratch — density at FULL res, box-averaged to the COARSE relax grid (Poisson/warp run coarse)
    _rhoF = env->createStorageBuffer(n * sizeof(float));      // full-res density
    _rho  = env->createStorageBuffer(nC * sizeof(float));     // coarse density, then reused as the RHS f
    _psi  = env->createStorageBuffer(nC * sizeof(float));     // SOR solves IN PLACE (no ping-pong)
    { // seed psi = 0 (solver start). Pre-dispatch (bakeAcquire runs between phases) so we never map a buffer mid-graph.
      auto m = fxi->mapStorageBuffer(_psi, 0, nC * sizeof(float), BufferMapAccess::WRITE_ONLY);
      std::memset(m->_mappedaddr, 0, nC * sizeof(float));
      fxi->unmapStorageBuffer(m.get());
    }
    _uvC  = env->createStorageBuffer(nC * 2 * sizeof(float)); // coarse relaxed uv
    _uv   = env->createStorageBuffer(n  * 2 * sizeof(float)); // upsampled to full dim (frame reads this)
    _sum  = env->createStorageBuffer(sizeof(uint32_t));
    // shaders — density/frame at DIM, boxavg/rhs/SOR/warp at CDIM, upsample bridges cdim->dim
    _csReset   = fxi->computeShader(fxi->shaderFromShaderText("terrain_relax_reset",   _reset_text()), "cs_reset");
    _csDensity = fxi->computeShader(fxi->shaderFromShaderText("terrain_relax_density", _density_text(dim, aspect)), "cs_density");
    _csBoxavg  = fxi->computeShader(fxi->shaderFromShaderText("terrain_relax_boxavg",  _boxavg_text(dim, cdim)), "cs_boxavg");
    _csRhs     = fxi->computeShader(fxi->shaderFromShaderText("terrain_relax_rhs",     _rhs_text(cdim)), "cs_rhs");
    _csSorR    = fxi->computeShader(fxi->shaderFromShaderText("terrain_relax_sor0",    _sor_text(cdim, 0, omega)), "cs_sor_p0");
    _csSorB    = fxi->computeShader(fxi->shaderFromShaderText("terrain_relax_sor1",    _sor_text(cdim, 1, omega)), "cs_sor_p1");
    _csWarp    = fxi->computeShader(fxi->shaderFromShaderText("terrain_relax_warp",    _warp_text(cdim, _d->_strength)), "cs_warp");
    _csUpsample= fxi->computeShader(fxi->shaderFromShaderText("terrain_relax_up",      _upsample_text(dim, cdim)), "cs_upsample");
    _csFrame   = fxi->computeShader(fxi->shaderFromShaderText("terrain_relax_frame",   _frame_text(dim, cell)), "cs_frame");
    _iters = (_d->_iterations > 0) ? _d->_iterations : std::min(4 * cdim, 4096);
  }
  void compute(dflow::GraphInst* inst, ui::updatedata_ptr_t) final {
    auto env = inst->_impl.getShared<BakeEnv>();
    auto ci  = env->_ctx->CI();
    auto in  = _srcImg(_input);
    OrkAssert(in && in->_ssbo);
    int gD = (env->_w + 7) / 8;       // full bake-dim dispatch
    int gC = (_cdim + 7) / 8;         // coarse relax-grid dispatch
    // 0) clear the mean accumulator
    ci->bindStorageBuffer(_csReset, 0, _sum);
    ci->dispatchCompute(_csReset, 1, 1, 1); ci->storageBarrier();
    // 1) density at FULL res (honest cliff demand), then box-average -> coarse (+ mean accumulate)
    ci->bindStorageBuffer(_csDensity, 0, in->_ssbo);
    ci->bindStorageBuffer(_csDensity, 1, _rhoF);
    ci->dispatchCompute(_csDensity, gD, gD, 1); ci->storageBarrier();
    ci->bindStorageBuffer(_csBoxavg, 0, _rhoF);
    ci->bindStorageBuffer(_csBoxavg, 1, _rho);
    ci->bindStorageBuffer(_csBoxavg, 2, _sum);
    ci->dispatchCompute(_csBoxavg, gC, gC, 1); ci->storageBarrier();
    // 2) RHS f = rho/mean - 1  (in place in _rho)
    ci->bindStorageBuffer(_csRhs, 0, _rho);
    ci->bindStorageBuffer(_csRhs, 1, _sum);
    ci->dispatchCompute(_csRhs, gC, gC, 1); ci->storageBarrier();
    // 3) red-black SOR: solve laplacian(psi)=f at COARSE res, IN PLACE. psi seeded 0 in bakeAcquire.
    //    One sweep = red half-dispatch + black half-dispatch (barrier between: black reads red's writes).
    for (int it = 0; it < _iters; it++) {
      ci->bindStorageBuffer(_csSorR, 0, _psi);
      ci->bindStorageBuffer(_csSorR, 1, _rho);
      ci->dispatchCompute(_csSorR, gC, gC, 1); ci->storageBarrier();
      ci->bindStorageBuffer(_csSorB, 0, _psi);
      ci->bindStorageBuffer(_csSorB, 1, _rho);
      ci->dispatchCompute(_csSorB, gC, gC, 1);
      if (((it + 1) % 64) == 0) { ci->endDispatchPhase(); ci->beginDispatchPhase(); }
      else ci->storageBarrier();
    }
    // 4) warp -> coarse relaxed uv  (psi holds the converged solution)
    ci->bindStorageBuffer(_csWarp, 0, _uvC);
    ci->bindStorageBuffer(_csWarp, 1, _psi);
    ci->dispatchCompute(_csWarp, gC, gC, 1); ci->storageBarrier();
    // 5) UPSAMPLE coarse uv -> full-dim uv
    ci->bindStorageBuffer(_csUpsample, 0, _uvC);
    ci->bindStorageBuffer(_csUpsample, 1, _uv);
    ci->dispatchCompute(_csUpsample, gD, gD, 1); ci->storageBarrier();
    // 6) frame -> Out (uv+normal) + Binormal — at FULL dim off the full-res height + upsampled uv
    ci->bindStorageBuffer(_csFrame, 0, _outUv->_value->_ssbo);
    ci->bindStorageBuffer(_csFrame, 1, _outBn->_value->_ssbo);
    ci->bindStorageBuffer(_csFrame, 2, in->_ssbo);
    ci->bindStorageBuffer(_csFrame, 3, _uv);
    ci->dispatchCompute(_csFrame, gD, gD, 1); ci->storageBarrier();
  }
  // WS4 fp16 wave: these planes quantize/store at fp16 (see TerrainComputeInst
  // notes — quantize-at-production keeps warm==cold bit-exact). Version salt
  // bumped alongside: the quantization changes the output.
  bool cookHalfOutput(const std::string& output_name) const final {
    return output_name == "Binormal";
  }

  bool cookCacheDefault() const final { return true; } // measured cache-point class (cost-model analysis)
  uint64_t cookComputeHash(const std::vector<uint64_t>& ih, uint64_t ctx) const final {
    auto h = DataBlock::createHasher();
    // v5: red-black SOR (Jacobi never converged -> zero equal-area), full-res rho box-averaged
    // (was gradients of a smoothed height), border-extrapolating upsample (was det=0 rim), and the
    // amplitude contract (normalized input via base.py). MIRROR any bump in _terrain.py _relax_tok.
    h->accumulateString("terrain.relaxuv.v7"); // v7: heights in meters (dropped height_scale from rho/frame); v6: Binormal fp16
    h->accumulateItem<int>(kRelaxCap);  // the cap changes the output — cache must be sensitive to it
    h->accumulateItem<float>(_d->_strength);
    h->accumulateItem<int>(_d->_iterations);
    _mixTail(h, ctx, ih);
    h->finish();
    return h->result();
  }

  const RelaxUvModuleData* _d;
  hfimg_outpluginst_ptr_t _outUv;
  hfimg_outpluginst_ptr_t _outBn;
  hfimg_inpluginst_ptr_t _input;
  int _cdim = 0;                              // coarse relax grid dim (<= bake dim)
  FxShaderStorageBuffer* _rhoF = nullptr;     // FULL-res density
  FxShaderStorageBuffer* _rho  = nullptr;     // coarse (box-averaged) density / Poisson RHS
  FxShaderStorageBuffer* _psi  = nullptr;     // coarse psi (SOR solves in place)
  FxShaderStorageBuffer* _uvC  = nullptr;     // coarse relaxed uv
  FxShaderStorageBuffer* _uv   = nullptr;     // full-dim upsampled uv (frame reads this)
  FxShaderStorageBuffer* _sum  = nullptr;
  const FxComputeShader* _csReset   = nullptr;
  const FxComputeShader* _csDensity = nullptr;
  const FxComputeShader* _csBoxavg  = nullptr;
  const FxComputeShader* _csRhs     = nullptr;
  const FxComputeShader* _csSorR    = nullptr; // red half-sweep (parity 0)
  const FxComputeShader* _csSorB    = nullptr; // black half-sweep (parity 1)
  const FxComputeShader* _csWarp    = nullptr;
  const FxComputeShader* _csUpsample= nullptr;
  const FxComputeShader* _csFrame   = nullptr;
  int _iters = 0;
};

///////////////////////////////////////////////////////////////////////////////

static void _reshapeRelaxUvIOs(dataflow::moduledata_ptr_t data) {
  dflow::ModuleData::createInputPlug<HfImagePlugTraits>(data, dflow::EPR_UNIFORM, "In");
  dflow::ModuleData::createOutputPlug<HfImagePlugTraits>(data, dflow::EPR_UNIFORM, "Out");
  dflow::ModuleData::createOutputPlug<HfImagePlugTraits>(data, dflow::EPR_UNIFORM, "Binormal");
}
RelaxUvModuleData::RelaxUvModuleData() {}
std::shared_ptr<RelaxUvModuleData> RelaxUvModuleData::createShared() {
  auto d = std::make_shared<RelaxUvModuleData>(); _reshapeRelaxUvIOs(d); return d;
}
dflow::dgmoduleinst_ptr_t RelaxUvModuleData::createInstance(dflow::GraphInst* g) const {
  return std::make_shared<RelaxUvModuleInst>(this, g);
}
void RelaxUvModuleData::describeX(class_t* clazz) {
  clazz->setSharedFactory([]() -> rtti::castable_ptr_t { return RelaxUvModuleData::createShared(); });
  clazz->annotateTyped<dataflow::moduleIOreshape_fn_t>("reshapeIOs",
      [](dataflow::moduledata_ptr_t m) { _reshapeRelaxUvIOs(m); });
  clazz->directProperty("strength", &RelaxUvModuleData::_strength);
  clazz->directProperty("iterations", &RelaxUvModuleData::_iterations);
}

} // namespace ork::lev2::terrain
