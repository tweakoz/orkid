////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
//
// hfdflow_module_scatterplace.cpp — see the ScatterPlaceModuleData block in hfdflow.h.
// The IN-GRAPH scatter placer + building-pad emitter. Runs CPU at cook (the basin_fill
// readback pattern; requires the per-op-synced HeightField bake driver), placing against
// its PRE-flatten input fields via the SHARED scatterPlaceCore (parity-pinned to
// scatter.py), rasterizing cut-and-fill pads (PadMask/PadElev) for stock MaskBlend to
// consume, and exporting the ScatterSet .ogeo itself (RouteSpine's artifact pattern).
//
////////////////////////////////////////////////////////////////
#include "hfdflow_module.h"
#include <ork/lev2/gfx/terrain/dflow/hfdflow_scatter.h>
#include <ork/lev2/gfx/asset_gen.h> // ScatterSinkData (the placement-param carrier scatterPlaceCore reads)
#include <ork/lev2/gfx/meshutil/geometry.h>
#include <ork/math/cvector3.h>
#include <ork/math/cvector4.h>
#include <ork/math/cmatrix4.h>
#include <filesystem>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <map>
#include <vector>

ImplementReflectionX(ork::lev2::terrain::ScatterPlaceModuleData, "terrain::ScatterPlaceModuleData");

namespace ork::lev2::terrain {

// max per-type WEIGHT inputs (W0..W{N-1}); the DSL wires only K contiguously and the
// rest are sorter-ignored (the ExprModule In0..In{N-1} pattern — no data-driven reshape).
static const int kMaxScatterTypes = 16;

///////////////////////////////////////////////////////////////////////////////
// host readback of a wired hfimg field's channel 0 (RouteSpine _readField). Empty
// if the input is unconnected. Sized at the input's own dims (== bake dims here).
///////////////////////////////////////////////////////////////////////////////
static bool _readFieldHost(Context* ctx, gpucomputeimage2d_inst_ptr_t img,
                           std::vector<float>& out, int& w, int& h) {
  if (not(img and img->_ssbo))
    return false;
  w      = img->_w;
  h      = img->_h;
  int ch = (img->_channels < 1) ? 1 : img->_channels;
  auto fxi = ctx->FXI();
  std::vector<float> raw(size_t(w) * size_t(h) * size_t(ch));
  auto mp = fxi->mapStorageBuffer(img->_ssbo, 0, raw.size() * sizeof(float), BufferMapAccess::READ_ONLY);
  std::memcpy(raw.data(), mp->_mappedaddr, raw.size() * sizeof(float));
  fxi->unmapStorageBuffer(mp.get());
  out.resize(size_t(w) * size_t(h));
  for (size_t i = 0; i < out.size(); i++)
    out[i] = raw[i * ch]; // channel 0
  return true;
}

// smoothstep (GLSL semantics) — the pad apron falloff.
static inline double _smoothstep(double e0, double e1, double x) {
  if (e1 <= e0)
    return (x < e0) ? 0.0 : 1.0;
  double t = (x - e0) / (e1 - e0);
  t        = t < 0.0 ? 0.0 : (t > 1.0 ? 1.0 : t);
  return t * t * (3.0 - 2.0 * t);
}

///////////////////////////////////////////////////////////////////////////////

struct ScatterPlaceModuleInst : public TerrainComputeInst {
  ScatterPlaceModuleInst(const ScatterPlaceModuleData* d, dflow::GraphInst* g)
      : TerrainComputeInst(d, g), _d(d) {}

  void onLink(dflow::GraphInst*) final {
    _outMask = typedOutputNamed<HfImagePlugTraits>("PadMask");
    _outElev = typedOutputNamed<HfImagePlugTraits>("PadElev");
    _outPass = typedOutputNamed<HfImagePlugTraits>("Out");
    _inHeight = typedInputNamed<HfImagePlugTraits>("Height");
    _inYaw    = typedInputNamed<HfImagePlugTraits>("YawField");
    _inW.clear();
    for (int t = 0; t < kMaxScatterTypes; t++)
      _inW.push_back(typedInputNamed<HfImagePlugTraits>(("W" + std::to_string(t)).c_str()));
  }

  void bakeAcquire(dflow::GraphInst* inst) final {
    auto env = inst->_impl.getShared<BakeEnv>();
    _allocOut(env.get(), _outMask->_value); // CPU module — output SSBOs only, no shaders
    _allocOut(env.get(), _outElev->_value);
    _allocOut(env.get(), _outPass->_value);
  }

  void compute(dflow::GraphInst* inst, ui::updatedata_ptr_t) final {
    auto env = inst->_impl.getShared<BakeEnv>();
    if (not env->_per_op_sync) { // ops self-defend: the CPU readbacks below need per-op submit+wait
      printf("terrain scatter_place<%s>: this graph's driver does NOT sync per op — the CPU readback "
             "would map unsubmitted data. Use scatter_place only in a HeightField bake.\n",
             _dgmodule_data->_name.c_str());
      OrkAssert(false);
    }
    auto fxi     = env->_ctx->FXI();
    const int W  = env->_w, H = env->_h;
    const size_t N = size_t(W) * size_t(H);
    const float extent_m = env->_extent_m;

    // --- host mirrors of the PRE-flatten input fields (RouteSpine namedField reads) ---
    std::vector<float> height;
    int fw = 0, fh = 0;
    if (not _readFieldHost(env->_ctx, _srcImg(_inHeight), height, fw, fh)) {
      printf("terrain scatter_place<%s>: no Height field connected — cannot place (wire the height)\n",
             _dgmodule_data->_name.c_str());
      OrkAssert(false);
    }
    // K = the CONTIGUOUS run of wired weight inputs (W0..W{K-1}); type_id = index.
    std::vector<std::vector<float>> wbuf;
    for (int t = 0; t < kMaxScatterTypes; t++) {
      auto src = _srcImg(_inW[t]);
      if (not(src and src->_ssbo))
        break; // contiguous requirement (matches the DSL's W0.. wiring)
      std::vector<float> w;
      int ww = 0, wh = 0;
      _readFieldHost(env->_ctx, src, w, ww, wh);
      wbuf.push_back(std::move(w));
    }
    if (wbuf.empty()) {
      printf("terrain scatter_place<%s>: no weight field wired (W0..) — cannot place\n",
             _dgmodule_data->_name.c_str());
      OrkAssert(false);
    }
    std::vector<float> yawbuf;
    bool have_yaw = _readFieldHost(env->_ctx, _srcImg(_inYaw), yawbuf, fw, fh);

    // --- build the placement-param carrier the shared core reads ----------------
    ScatterSinkData sink;
    sink._name       = _dgmodule_data->_name;
    sink._density    = _d->_density;
    sink._count      = _d->_count;
    sink._seed       = _d->_seed;
    sink._align      = _d->_align;
    sink._yaw_lo     = _d->_yaw_lo;
    sink._yaw_hi     = _d->_yaw_hi;
    sink._scale_lo   = _d->_scale_lo;
    sink._scale_hi   = _d->_scale_hi;
    sink._cutoff     = _d->_cutoff;
    sink._jitter     = _d->_jitter;
    sink._max_points = _d->_max_points;
    sink._lift       = _d->_lift;
    sink._lattice_m  = _d->_lattice_m;   // v2 — aggregation lattice (0 = off)
    sink._lane_every = _d->_lane_every;
    sink._lane_m     = _d->_lane_m;
    sink._yaw_mode   = _d->_yaw_mode;    // v2 — "hash" | "direct"
    sink._type_names = _d->_type_names;
    sink._type_colliders = _d->_type_colliders;

    ScatterField hview{height.data(), W, H};
    std::vector<ScatterField> wview;
    wview.reserve(wbuf.size());
    for (auto& w : wbuf)
      wview.push_back(ScatterField{w.data(), W, H});
    ScatterField yview{have_yaw ? yawbuf.data() : nullptr, W, H};

    auto geo = scatterPlaceCore(sink, hview, wview, have_yaw ? &yview : nullptr, extent_m);
    OrkAssert(geo);

    // --- v2 CLUSTER PADS: union intersecting footprints -> one grade plane each,
    //     rewrite member P.y, reject cross-level seams (before the raster reads P.y).
    if (_d->_cluster_pads)
      _clusterPads(geo, W, H, extent_m);

    // --- rasterize cut-and-fill PADS from the placed points ---------------------
    std::vector<float> padmask(N, 0.0f), padelev(N, 0.0f);
    _rasterizePads(geo, padmask, padelev, W, H, extent_m);

    // --- write outputs (PadMask, PadElev, Out=Height passthrough) ---------------
    auto writeField = [&](gpucomputeimage2d_inst_ptr_t img, const std::vector<float>& src) {
      auto mp = fxi->mapStorageBuffer(img->_ssbo, 0, N * sizeof(float), BufferMapAccess::WRITE_ONLY);
      std::memcpy(mp->_mappedaddr, src.data(), N * sizeof(float));
      fxi->unmapStorageBuffer(mp.get());
    };
    writeField(_outMask->_value, padmask);
    writeField(_outElev->_value, padelev);
    writeField(_outPass->_value, height);

    // --- artifact export (RouteSpine pattern): the ScatterSet .ogeo consumers read
    //     BY NAME. Positions/xforms carry the PAD elevation (P.y == the sampled grade,
    //     == PadElev under the footprint), so a building sits ON its pad. _export_path
    //     is stamped by HeightFieldGenData::materialize (empty in a bare test bake). ---
    if (not _d->_export_path.empty()) {
      std::string opath = _d->_export_path.c_str();
      std::filesystem::path pp(opath);
      if (pp.has_parent_path())
        std::filesystem::create_directories(pp.parent_path());
      geo->writeChunkfile(_d->_export_path);
      printf("terrain scatter_place: %s -> %d points -> %s\n",
             _dgmodule_data->_name.c_str(), geo->numPoints(), opath.c_str());
    } else {
      printf("terrain scatter_place<%s>: placed %d points (no export path — .ogeo skipped)\n",
             _dgmodule_data->_name.c_str(), geo->numPoints());
    }
  }

  // Rasterize each placed footprint (per-type half-extents + apron feather) into
  // PadMask (coverage, oriented rounded-rect SDF -> smoothstep falloff) and PadElev
  // (= the point's sampled ground elevation). Overlap rule: MAX coverage wins the
  // texel; PadElev = the elevation of that max-coverage point (ties: earliest point
  // in grid-cell order, which is the Geometry's point order).
  void _rasterizePads(std::shared_ptr<meshutil::Geometry> geo, std::vector<float>& padmask,
                      std::vector<float>& padelev, int W, int H, float extent_m) {
    auto chP = geo->_point.channelAs<fvec3>("P");
    auto chX = geo->_point.channelAs<fmtx4>("xform");
    auto chT = geo->_point.channelAs<int>("type_id");
    if (not(chP and chX and chT))
      return;
    const int Np = int(chP->_data.size());
    const double ext = double(extent_m);
    const double texel_m = ext / double(W);
    const double apron = double(_d->_apron_m);

    // per-type footprint half-extents (meters) — "hx:hz"; default a small square so a
    // type without a declared footprint still emits a (minimal) pad rather than nothing.
    const int K = int(_d->_type_names.size());
    std::vector<double> fhx(std::max(K, 1), texel_m), fhz(std::max(K, 1), texel_m);
    for (int t = 0; t < K; t++) {
      auto it = _d->_type_footprints.find(_d->_type_names[t]);
      if (it == _d->_type_footprints.end())
        continue;
      float hx = 0, hz = 0;
      if (sscanf(it->second.c_str(), "%f:%f", &hx, &hz) >= 1) {
        fhx[t] = (hx > 0.0f) ? double(hx) : texel_m;
        fhz[t] = (hz > 0.0f) ? double(hz) : fhx[t];
      }
    }

    for (int n = 0; n < Np; n++) {
      const fvec3 P    = chP->_data[n];
      const fmtx4 M    = chX->_data[n];
      const int   tid  = chT->_data[n];
      const double elev = double(P.y);
      const double hx = (tid >= 0 && tid < int(fhx.size())) ? fhx[tid] : texel_m;
      const double hz = (tid >= 0 && tid < int(fhz.size())) ? fhz[tid] : texel_m;

      // local frame from the xform's scaled tangent (col0) — the footprint's X axis in
      // XZ. Fall back to axis-aligned when the planar projection degenerates (a cliff-
      // tilted align="normal" basis).
      fvec4 c0 = M.column(0);
      double ax = c0.x, az = c0.z;
      double al = std::sqrt(ax * ax + az * az);
      if (al > 1e-6) { ax /= al; az /= al; } else { ax = 1.0; az = 0.0; }
      const double bx = -az, bz = ax; // local Z axis = local X rotated +90 in XZ

      // world -> texel bbox (pad reaches half-extent + apron)
      const double reach = std::max(hx, hz) + apron;
      const double cx = double(P.x), cz = double(P.z);
      auto worldToTexel = [&](double w, int dim) -> int {
        return int((w / ext + 0.5) * double(dim));
      };
      int rtx = int(std::ceil(reach / texel_m)) + 1;
      int cix = worldToTexel(cx, W), ciz = worldToTexel(cz, H);
      int x0 = std::max(0, cix - rtx), x1 = std::min(W - 1, cix + rtx);
      int z0 = std::max(0, ciz - rtx), z1 = std::min(H - 1, ciz + rtx);

      for (int zi = z0; zi <= z1; zi++) {
        const double wz = ((double(zi) + 0.5) / double(H) - 0.5) * ext;
        for (int xi = x0; xi <= x1; xi++) {
          const double wx = ((double(xi) + 0.5) / double(W) - 0.5) * ext;
          const double dx = wx - cx, dz = wz - cz;
          const double lx = dx * ax + dz * az; // project onto the footprint's local axes
          const double lz = dx * bx + dz * bz;
          const double qx = std::fabs(lx) - hx;
          const double qz = std::fabs(lz) - hz;
          const double outside =
              std::sqrt(std::max(qx, 0.0) * std::max(qx, 0.0) + std::max(qz, 0.0) * std::max(qz, 0.0));
          const double inside = std::min(std::max(qx, qz), 0.0);
          const double d      = outside + inside; // rounded-rect SDF: <=0 inside
          double cov;
          if (apron <= 0.0)
            cov = (d <= 0.0) ? 1.0 : 0.0;
          else
            cov = 1.0 - _smoothstep(0.0, apron, d);
          if (cov <= 0.0)
            continue;
          const size_t idx = size_t(zi) * size_t(W) + size_t(xi);
          if (float(cov) > padmask[idx]) { // MAX coverage wins; ties keep the earlier point
            padmask[idx] = float(cov);
            padelev[idx] = float(elev);
          }
        }
      }
    }
  }

  // v2 CLUSTER PADS — union INTERSECTING footprints into connected components (OBB-SAT
  // overlap in XZ), assign ONE area-weighted grade plane per component, and rewrite each
  // member point's P.y (+ xform translation, preserving the lift offset) to that plane.
  // Admission is the existing point order (== grid-cell priority order): a late candidate
  // whose natural grade would step > _max_seam_m against an already-admitted OVERLAPPING
  // component is REJECTED (dropped from the ScatterSet) — killing cross-level party-wall
  // interpenetration by construction. A component whose natural elevation spread exceeds
  // _cluster_step_m (>0) is permitted ONE terrace step (two sub-planes split at the
  // component mean); default (_cluster_step_m==0) is a single plane per component.
  void _clusterPads(std::shared_ptr<meshutil::Geometry> geo, int W, int /*H*/, float extent_m) {
    auto chP = geo->_point.channelAs<fvec3>("P");
    auto chX = geo->_point.channelAs<fmtx4>("xform");
    auto chT = geo->_point.channelAs<int>("type_id");
    if (not(chP and chX))
      return;
    const int Np = int(chP->_data.size());
    if (Np == 0)
      return;

    const double ext     = double(extent_m);
    const double texel_m = ext / double(W);

    // per-type footprint half-extents (meters) — the SAME table _rasterizePads uses.
    const int K = int(_d->_type_names.size());
    std::vector<double> fhx(std::max(K, 1), texel_m), fhz(std::max(K, 1), texel_m);
    for (int t = 0; t < K; t++) {
      auto it = _d->_type_footprints.find(_d->_type_names[t]);
      if (it == _d->_type_footprints.end())
        continue;
      float hx = 0, hz = 0;
      if (sscanf(it->second.c_str(), "%f:%f", &hx, &hz) >= 1) {
        fhx[t] = (hx > 0.0f) ? double(hx) : texel_m;
        fhz[t] = (hz > 0.0f) ? double(hz) : fhx[t];
      }
    }

    // per-point footprint OBB (center, unit local-X axis, half-extents) + natural grade.
    struct Foot { double cx, cz, ax, az, hx, hz, elev; };
    std::vector<Foot> ft(Np);
    for (int n = 0; n < Np; n++) {
      const fvec3 P   = chP->_data[n];
      const fmtx4 M   = chX->_data[n];
      const int   tid = chT ? chT->_data[n] : -1;
      fvec4 c0        = M.column(0);
      double ax = c0.x, az = c0.z, al = std::sqrt(ax * ax + az * az);
      if (al > 1e-6) { ax /= al; az /= al; } else { ax = 1.0; az = 0.0; }
      const double hx = (tid >= 0 && tid < int(fhx.size())) ? fhx[tid] : texel_m;
      const double hz = (tid >= 0 && tid < int(fhz.size())) ? fhz[tid] : texel_m;
      ft[n] = Foot{double(P.x), double(P.z), ax, az, hx, hz, double(P.y)};
    }

    // OBB-OBB overlap in XZ (separating-axis over the 4 box edge normals).
    auto overlap = [](const Foot& A, const Foot& B) -> bool {
      const double Aa[2] = {A.ax, A.az}, Ab[2] = {-A.az, A.ax};
      const double Ba[2] = {B.ax, B.az}, Bb[2] = {-B.az, B.ax};
      const double dx = B.cx - A.cx, dz = B.cz - A.cz;
      auto sep = [&](double Lx, double Lz) -> bool {
        const double d  = std::fabs(dx * Lx + dz * Lz);
        const double rA = A.hx * std::fabs(Aa[0] * Lx + Aa[1] * Lz) + A.hz * std::fabs(Ab[0] * Lx + Ab[1] * Lz);
        const double rB = B.hx * std::fabs(Ba[0] * Lx + Ba[1] * Lz) + B.hz * std::fabs(Bb[0] * Lx + Bb[1] * Lz);
        return d > rA + rB;
      };
      return not(sep(Aa[0], Aa[1]) or sep(Ab[0], Ab[1]) or sep(Ba[0], Ba[1]) or sep(Bb[0], Bb[1]));
    };

    // union-find over ADMITTED points; component accumulators keyed by root index.
    std::vector<int> parent(Np, -1); // -1 == not admitted (rejected or unseen)
    std::vector<double> csumAE(Np, 0.0), csumA(Np, 0.0), cplane(Np, 0.0);
    auto findRoot = [&](int x) { while (parent[x] != x) { parent[x] = parent[parent[x]]; x = parent[x]; } return x; };
    std::vector<char> rejected(Np, 0);
    std::vector<int> admitted;
    admitted.reserve(Np);
    const double max_seam = double(_d->_max_seam_m);

    for (int n = 0; n < Np; n++) {
      const double area_n = std::max(1e-6, 4.0 * ft[n].hx * ft[n].hz);
      const double elev_n = ft[n].elev;
      std::vector<int> roots;
      bool seam_violation = false;
      for (int m : admitted) {
        if (not overlap(ft[n], ft[m]))
          continue;
        const int r = findRoot(m);
        if (std::fabs(elev_n - cplane[r]) > max_seam)
          seam_violation = true;
        roots.push_back(r);
      }
      if (seam_violation) { rejected[n] = 1; continue; } // cross-level seam -> drop the late candidate
      parent[n] = n; csumAE[n] = area_n * elev_n; csumA[n] = area_n; cplane[n] = elev_n;
      for (int r0 : roots) {
        const int r = findRoot(r0), a = findRoot(n);
        if (r == a)
          continue;
        parent[r] = a;
        csumAE[a] += csumAE[r]; csumA[a] += csumA[r];
        cplane[a] = csumAE[a] / csumA[a];
      }
      admitted.push_back(n);
    }

    // resolve each point's target plane (single, or one terrace step when spread exceeds).
    const double step_thresh = double(_d->_cluster_step_m);
    std::map<int, std::vector<int>> comps;
    for (int n : admitted)
      comps[findRoot(n)].push_back(n);
    std::vector<double> pointPlane(Np, 0.0);
    for (auto& kv : comps) {
      auto& members = kv.second;
      double lo = 1e30, hi = -1e30;
      for (int n : members) { lo = std::min(lo, ft[n].elev); hi = std::max(hi, ft[n].elev); }
      if (step_thresh > 0.0 and (hi - lo) > step_thresh) {
        const double mid = cplane[kv.first]; // split at the component mean -> two sub-planes
        double aeL = 0, aL = 0, aeH = 0, aH = 0;
        for (int n : members) {
          const double area = std::max(1e-6, 4.0 * ft[n].hx * ft[n].hz);
          if (ft[n].elev < mid) { aeL += area * ft[n].elev; aL += area; }
          else                  { aeH += area * ft[n].elev; aH += area; }
        }
        const double pl = (aL > 0) ? aeL / aL : mid;
        const double ph = (aH > 0) ? aeH / aH : mid;
        for (int n : members)
          pointPlane[n] = (ft[n].elev < mid) ? pl : ph;
      } else {
        const double pl = cplane[kv.first];
        for (int n : members)
          pointPlane[n] = pl;
      }
    }

    // rewrite admitted P.y + xform.col3.y (preserve lift), COMPACT out the rejected.
    auto chS  = geo->_point.channelAs<int>("variant_seed");
    auto chPK = geo->_point.channelAs<int>("proxy_kind");
    auto chPD = geo->_point.channelAs<fvec3>("proxy_dims");
    int nk = 0;
    for (int n = 0; n < Np; n++) {
      if (parent[n] == -1) // rejected / unadmitted
        continue;
      fvec3 P     = chP->_data[n];
      fmtx4 M     = chX->_data[n];
      const double dy = pointPlane[n] - double(P.y); // shift keeps the lift/normal offset
      fvec4 c3    = M.column(3);
      c3.y        = float(double(c3.y) + dy);
      M.setColumn(3, c3);
      P.y         = float(pointPlane[n]);
      chP->_data[nk] = P;
      chX->_data[nk] = M;
      if (chT)  chT->_data[nk]  = chT->_data[n];
      if (chS)  chS->_data[nk]  = chS->_data[n];
      if (chPK) chPK->_data[nk] = chPK->_data[n];
      if (chPD) chPD->_data[nk] = chPD->_data[n];
      nk++;
    }
    chP->_data.resize(nk);
    chX->_data.resize(nk);
    if (chT)  chT->_data.resize(nk);
    if (chS)  chS->_data.resize(nk);
    if (chPK) chPK->_data.resize(nk);
    if (chPD) chPD->_data.resize(nk);
    if (nk != Np)
      printf("terrain scatter_place<%s>: cluster pads admitted %d/%d (rejected %d cross-level seams)\n",
             _dgmodule_data->_name.c_str(), nk, Np, Np - nk);
  }

  bool cookCacheDefault() const final { return false; } // .ogeo side-effect => must recompute (never cache)
  uint64_t cookComputeHash(const std::vector<uint64_t>& ih, uint64_t ctx) const final {
    auto h = DataBlock::createHasher();
    h->accumulateString("terrain.scatterplace.v1");
    h->accumulateItem<float>(_d->_density);
    h->accumulateItem<int>(_d->_count);
    h->accumulateItem<int>(_d->_seed);
    h->accumulateString(_d->_align);
    h->accumulateItem<float>(_d->_yaw_lo);
    h->accumulateItem<float>(_d->_yaw_hi);
    h->accumulateItem<float>(_d->_scale_lo);
    h->accumulateItem<float>(_d->_scale_hi);
    h->accumulateItem<float>(_d->_cutoff);
    h->accumulateItem<float>(_d->_jitter);
    h->accumulateItem<int>(_d->_max_points);
    h->accumulateItem<float>(_d->_lift);
    h->accumulateItem<float>(_d->_apron_m);
    h->accumulateItem<float>(_d->_lattice_m);      // v2
    h->accumulateItem<int>(_d->_lane_every);
    h->accumulateItem<float>(_d->_lane_m);
    h->accumulateString(_d->_yaw_mode);
    h->accumulateItem<int>(_d->_cluster_pads ? 1 : 0);
    h->accumulateItem<float>(_d->_cluster_step_m);
    h->accumulateItem<float>(_d->_max_seam_m);
    h->accumulateString(_d->_export_name);
    for (const auto& nm : _d->_type_names)
      h->accumulateString(nm);
    for (const auto& kv : _d->_type_footprints) { h->accumulateString(kv.first); h->accumulateString(kv.second); }
    for (const auto& kv : _d->_type_colliders)  { h->accumulateString(kv.first); h->accumulateString(kv.second); }
    _mixTail(h, ctx, ih);
    h->finish();
    return h->result();
  }

  const ScatterPlaceModuleData* _d;
  hfimg_outpluginst_ptr_t _outMask, _outElev, _outPass;
  hfimg_inpluginst_ptr_t _inHeight, _inYaw;
  std::vector<hfimg_inpluginst_ptr_t> _inW;
};

///////////////////////////////////////////////////////////////////////////////

static void _reshapeScatterPlaceIOs(dataflow::moduledata_ptr_t data) {
  // FIXED-count weight inputs (W0..W{kMaxScatterTypes-1}); the DSL wires K contiguously,
  // the rest are unconnected -> sorter-ignored (ExprModule In0.. pattern). createInputPlug
  // dedups, so the double reshape on deserialize is idempotent.
  for (int t = 0; t < kMaxScatterTypes; t++)
    dflow::ModuleData::createInputPlug<HfImagePlugTraits>(data, dflow::EPR_UNIFORM, ("W" + std::to_string(t)).c_str());
  dflow::ModuleData::createInputPlug<HfImagePlugTraits>(data, dflow::EPR_UNIFORM, "Height");
  dflow::ModuleData::createInputPlug<HfImagePlugTraits>(data, dflow::EPR_UNIFORM, "YawField"); // optional
  dflow::ModuleData::createOutputPlug<HfImagePlugTraits>(data, dflow::EPR_UNIFORM, "PadMask");
  dflow::ModuleData::createOutputPlug<HfImagePlugTraits>(data, dflow::EPR_UNIFORM, "PadElev");
  dflow::ModuleData::createOutputPlug<HfImagePlugTraits>(data, dflow::EPR_UNIFORM, "Out");
}
ScatterPlaceModuleData::ScatterPlaceModuleData() {}
std::shared_ptr<ScatterPlaceModuleData> ScatterPlaceModuleData::createShared() {
  auto d = std::make_shared<ScatterPlaceModuleData>();
  _reshapeScatterPlaceIOs(d);
  return d;
}
dflow::dgmoduleinst_ptr_t ScatterPlaceModuleData::createInstance(dflow::GraphInst* g) const {
  return std::make_shared<ScatterPlaceModuleInst>(this, g);
}
void ScatterPlaceModuleData::describeX(class_t* clazz) {
  clazz->setSharedFactory([]() -> rtti::castable_ptr_t { return ScatterPlaceModuleData::createShared(); });
  clazz->annotateTyped<dataflow::moduleIOreshape_fn_t>("reshapeIOs",
      [](dataflow::moduledata_ptr_t m) { _reshapeScatterPlaceIOs(m); });
  clazz->annotateTyped<ConstString>("dsl.verb", "scatter_place");
  clazz->directProperty("density", &ScatterPlaceModuleData::_density);
  clazz->directProperty("count", &ScatterPlaceModuleData::_count);
  clazz->directProperty("seed", &ScatterPlaceModuleData::_seed);
  clazz->directProperty("align", &ScatterPlaceModuleData::_align);
  clazz->directProperty("yaw_lo", &ScatterPlaceModuleData::_yaw_lo);
  clazz->directProperty("yaw_hi", &ScatterPlaceModuleData::_yaw_hi);
  clazz->directProperty("scale_lo", &ScatterPlaceModuleData::_scale_lo);
  clazz->directProperty("scale_hi", &ScatterPlaceModuleData::_scale_hi);
  clazz->directProperty("cutoff", &ScatterPlaceModuleData::_cutoff);
  clazz->directProperty("jitter", &ScatterPlaceModuleData::_jitter);
  clazz->directProperty("max_points", &ScatterPlaceModuleData::_max_points);
  clazz->directProperty("lift", &ScatterPlaceModuleData::_lift);
  clazz->directProperty("apron_m", &ScatterPlaceModuleData::_apron_m);
  clazz->directProperty("lattice_m", &ScatterPlaceModuleData::_lattice_m);
  clazz->directProperty("lane_every", &ScatterPlaceModuleData::_lane_every);
  clazz->directProperty("lane_m", &ScatterPlaceModuleData::_lane_m);
  clazz->directProperty("yaw_mode", &ScatterPlaceModuleData::_yaw_mode);
  clazz->directProperty("cluster_pads", &ScatterPlaceModuleData::_cluster_pads);
  clazz->directProperty("cluster_step_m", &ScatterPlaceModuleData::_cluster_step_m);
  clazz->directProperty("max_seam_m", &ScatterPlaceModuleData::_max_seam_m);
  clazz->directProperty("export_name", &ScatterPlaceModuleData::_export_name);
  clazz->directVectorProperty("type_names", &ScatterPlaceModuleData::_type_names);
  clazz->directMapProperty("type_footprints", &ScatterPlaceModuleData::_type_footprints);
  clazz->directMapProperty("type_colliders", &ScatterPlaceModuleData::_type_colliders);
  clazz->directMapProperty("type_assets", &ScatterPlaceModuleData::_type_assets);
  clazz->directMapProperty("type_materials", &ScatterPlaceModuleData::_type_materials);
}

} // namespace ork::lev2::terrain
