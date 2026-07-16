#pragma once
////////////////////////////////////////////////////////////////
// Global debug kill-switches for culling — env-driven, read ONCE (cached), logged
// ONCE on engagement. Zero overhead when unset (a single cached bool test) and NO
// behavior change whatsoever when unset; these are debug levers for bisecting cull
// bugs (e.g. a VR invisibility regression), not tunables.
//
//   ORKID_DISABLE_FRUSTUM_CULL=1   -> every frustum-cull decision engine-wide treats
//                                     the candidate as visible. Host sites stamp the
//                                     cull param a shader-recognized "pass-all"
//                                     sentinel (never a recompiled constant — A8):
//                                       hypermesh  cs_cull        : u_tighten < 0
//                                       terrain    cs_terrain_cull: c_misc.x  < 0
//                                       rigidprim  cs_cull        : u_p0 == 1
//   ORKID_DISABLE_OCCLUSION_CULL=1 -> the HZB occlusion component is disabled (frustum
//                                     stays active). Host-side only: the HZB is not
//                                     bound / mode forced 0, so the occlusion branch in
//                                     each cull shader is skipped.
////////////////////////////////////////////////////////////////

#include <cstdlib>
#include <cstdio>

namespace ork::lev2 {

// true iff ORKID_DISABLE_FRUSTUM_CULL is set to a nonzero value (=0 / unset -> false).
inline bool cullFrustumDisabled() {
  static const bool s = []() {
    const char* e = std::getenv("ORKID_DISABLE_FRUSTUM_CULL");
    bool v        = e and (atoi(e) != 0);
    if (v)
      printf("[CULL] ORKID_DISABLE_FRUSTUM_CULL=1 — frustum culling DISABLED engine-wide (debug)\n");
    return v;
  }();
  return s;
}

// true iff ORKID_DISABLE_OCCLUSION_CULL is set to a nonzero value (=0 / unset -> false).
inline bool cullOcclusionDisabled() {
  static const bool s = []() {
    const char* e = std::getenv("ORKID_DISABLE_OCCLUSION_CULL");
    bool v        = e and (atoi(e) != 0);
    if (v)
      printf("[CULL] ORKID_DISABLE_OCCLUSION_CULL=1 — occlusion culling DISABLED engine-wide (debug)\n");
    return v;
  }();
  return s;
}

} // namespace ork::lev2
