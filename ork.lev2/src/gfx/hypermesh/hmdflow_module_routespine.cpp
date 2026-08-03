////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
#include "hmdflow_module.h"
#include <ork/kernel/string/ConstString.h>
#include <ork/reflect/properties/DirectTypedVector.hpp> // directVectorProperty on std::vector<float>
#include <ork/lev2/gfx/terrain/dflow/hfdflow.h> // the terrain BakeEnv THIS module stocks for its field subgraph
#include <ork/lev2/gfx/meshutil/geometry.h>     // street_spine .ogeo artifact export (physics-proxy law)
#include <filesystem>
#include <cmath>
#include <queue>
#include <vector>
#include <deque>
#include <map>
#include <set>

ImplementReflectionX(ork::lev2::hypermesh::RouteSpineModuleData, "hypermesh::RouteSpineModuleData");

namespace ork::lev2::hypermesh {

///////////////////////////////////////////////////////////////////////////////
// RouteSpineModule (R-family, v1) — the FIRST layout producer: least-cost routing
// on a DECLARED cost grid (layout_cell_m; dim-INDEPENDENT — Q8) derived from the
// terrain family's slope/curvature/discharge field channels (spec §0 FORWARD
// coupling; the DisplaceByField ENV-OWNERSHIP pattern — this module stocks the
// terrain BakeEnv for the field subgraph living in the same graph). Produces the
// XfNodeGraph spine FOREST (single-parent v1; cycles are the v2 currency, refused
// loudly). CPU search at onTopologyReady (after the terrain fields have baked +
// become host-readable — the subdivide-readback pattern).
//
// THE ALGORITHM IS MIRRORED OPERATION-FOR-OPERATION from the pure-python reference
// obt.project/scripts/ork/hypergraph/dflow/roads/route_ref.py — the scatter.py <->
// hfdflow_scatter.cpp precedent. Any change here changes route_ref.py + bumps the
// content cook salt below (the parity gate pins them).
///////////////////////////////////////////////////////////////////////////////

static constexpr uint32_t kRoadGid = 1; // A1 gid band [20:32): road-surface material key

// truncating nearest-texel sample (channel 0) — hfdflow_scatter.cpp:_sampleNearest EXACT.
static inline float _sampleNearest(const std::vector<float>& f, int w, int h, double u, double v) {
  int xi = int(u * w), yi = int(v * h);
  xi = std::min(std::max(xi, 0), w - 1);
  yi = std::min(std::max(yi, 0), h - 1);
  return f[size_t(yi) * w + xi];
}

// the SOURCE field for an hfimg input = the CONNECTED output's value (mirrors displace _srcField).
static dflowgfx::gpucomputeimage2d_inst_ptr_t _srcField(dflowgfx::hfimg_inpluginst_ptr_t inp) {
  if (not inp) return nullptr;
  auto out = std::dynamic_pointer_cast<dflowgfx::hfimg_outpluginst_t>(inp->_connectedOutput);
  return out ? out->_value : nullptr;
}

// map an SSBO READ_ONLY -> host bytes (mid-graph-safe; readStorageBuffer's device-local
// readback aborts the command buffer mid-cascade — the documented Vulkan trap. This is the
// vtxFaceAdjacency mesh-readback pattern).
static void _mapRead(Context* ctx, FxShaderStorageBuffer* ssbo, size_t bytes, void* dst) {
  auto fxi = ctx->FXI();
  auto mp  = fxi->mapStorageBuffer(ssbo, 0, bytes, BufferMapAccess::READ_ONLY);
  std::memcpy(dst, mp->_mappedaddr, bytes);
  fxi->unmapStorageBuffer(mp.get());
}

// read an hfimg field's channel-0 back to a flat W*H CPU array. Empty if unconnected.
static bool _readField(Context* ctx, dflowgfx::gpucomputeimage2d_inst_ptr_t img,
                       std::vector<float>& out, int& w, int& h) {
  if (not(img and img->_ssbo)) return false;
  w = img->_w; h = img->_h;
  int ch = (img->_channels < 1) ? 1 : img->_channels;
  std::vector<float> raw(size_t(w) * h * ch);
  _mapRead(ctx, img->_ssbo, raw.size() * sizeof(float), raw.data());
  out.resize(size_t(w) * h);
  for (size_t i = 0; i < out.size(); i++)
    out[i] = raw[i * ch]; // channel 0
  return true;
}

// build one XfNode frame from a heading in the XZ plane, up = +Y (roads are ~planar).
static void _roadFrame(ork::hyper::XfNode& n, float hx, float hz, const fvec3& pos) {
  fvec3 d(hx, 0.0f, hz);
  float m = std::sqrt(d.dotWith(d));
  if (m < 1e-6f) d = fvec3(0, 0, 1); else d = d * (1.0f / m);
  fvec3 up(0, 1, 0);
  fvec3 r = up.crossWith(d).normalized(); // right (lateral)
  float* M = n._xform;                    // col-major: X=right, Y=heading, Z=up, T=pos
  M[0] = r.x;  M[1] = r.y;  M[2] = r.z;  M[3]  = 0;
  M[4] = d.x;  M[5] = d.y;  M[6] = d.z;  M[7]  = 0;
  M[8] = up.x; M[9] = up.y; M[10] = up.z; M[11] = 0;
  M[12] = pos.x; M[13] = pos.y; M[14] = pos.z; M[15] = 1;
}

///////////////////////////////////////////////////////////////////////////////
// SPINE SMOOTHING (curvature) — MIRRORS route_ref.py smooth_spine operation-for-
// operation. The raw Dijkstra path is an 8-neighbour staircase; each degree-2 run
// between ANCHORS (root/fork/leaf/POI, held fixed so the junction welds survive)
// becomes a centripetal Catmull-Rom curve resampled at station_m, then a per-station
// turn cap of station/min_radius (a hard curvature floor). Anchors keep the fork
// nodes so RoadMesh still welds aprons; grade is RE-limited afterwards.
///////////////////////////////////////////////////////////////////////////////
namespace {
using DPt = std::pair<double, double>;
static const int    kCRSub      = 16;
static const int    kCurvIters  = 64;
static const double kCurvRelax  = 0.5;
static const double kSmPi       = 3.14159265358979323846;

static DPt _crInterp(const DPt& p0, const DPt& p1, const DPt& p2, const DPt& p3,
                     double t, double t0, double t1, double t2, double t3) {
  auto lerp = [&](const DPt& a, const DPt& b, double ta, double tb) -> DPt {
    if (tb - ta < 1e-12) return a;
    double w = (t - ta) / (tb - ta);
    return {a.first + (b.first - a.first) * w, a.second + (b.second - a.second) * w};
  };
  DPt A1 = lerp(p0, p1, t0, t1), A2 = lerp(p1, p2, t1, t2), A3 = lerp(p2, p3, t2, t3);
  double wB1 = (t - t0) / std::max(1e-12, t2 - t0);
  DPt B1 = {A1.first + (A2.first - A1.first) * wB1, A1.second + (A2.second - A1.second) * wB1};
  double wB2 = (t - t1) / std::max(1e-12, t3 - t1);
  DPt B2 = {A2.first + (A3.first - A2.first) * wB2, A2.second + (A3.second - A2.second) * wB2};
  double wC = (t - t1) / std::max(1e-12, t2 - t1);
  return {B1.first + (B2.first - B1.first) * wC, B1.second + (B2.second - B1.second) * wC};
}

static std::vector<DPt> _crDense(const std::vector<DPt>& wp) {
  int n = int(wp.size());
  std::vector<DPt> out;
  if (n <= 1) { out = wp; return out; }
  if (n == 2) {
    for (int s = 0; s <= kCRSub; s++) {
      double f = double(s) / kCRSub;
      out.push_back({wp[0].first + (wp[1].first - wp[0].first) * f,
                     wp[0].second + (wp[1].second - wp[0].second) * f});
    }
    return out;
  }
  DPt pad0 = {2.0 * wp[0].first - wp[1].first, 2.0 * wp[0].second - wp[1].second};
  DPt padN = {2.0 * wp[n - 1].first - wp[n - 2].first, 2.0 * wp[n - 1].second - wp[n - 2].second};
  std::vector<DPt> P; P.push_back(pad0);
  for (auto& w : wp) P.push_back(w);
  P.push_back(padN);
  int M = int(P.size());
  for (int i = 1; i < M - 2; i++) {
    DPt p0 = P[i - 1], p1 = P[i], p2 = P[i + 1], p3 = P[i + 2];
    double t0 = 0.0;
    double t1 = t0 + std::sqrt(std::max(1e-9, std::hypot(p1.first - p0.first, p1.second - p0.second)));
    double t2 = t1 + std::sqrt(std::max(1e-9, std::hypot(p2.first - p1.first, p2.second - p1.second)));
    double t3 = t2 + std::sqrt(std::max(1e-9, std::hypot(p3.first - p2.first, p3.second - p2.second)));
    bool last = (i == M - 3);
    int steps = kCRSub + (last ? 1 : 0);
    for (int s = 0; s < steps; s++) {
      double t = t1 + (t2 - t1) * (double(s) / kCRSub);
      out.push_back(_crInterp(p0, p1, p2, p3, t, t0, t1, t2, t3));
    }
  }
  return out;
}

static std::vector<DPt> _resampleArclen(const std::vector<DPt>& dense, double station_m) {
  if (int(dense.size()) < 2) return dense;
  std::vector<double> clen(dense.size(), 0.0);
  for (size_t k = 1; k < dense.size(); k++)
    clen[k] = clen[k - 1] + std::hypot(dense[k].first - dense[k - 1].first, dense[k].second - dense[k - 1].second);
  double total = clen.back();
  std::vector<DPt> out;
  if (total < 1e-9) { out.push_back(dense.front()); out.push_back(dense.back()); return out; }
  int nseg = std::max(1, int(std::lround(total / std::max(1e-6, station_m))));
  int seg = 0;
  for (int s = 0; s <= nseg; s++) {
    double target = total * (double(s) / nseg);
    while (seg < int(clen.size()) - 2 && clen[seg + 1] < target) seg++;
    double span = clen[seg + 1] - clen[seg];
    double w = (span < 1e-12) ? 0.0 : (target - clen[seg]) / span;
    const DPt& a = dense[seg]; const DPt& b = dense[seg + 1];
    out.push_back({a.first + (b.first - a.first) * w, a.second + (b.second - a.second) * w});
  }
  return out;
}

static std::vector<DPt> _curvatureLimit(const std::vector<DPt>& pts, double station_m, double min_radius_m) {
  int n = int(pts.size());
  if (n < 3 || min_radius_m <= 1e-6) return pts;
  double cap = station_m / min_radius_m;
  std::vector<DPt> P = pts;
  for (int it = 0; it < kCurvIters; it++) {
    std::vector<DPt> Q = P;
    for (int i = 1; i < n - 1; i++) {
      double v0x = P[i].first - P[i - 1].first, v0z = P[i].second - P[i - 1].second;
      double v1x = P[i + 1].first - P[i].first, v1z = P[i + 1].second - P[i].second;
      double a0 = std::atan2(v0z, v0x), a1 = std::atan2(v1z, v1x);
      double dth = a1 - a0;
      while (dth > kSmPi)  dth -= 2.0 * kSmPi;
      while (dth < -kSmPi) dth += 2.0 * kSmPi;
      if (std::fabs(dth) > cap) {
        double midx = 0.5 * (P[i - 1].first + P[i + 1].first);
        double midz = 0.5 * (P[i - 1].second + P[i + 1].second);
        Q[i] = {P[i].first + kCurvRelax * (midx - P[i].first),
                P[i].second + kCurvRelax * (midz - P[i].second)};
      }
    }
    P = Q;
  }
  return P;
}

///////////////////////////////////////////////////////////////////////////////
// VERTICAL PROFILE — road_elev C1 continuity (parabolic sag/crest curves) + a
// no-burial clearance floor. Mirrors route_ref._apply_vertical_profile operation-
// for-operation. _verticalCurveLimit is the elevation analogue of _curvatureLimit:
// a Jacobi relaxation capping the per-station GRADE CHANGE at station/vcurve_len_m,
// so a grade change spreads over a parabolic curve at a constant rate of grade change.
///////////////////////////////////////////////////////////////////////////////
static const int    kVCurveIters = 64;
static const double kVCurveRelax  = 0.5;

// bilinear sample (channel 0) of a flat W*H field — the RoadMesh grounding sampler EXACT
// (hmdflow_module_roadmesh.cpp _bilinearField), so the clearance floor rides the SAME terrain
// the shoulder grounds on.
static double _bilinearField(const std::vector<float>& f, int W, int H, double u, double v) {
  if (W < 1 || H < 1 || f.empty()) return 0.0;
  double tx = u * W - 0.5, tz = v * H - 0.5;
  double x0 = std::floor(tx), z0 = std::floor(tz);
  double fx = tx - x0, fz = tz - z0;
  auto cl = [](int a, int lo, int hi) { return a < lo ? lo : (a > hi ? hi : a); };
  int xi0 = cl(int(x0), 0, W - 1), xi1 = cl(int(x0) + 1, 0, W - 1);
  int zi0 = cl(int(z0), 0, H - 1), zi1 = cl(int(z0) + 1, 0, H - 1);
  double a = f[size_t(zi0) * W + xi0], b = f[size_t(zi0) * W + xi1];
  double c = f[size_t(zi1) * W + xi0], d = f[size_t(zi1) * W + xi1];
  double top = a + (b - a) * fx, bot = c + (d - c) * fx;
  return top + (bot - top) * fz;
}

// cap the per-station grade change at station/vcurve_len_m (endpoints fixed), clamping to a
// per-node clearance floor after every pass. Only tight sag/crest grade breaks move (toward
// the neighbour midpoint); gentle constant-grade runs are untouched.
static std::vector<double> _verticalCurveLimit(const std::vector<double>& elev, double station_m,
                                               double vcurve_len_m, const std::vector<double>& floor) {
  int n = int(elev.size());
  std::vector<double> E = elev;
  for (int i = 0; i < n; i++) if (E[i] < floor[i]) E[i] = floor[i];
  if (n < 3 || vcurve_len_m <= 1e-6 || station_m <= 1e-9) return E;
  double cap = station_m / vcurve_len_m;
  for (int it = 0; it < kVCurveIters; it++) {
    std::vector<double> Q = E;
    for (int i = 1; i < n - 1; i++) {
      double g0 = (E[i] - E[i - 1]) / station_m;
      double g1 = (E[i + 1] - E[i]) / station_m;
      if (std::fabs(g1 - g0) > cap) {
        double mid = 0.5 * (E[i - 1] + E[i + 1]);
        Q[i] = E[i] + kVCurveRelax * (mid - E[i]);
      }
    }
    for (int i = 0; i < n; i++) if (Q[i] < floor[i]) Q[i] = floor[i];
    E = Q;
  }
  return E;
}
} // namespace

struct RouteSpineModuleInst : public MeshComputeInst {
  RouteSpineModuleInst(const RouteSpineModuleData* d, dflow::GraphInst* g)
      : MeshComputeInst(d, g), _d(d) {}

  const char* _cookSalt() const final {
    // new family = new cook-salt NAMESPACE (content salt, MANUAL bump on any algo change — cf. LSystem).
    // v2.6-vprofile: road_elev now gets C1 parabolic vertical curves + a no-burial clearance floor
    // (in addition to the v2-smooth Catmull-Rom XZ smoothing).
    return "roads routespine.v2.6-vprofile evalv=1";
  }

  void onLink(dflow::GraphInst* ginst) final {
    _output    = typedOutputNamed<dflowgfx::XfNodeGraphPlugTraits>("Out");
    _inHeight  = typedInputNamed<dflowgfx::HfImagePlugTraits>("Height");
    _inSlope   = typedInputNamed<dflowgfx::HfImagePlugTraits>("Slope");
    _inCurv    = typedInputNamed<dflowgfx::HfImagePlugTraits>("Curvature");
    _inDisch   = typedInputNamed<dflowgfx::HfImagePlugTraits>("Discharge");
    // ENV OWNERSHIP: stock the terrain BakeEnv for the field subgraph (DisplaceByField pattern).
    // _field_dim is RUNTIME terrain-bake resolution; the LAYOUT grid is layout_cell_m (declared),
    // so the produced spine is dim-independent regardless of the field bake resolution.
    auto fenv = ginst->_impl.getShared<terrain::BakeEnv>();
    if (not fenv) {
      auto menv = ginst->_impl.getShared<MeshEnv>();
      OrkAssert(menv and menv->_ctx);
      fenv            = std::make_shared<terrain::BakeEnv>();
      fenv->_ctx      = menv->_ctx;
      fenv->_w        = _d->_field_dim;
      fenv->_h        = _d->_field_dim;
      fenv->_extent_m = _d->_extent_m; // TRUE METERS across the field (natural-units law)
      ginst->_impl.setShared<terrain::BakeEnv>(fenv);
    } else if (fenv->_w != _d->_field_dim) {
      printf("RouteSpine<%s>: field_dim<%d> DIFFERS from the graph's already-stocked field env dim<%d> "
             "(first stocker wins; one field resolution per graph)\n",
             _dgmodule_data->_name.c_str(), _d->_field_dim, fenv->_w);
    }
  }

  // build the spine at onTopologyReady (after the terrain fields have baked + are host-readable).
  bool onTopologyReady(Context* ctx) final {
    if (_built) return false;
    auto xng = _output->_value; // XfNodeGraphInst (created via data_to_inst)
    if (not xng) return false;

    // --- sample the terrain fields to CPU (must be computed by now: eval 1 ran) ---
    // fw/fh = the (shared) field dim; each present field overwrites with the same value.
    std::vector<float> H, S, C, D;
    int fw = 0, fh = 0;
    bool haveH = _readField(ctx, _srcField(_inHeight), H, fw, fh);
    _readField(ctx, _srcField(_inSlope), S, fw, fh);
    _readField(ctx, _srcField(_inCurv),  C, fw, fh);
    _readField(ctx, _srcField(_inDisch), D, fw, fh);
    if (not haveH && S.empty() && C.empty() && D.empty()) {
      if (not _warned) {
        printf("RouteSpine<%s>: no terrain field connected (Height/Slope/Curvature/Discharge) — "
               "cannot route (wire terrain field channels, e.g. T.slope/T.discharge)\n",
               _dgmodule_data->_name.c_str());
        _warned = true;
      }
      return false;
    }
    if (fw == 0) { fw = _d->_field_dim; fh = _d->_field_dim; }

    // --- declared layout grid (dim-independent) ---
    const double extent = _d->_extent_m;
    const int L = std::max(1, int(std::lround(extent / double(_d->_layout_cell_m))));
    const double cell_m = extent / double(L);
    const int N = L * L;

    // --- cost / elevation / water at layout resolution (route_ref.CostGrid) ---
    std::vector<double> cost(N), elev(N);
    std::vector<uint8_t> water(N);
    for (int j = 0; j < L; j++) {
      for (int i = 0; i < L; i++) {
        double wx = (i + 0.5) * cell_m - extent * 0.5;
        double wz = (j + 0.5) * cell_m - extent * 0.5;
        double u = wx / extent + 0.5, v = wz / extent + 0.5;
        double slope = S.empty() ? 0.0 : _sampleNearest(S, fw, fh, u, v);
        double curv  = C.empty() ? 0.0 : _sampleNearest(C, fw, fh, u, v);
        double disch = D.empty() ? 0.0 : _sampleNearest(D, fw, fh, u, v);
        double eh    = H.empty() ? 0.0 : _sampleNearest(H, fw, fh, u, v);
        int c = j * L + i;
        elev[c] = eh;
        bool w  = disch >= _d->_disch_thresh;
        water[c] = w ? 1 : 0;
        double cc = _d->_base_cost + _d->_w_slope * std::max(0.0, slope) + _d->_w_curv * std::max(0.0, curv);
        if (w) cc += _d->_w_water;
        cost[c] = cc;
      }
    }
    auto cellOfWorld = [&](double wx, double wz) -> int {
      int i = int((wx + extent * 0.5) / cell_m);
      int j = int((wz + extent * 0.5) / cell_m);
      i = std::min(std::max(i, 0), L - 1); j = std::min(std::max(j, 0), L - 1);
      return j * L + i;
    };

    // --- POIs (data; flat x,z pairs) ---
    std::vector<int> poiCells;
    for (size_t k = 0; k + 1 < _d->_pois.size(); k += 2)
      poiCells.push_back(cellOfWorld(_d->_pois[k], _d->_pois[k + 1]));
    if (poiCells.size() < 2) {
      printf("RouteSpine<%s>: need >= 2 POIs to route a spine (got %zu) — refuse\n",
             _dgmodule_data->_name.c_str(), poiCells.size());
      _built = true; return false;
    }

    // --- Dijkstra predecessor tree from POI[0] (route_ref.dijkstra_tree) ---
    static const int NEI[8][2] = {{1,0},{-1,0},{0,1},{0,-1},{1,1},{1,-1},{-1,1},{-1,-1}};
    const double SQRT2 = std::sqrt(2.0), INF = 1e300;
    std::vector<double> dist(N, INF);
    std::vector<int> parent(N, -1);
    std::vector<uint8_t> done(N, 0);
    int root = poiCells[0];
    if (water[root]) {
      printf("RouteSpine<%s>: POI[0] cell %d is INSIDE water — cannot root there (raise disch_thresh "
             "or move the POI)\n", _dgmodule_data->_name.c_str(), root);
      _built = true; return false;
    }
    using QN = std::pair<double, int>; // (dist, cell) min-heap, lexicographic tie by cell
    std::priority_queue<QN, std::vector<QN>, std::greater<QN>> pq;
    dist[root] = 0.0; pq.push({0.0, root});
    auto edgeCost = [&](int a, int b, bool diag) -> double {
      if (water[a] || water[b]) return -1.0;
      double wd = cell_m * (diag ? SQRT2 : 1.0);
      double grade = std::fabs(elev[b] - elev[a]) / wd;
      if (grade > _d->_max_grade) return -1.0;
      return 0.5 * (cost[a] + cost[b]) * wd * (1.0 + _d->_grade_weight * grade);
    };
    while (not pq.empty()) {
      auto [d, c] = pq.top(); pq.pop();
      if (done[c]) continue;
      done[c] = 1;
      int ci = c % L, cj = c / L;
      for (auto& nb : NEI) {
        int ni = ci + nb[0], nj = cj + nb[1];
        if (ni < 0 || nj < 0 || ni >= L || nj >= L) continue;
        int nc = nj * L + ni;
        if (done[nc]) continue;
        double w = edgeCost(c, nc, nb[0] != 0 && nb[1] != 0);
        if (w < 0.0) continue;
        double nd = d + w;
        if (nd < dist[nc] - 1e-12) {
          dist[nc] = nd; parent[nc] = c; pq.push({nd, nc});
        } else if (std::fabs(nd - dist[nc]) <= 1e-12 && parent[nc] != -1 && c < parent[nc]) {
          parent[nc] = c; // equal-dist tie-break: lower predecessor
        }
      }
    }

    // --- union of predecessor chains POI->root; refuse on unreachable (route_ref.build_spine) ---
    std::set<int> onSpine;
    for (size_t k = 0; k < poiCells.size(); k++) {
      int pc = poiCells[k];
      if (dist[pc] >= INF) {
        printf("RouteSpine<%s>: POI #%zu (cell %d) UNREACHABLE from root (blocked by water/grade) — "
               "refuse (v1 has no bridges/cuts)\n", _dgmodule_data->_name.c_str(), k, pc);
        _built = true; return false;
      }
      int c = pc;
      while (c != -1) {
        if (onSpine.count(c)) break;
        onSpine.insert(c);
        c = parent[c];
      }
    }
    onSpine.insert(root);

    // --- deterministic BFS node order (children ascending cell index) ---
    std::map<int, std::vector<int>> childrenOf;
    for (int c : onSpine) {
      int pc = parent[c];
      if (pc != -1 && onSpine.count(pc)) childrenOf[pc].push_back(c);
    }
    for (auto& kv : childrenOf) std::sort(kv.second.begin(), kv.second.end());
    std::vector<int> order;
    std::map<int, int> cellToNode;
    std::deque<int> q;
    q.push_back(root); cellToNode[root] = 0; order.push_back(root);
    while (not q.empty()) {
      int c = q.front(); q.pop_front();
      auto it = childrenOf.find(c);
      if (it == childrenOf.end()) continue;
      for (int ch : it->second)
        if (not cellToNode.count(ch)) { cellToNode[ch] = int(order.size()); order.push_back(ch); q.push_back(ch); }
    }

    // --- build the RAW spine (BFS order), then SMOOTH it, then emit XfNodes ---
    std::set<int> poiSet(poiCells.begin(), poiCells.end());
    struct RawN { double wx, wz, elev; int parent; bool is_poi; };
    int Rn = int(order.size());
    std::vector<RawN> raw(Rn);
    for (int ni = 0; ni < Rn; ni++) {
      int c = order[ni];
      raw[ni].wx     = ((c % L) + 0.5) * cell_m - extent * 0.5;
      raw[ni].wz     = ((c / L) + 0.5) * cell_m - extent * 0.5;
      raw[ni].elev   = elev[c];
      int pc         = parent[c];
      raw[ni].parent = (pc != -1 && cellToNode.count(pc)) ? cellToNode[pc] : -1;
      raw[ni].is_poi = poiSet.count(c) > 0;
    }

    // chains: maximal degree-2 runs between anchors (root/fork/leaf/POI).
    std::vector<std::vector<int>> rchildren(Rn);
    for (int i = 0; i < Rn; i++) if (raw[i].parent >= 0) rchildren[raw[i].parent].push_back(i);
    for (auto& c : rchildren) std::sort(c.begin(), c.end());
    auto isAnchor = [&](int i) { return raw[i].parent < 0 || rchildren[i].size() != 1 || raw[i].is_poi; };
    std::vector<std::vector<int>> chains;
    for (int a = 0; a < Rn; a++) {
      if (not isAnchor(a)) continue;
      for (int c : rchildren[a]) {
        std::vector<int> ch = {a, c};
        int cur = c;
        while (not isAnchor(cur)) { int nxt = rchildren[cur][0]; ch.push_back(nxt); cur = nxt; }
        chains.push_back(ch);
      }
    }
    // smoothed XZ per chain (parallel to `chains`)
    std::vector<std::vector<DPt>> chainPts(chains.size());
    for (size_t ci = 0; ci < chains.size(); ci++) {
      std::vector<DPt> wp;
      for (int idx : chains[ci]) wp.push_back({raw[idx].wx, raw[idx].wz});
      auto dense = _crDense(wp);
      auto res   = _resampleArclen(dense, _d->_station_m);
      double tot = 0.0;
      for (size_t k = 1; k < res.size(); k++) tot += std::hypot(res[k].first - res[k - 1].first, res[k].second - res[k - 1].second);
      double spacing = tot / std::max(size_t(1), res.size() - 1);
      res = _curvatureLimit(res, spacing, _d->_min_radius_m);
      res.front() = wp.front(); res.back() = wp.back();
      chainPts[ci] = res;
    }
    // BFS reassembly (parent<index); FORK-STUB trim so the apron fits.
    struct SmN { double wx, wz, elev, road_elev, arclen, hx, hz; int parent; bool is_poi; };
    std::map<int, std::vector<int>> outgoing;
    for (size_t ci = 0; ci < chains.size(); ci++) outgoing[chains[ci][0]].push_back(int(ci));
    int rootRaw = -1;
    for (int i = 0; i < Rn; i++) if (raw[i].parent < 0) { rootRaw = i; break; }
    double stub_m = 3.0 * double(_d->_width_m);
    auto isFork = [&](int a) { auto it = outgoing.find(a); return it != outgoing.end() && it->second.size() >= 2; };
    std::vector<SmN> sm;
    std::map<int, int> anchorNew;
    auto mk = [&](double wx, double wz, bool is_poi, int parentNew) -> int {
      SmN n; n.wx = wx; n.wz = wz;
      int cc = cellOfWorld(wx, wz);
      n.elev = elev[cc]; n.road_elev = elev[cc]; n.parent = parentNew; n.is_poi = is_poi;
      n.arclen = 0.0; n.hx = 0.0; n.hz = 1.0;
      sm.push_back(n); return int(sm.size()) - 1;
    };
    anchorNew[rootRaw] = mk(raw[rootRaw].wx, raw[rootRaw].wz, raw[rootRaw].is_poi, -1);
    std::deque<int> sq; sq.push_back(rootRaw);
    while (not sq.empty()) {
      int a = sq.front(); sq.pop_front();
      int ai = anchorNew[a];
      auto it = outgoing.find(a);
      if (it == outgoing.end()) continue;
      std::vector<int> outs = it->second;
      std::sort(outs.begin(), outs.end(), [&](int x, int y) { return chains[x].back() < chains[y].back(); });
      for (int ci : outs) {
        auto& pts = chainPts[ci];
        int b = chains[ci].back();
        bool sf = isFork(a), ef = isFork(b);
        std::vector<double> arc(pts.size(), 0.0);
        for (size_t k = 1; k < pts.size(); k++) arc[k] = arc[k - 1] + std::hypot(pts[k].first - pts[k - 1].first, pts[k].second - pts[k - 1].second);
        double tot = arc.back();
        int prev = ai;
        for (size_t k = 1; k + 1 < pts.size(); k++) {
          if (sf && arc[k] < stub_m) continue;
          if (ef && (tot - arc[k]) < stub_m) continue;
          prev = mk(pts[k].first, pts[k].second, false, prev);
        }
        anchorNew[b] = mk(pts.back().first, pts.back().second, raw[b].is_poi, prev);
        sq.push_back(b);
      }
    }
    // frames + arc-length along the smoothed sequence
    for (size_t ni = 0; ni < sm.size(); ni++) {
      auto& n = sm[ni];
      if (n.parent >= 0) {
        auto& pn = sm[n.parent];
        double hx = n.wx - pn.wx, hz = n.wz - pn.wz, m = std::hypot(hx, hz);
        if (m > 1e-9) { n.hx = hx / m; n.hz = hz / m; }
        n.arclen = pn.arclen + m;
      }
    }
    if (sm.size() > 1) {
      int k0 = -1;
      for (size_t i = 0; i < sm.size(); i++) if (sm[i].parent == 0) { k0 = int(i); break; }
      if (k0 >= 0) {
        double hx = sm[k0].wx - sm[0].wx, hz = sm[k0].wz - sm[0].wz, m = std::hypot(hx, hz);
        if (m > 1e-9) { sm[0].hx = hx / m; sm[0].hz = hz / m; }
      }
    }
    // --- VERTICAL PROFILE (mirrors route_ref._apply_vertical_profile op-for-op) ---
    // (1) base grade-limited road_elev profile (follows terrain within max_grade).
    for (size_t ni = 1; ni < sm.size(); ni++) {
      auto& n = sm[ni]; if (n.parent < 0) continue;
      auto& pn = sm[n.parent];
      double seg = std::hypot(n.wx - pn.wx, n.wz - pn.wz);
      double lim = _d->_max_grade * seg, dz = n.elev - pn.road_elev;
      if (dz > lim) n.road_elev = pn.road_elev + lim;
      else if (dz < -lim) n.road_elev = pn.road_elev - lim;
      else n.road_elev = n.elev;
    }
    // smChildren for BOTH the clearance-floor miter footprint AND the chain decomposition.
    const int Sn = int(sm.size());
    std::vector<std::vector<int>> smChildren(Sn);
    for (int i = 0; i < Sn; i++) if (sm[i].parent >= 0) smChildren[sm[i].parent].push_back(i);
    for (auto& c : smChildren) std::sort(c.begin(), c.end());
    // (2) no-burial clearance floor: MAX terrain (bilinear on the shared Height) under the deck
    //     footprint (centre + both rails) + clearance_m. The rail dir is the MITER BISECTOR of the
    //     incident travel dirs (== RoadMesh PASS-1 node-ring heading), so the footprint lands on
    //     the deck's actual ±half-width rail positions and the clearance holds exactly there.
    std::vector<double> floorv(Sn, 0.0);
    {
      double hw = _d->_width_m * 0.5;
      auto sf = [&](double x, double z) {
        return _bilinearField(H, fw, fh, x / extent + 0.5, z / extent + 0.5);
      };
      for (int i = 0; i < Sn; i++) {
        double hx = 0.0, hz = 0.0;
        if (sm[i].parent >= 0) {
          double dx = sm[i].wx - sm[sm[i].parent].wx, dz = sm[i].wz - sm[sm[i].parent].wz;
          double m = std::hypot(dx, dz); if (m > 1e-9) { hx += dx / m; hz += dz / m; }
        }
        for (int c : smChildren[i]) {
          double dx = sm[c].wx - sm[i].wx, dz = sm[c].wz - sm[i].wz;
          double m = std::hypot(dx, dz); if (m > 1e-9) { hx += dx / m; hz += dz / m; }
        }
        double m = std::hypot(hx, hz);
        if (m > 1e-9) { hx /= m; hz /= m; } else { hx = 0.0; hz = 1.0; }
        double rx = hz, rz = -hx;
        double tc = sf(sm[i].wx, sm[i].wz);
        double tr = sf(sm[i].wx + rx * hw, sm[i].wz + rz * hw);
        double tl = sf(sm[i].wx - rx * hw, sm[i].wz - rz * hw);
        floorv[i] = std::max(std::max(tc, tr), tl) + _d->_clearance_m;
      }
    }
    // (3) per-chain parabolic vertical-curve smoothing (chains = degree-2 runs between anchors),
    //     solved against the clearance floor. Anchors iterated in index order (parent<child), so
    //     a chain ending at an anchor is smoothed before that anchor's outgoing chains read it.
    {
      auto smAnchor = [&](int i) { return sm[i].parent < 0 || smChildren[i].size() != 1 || sm[i].is_poi; };
      for (int a = 0; a < Sn; a++) {
        if (not smAnchor(a)) continue;
        for (int c0 : smChildren[a]) {
          std::vector<int> chain = {a, c0};
          int cur = c0;
          while (not smAnchor(cur)) { int nxt = smChildren[cur][0]; chain.push_back(nxt); cur = nxt; }
          int cn = int(chain.size());
          std::vector<double> elev(cn), fl(cn);
          double total = 0.0;
          for (int k = 0; k < cn; k++) { elev[k] = sm[chain[k]].road_elev; fl[k] = floorv[chain[k]]; }
          for (int k = 1; k < cn; k++) total += std::fabs(sm[chain[k]].arclen - sm[chain[k - 1]].arclen);
          double spacing = total / std::max(1, cn - 1);
          auto E = _verticalCurveLimit(elev, spacing, _d->_vcurve_len_m, fl);
          E.front() = std::max(elev.front(), fl.front());
          E.back()  = std::max(elev.back(),  fl.back());
          for (int k = 0; k < cn; k++) sm[chain[k]].road_elev = E[k];
        }
      }
    }
    // (4) re-enforce max_grade on the smoothed profile (safety; no-op on feasible terrain).
    for (size_t ni = 1; ni < sm.size(); ni++) {
      auto& n = sm[ni]; if (n.parent < 0) continue;
      auto& pn = sm[n.parent];
      double seg = std::hypot(n.wx - pn.wx, n.wz - pn.wz);
      double lim = _d->_max_grade * seg, dz = n.road_elev - pn.road_elev;
      if (dz > lim) n.road_elev = pn.road_elev + lim;
      else if (dz < -lim) n.road_elev = pn.road_elev - lim;
    }
    // emit XfNodes (position Y = terrain elev; attrs.z = grade-limited road_elev)
    xng->_nodes.clear();
    xng->_slots.clear();
    xng->_nodes.resize(sm.size());
    for (size_t ni = 0; ni < sm.size(); ni++) {
      auto& s = sm[ni];
      auto& n = xng->_nodes[ni];
      n._parent = (s.parent < 0) ? 0xffffffffu : uint32_t(s.parent);
      _roadFrame(n, float(s.hx), float(s.hz), fvec3(float(s.wx), float(s.elev), float(s.wz)));
      n._attrs[0] = _d->_width_m;
      n._attrs[1] = float(s.arclen);
      n._attrs[2] = float(s.road_elev);
      n._attrs[3] = s.is_poi ? 1.0f : 0.0f;
      n._tags = (kRoadGid & 0xFFFu) << 20;
    }

    // --- upload SoA SSBOs (the LSystem _buildSkeleton pattern) ---
    _uploadGraph(ctx, xng);

    // PHYSICS-PROXY LAW artifact export (owner 2026-07-22): write the spine's GENERATING
    // data as a named baked artifact — the road collider (and future nav/audio) consume it
    // BY NAME, never the render mesh (embellishments are render-only). Deck line
    // P=(x, road_elev, z); width per node; parent for segment topology. Runs CPU-side at
    // build time (materializeAll GPU-thread), BEFORE any entity activates — the scatter
    // .ogeo "declaration order = dependency order" convention exactly.
    if (not _d->_export_name.empty()) {
      auto geo   = std::make_shared<meshutil::Geometry>();
      auto chP   = geo->_point.createChannel<fvec3>("P");
      auto chW   = geo->_point.createChannel<float>("width");
      auto chPar = geo->_point.createChannel<int>("parent");
      const size_t N = sm.size();
      chP->_data.resize(N);
      chW->_data.resize(N);
      chPar->_data.resize(N);
      for (size_t ni = 0; ni < N; ni++) {
        auto& s          = sm[ni];
        chP->_data[ni]   = fvec3(float(s.wx), float(s.road_elev), float(s.wz));
        chW->_data[ni]   = _d->_width_m;
        chPar->_data[ni] = int(s.parent);
      }
      std::string outdir = file::Path::expandPathString("<assetcache>/roads/" + _d->_export_name);
      std::filesystem::create_directories(outdir);
      std::string opath = outdir + "/street_spine.ogeo";
      geo->writeChunkfile(file::Path(opath.c_str()));
      printf("routespine: exported street_spine artifact -> %s (%d nodes)\n", opath.c_str(), int(N));
    }

    _built = true;
    return true; // one re-eval so masks / parcelize consume the built spine
  }

  void _uploadGraph(Context* ctx, ork::hyper::xfnodegraph_inst_ptr_t xng) {
    auto fxi = ctx->FXI();
    const int N = int(xng->_nodes.size());
    xng->_count = N;
    if (N == 0) { xng->markTopoChanged(); return; }
    xng->_xform  = fxi->createStorageBuffer(size_t(N) * 16 * sizeof(float));
    xng->_parent = fxi->createStorageBuffer(size_t(N) * sizeof(uint32_t));
    xng->_attrs  = fxi->createStorageBuffer(size_t(N) * 4 * sizeof(float));
    xng->_tags   = fxi->createStorageBuffer(size_t(N) * sizeof(uint32_t));
    {
      auto mp  = fxi->mapStorageBuffer(xng->_xform, 0, size_t(N) * 16 * sizeof(float), BufferMapAccess::WRITE_ONLY);
      auto dst = (float*)mp->_mappedaddr;
      for (int i = 0; i < N; i++) for (int j = 0; j < 16; j++) dst[i * 16 + j] = xng->_nodes[i]._xform[j];
      fxi->unmapStorageBuffer(mp.get());
    }
    {
      auto mp  = fxi->mapStorageBuffer(xng->_parent, 0, size_t(N) * sizeof(uint32_t), BufferMapAccess::WRITE_ONLY);
      auto dst = (uint32_t*)mp->_mappedaddr;
      for (int i = 0; i < N; i++) dst[i] = xng->_nodes[i]._parent;
      fxi->unmapStorageBuffer(mp.get());
    }
    {
      auto mp  = fxi->mapStorageBuffer(xng->_attrs, 0, size_t(N) * 4 * sizeof(float), BufferMapAccess::WRITE_ONLY);
      auto dst = (float*)mp->_mappedaddr;
      for (int i = 0; i < N; i++) for (int j = 0; j < 4; j++) dst[i * 4 + j] = xng->_nodes[i]._attrs[j];
      fxi->unmapStorageBuffer(mp.get());
    }
    {
      auto mp  = fxi->mapStorageBuffer(xng->_tags, 0, size_t(N) * sizeof(uint32_t), BufferMapAccess::WRITE_ONLY);
      auto dst = (uint32_t*)mp->_mappedaddr;
      for (int i = 0; i < N; i++) dst[i] = xng->_nodes[i]._tags;
      fxi->unmapStorageBuffer(mp.get());
    }
    xng->markTopoChanged();
  }

  void compute(dflow::GraphInst*, ui::updatedata_ptr_t) final {} // static graph: produced at onTopologyReady

  const RouteSpineModuleData* _d;
  dflowgfx::xfng_outpluginst_ptr_t _output;
  dflowgfx::hfimg_inpluginst_ptr_t _inHeight, _inSlope, _inCurv, _inDisch;
  bool _built  = false;
  bool _warned = false;
};

///////////////////////////////////////////////////////////////////////////////
// roadsBakeReadout — Tier-B gate seam (see hmdflow.h). Navigates the materialized
// GraphInst by module-DATA class name + output plug type, reads the baked spine /
// fields / seed count back to the CPU for the coupling oracles.
///////////////////////////////////////////////////////////////////////////////
static bool _readHfField(Context* ctx, dflowgfx::gpucomputeimage2d_inst_ptr_t img,
                         std::vector<float>& out, int& dim) {
  if (not(img and img->_ssbo)) return false;
  int w = img->_w, h = img->_h, ch = (img->_channels < 1) ? 1 : img->_channels;
  std::vector<float> raw(size_t(w) * h * ch);
  _mapRead(ctx, img->_ssbo, raw.size() * sizeof(float), raw.data());
  out.resize(size_t(w) * h);
  for (size_t i = 0; i < out.size(); i++) out[i] = raw[i * ch];
  dim = w;
  return true;
}

RoadsBakeReadout roadsBakeReadout(livehypermesh_ptr_t live, Context* ctx) {
  RoadsBakeReadout R;
  if (not(live and live->_ginst)) return R;
  auto namedField = [&](dflow::dgmoduleinst_ptr_t inst, const char* out_name) -> dflowgfx::gpucomputeimage2d_inst_ptr_t {
    for (int o = 0; o < int(inst->numOutputs()); o++) {
      auto op = std::dynamic_pointer_cast<dflowgfx::hfimg_outpluginst_t>(inst->output(o));
      if (op and op->_plugdata and op->_plugdata->_name == out_name) return op->_value;
    }
    return nullptr;
  };
  for (auto inst : live->_ginst->_ordered_module_insts) {
    if (not(inst and inst->_dgmodule_data)) continue;
    std::string cn = inst->_dgmodule_data->GetClass()->Name().c_str();
    if (cn == "hypermesh::RouteSpineModuleData") {
      // spine: the XfNodeGraph output CPU mirror
      for (int o = 0; o < int(inst->numOutputs()); o++) {
        auto op = std::dynamic_pointer_cast<dflowgfx::xfng_outpluginst_t>(inst->output(o));
        if (not(op and op->_value)) continue;
        auto xng = op->_value;
        R.spine_count = int(xng->_nodes.size());
        R.spine_positions.reserve(R.spine_count * 3);
        R.spine_parents.reserve(R.spine_count);
        R.spine_road_elev.reserve(R.spine_count);
        R.spine_bytes.reserve(size_t(R.spine_count) * 88);
        for (auto& n : xng->_nodes) {
          R.spine_positions.push_back(n._xform[12]);
          R.spine_positions.push_back(n._xform[13]);
          R.spine_positions.push_back(n._xform[14]);
          R.spine_parents.push_back(n._parent);
          R.spine_road_elev.push_back(n._attrs[2]);
          R.spine_bytes.append(reinterpret_cast<const char*>(n._xform), 16 * sizeof(float));
          R.spine_bytes.append(reinterpret_cast<const char*>(&n._parent), sizeof(uint32_t));
          R.spine_bytes.append(reinterpret_cast<const char*>(n._attrs), 4 * sizeof(float));
          R.spine_bytes.append(reinterpret_cast<const char*>(&n._tags), sizeof(uint32_t));
        }
      }
      // the input fields wired to RouteSpine (flatten reference + exact reference parity)
      auto readInput = [&](const char* name, std::vector<float>& dst) {
        auto in = inst->typedInputNamed<dflowgfx::HfImagePlugTraits>(name);
        if (not in) return;
        auto out = std::dynamic_pointer_cast<dflowgfx::hfimg_outpluginst_t>(in->_connectedOutput);
        if (out) { int d = 0; if (_readHfField(ctx, out->_value, dst, d)) R.field_dim = d; }
      };
      readInput("Height", R.height_field);
      readInput("Slope", R.slope_field);
      readInput("Curvature", R.curv_field);
      readInput("Discharge", R.disch_field);
    } else if (cn == "hypermesh::RoadbedMaskModuleData") {
      int d = 0;
      _readHfField(ctx, namedField(inst, "Out"), R.roadbed, d);       // RoadbedMask primary = coverage
      _readHfField(ctx, namedField(inst, "RoadElev"), R.road_elev_field, d);
      if (d > 0) R.field_dim = d;
    } else if (cn == "hypermesh::KeepoutMaskModuleData") {
      int d = 0;
      _readHfField(ctx, namedField(inst, "Out"), R.keepout, d);       // KeepoutMask primary = keepout mask
    } else if (cn == "hypermesh::BuildingSeedsModuleData") {
      for (int o = 0; o < int(inst->numOutputs()); o++) {
        auto op = std::dynamic_pointer_cast<dflowgfx::instset_outpluginst_t>(inst->output(o));
        if (op and op->_value) R.seed_count = op->_value->_count;
      }
    }
  }
  return R;
}

static void _reshapeRouteSpineIOs(dataflow::moduledata_ptr_t data) {
  dflow::ModuleData::createInputPlug<dflowgfx::HfImagePlugTraits>(data, dflow::EPR_UNIFORM, "Height");
  dflow::ModuleData::createInputPlug<dflowgfx::HfImagePlugTraits>(data, dflow::EPR_UNIFORM, "Slope");
  dflow::ModuleData::createInputPlug<dflowgfx::HfImagePlugTraits>(data, dflow::EPR_UNIFORM, "Curvature");
  dflow::ModuleData::createInputPlug<dflowgfx::HfImagePlugTraits>(data, dflow::EPR_UNIFORM, "Discharge");
  dflow::ModuleData::createOutputPlug<dflowgfx::XfNodeGraphPlugTraits>(data, dflow::EPR_UNIFORM, "Out");
}
RouteSpineModuleData::RouteSpineModuleData() {}
std::shared_ptr<RouteSpineModuleData> RouteSpineModuleData::createShared() {
  auto d = std::make_shared<RouteSpineModuleData>();
  _reshapeRouteSpineIOs(d);
  return d;
}
dflow::dgmoduleinst_ptr_t RouteSpineModuleData::createInstance(dflow::GraphInst* g) const {
  return std::make_shared<RouteSpineModuleInst>(this, g);
}
void RouteSpineModuleData::describeX(class_t* clazz) {
  clazz->setSharedFactory([]() -> rtti::castable_ptr_t { return RouteSpineModuleData::createShared(); });
  clazz->annotateTyped<dataflow::moduleIOreshape_fn_t>(
      "reshapeIOs", [](dataflow::moduledata_ptr_t m) { _reshapeRouteSpineIOs(m); });
  clazz->annotateTyped<ConstString>("dsl.verb", "route_spine");
  clazz->annotateTyped<bool>("editor.palette", true);
  clazz->annotateTyped<int>("editor.palette.sort", 20);
  clazz->annotateTyped<ConstString>("editor.palette.recipe", "route_spine");
  clazz->directVectorProperty("pois", &RouteSpineModuleData::_pois);       // flat world x,z pairs (DATA)
  clazz->directProperty("extent_m", &RouteSpineModuleData::_extent_m);
  clazz->directProperty("layout_cell_m", &RouteSpineModuleData::_layout_cell_m);
  clazz->directProperty("field_dim", &RouteSpineModuleData::_field_dim);
  clazz->directProperty("width_m", &RouteSpineModuleData::_width_m);
  clazz->directProperty("max_grade", &RouteSpineModuleData::_max_grade);
  clazz->directProperty("w_slope", &RouteSpineModuleData::_w_slope);
  clazz->directProperty("w_curv", &RouteSpineModuleData::_w_curv);
  clazz->directProperty("w_water", &RouteSpineModuleData::_w_water);
  clazz->directProperty("disch_thresh", &RouteSpineModuleData::_disch_thresh);
  clazz->directProperty("grade_weight", &RouteSpineModuleData::_grade_weight);
  clazz->directProperty("base_cost", &RouteSpineModuleData::_base_cost);
  clazz->directProperty("export_name", &RouteSpineModuleData::_export_name);
  clazz->directProperty("seed", &RouteSpineModuleData::_seed);
  clazz->directProperty("min_radius_m", &RouteSpineModuleData::_min_radius_m);
  clazz->directProperty("station_m", &RouteSpineModuleData::_station_m);
  clazz->directProperty("vcurve_len_m", &RouteSpineModuleData::_vcurve_len_m);
  clazz->directProperty("clearance_m", &RouteSpineModuleData::_clearance_m);
}

} // namespace ork::lev2::hypermesh
