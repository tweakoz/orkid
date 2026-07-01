////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
#include "hfdflow_module.h"

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
// ALGORITHM (linearized optimal transport — fully Eulerian Jacobi stencils, no global
// solve, no point advection; the same compute-pass-iteration idiom as erox/flow3d):
//   rho   = sqrt(1+|grad h|^2)                            // surface-area density (>1 on slopes)
//   solve  laplacian(psi) = (rho - mean(rho))  (Neumann)  // Jacobi, N iterations
//   uv    = planar + strength * grad(psi) / dim           // high-rho regions EXPAND -> more texels
// Neumann BC makes grad(psi) tangential at the border, so the boundary slides along the
// unit-square edges and the map stays in [0,1] for free.
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

static constexpr double kSumFix = 4096.0; // fixed-point scale for the atomic-uint density sum

// 0) clear the density accumulator (single uint)
static std::string _reset_text() {
  return R"S(
fxconfig fxcfg_default {}
storage_interface si_sum (descriptor_set 0) { buffer layout(std430) sb { uint sdata[1]; }; }
compute_interface iface { storage { si_sum } inputs { layout(local_size_x = 1, local_size_y = 1, local_size_z = 1); } }
compute_shader cs_reset : iface { sdata[0] = 0u; }
)S";
}

// 0a) DOWNSAMPLE the full-res height (dim) -> a COARSE grid (cdim) by bilinear sampling at the coarse
//     texel centers. The relaxation runs on this coarse height: a fold-free (bijective) UV map can only
//     redistribute area as a LOW-FREQUENCY deformation, so driving it from the sharp full-res density
//     self-intersects (folds). Coarse density -> smooth warp -> no folds (and ~(dim/cdim)^2 less Jacobi).
static std::string _downsample_text(int dim, int cdim) {
  std::string t = R"S(
fxconfig fxcfg_default {}
storage_interface si_in  (descriptor_set 0) { buffer layout(std430) ib { float hin[%DIMSQ%]; }; }
storage_interface si_out (descriptor_set 0) { buffer layout(std430) ob { float hout[%CDIMSQ%]; }; }
compute_interface iface { storage { si_in si_out } inputs { layout(local_size_x = 8, local_size_y = 8, local_size_z = 1); } }
compute_shader cs_downsample : iface {
  if (gl_GlobalInvocationID.x >= %CDIMU% || gl_GlobalInvocationID.y >= %CDIMU%) { return; }
  int cx = int(gl_GlobalInvocationID.x); int cy = int(gl_GlobalInvocationID.y);
  int D  = int(%DIMU%); uint Du = %DIMU%;
  float r  = float(%DIMU%) / float(%CDIMU%);
  float fx = (float(cx) + 0.5) * r - 0.5;          // dim-space sample center
  float fz = (float(cy) + 0.5) * r - 0.5;
  int x0 = int(floor(fx)); int z0 = int(floor(fz));
  float tx = fx - float(x0); float tz = fz - float(z0);
  int x0c = clamp(x0,   0, D-1); int x1c = clamp(x0+1, 0, D-1);
  int z0c = clamp(z0,   0, D-1); int z1c = clamp(z0+1, 0, D-1);
  float h00 = hin[uint(z0c)*Du+uint(x0c)]; float h10 = hin[uint(z0c)*Du+uint(x1c)];
  float h01 = hin[uint(z1c)*Du+uint(x0c)]; float h11 = hin[uint(z1c)*Du+uint(x1c)];
  hout[uint(cy)*%CDIMU%+uint(cx)] = mix(mix(h00,h10,tx), mix(h01,h11,tx), tz);
}
)S";
  _shadersub(t, "%DIMSQ%",  FormatString("%d", dim * dim));
  _shadersub(t, "%CDIMSQ%", FormatString("%d", cdim * cdim));
  _shadersub(t, "%DIMU%",   FormatString("%du", dim));
  _shadersub(t, "%CDIMU%",  FormatString("%du", cdim));
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
  float tx = fx - float(x0); float tz = fz - float(z0);
  int x0c = clamp(x0,   0, C-1); int x1c = clamp(x0+1, 0, C-1);
  int z0c = clamp(z0,   0, C-1); int z1c = clamp(z0+1, 0, C-1);
  vec2 c00 = vec2(cuv[2u*(uint(z0c)*Cu+uint(x0c))+0u], cuv[2u*(uint(z0c)*Cu+uint(x0c))+1u]);
  vec2 c10 = vec2(cuv[2u*(uint(z0c)*Cu+uint(x1c))+0u], cuv[2u*(uint(z0c)*Cu+uint(x1c))+1u]);
  vec2 c01 = vec2(cuv[2u*(uint(z1c)*Cu+uint(x0c))+0u], cuv[2u*(uint(z1c)*Cu+uint(x0c))+1u]);
  vec2 c11 = vec2(cuv[2u*(uint(z1c)*Cu+uint(x1c))+0u], cuv[2u*(uint(z1c)*Cu+uint(x1c))+1u]);
  vec2 uv  = mix(mix(c00,c10,tx), mix(c01,c11,tx), tz);
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

// 1) rho = sqrt(1+|grad h|^2)  (physical slope), and atomic-sum rho for the mean
static std::string _density_text(int dim, float aspect) {
  std::string t = R"S(
fxconfig fxcfg_default {}
storage_interface si_h   (descriptor_set 0) { buffer layout(std430) hb { float hdata[%DIMSQ%]; }; }
storage_interface si_rho (descriptor_set 0) { buffer layout(std430) rb { float rdata[%DIMSQ%]; }; }
storage_interface si_sum (descriptor_set 0) { buffer layout(std430) sb { uint  sdata[1]; }; }
compute_interface iface { storage { si_h si_rho si_sum } inputs { layout(local_size_x = 8, local_size_y = 8, local_size_z = 1); } }
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
  float rho = sqrt(1.0 + gx*gx + gz*gz);          // surface-area density (>=1)
  rdata[i] = rho;
  atomicAdd(sdata[0], uint(rho * %SUMFIX%));
}
)S";
  _shadersub(t, "%DIMSQ%", FormatString("%d", dim * dim));
  _shadersub(t, "%DIMU%", FormatString("%du", dim));
  _shadersub(t, "%ASPECT%", FormatString("%g", aspect));
  _shadersub(t, "%SUMFIX%", FormatString("%g", kSumFix));
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

// 3) one Jacobi sweep of laplacian(psi)=f, Neumann (clamped neighbors). out=slot0, in=slot1, f=slot2.
static std::string _jacobi_text(int dim) {
  std::string t = R"S(
fxconfig fxcfg_default {}
storage_interface si_o (descriptor_set 0) { buffer layout(std430) ob { float odata[%DIMSQ%]; }; }
storage_interface si_i (descriptor_set 0) { buffer layout(std430) ib { float idata[%DIMSQ%]; }; }
storage_interface si_f (descriptor_set 0) { buffer layout(std430) fb { float fdata[%DIMSQ%]; }; }
compute_interface iface { storage { si_o si_i si_f } inputs { layout(local_size_x = 8, local_size_y = 8, local_size_z = 1); } }
compute_shader cs_jacobi : iface {
  if (gl_GlobalInvocationID.x >= %DIMU% || gl_GlobalInvocationID.y >= %DIMU%) { return; }
  int  xi = int(gl_GlobalInvocationID.x);
  int  yi = int(gl_GlobalInvocationID.y);
  int  W  = int(%DIMU%); uint Wu = %DIMU%; uint i = uint(yi)*Wu + uint(xi);
  float pl = idata[(xi>0)   ? i-1u : i];   // Neumann: reflect the edge (no flux)
  float pr = idata[(xi<W-1) ? i+1u : i];
  float pd = idata[(yi>0)   ? i-Wu : i];
  float pu = idata[(yi<W-1) ? i+Wu : i];
  odata[i] = (pl + pr + pd + pu - fdata[i]) * 0.25;   // laplacian(psi)=f, unit texel spacing
}
)S";
  _shadersub(t, "%DIMSQ%", FormatString("%d", dim * dim));
  _shadersub(t, "%DIMU%", FormatString("%du", dim));
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
static std::string _frame_text(int dim, float cell, float height_m) {
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
  float C  = float(%CELL%); float Hm = float(%HEIGHTM%);
  // world surface tangents (central diff). x-step = (xr-xl) cells, etc.
  vec3 dPx = vec3(float(xr-xl)*C, (hdata[uint(yi)*Wu+uint(xr)] - hdata[uint(yi)*Wu+uint(xl)])*Hm, 0.0);
  vec3 dPz = vec3(0.0,            (hdata[uint(yu)*Wu+uint(xi)] - hdata[uint(yd)*Wu+uint(xi)])*Hm, float(yu-yd)*C);
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
  _shadersub(t, "%HEIGHTM%", FormatString("%g", height_m));
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
  void onActivate(dflow::GraphInst* inst) final {
    auto env = inst->_impl.getShared<BakeEnv>();
    auto fxi = env->_ctx->FXI();
    int dim  = env->_w;
    size_t n = size_t(dim) * size_t(dim);
    // COARSE relaxation grid: the deformation is low-frequency, so solve it on a capped grid (fold-free,
    // best equal-area, ~(dim/cdim)^2 less Jacobi) and upsample the uv. The FRAME (normal/binormal) still
    // runs at full dim. RELAX_CAP picked from the fold sweep (<=~2k stays fold-free; 512 is the sweet spot).
    const int RELAX_CAP = 256;
    int   cdim   = std::min(dim, RELAX_CAP);
    _cdim = cdim;
    size_t nC    = size_t(cdim) * size_t(cdim);
    float cell   = (dim  > 0) ? (env->_extent_m / float(dim))  : 1.0f;  // full-res cell (frame normals)
    float cellC  = (cdim > 0) ? (env->_extent_m / float(cdim)) : 1.0f;  // coarse cell (relax density)
    float aspect = env->_height_scale_m / cellC; // d(h_m)/d(x_m) per unit normalized-height gradient (COARSE)
    // outputs (RGBA32F) — at full bake dim
    _outUv->_value->_w = dim; _outUv->_value->_h = dim; _outUv->_value->_channels = 4;
    _outUv->_value->_ssbo = fxi->createStorageBuffer(n * 4 * sizeof(float));
    _outBn->_value->_w = dim; _outBn->_value->_h = dim; _outBn->_value->_channels = 4;
    _outBn->_value->_ssbo = fxi->createStorageBuffer(n * 4 * sizeof(float));
    // scratch — COARSE (density/Poisson/warp run here)
    _hC   = fxi->createStorageBuffer(nC * sizeof(float));     // downsampled height
    _rho  = fxi->createStorageBuffer(nC * sizeof(float));     // density, then reused as the Poisson RHS f
    _psiA = fxi->createStorageBuffer(nC * sizeof(float));
    _psiB = fxi->createStorageBuffer(nC * sizeof(float));
    { // seed psi = 0 (Jacobi start). Pre-dispatch (onActivate) so we never map a buffer mid-graph.
      auto m = fxi->mapStorageBuffer(_psiA, 0, nC * sizeof(float), BufferMapAccess::WRITE_ONLY);
      std::memset(m->_mappedaddr, 0, nC * sizeof(float));
      fxi->unmapStorageBuffer(m.get());
    }
    _uvC  = fxi->createStorageBuffer(nC * 2 * sizeof(float)); // coarse relaxed uv
    _uv   = fxi->createStorageBuffer(n  * 2 * sizeof(float)); // upsampled to full dim (frame reads this)
    _sum  = fxi->createStorageBuffer(sizeof(uint32_t));
    // shaders — relax passes at CDIM, downsample/upsample bridge dim<->cdim, frame at DIM
    _csDownsample = fxi->computeShader(fxi->shaderFromShaderText("terrain_relax_down", _downsample_text(dim, cdim)), "cs_downsample");
    _csReset   = fxi->computeShader(fxi->shaderFromShaderText("terrain_relax_reset",   _reset_text()), "cs_reset");
    _csDensity = fxi->computeShader(fxi->shaderFromShaderText("terrain_relax_density", _density_text(cdim, aspect)), "cs_density");
    _csRhs     = fxi->computeShader(fxi->shaderFromShaderText("terrain_relax_rhs",     _rhs_text(cdim)), "cs_rhs");
    _csJacobi  = fxi->computeShader(fxi->shaderFromShaderText("terrain_relax_jacobi",  _jacobi_text(cdim)), "cs_jacobi");
    _csWarp    = fxi->computeShader(fxi->shaderFromShaderText("terrain_relax_warp",    _warp_text(cdim, _d->_strength)), "cs_warp");
    _csUpsample= fxi->computeShader(fxi->shaderFromShaderText("terrain_relax_up",      _upsample_text(dim, cdim)), "cs_upsample");
    _csFrame   = fxi->computeShader(fxi->shaderFromShaderText("terrain_relax_frame",   _frame_text(dim, cell, env->_height_scale_m)), "cs_frame");
    _iters = (_d->_iterations > 0) ? _d->_iterations : std::min(4 * cdim, 4096);
  }
  void compute(dflow::GraphInst* inst, ui::updatedata_ptr_t) final {
    auto env = inst->_impl.getShared<BakeEnv>();
    auto ci  = env->_ctx->CI();
    auto in  = _srcImg(_input);
    OrkAssert(in && in->_ssbo);
    int gD = (env->_w + 7) / 8;       // full bake-dim dispatch
    int gC = (_cdim + 7) / 8;         // coarse relax-grid dispatch
    // 0) DOWNSAMPLE full-res height -> coarse height (the relax grid)
    ci->bindStorageBuffer(_csDownsample, 0, in->_ssbo);
    ci->bindStorageBuffer(_csDownsample, 1, _hC);
    ci->dispatchCompute(_csDownsample, gC, gC, 1); ci->storageBarrier();
    // 1) clear sum, density (+ atomic mean accumulate) — on the COARSE height
    ci->bindStorageBuffer(_csReset, 0, _sum);
    ci->dispatchCompute(_csReset, 1, 1, 1); ci->storageBarrier();
    ci->bindStorageBuffer(_csDensity, 0, _hC);
    ci->bindStorageBuffer(_csDensity, 1, _rho);
    ci->bindStorageBuffer(_csDensity, 2, _sum);
    ci->dispatchCompute(_csDensity, gC, gC, 1); ci->storageBarrier();
    // 2) RHS f = rho/mean - 1  (in place in _rho)
    ci->bindStorageBuffer(_csRhs, 0, _rho);
    ci->bindStorageBuffer(_csRhs, 1, _sum);
    ci->dispatchCompute(_csRhs, gC, gC, 1); ci->storageBarrier();
    // 3) Jacobi: solve laplacian(psi)=f at COARSE res. psi seeded to 0 in onActivate (clean Neumann start).
    FxShaderStorageBuffer* cur = _psiA; FxShaderStorageBuffer* nxt = _psiB;
    for (int it = 0; it < _iters; it++) {
      ci->bindStorageBuffer(_csJacobi, 0, nxt);
      ci->bindStorageBuffer(_csJacobi, 1, cur);
      ci->bindStorageBuffer(_csJacobi, 2, _rho);
      ci->dispatchCompute(_csJacobi, gC, gC, 1); std::swap(cur, nxt);
      if (((it + 1) % 64) == 0) { ci->endDispatchPhase(); ci->beginDispatchPhase(); }
      else ci->storageBarrier();
    }
    // 4) warp -> coarse relaxed uv  (cur holds the converged psi)
    ci->bindStorageBuffer(_csWarp, 0, _uvC);
    ci->bindStorageBuffer(_csWarp, 1, cur);
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
  uint64_t cookComputeHash(const std::vector<uint64_t>& ih, uint64_t ctx) const final {
    auto h = DataBlock::createHasher();
    h->accumulateString("terrain.relaxuv.v4"); // v4: COARSE-grid relax (fold-free low-freq) + upsample; fold-safe warp
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
  FxShaderStorageBuffer* _hC   = nullptr;     // downsampled (coarse) height
  FxShaderStorageBuffer* _rho  = nullptr;     // coarse density / Poisson RHS
  FxShaderStorageBuffer* _psiA = nullptr;     // coarse psi (ping)
  FxShaderStorageBuffer* _psiB = nullptr;     // coarse psi (pong)
  FxShaderStorageBuffer* _uvC  = nullptr;     // coarse relaxed uv
  FxShaderStorageBuffer* _uv   = nullptr;     // full-dim upsampled uv (frame reads this)
  FxShaderStorageBuffer* _sum  = nullptr;
  const FxComputeShader* _csDownsample = nullptr;
  const FxComputeShader* _csReset   = nullptr;
  const FxComputeShader* _csDensity = nullptr;
  const FxComputeShader* _csRhs     = nullptr;
  const FxComputeShader* _csJacobi  = nullptr;
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
