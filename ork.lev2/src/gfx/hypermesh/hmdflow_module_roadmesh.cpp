////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
#include "hmdflow_module.h"
#include <ork/kernel/string/ConstString.h>
#include <ork/lev2/gfx/terrain/dflow/hfdflow.h> // Height HfImage input (the shared terrain field, for grounding)
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <map>
#include <vector>

ImplementReflectionX(ork::lev2::hypermesh::RoadMeshModuleData, "hypermesh::RoadMeshModuleData");

namespace ork::lev2::hypermesh {

namespace dgfx = ork::lev2::dflowgfx;

///////////////////////////////////////////////////////////////////////////////
// RoadMeshModule (R-family v2) — the SKINNER. Consumes the RouteSpine XfNodeGraph
// FOREST and emits a swept road-ribbon GpuMesh + junction patches + a gid material
// split. MIRRORS obt.project/scripts/ork/hypergraph/dflow/roads/roadmesh_ref.py
// OPERATION-FOR-OPERATION (the LSweep <-> route_ref precedent). CPU build from the
// XfNodeGraph CPU mirror + upload (the LSweep/MergeMesh path). The junction weld is
// crack-free by construction (patch boundary verts COINCIDE with the segment mouths
// -> the render + meshvet position-weld collapse the seam to a manifold edge).
///////////////////////////////////////////////////////////////////////////////

static ::ork::hyper::xfnodegraph_inst_ptr_t _srcXfng(dgfx::xfng_inpluginst_ptr_t inp) {
  auto out = std::dynamic_pointer_cast<dgfx::xfng_outpluginst_t>(inp->_connectedOutput);
  return out ? out->_value : nullptr;
}
static dgfx::gpucomputeimage2d_inst_ptr_t _srcHfImg(dgfx::hfimg_inpluginst_ptr_t inp) {
  if (not inp) return nullptr;
  auto out = std::dynamic_pointer_cast<dgfx::hfimg_outpluginst_t>(inp->_connectedOutput);
  return out ? out->_value : nullptr;
}

// bilinear sample of a flat W*H channel-0 field at uv — the grounding sampler (matches
// roadmesh_ref's terrain_h field wrapper EXACTLY so the outer-ring Y is byte-exact vs the
// shared terrain the layout routes on).
static double _bilinearField(const std::vector<float>& f, int W, int H, double u, double v) {
  if (W < 1 or H < 1 or f.empty()) return 0.0;
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

// the geometry is computed in DOUBLE (matching roadmesh_ref.py) then stored as float,
// so the C++ tracks the reference to float precision (the parity gate's tolerance).
static const double kPi          = 3.14159265358979323846;
static const double kEps         = 1e-6;
static const double kAngleFloor  = 3.0  * kPi / 180.0;  // cap the apron auto-grow
static const double kAngleHard   = 6.0  * kPi / 180.0;  // below this stub gap -> near-tangent (refuse)
static const double kCornerMargin = 0.85;               // keep corner half-angle strictly < half-gap

static inline void _norm2(double dx, double dz, double& nx, double& nz, double& len) {
  len = std::hypot(dx, dz);
  if (len < kEps) { nx = 0.0; nz = 0.0; return; }
  nx = dx / len; nz = dz / len;
}
static inline void _right(double dx, double dz, double& rx, double& rz) { rx = dz;  rz = -dx; } // right of heading
static inline void _left (double dx, double dz, double& lx, double& lz) { lx = -dz; lz = dx;  } // left  of heading

struct RmNode {
  double wx, wz, y, width, arclen;
  int parent;
};

struct RoadMeshModuleInst : public MeshComputeInst {
  RoadMeshModuleInst(const RoadMeshModuleData* d, dflow::GraphInst* g) : MeshComputeInst(d, g), _d(d) {}

  void onLink(dflow::GraphInst*) final {
    _output   = typedOutputNamed<MeshPlugTraits>("Out");
    _input    = typedInputNamed<dgfx::XfNodeGraphPlugTraits>("In");
    _inHeight = typedInputNamed<dgfx::HfImagePlugTraits>("Height"); // OPTIONAL grounding field
  }

  bool _isJunction(int i) const { return int(_children[i].size()) >= 2; }

  // apron setback for junction J: auto-grows so incident stubs' mouths don't overlap,
  // clamped to junction_min_edge_frac of the shortest incident edge. ok=false + reason.
  double _junctionSetback(int J, bool& ok, int& reason) const {
    const auto& a = _nodes[J];
    double hw = a.width * 0.5;
    std::vector<double> thetas;
    double edge_clamp = 1e300;
    for (int nbr : _incident[J]) {
      const auto& b = _nodes[nbr];
      double ux, uz, el; _norm2(b.wx - a.wx, b.wz - a.wz, ux, uz, el);
      thetas.push_back(std::atan2(uz, ux));
      edge_clamp = std::min(edge_clamp, _d->_junction_min_edge_frac * el);
    }
    std::sort(thetas.begin(), thetas.end());
    int n = int(thetas.size());
    double dmin = 2.0 * kPi;
    for (int k = 0; k < n; k++) {
      double gap = thetas[(k + 1) % n] - thetas[k];
      if (k == n - 1) gap += 2.0 * kPi;
      dmin = std::min(dmin, gap);
    }
    if (dmin < kAngleHard) { ok = false; reason = 0; return 0.0; }  // near-tangent
    double base   = _d->_junction_setback_scale * hw;
    double needed = hw / std::tan(std::max(dmin, kAngleFloor) * 0.5 * kCornerMargin);
    double sb = std::min(std::max(base, needed), edge_clamp);
    if (needed > edge_clamp + 1e-6) { ok = false; reason = 1; return 0.0; } // apron can't fit
    ok = true; reason = -1; return sb;
  }

  // the setback ring for junction node i toward nbr: outward dir + ring center + arc-length.
  void _ringCenter(int i, int nbr, double sb, double& ux, double& uz,
                   double& cx, double& cy, double& cz, double& arc) const {
    const auto& a = _nodes[i]; const auto& b = _nodes[nbr];
    double el; _norm2(b.wx - a.wx, b.wz - a.wz, ux, uz, el);
    double t = (el > kEps) ? sb / el : 0.0;
    cx = a.wx + ux * sb;
    cz = a.wz + uz * sb;
    cy = a.y + (b.y - a.y) * t;
    bool toward_parent = (nbr == a.parent);
    arc = a.arclen + (toward_parent ? -sb : sb);
  }

  // degree-2 chains between anchors (root/fork/leaf) over _nodes — the banking decomposition.
  std::vector<std::vector<int>> _bankChains() const {
    int N = int(_nodes.size());
    auto isAnchor = [&](int i) { return _nodes[i].parent < 0 || int(_children[i].size()) != 1; };
    std::vector<std::vector<int>> chains;
    for (int a = 0; a < N; a++) {
      if (not isAnchor(a)) continue;
      for (int c : _children[a]) {
        std::vector<int> chain = {a, c}; int cur = c;
        while (not isAnchor(cur)) { int nxt = _children[cur][0]; chain.push_back(nxt); cur = nxt; }
        chains.push_back(chain);
      }
    }
    return chains;
  }

  // SUPERELEVATION: per-node signed deck bank (radians; + = RIGHT rail raised). bank magnitude
  // = max_bank*clamp(bank_ref_radius/R,0,1) from the local turn radius, zeroed on straights + at
  // junctions + within bank_runoff_m of a junction (the apron stays flat), then rate-limited to a
  // roll-rate ceiling (max_bank*spacing/runoff) along each chain (C1 roll, no kinks). Mirrors
  // roadmesh_ref._superelevation OPERATION-FOR-OPERATION.
  static constexpr double kBankSign = 1.0; // +1 => left turn (cross>0) raises the RIGHT rail
  std::vector<double> _superelevation() const {
    int N = int(_nodes.size());
    double maxb = _d->_max_bank_rad, refr = _d->_bank_ref_radius_m;
    double runoff = std::max(kEps, double(_d->_bank_runoff_m));
    std::vector<double> raw(N, 0.0);
    for (int i = 0; i < N; i++) {
      if (int(_children[i].size()) != 1 || _nodes[i].parent < 0) continue;
      const auto& nd = _nodes[i]; const auto& pn = _nodes[nd.parent]; const auto& c = _nodes[_children[i][0]];
      double ix, iz, si; _norm2(nd.wx - pn.wx, nd.wz - pn.wz, ix, iz, si);
      double ox, oz, so; _norm2(c.wx - nd.wx, c.wz - nd.wz, ox, oz, so);
      if (si < kEps || so < kEps) continue;
      double dth = std::atan2(oz, ox) - std::atan2(iz, ix);
      while (dth > kPi)  dth -= 2.0 * kPi;
      while (dth < -kPi) dth += 2.0 * kPi;
      double ds = 0.5 * (si + so);
      double kappa = ds > kEps ? std::fabs(dth) / ds : 0.0;
      double mag = maxb * std::min(1.0, refr * kappa);
      double cross = ix * oz - iz * ox;
      if (std::fabs(cross) > 1e-12) raw[i] = mag * (cross > 0.0 ? kBankSign : -kBankSign);
    }
    std::vector<double> sgn = raw;
    for (auto& chain : _bankChains()) {
      int cn = int(chain.size());
      std::vector<double> arc(cn, 0.0);
      for (int k = 1; k < cn; k++)
        arc[k] = arc[k - 1] + std::hypot(_nodes[chain[k]].wx - _nodes[chain[k - 1]].wx,
                                         _nodes[chain[k]].wz - _nodes[chain[k - 1]].wz);
      double total = arc.back();
      bool aj = _isJunction(chain.front()), bj = _isJunction(chain.back());
      std::vector<double> vals(cn);
      for (int k = 0; k < cn; k++) {
        double v = raw[chain[k]];
        if (aj && arc[k] < runoff) v = 0.0;
        if (bj && (total - arc[k]) < runoff) v = 0.0;
        vals[k] = v;
      }
      double spacing = total / std::max(1, cn - 1);
      double rate_cap = maxb * spacing / runoff;
      for (int k = 1; k < cn; k++) { double d = vals[k] - vals[k - 1]; if (d > rate_cap) vals[k] = vals[k - 1] + rate_cap; else if (d < -rate_cap) vals[k] = vals[k - 1] - rate_cap; }
      for (int k = cn - 2; k >= 0; k--) { double d = vals[k] - vals[k + 1]; if (d > rate_cap) vals[k] = vals[k + 1] + rate_cap; else if (d < -rate_cap) vals[k] = vals[k + 1] - rate_cap; }
      for (int k = 0; k < cn; k++) sgn[chain[k]] = vals[k];
    }
    return sgn;
  }

  void compute(dflow::GraphInst* inst, ui::updatedata_ptr_t) final {
    auto xng = _srcXfng(_input);
    if (not xng or xng->_nodes.empty())
      return; // producer not ready yet (or empty spine)
    if (xng->_version == _lastXngV)
      return; // static layout: reskin only when the spine re-emits (edit)
    _lastXngV = xng->_version;
    auto env = inst->_impl.getShared<MeshEnv>();
    auto fxi = env->_ctx->FXI();

    // --- adapt the XfNodeGraph CPU mirror (positions from _xform[12..14]; attrs
    //     x=width, y=arclen, z=road_elev -> the road surface Y). ---
    const int N = int(xng->_nodes.size());
    _nodes.resize(N);
    for (int i = 0; i < N; i++) {
      const auto& xn = xng->_nodes[i];
      auto& nd = _nodes[i];
      nd.wx     = xn._xform[12];
      nd.wz     = xn._xform[14];
      nd.width  = xn._attrs[0];
      nd.arclen = xn._attrs[1];
      nd.y      = xn._attrs[2]; // grade-limited road_elev (NOT raw terrain elev)
      nd.parent = (xn._parent == 0xffffffffu) ? -1 : int(xn._parent);
    }
    // children (ascending) + incident (parent first, then children) — the reference order.
    _children.assign(N, {});
    for (int i = 0; i < N; i++)
      if (_nodes[i].parent >= 0) _children[_nodes[i].parent].push_back(i);
    for (auto& c : _children) std::sort(c.begin(), c.end());
    _incident.assign(N, {});
    for (int i = 0; i < N; i++) {
      if (_nodes[i].parent >= 0) _incident[i].push_back(_nodes[i].parent);
      for (int c : _children[i]) _incident[i].push_back(c);
    }
    // per-node deck bank (superelevation) — needs _children/_incident; applied in PASS 1.
    std::vector<double> bank = _superelevation();

    // --- self-defense: widths + non-degenerate edges (ops fail loudly) ---
    double weld_eps = 1e-4;
    for (int i = 0; i < N; i++) {
      if (_nodes[i].width <= kEps) {
        _refuse("node %d has non-positive width %.6g — need width_m > 0", i, _nodes[i].width);
        return;
      }
      int pa = _nodes[i].parent;
      if (pa >= 0) {
        double dl = std::hypot(_nodes[i].wx - _nodes[pa].wx, _nodes[i].wz - _nodes[pa].wz);
        if (dl < weld_eps) { _refuse("spine edge %d<-%d is zero-length (coincident nodes)", i, pa); return; }
      }
    }

    // --- build buffers ---
    std::vector<float> P, Nr, Bn, UV, Cl;
    std::vector<uint32_t> VI, FO, TG;
    fvec3 bbmin(1e30f, 1e30f, 1e30f), bbmax(-1e30f, -1e30f, -1e30f);
    // Cl carries the ctx.Cd SELECTOR: .x = region (0 road / 1 junction / 2 shoulder — the
    // material band + marking suppression), .y = lift factor (1 = follow the scene lift, 0 =
    // grounded), so the outer skirt ring stays pinned to the terrain while the deck lifts.
    auto addVert = [&](double x, double y, double z, double bx, double bz, double u, double v,
                       double region = 0.0, double lift = 1.0) -> uint32_t {
      uint32_t vi = uint32_t(P.size() / 4);
      P.push_back(float(x)); P.push_back(float(y)); P.push_back(float(z)); P.push_back(1.0f);
      Nr.push_back(0.0f); Nr.push_back(1.0f); Nr.push_back(0.0f); Nr.push_back(0.0f); // road faces up
      Bn.push_back(float(bx)); Bn.push_back(0.0f); Bn.push_back(float(bz)); Bn.push_back(0.0f);
      UV.push_back(float(u)); UV.push_back(float(v)); UV.push_back(0.0f); UV.push_back(0.0f);
      Cl.push_back(float(region)); Cl.push_back(float(lift)); Cl.push_back(0.0f); Cl.push_back(1.0f);
      bbmin.x = std::min(bbmin.x, float(x)); bbmin.y = std::min(bbmin.y, float(y)); bbmin.z = std::min(bbmin.z, float(z));
      bbmax.x = std::max(bbmax.x, float(x)); bbmax.y = std::max(bbmax.y, float(y)); bbmax.z = std::max(bbmax.z, float(z));
      return vi;
    };
    auto addFace = [&](std::initializer_list<uint32_t> idx, int gid) {
      FO.push_back(uint32_t(VI.size()));
      for (uint32_t v : idx) VI.push_back(v);
      TG.push_back((uint32_t(gid) & 0xFFFu) << 20); // A1 gid band; low bits 0 (fresh)
    };
    auto addFaceVec = [&](const std::vector<uint32_t>& idx, int gid) {
      FO.push_back(uint32_t(VI.size()));
      for (uint32_t v : idx) VI.push_back(v);
      TG.push_back((uint32_t(gid) & 0xFFFu) << 20);
    };
    double vmpt = std::max(kEps, double(_d->_v_meters_per_tile));

    // --- OPTIONAL grounding field: read the shared terrain Height to CPU. The outer skirt
    //     ring is bilinear-sampled on it so it rides EXACTLY on the terrain the layout
    //     routes on (mirrors roadmesh_ref.build_roadmesh(terrain_h=...)). ---
    std::vector<float> Hf; int Hw = 0, Hh = 0; bool haveH = false;
    {
      auto himg = _srcHfImg(_inHeight);
      if (himg and himg->_ssbo) {
        Hw = himg->_w; Hh = himg->_h;
        int ch = (himg->_channels < 1) ? 1 : himg->_channels;
        std::vector<float> raw(size_t(Hw) * Hh * ch);
        auto mp = fxi->mapStorageBuffer(himg->_ssbo, 0, raw.size() * sizeof(float), BufferMapAccess::READ_ONLY);
        std::memcpy(raw.data(), mp->_mappedaddr, raw.size() * sizeof(float));
        fxi->unmapStorageBuffer(mp.get());
        Hf.resize(size_t(Hw) * Hh);
        for (size_t i = 0; i < Hf.size(); i++) Hf[i] = raw[i * ch];
        haveH = true;
      }
    }
    const double extent = double(_d->_extent_m);
    const double shw    = double(_d->_shoulder_m);
    const bool shoulders = haveH and (shw > kEps);
    auto terrainH = [&](double x, double z) -> double {
      return _bilinearField(Hf, Hw, Hh, x / extent + 0.5, z / extent + 0.5);
    };
    std::map<uint32_t, std::pair<uint32_t, uint32_t>> shoulder_of; // rail -> (inner,outer); shared -> welded
    const double R_SHOULDER = 2.0, R_JUNCTION = 1.0, LIFT_DECK = 1.0, LIFT_GROUND = 0.0;
    auto emitShoulder = [&](uint32_t rail, double ox, double oz, double u, double vv) {
      double px = P[rail * 4 + 0], py = P[rail * 4 + 1], pz = P[rail * 4 + 2];
      uint32_t inner = addVert(px, py, pz, 0.0, 0.0, u, vv, R_SHOULDER, LIFT_DECK);
      double gx = px + ox * shw, gz = pz + oz * shw;
      uint32_t outer = addVert(gx, terrainH(gx, gz), gz, 0.0, 0.0, u, vv, R_SHOULDER, LIFT_GROUND);
      shoulder_of[rail] = {inner, outer};
    };

    // ---- PASS 1: node rings for every NON-junction node (shared by incident segs). ----
    std::vector<std::pair<int, int>> node_ring(N, {-1, -1}); // (vLeft, vRight)
    for (int i = 0; i < N; i++) {
      if (_isJunction(i)) continue;
      const auto& nd = _nodes[i];
      double hx = 0, hz = 0;
      if (nd.parent >= 0) {
        double dx, dz, l; _norm2(nd.wx - _nodes[nd.parent].wx, nd.wz - _nodes[nd.parent].wz, dx, dz, l);
        hx += dx; hz += dz;
      }
      for (int c : _children[i]) {
        double dx, dz, l; _norm2(_nodes[c].wx - nd.wx, _nodes[c].wz - nd.wz, dx, dz, l);
        hx += dx; hz += dz;
      }
      double hn, hzn, hm; _norm2(hx, hz, hn, hzn, hm);
      if (hm < kEps) { hn = 0.0; hzn = 1.0; } // hairpin -> +Z fallback
      double rx, rz; _right(hn, hzn, rx, rz);
      double hw = nd.width * 0.5;
      double vv = nd.arclen / vmpt;
      // superelevation: pivot about the INSIDE (low) rail -> low rail stays at road_elev, outside
      // rail rises by width*sin(bank). No deck vert drops below road_elev (clearance preserved).
      double bnk = bank[i];
      double lift = nd.width * std::sin(std::fabs(bnk));
      double yR = nd.y + (bnk > 0.0 ? lift : 0.0); // right rail high on a left turn (bank>0)
      double yL = nd.y + (bnk < 0.0 ? lift : 0.0); // left  rail high on a right turn (bank<0)
      uint32_t vR = addVert(nd.wx + rx * hw, yR, nd.wz + rz * hw, hn, hzn, 0.0, vv); // right rail u=0
      uint32_t vL = addVert(nd.wx - rx * hw, yL, nd.wz - rz * hw, hn, hzn, 1.0, vv); // left  rail u=1
      node_ring[i] = {int(vL), int(vR)};
      if (shoulders) {
        emitShoulder(vR, rx, rz, 0.0, vv);    // right rail -> outward +right, grounds to terrain
        emitShoulder(vL, -rx, -rz, 1.0, vv);  // left  rail -> outward -right
      }
    }

    // ---- PASS 2: per-junction setback seg-rings (road UV) + patch n-gon (junction UV). ----
    std::map<std::pair<int, int>, std::pair<int, int>> seg_ring; // (J,nbr) -> (vLeft,vRight)
    for (int J = 0; J < N; J++) {
      if (not _isJunction(J)) continue;
      bool ok; int reason;
      double sb = _junctionSetback(J, ok, reason);
      if (not ok) {
        if (reason == 0)
          _refuse("junction node %d is NEAR-TANGENT (two fork edges within %.1f deg) — "
                  "separate the routes / raise layout_cell_m", J, kAngleHard * 180.0 / kPi);
        else
          _refuse("junction node %d apron can't fit a %.1fm-wide road — raise layout_cell_m / "
                  "narrow width_m", J, _nodes[J].width);
        return;
      }
      double hw = _nodes[J].width * 0.5;

      struct Stub { int nbr; double ux, uz, cx, cy, cz, arc; };
      std::vector<Stub> stubs;
      for (int nbr : _incident[J]) {
        Stub s; s.nbr = nbr;
        _ringCenter(J, nbr, sb, s.ux, s.uz, s.cx, s.cy, s.cz, s.arc);
        // apron gets the same no-burial treatment as the deck: raise a buried stub ring centre
        // (and its two mouth corners, which share s.cy) to terrain + clearance (bounded local lift).
        if (shoulders) {
          double srx, srz; _right(s.ux, s.uz, srx, srz);
          double tmax = std::max(std::max(terrainH(s.cx, s.cz), terrainH(s.cx + srx * hw, s.cz + srz * hw)),
                                 terrainH(s.cx - srx * hw, s.cz - srz * hw));
          s.cy = std::max(s.cy, tmax + double(_d->_clearance_m));
        }
        stubs.push_back(s);
      }
      // segment-side mouth rings (road UV: u=1 left / 0 right, V from arc)
      for (auto& s : stubs) {
        double rx, rz; _right(s.ux, s.uz, rx, rz);
        double vv = s.arc / vmpt;
        uint32_t vR = addVert(s.cx + rx * hw, s.cy, s.cz + rz * hw, s.ux, s.uz, 0.0, vv);
        uint32_t vL = addVert(s.cx - rx * hw, s.cy, s.cz - rz * hw, s.ux, s.uz, 1.0, vv);
        seg_ring[{J, s.nbr}] = {int(vL), int(vR)};
        if (shoulders) {
          emitShoulder(vR, rx, rz, 0.0, vv);
          emitShoulder(vL, -rx, -rz, 1.0, vv);
        }
      }
      // patch corners (own local UV chart), COINCIDENT with the segment mouths.
      struct Corner { double x, y, z; int stub; };
      std::vector<Corner> corners;
      for (size_t si = 0; si < stubs.size(); si++) {
        auto& s = stubs[si];
        double rx, rz; _right(s.ux, s.uz, rx, rz);
        corners.push_back({s.cx + rx * hw, s.cy, s.cz + rz * hw, int(si)}); // right
        corners.push_back({s.cx - rx * hw, s.cy, s.cz - rz * hw, int(si)}); // left
      }
      double Jx = _nodes[J].wx, Jz = _nodes[J].wz;
      auto ang = [&](const Corner& c) { return std::atan2(c.z - Jz, c.x - Jx); };
      std::vector<int> order(corners.size());
      for (size_t k = 0; k < order.size(); k++) order[k] = int(k);
      std::sort(order.begin(), order.end(), [&](int a, int b) {
        double aa = -ang(corners[a]), ab = -ang(corners[b]);
        if (aa != ab) return aa < ab;
        return corners[a].stub < corners[b].stub;
      });
      // near-tangent defensive check: each stub's two corners must be adjacent in `order`.
      std::vector<int> pos_in_order(corners.size());
      for (size_t o = 0; o < order.size(); o++) pos_in_order[order[o]] = int(o);
      int nc = int(corners.size());
      for (size_t si = 0; si < stubs.size(); si++) {
        int c0 = 2 * int(si), c1 = 2 * int(si) + 1;
        int dd = std::abs(pos_in_order[c0] - pos_in_order[c1]);
        if (not (dd == 1 or dd == nc - 1)) {
          _refuse("junction node %d fork mouths interleave (near-tangent) — separate the routes", J);
          return;
        }
      }
      // local planar UV chart normalized to the patch XZ bbox (top-down).
      double xmin = 1e300, xmax = -1e300, zmin = 1e300, zmax = -1e300;
      for (auto& c : corners) {
        xmin = std::min(xmin, c.x); xmax = std::max(xmax, c.x);
        zmin = std::min(zmin, c.z); zmax = std::max(zmax, c.z);
      }
      double du = std::max(kEps, xmax - xmin), dv = std::max(kEps, zmax - zmin);
      std::vector<uint32_t> poly;
      for (int cidx : order) {
        auto& c = corners[cidx];
        double u = (c.x - xmin) / du, v = (c.z - zmin) / dv;
        poly.push_back(addVert(c.x, c.y, c.z, 1.0, 0.0, u, v, R_JUNCTION, LIFT_DECK));
      }
      addFaceVec(poly, _d->_junction_gid);

      // apron SKIRT: extrude the FILLET-GAP perimeter edges (consecutive corners from
      // DIFFERENT stubs) radially outward to the terrain. The mouth edges (same stub) are
      // welded to the segment decks, so they are NOT skirted here.
      if (shoulders) {
        for (int oi = 0; oi < nc; oi++) {
          int c0 = order[oi], c1 = order[(oi + 1) % nc];
          if (corners[c0].stub == corners[c1].stub) continue;
          uint32_t v0 = poly[oi], v1 = poly[(oi + 1) % nc];
          double p0x = P[v0 * 4 + 0], p0y = P[v0 * 4 + 1], p0z = P[v0 * 4 + 2];
          double p1x = P[v1 * 4 + 0], p1y = P[v1 * 4 + 1], p1z = P[v1 * 4 + 2];
          uint32_t i0 = addVert(p0x, p0y, p0z, 0.0, 0.0, 0.0, 0.0, R_SHOULDER, LIFT_DECK);
          uint32_t i1 = addVert(p1x, p1y, p1z, 0.0, 0.0, 0.0, 0.0, R_SHOULDER, LIFT_DECK);
          double o0x, o0z, l0; _norm2(p0x - Jx, p0z - Jz, o0x, o0z, l0);
          double o1x, o1z, l1; _norm2(p1x - Jx, p1z - Jz, o1x, o1z, l1);
          double g0x = p0x + o0x * shw, g0z = p0z + o0z * shw;
          double g1x = p1x + o1x * shw, g1z = p1z + o1z * shw;
          uint32_t o0 = addVert(g0x, terrainH(g0x, g0z), g0z, 0.0, 0.0, 0.0, 0.0, R_SHOULDER, LIFT_GROUND);
          uint32_t o1 = addVert(g1x, terrainH(g1x, g1z), g1z, 0.0, 0.0, 0.0, 0.0, R_SHOULDER, LIFT_GROUND);
          addFace({i0, i1, o1, o0}, _d->_shoulder_gid);
        }
      }
    }

    // ---- PASS 3: segment quads (one per parent->child edge). ----
    auto ringXZ = [&](std::pair<int, int> rp, double& cx, double& cz) {
      cx = 0.5 * (P[rp.first * 4] + P[rp.second * 4]);
      cz = 0.5 * (P[rp.first * 4 + 2] + P[rp.second * 4 + 2]);
    };
    for (int b = 0; b < N; b++) {
      int a = _nodes[b].parent;
      if (a < 0) continue;
      double tx, tz, tl; _norm2(_nodes[b].wx - _nodes[a].wx, _nodes[b].wz - _nodes[a].wz, tx, tz, tl);
      double lx, lz; _left(tx, tz, lx, lz);
      auto pick = [&](std::pair<int, int> rp, double cx, double cz, uint32_t& L, uint32_t& R) {
        auto latdot = [&](int vi) { return (P[vi * 4] - cx) * lx + (P[vi * 4 + 2] - cz) * lz; };
        int vA = rp.first, vB = rp.second;
        if (latdot(vA) >= latdot(vB)) { L = uint32_t(vA); R = uint32_t(vB); }
        else                          { L = uint32_t(vB); R = uint32_t(vA); }
      };
      uint32_t aL, aR, bL, bR;
      if (_isJunction(a)) { auto rp = seg_ring[{a, b}]; double cx, cz; ringXZ(rp, cx, cz); pick(rp, cx, cz, aL, aR); }
      else                pick(node_ring[a], _nodes[a].wx, _nodes[a].wz, aL, aR);
      if (_isJunction(b)) { auto rp = seg_ring[{b, a}]; double cx, cz; ringXZ(rp, cx, cz); pick(rp, cx, cz, bL, bR); }
      else                pick(node_ring[b], _nodes[b].wx, _nodes[b].wz, bL, bR);
      addFace({aL, bL, bR, aR}, _d->_road_gid);
      if (shoulders) {
        auto& rA = shoulder_of[aR]; auto& rB = shoulder_of[bR];   // right side skirt
        auto& lA = shoulder_of[aL]; auto& lB = shoulder_of[bL];   // left  side skirt
        addFace({rA.first, rB.first, rB.second, rA.second}, _d->_shoulder_gid);
        addFace({lA.first, lA.second, lB.second, lB.first}, _d->_shoulder_gid);
      }
    }

    const int nverts   = int(P.size() / 4);
    const int nfaces   = int(FO.size());
    const int ncorners = int(VI.size());
    FO.push_back(uint32_t(ncorners)); // CSR terminal
    if (nfaces == 0) { _refuse_empty(); return; }

    // ---- allocate + upload (the LSweep/MergeMesh staging path) ----
    auto mesh = _output->_value;
    allocMesh(env, mesh, nverts, ncorners, nfaces,
              {MeshChannel::POSITION, MeshChannel::NORMAL, MeshChannel::BINORMAL,
               MeshChannel::UV0, MeshChannel::COLOR});
    mesh->_vattrs.clear();
    mesh->_faces.clear();
    mesh->_faces["__tags"] = env->_pool->acquireChannel(4, std::max(1, nfaces)); // per-face gid

    {
      auto mp = fxi->mapStorageBuffer(mesh->_header, 0, 64, BufferMapAccess::WRITE_ONLY);
      auto hu = (uint32_t*)mp->_mappedaddr; auto hf = (float*)mp->_mappedaddr;
      hu[0] = uint32_t(nverts); hu[1] = uint32_t(ncorners); hu[2] = uint32_t(nfaces); hu[3] = 0u;
      hf[4] = bbmin.x; hf[5] = bbmin.y; hf[6] = bbmin.z; hf[7] = 1;
      hf[8] = bbmax.x; hf[9] = bbmax.y; hf[10] = bbmax.z; hf[11] = 1;
      fxi->unmapStorageBuffer(mp.get());
    }
    auto upf = [&](MeshChannel ch, const std::vector<float>& v) {
      auto ss = mesh->channel(ch)->_ssbo;
      auto mp = fxi->mapStorageBuffer(ss, 0, v.size() * sizeof(float), BufferMapAccess::WRITE_ONLY);
      std::memcpy(mp->_mappedaddr, v.data(), v.size() * sizeof(float));
      fxi->unmapStorageBuffer(mp.get());
    };
    auto upu = [&](FxShaderStorageBuffer* ss, const std::vector<uint32_t>& v) {
      auto mp = fxi->mapStorageBuffer(ss, 0, v.size() * sizeof(uint32_t), BufferMapAccess::WRITE_ONLY);
      std::memcpy(mp->_mappedaddr, v.data(), v.size() * sizeof(uint32_t));
      fxi->unmapStorageBuffer(mp.get());
    };
    upf(MeshChannel::POSITION, P);
    upf(MeshChannel::NORMAL, Nr);
    upf(MeshChannel::BINORMAL, Bn);
    upf(MeshChannel::UV0, UV);
    upf(MeshChannel::COLOR, Cl);
    upu(mesh->_vidx->_ssbo, VI);
    upu(mesh->_face_offsets->_ssbo, FO);
    upu(mesh->_faces["__tags"]->_ssbo, TG);

    mesh->markTopoChanged();
    int njunc = 0;
    for (int i = 0; i < N; i++) if (_isJunction(i)) njunc++;
    if (not _announced) {
      printf("RoadMesh<%s>: %d nodes -> %dv/%df (%d ribbon quads, %d junction patches)\n",
             _dgmodule_data->_name.c_str(), N, nverts, nfaces, nfaces - njunc, njunc);
      _announced = true;
    }
  }

  // loud refusal (ops self-defend): print once + leave the output empty (matches the
  // reference RAISE — no mesh is emitted for a degenerate spine). The printf-format
  // attribute keeps the vsnprintf wrapper warning-clean (callers pass literal formats).
  __attribute__((format(printf, 2, 3))) void _refuse(const char* fmt, ...) {
    if (_refused) return;
    va_list ap;
    va_start(ap, fmt);
    char buf[512];
    std::vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    printf("RoadMesh<%s>: REFUSE — %s\n", _dgmodule_data->_name.c_str(), buf);
    _refused = true;
  }
  void _refuse_empty() { _refuse("spine produced no sweepable edges (need >= 2 connected nodes)"); }

  const char* _cookSalt() const final;

  const RoadMeshModuleData* _d;
  mesh_outpluginst_ptr_t _output;
  dgfx::xfng_inpluginst_ptr_t _input;
  dgfx::hfimg_inpluginst_ptr_t _inHeight;
  std::vector<RmNode> _nodes;
  std::vector<std::vector<int>> _children, _incident;
  uint64_t _lastXngV = ~0ull;
  bool _announced = false;
  bool _refused   = false;
};

// cook salt ties to THIS TU's compile time -> any RoadMesh kernel change auto-invalidates
// the disk cook-cache on rebuild (the LSweep/MergeMesh convention).
const char* RoadMeshModuleInst::_cookSalt() const {
  return "roadmesh.v2.6-bank-clearance " __DATE__ " " __TIME__;
}

static void _reshapeRoadMeshIOs(dataflow::moduledata_ptr_t data) {
  dflow::ModuleData::createInputPlug<dgfx::XfNodeGraphPlugTraits>(data, dflow::EPR_UNIFORM, "In");
  dflow::ModuleData::createInputPlug<dgfx::HfImagePlugTraits>(data, dflow::EPR_UNIFORM, "Height"); // OPTIONAL grounding field
  dflow::ModuleData::createOutputPlug<MeshPlugTraits>(data, dflow::EPR_UNIFORM, "Out");
}
RoadMeshModuleData::RoadMeshModuleData() {}
std::shared_ptr<RoadMeshModuleData> RoadMeshModuleData::createShared() {
  auto d = std::make_shared<RoadMeshModuleData>();
  _reshapeRoadMeshIOs(d);
  return d;
}
dflow::dgmoduleinst_ptr_t RoadMeshModuleData::createInstance(dflow::GraphInst* g) const {
  return std::make_shared<RoadMeshModuleInst>(this, g);
}
void RoadMeshModuleData::describeX(class_t* clazz) {
  clazz->setSharedFactory([]() -> rtti::castable_ptr_t { return RoadMeshModuleData::createShared(); });
  clazz->annotateTyped<dataflow::moduleIOreshape_fn_t>(
      "reshapeIOs", [](dataflow::moduledata_ptr_t m) { _reshapeRoadMeshIOs(m); });
  clazz->annotateTyped<ConstString>("dsl.verb", "road_mesh");
  clazz->annotateTyped<bool>("editor.palette", true);
  clazz->annotateTyped<int>("editor.palette.sort", 21);
  clazz->annotateTyped<ConstString>("editor.palette.recipe", "road_mesh");
  clazz->directProperty("v_meters_per_tile", &RoadMeshModuleData::_v_meters_per_tile);
  clazz->directProperty("junction_setback_scale", &RoadMeshModuleData::_junction_setback_scale);
  clazz->directProperty("junction_min_edge_frac", &RoadMeshModuleData::_junction_min_edge_frac);
  clazz->directProperty("road_gid", &RoadMeshModuleData::_road_gid);
  clazz->directProperty("junction_gid", &RoadMeshModuleData::_junction_gid);
  clazz->directProperty("extent_m", &RoadMeshModuleData::_extent_m);
  clazz->directProperty("shoulder_m", &RoadMeshModuleData::_shoulder_m);
  clazz->directProperty("shoulder_gid", &RoadMeshModuleData::_shoulder_gid);
  clazz->directProperty("clearance_m", &RoadMeshModuleData::_clearance_m);
  clazz->directProperty("max_bank_rad", &RoadMeshModuleData::_max_bank_rad);
  clazz->directProperty("bank_runoff_m", &RoadMeshModuleData::_bank_runoff_m);
  clazz->directProperty("bank_ref_radius_m", &RoadMeshModuleData::_bank_ref_radius_m);
}

} // namespace ork::lev2::hypermesh
