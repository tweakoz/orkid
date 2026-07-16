////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
#include "hmdflow_module.h"
#include <cmath>
#include <vector>

ImplementReflectionX(ork::lev2::hypermesh::LSystemModuleData, "hypermesh::LSystemModuleData");

// the family-neutral XfNodeGraph plug (G0a, ork::hyper::XfNodeGraph) — plug-data template
// reflection + instances, defined in the FIRST producing family's TU (the ScatterSource pattern).
// The data type + traits + inline data_to_inst already live in dflow/interchange.h.
namespace dflow = ::ork::dataflow;
namespace dgfx  = ork::lev2::dflowgfx;

template <> void dgfx::xfng_outplugdata_t::describeX(class_t*) {}
template <> void dgfx::xfng_inplugdata_t::describeX(class_t*) {}
template <> dflow::inpluginst_ptr_t dgfx::xfng_inplugdata_t::createInstance(ModuleInst* minst) const {
  return std::make_shared<dgfx::xfng_inpluginst_t>(this, minst);
}
template <> dflow::outpluginst_ptr_t dgfx::xfng_outplugdata_t::createInstance(ModuleInst* minst) const {
  return std::make_shared<dgfx::xfng_outpluginst_t>(this, minst);
}
ImplementTemplateReflectionX(dgfx::xfng_outplugdata_t, "dflowgfx::xfngoutplug");
ImplementTemplateReflectionX(dgfx::xfng_inplugdata_t, "dflowgfx::xfnginplug");

namespace ork::lev2::hypermesh {

///////////////////////////////////////////////////////////////////////////////
// LSystemModule (L-system family, M1) — the PRODUCER of the XfNodeGraph spine.
//
// A parametric, stochastic turtle: it grows a branch SKELETON (one XfNode per
// joint — frame + parent + radius) that the LSweep skinner sweeps into tubes.
// Four growth archetypes cover a realistic (desert-forward) plant range, all
// sharing the same primitives (a tropism-bent, optionally-curved `shoot`, a
// deterministic hash RNG, phyllotactic roll). The reflected LRuleSet/LExpr
// grammar (authored from the Python DSL) replaces these hardcoded models later;
// the MODULE + plug + skinner contract is what M1 proves out.
///////////////////////////////////////////////////////////////////////////////

static constexpr float kDeg2Rad = 0.01745329252f;

static inline float _len3(const fvec3& v) {
  return std::sqrt(v.dotWith(v));
}

// Rodrigues: rotate v around (unit) axis by ang.
static fvec3 _rot(const fvec3& v, const fvec3& axis, float ang) {
  fvec3 k = axis.normalized();
  float c = std::cos(ang), s = std::sin(ang);
  return v * c + k.crossWith(v) * s + k * (k.dotWith(v) * (1.0f - c));
}

// a vector perpendicular to d (the seed for rolling a bend axis around d).
static fvec3 _perp(const fvec3& d) {
  fvec3 ref = (std::fabs(d.y) < 0.99f) ? fvec3(0, 1, 0) : fvec3(1, 0, 0);
  return ref.crossWith(d).normalized();
}

// gravitropism: bend heading toward +Y (s>0) or -Y (s<0) by |s| radians — but CLAMPED to the
// remaining angle so it never rotates PAST vertical. Without the clamp, a large bend near
// vertical overshoots to the other side and the heading oscillates (ping-pongs) around the axis.
static fvec3 _tropismBend(const fvec3& dir, float s) {
  if (std::fabs(s) < 1e-6f)
    return dir;
  fvec3 t(0.0f, (s >= 0.0f) ? 1.0f : -1.0f, 0.0f);
  fvec3 axis = dir.crossWith(t);
  float m = _len3(axis);
  if (m < 1e-5f)
    return dir; // already (anti)parallel to up
  float toTarget = std::acos(std::max(-1.0f, std::min(1.0f, dir.dotWith(t))));
  float ang      = std::min(std::fabs(s), toTarget); // never overshoot the target → no oscillation
  return _rot(dir, axis * (1.0f / m), ang);
}

// emit one node at `pos` with a frame built from `heading`, parented to `parent`.
static uint32_t _emit(ork::hyper::xfnode_vect& nodes, const fvec3& pos, const fvec3& heading,
                      float rad, int gen, uint32_t parent) {
  fvec3 d   = heading.normalized();
  fvec3 ref = (std::fabs(d.y) < 0.99f) ? fvec3(0, 1, 0) : fvec3(1, 0, 0);
  fvec3 r   = ref.crossWith(d).normalized(); // right
  fvec3 u   = d.crossWith(r);                // up (completes the frame)
  ork::hyper::XfNode n;
  float* m = n._xform;                       // column-major mat4: X=r, Y=heading, Z=u, T=pos
  m[0] = r.x; m[1] = r.y; m[2] = r.z; m[3] = 0;
  m[4] = d.x; m[5] = d.y; m[6] = d.z; m[7] = 0;
  m[8] = u.x; m[9] = u.y; m[10] = u.z; m[11] = 0;
  m[12] = pos.x; m[13] = pos.y; m[14] = pos.z; m[15] = 1;
  n._parent   = parent;
  n._attrs[0] = rad; n._attrs[1] = float(gen); n._attrs[2] = 0; n._attrs[3] = 0;
  n._tags     = 0;
  uint32_t idx = uint32_t(nodes.size());
  nodes.push_back(n);
  return idx;
}

// parallel-transport (rotation-minimizing) frames in a forward pass: each node inherits
// its parent's right/up rotated by the minimal rotation aligning their headings, instead
// of rebuilding the frame from heading alone (which spins when heading ≈ ±Y → ring twist +
// pinch). Parents always precede children in index order, so one forward sweep suffices.
static void _buildFrames(ork::hyper::xfnode_vect& nodes) {
  int N = int(nodes.size());
  for (int i = 0; i < N; i++) {
    float* m = nodes[i]._xform;
    fvec3 head(m[4], m[5], m[6]);
    head = head.normalized();
    uint32_t par = nodes[i]._parent;
    fvec3 right, up;
    if (par == 0xffffffffu or int(par) >= i) { // root / unframed parent: stable seed frame
      fvec3 ref = (std::fabs(head.y) < 0.99f) ? fvec3(0, 1, 0) : fvec3(1, 0, 0);
      right     = ref.crossWith(head).normalized();
    } else {
      const float* pm = nodes[par]._xform;
      fvec3 pr(pm[0], pm[1], pm[2]), ph(pm[4], pm[5], pm[6]);
      fvec3 axis = ph.crossWith(head);
      float s    = _len3(axis);
      if (s < 1e-6f)
        right = pr; // (nearly) parallel headings: carry the parent frame unchanged
      else
        right = _rot(pr, axis * (1.0f / s), std::atan2(s, ph.dotWith(head)));
    }
    // re-orthonormalize right against head, rebuild up
    right     = right - head * head.dotWith(right);
    float rl  = _len3(right);
    right     = (rl > 1e-5f) ? right * (1.0f / rl) : _perp(head);
    up        = head.crossWith(right).normalized();
    m[0] = right.x; m[1] = right.y; m[2] = right.z;
    m[8] = up.x;    m[9] = up.y;    m[10] = up.z;
  }
}

// the turtle: carries params + node sink + a deterministic stochastic stream.
struct Turtle {
  ork::hyper::xfnode_vect& nodes;
  const LSystemModuleData* P;
  uint32_t rng;
  int budget;

  bool full() const { return int(nodes.size()) >= budget; }
  float rnd() { // xorshift-mul hash → [0,1)
    uint32_t x = rng;
    x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15; x *= 0x846ca68bu; x ^= x >> 16;
    rng = x;
    return float(x) * (1.0f / 4294967296.0f);
  }
  float srnd() { return rnd() * 2.0f - 1.0f; } // [-1,1)

  // grow a shoot of total `len` (radius rad0→rad1) as `internodes` segments, bending by
  // `trop` (gravitropism) + jitter each step. Advances pos/dir; returns the tip node idx.
  uint32_t shoot(fvec3& pos, fvec3& dir, float len, float rad0, float rad1,
                 int internodes, float trop, int gen, uint32_t parent) {
    int IN    = std::max(1, internodes);
    float sl  = len / float(IN);
    dir       = dir.normalized();
    uint32_t par = parent;
    for (int i = 0; i < IN and not full(); i++) {
      dir = _tropismBend(dir, trop);
      if (P->_jitter > 0.0f) {
        fvec3 ax(srnd(), srnd(), srnd());
        if (_len3(ax) > 1e-4f)
          dir = _rot(dir, ax.normalized(), P->_jitter * P->_jit_wave * std::fabs(srnd()));
      }
      pos        = pos + dir * sl;
      float t    = float(i + 1) / float(IN);
      float rad  = rad0 + (rad1 - rad0) * t;
      par        = _emit(nodes, pos, dir, std::max(rad, 0.002f), gen, par);
    }
    return par;
  }

  // archetype 0 — sympodial: each shoot forks into `_children` laterals (phyllotactic
  // roll, branch_angle pitch, optional apical dominance), recursing with len/rad decay.
  void sympodial(fvec3 pos, fvec3 dir, float len, float rad, int gen, int maxgen,
                 uint32_t parent, float trop) {
    if (gen >= maxgen or full())
      return;
    float rad1     = rad * P->_rad_decay;
    fvec3 tipPos   = pos, tipDir = dir;
    uint32_t tip   = shoot(tipPos, tipDir, len, rad, std::max(rad1, 0.002f),
                           P->_internodes, trop, gen, parent);
    if (gen + 1 >= maxgen or full())
      return;
    int kids    = std::max(1, P->_children);
    float pitch = P->_branch_angle * kDeg2Rad;
    float div   = P->_roll * kDeg2Rad;
    float spread = (kids > 1) ? (6.2831853f / float(kids)) : div;
    for (int k = 0; k < kids and not full(); k++) {
      float rollk = div * float(gen) + float(k) * spread + div * P->_jit_azimuth * srnd() * P->_jitter;
      fvec3 ax    = _rot(_perp(tipDir), tipDir, rollk);
      float p     = pitch;
      if (P->_apical > 0.0f and k == 0)
        p = pitch * (1.0f - P->_apical); // leader continues straighter
      p *= (1.0f + P->_jit_pitch * P->_jitter * srnd());
      fvec3 nd = _rot(tipDir, ax, p);
      sympodial(tipPos, nd, len * P->_len_decay, rad1, gen + 1, maxgen, tip, trop);
    }
  }

  // archetype 1 — conifer (monopodial): a straight central leader, with a whorl of
  // drooping laterals (shorter toward the top → conical) emitted at each level.
  void conifer(uint32_t root) {
    int whorls = std::max(3, P->_depth);
    float seg  = P->_seg_len, rad = P->_base_radius;
    fvec3 p(0, 0, 0), d(0, 1, 0);
    uint32_t leader = root;
    for (int w = 0; w < whorls and not full(); w++) {
      float r0 = rad * (1.0f - float(w) / float(whorls));
      float r1 = rad * (1.0f - float(w + 1) / float(whorls));
      fvec3 sp = p, sd = d;
      float wseg = seg * (1.0f + P->_jit_spacing * P->_jitter * srnd()); // jittered whorl spacing
      leader   = shoot(sp, sd, wseg, std::max(r0, 0.004f), std::max(r1, 0.004f),
                       std::max(1, P->_internodes), P->_tropism, 0, leader);
      p = sp; d = sd;
      int kids        = std::max(3, P->_children);
      float branchLen = seg * 2.4f * (1.0f - 0.72f * float(w) / float(whorls));
      branchLen       = std::max(branchLen, seg * 0.2f);
      float pitch     = P->_branch_angle * kDeg2Rad; // near-horizontal (~80°)
      float droop     = -std::fabs(P->_tropism) * 0.6f - 0.04f;
      for (int k = 0; k < kids and not full(); k++) {
        if (P->_jitter > 0.0f and rnd() < P->_jitter * P->_jit_drop)
          continue; // occasionally drop a lateral → asymmetric gaps, not a perfect whorl
        // jitter breaks the synthetic regularity: azimuth (radial symmetry), pitch, length
        float rollk = (P->_roll * kDeg2Rad) * float(w) + float(k) * (6.2831853f / float(kids))
                      + P->_jitter * P->_jit_azimuth * srnd();
        fvec3 ax    = _rot(_perp(d), d, rollk);
        float pj    = pitch * (1.0f + P->_jit_pitch * P->_jitter * srnd());
        fvec3 nd    = _rot(d, ax, pj);
        float blen  = branchLen * (1.0f + P->_jit_length * P->_jitter * srnd());
        sympodial(p, nd, blen, std::max(r1 * 0.55f, 0.004f), 0, 2, leader, droop);
      }
    }
  }

  // archetype 2 — saguaro: a thick vertical trunk, with a couple of arms that grow out
  // then curl sharply upward (accelerating gravitropism), parallel to the trunk.
  void saguaro(uint32_t root) {
    int trunkN = std::max(8, P->_depth);
    float seg = P->_seg_len, rad = P->_base_radius;
    fvec3 p(0, 0, 0), d(0, 1, 0);
    uint32_t par = root;
    struct Site { uint32_t idx; fvec3 pos; float rad; };
    std::vector<Site> trunkNodes;
    for (int i = 0; i < trunkN and not full(); i++) {
      d = _tropismBend(d, P->_tropism + 0.02f); // hold vertical
      if (P->_jitter > 0.0f) {
        fvec3 ax(srnd(), 0.0f, srnd());
        if (_len3(ax) > 1e-4f)
          d = _rot(d, ax.normalized(), P->_jitter * P->_jit_wave * std::fabs(srnd()));
      }
      p       = p + d * seg;
      float t = float(i + 1) / float(trunkN);
      float r = std::max(rad * (1.0f - P->_taper * t), 0.01f); // DSL taper (cactus = gentle)
      par     = _emit(nodes, p, d, r, 0, par);
      trunkNodes.push_back({par, p, r});
    }
    // arms: `_children` of them, spread over the upper-middle trunk, branching at rad_decay of
    // the trunk thickness there (the "% per level" the DSL sets), growing out then curling up.
    int arms = std::max(0, P->_children);
    int TN   = int(trunkNodes.size());
    for (int a = 0; a < arms and not full() and TN > 2; a++) {
      float frac   = (arms > 1) ? (0.38f + 0.30f * float(a) / float(arms - 1)) : 0.45f;
      Site st      = trunkNodes[std::min(TN - 1, std::max(1, int(float(TN) * frac)))];
      // arm heading is DSL-driven: `roll` = azimuth (heading direction) per arm around the
      // trunk, `branch_angle` = initial pitch off vertical, `jitter` = per-arm variation.
      float az     = P->_roll * kDeg2Rad * float(a) + P->_jitter * P->_jit_azimuth * srnd();
      fvec3 outd   = _rot(fvec3(1, 0, 0), fvec3(0, 1, 0), az);
      fvec3 axis   = fvec3(0, 1, 0).crossWith(outd).normalized();
      float pitch  = P->_branch_angle * kDeg2Rad; // 90° = straight out, smaller = more upright
      fvec3 ad     = _rot(fvec3(0, 1, 0), axis, pitch);
      fvec3 ap     = st.pos;
      uint32_t cur = st.idx;
      int armN     = std::max(6, trunkN / 2);
      float aseg = seg * 0.85f, arad = st.rad * P->_rad_decay; // thickness step per level
      for (int j = 0; j < armN and not full(); j++) {
        float curl = 0.06f + 0.5f * float(j) / float(armN); // accelerate the upward curl
        ad         = _tropismBend(ad, curl);
        ap         = ap + ad * aseg;
        float r    = arad * (1.0f - P->_taper * float(j) / float(armN));
        cur        = _emit(nodes, ap, ad, std::max(r, 0.01f), 1, cur);
      }
    }
  }

  // archetype 3 — ocotillo: many long unbranched whip stems from a common base, splaying
  // outward then curving up.
  void ocotillo(uint32_t root) {
    int stems = std::max(8, P->_children);
    int stemN = std::max(8, P->_depth);
    float seg = P->_seg_len, rad = P->_base_radius;
    float splay = P->_branch_angle * kDeg2Rad; // splay from vertical (DSL)
    for (int s = 0; s < stems and not full(); s++) {
      // chaotic per-stem launch: jittered azimuth + jittered splay angle (real whips don't
      // converge to vertical, so the preset keeps tropism ~0 and lets jitter dominate).
      float rollk = float(s) * (6.2831853f / float(stems)) + (P->_roll * kDeg2Rad)
                    + P->_jitter * P->_jit_azimuth * srnd();
      fvec3 outd  = _rot(fvec3(1, 0, 0), fvec3(0, 1, 0), rollk);
      fvec3 axis  = fvec3(0, 1, 0).crossWith(outd).normalized();
      float tilt  = splay * (1.0f + P->_jit_pitch * P->_jitter * srnd());
      fvec3 d     = _rot(fvec3(0, 1, 0), axis, tilt);
      fvec3 p(0, 0, 0);
      uint32_t cur = root;
      for (int j = 0; j < stemN and not full(); j++) {
        d = _tropismBend(d, P->_tropism);
        if (P->_jitter > 0.0f) {
          fvec3 ax(srnd(), srnd(), srnd());
          if (_len3(ax) > 1e-4f)
            d = _rot(d, ax.normalized(), P->_jitter * P->_jit_wave * std::fabs(srnd())); // waviness
        }
        p       = p + d * seg;
        float r = rad * (1.0f - P->_taper * float(j) / float(stemN));
        cur     = _emit(nodes, p, d, std::max(r, 0.004f), 0, cur);
      }
    }
  }
};

struct LSystemModuleInst : public MeshComputeInst {
  LSystemModuleInst(const LSystemModuleData* d, dflow::GraphInst* g) : MeshComputeInst(d, g), _d(d) {}

  const char* _cookSalt() const final;

  void onLink(dflow::GraphInst*) final {
    _output = typedOutputNamed<dflowgfx::XfNodeGraphPlugTraits>("Out");
  }

  void onActivate(dflow::GraphInst* inst) final {
    _buildSkeleton(inst); // static: the skeleton is built once here (wind is VS-side, not a rebuild)
  }

  // _buildSkeleton is split out (re-callable) so a future TOPOLOGY animation (growth / L-system
  // rewrite) can drive it per-frame from a writeParams hook; the stable/non-stable upload below +
  // the LSweep version gate are the foundation for that. (Deformation anims like wind are VS-side.)
  void _buildSkeleton(dflow::GraphInst* inst) {
    auto env = inst->_impl.getShared<MeshEnv>();
    if (not env)
      return;
    auto fxi = env->_ctx->FXI();
    auto xng = _output->_value; // XfNodeGraphInst (created via data_to_inst)

    // --- run the rewrite + turtle interpret on the CPU ---
    xng->_nodes.clear();
    xng->_slots.clear();
    if (_d->_grammar) {
      // GR1.b: reflected grammar present → derive it (rewrite + turtle-interpret) INSTEAD of the
      // hardcoded archetype switch. The 21 scalar props are the LExpr PARAM env (A8). The legacy path
      // below is untouched + stays the default (null _grammar) through the GR1.d parity window (T12).
      deriveLRuleSet(_d->_grammar.get(), _d, xng->_nodes, xng->_slots);
    } else {
      uint32_t root = _emit(xng->_nodes, fvec3(0, 0, 0), fvec3(0, 1, 0), _d->_base_radius, 0, 0xffffffffu);
      Turtle T{xng->_nodes, _d, uint32_t(std::max(1, _d->_seed)), std::max(16, _d->_budget)};
      switch (LArchetype(_d->_archetype)) {
        case LArchetype::Conifer:  T.conifer(root);  break;
        case LArchetype::Saguaro:  T.saguaro(root);  break;
        case LArchetype::Ocotillo: T.ocotillo(root); break;
        case LArchetype::Sympodial:
        default:
          T.sympodial(fvec3(0, 0, 0), fvec3(0, 1, 0), _d->_seg_len, _d->_base_radius, 0,
                      std::max(1, _d->_depth), root, _d->_tropism);
          break;
      }
    }
    _buildFrames(xng->_nodes); // parallel-transport frames → no ring twist on vertical runs (both paths)
    const int N = int(xng->_nodes.size());
    xng->_count = N;

    // STABLE vs NON-STABLE topology. Count unchanged (wind deforming a fixed tree) = a POSITION-only
    // update: keep the buffers, re-upload only the frames (_xform), and markChanged (NOT markTopoChanged)
    // so the skinner re-skins positions WITHOUT realloc/re-triangulation. Count changed (growth /
    // L-system rewrite) = realloc + upload everything + markTopoChanged (a full rebuild downstream).
    bool topoChanged = (not xng->_xform) or (N != _lastN);
    if (topoChanged) {
      xng->_xform  = fxi->createStorageBuffer(size_t(N) * 16 * sizeof(float));
      xng->_parent = fxi->createStorageBuffer(size_t(N) * sizeof(uint32_t));
      xng->_attrs  = fxi->createStorageBuffer(size_t(N) * 4 * sizeof(float));
      xng->_tags   = fxi->createStorageBuffer(size_t(N) * sizeof(uint32_t));
      _lastN = N;
    }
    { // frames (positions + orientation) — re-uploaded on any (re)build
      auto mp  = fxi->mapStorageBuffer(xng->_xform, 0, size_t(N) * 16 * sizeof(float), BufferMapAccess::WRITE_ONLY);
      auto dst = (float*)mp->_mappedaddr;
      for (int i = 0; i < N; i++)
        for (int j = 0; j < 16; j++) dst[i * 16 + j] = xng->_nodes[i]._xform[j];
      fxi->unmapStorageBuffer(mp.get());
    }
    if (topoChanged) { // parentage / radius / tags only change with topology
      {
        auto mp  = fxi->mapStorageBuffer(xng->_parent, 0, size_t(N) * sizeof(uint32_t), BufferMapAccess::WRITE_ONLY);
        auto dst = (uint32_t*)mp->_mappedaddr;
        for (int i = 0; i < N; i++) dst[i] = xng->_nodes[i]._parent;
        fxi->unmapStorageBuffer(mp.get());
      }
      {
        auto mp  = fxi->mapStorageBuffer(xng->_attrs, 0, size_t(N) * 4 * sizeof(float), BufferMapAccess::WRITE_ONLY);
        auto dst = (float*)mp->_mappedaddr;
        for (int i = 0; i < N; i++)
          for (int j = 0; j < 4; j++) dst[i * 4 + j] = xng->_nodes[i]._attrs[j];
        fxi->unmapStorageBuffer(mp.get());
      }
      {
        auto mp  = fxi->mapStorageBuffer(xng->_tags, 0, size_t(N) * sizeof(uint32_t), BufferMapAccess::WRITE_ONLY);
        auto dst = (uint32_t*)mp->_mappedaddr;
        for (int i = 0; i < N; i++) dst[i] = xng->_nodes[i]._tags;
        fxi->unmapStorageBuffer(mp.get());
      }
    }
    if (topoChanged) xng->markTopoChanged(); else xng->markChanged();
    if (not _announced) {
      if(0)printf("LSystem<%s>: %d nodes (archetype=%d depth=%d budget=%d)\n",
             _dgmodule_data->_name.c_str(), N, _d->_archetype, _d->_depth, _d->_budget);
      _announced = true;
    }
  }

  void compute(dflow::GraphInst*, ui::updatedata_ptr_t) final {} // static graph: produced at onActivate

  const LSystemModuleData* _d;
  dflowgfx::xfng_outpluginst_ptr_t _output;
  int  _lastN     = -1;    // SSBO sizing guard (recreate only when node count changes)
  bool _announced = false; // print the node-count line once
};

// cook salt. Two disjoint salts keep the grammar + legacy paths from EVER sharing a cache entry (T12).
//
//  * grammar path (T9): a CONTENT salt. Unlike the __DATE__ salt below, a content salt does NOT
//    auto-bust the cache on rebuild — so a C++ evaluator change (derive/turtle/RNG) leaves stale cooks
//    in place SILENTLY. Therefore `evalv=N` is a MANDATORY MANUAL BUMP on ANY change to deriveLRuleSet /
//    the turtle interpret / the counter-hash RNG in hmdflow_lruleset.cpp. Bump it or ship stale geometry.
//  * legacy archetype path: keeps the __DATE__/__TIME__ salt (auto-busts every rebuild) — deleted at GR1.d.
const char* LSystemModuleInst::_cookSalt() const {
  if (_d->_grammar)
    return "lsystem lsys.v3-ruleset evalv=1";
  return "lsystem " __DATE__ " " __TIME__;
}

static void _reshapeLSystemIOs(dataflow::moduledata_ptr_t data) {
  dflow::ModuleData::createOutputPlug<dflowgfx::XfNodeGraphPlugTraits>(data, dflow::EPR_UNIFORM, "Out");
}
LSystemModuleData::LSystemModuleData() {
}
std::shared_ptr<LSystemModuleData> LSystemModuleData::createShared() {
  auto d = std::make_shared<LSystemModuleData>();
  _reshapeLSystemIOs(d);
  return d;
}
dflow::dgmoduleinst_ptr_t LSystemModuleData::createInstance(dflow::GraphInst* g) const {
  return std::make_shared<LSystemModuleInst>(this, g);
}
void LSystemModuleData::describeX(class_t* clazz) {
  clazz->setSharedFactory([]() -> rtti::castable_ptr_t { return LSystemModuleData::createShared(); });
  clazz->annotateTyped<dataflow::moduleIOreshape_fn_t>(
      "reshapeIOs", [](dataflow::moduledata_ptr_t m) { _reshapeLSystemIOs(m); });
  // GR1.a: the reflected grammar rides the module (nullable directObjectProperty). Flows into
  // hypermeshModuleIdentityHash automatically; unused by any runtime path until GR1.b's evaluator.
  clazz->directObjectProperty("grammar", &LSystemModuleData::_grammar);
  clazz->directProperty("archetype", &LSystemModuleData::_archetype);
  clazz->directProperty("depth", &LSystemModuleData::_depth);
  clazz->directProperty("budget", &LSystemModuleData::_budget);
  clazz->directProperty("children", &LSystemModuleData::_children);
  clazz->directProperty("internodes", &LSystemModuleData::_internodes);
  clazz->directProperty("seed", &LSystemModuleData::_seed);
  clazz->directProperty("seg_len", &LSystemModuleData::_seg_len);
  clazz->directProperty("base_radius", &LSystemModuleData::_base_radius);
  clazz->directProperty("branch_angle", &LSystemModuleData::_branch_angle);
  clazz->directProperty("roll", &LSystemModuleData::_roll);
  clazz->directProperty("len_decay", &LSystemModuleData::_len_decay);
  clazz->directProperty("rad_decay", &LSystemModuleData::_rad_decay);
  clazz->directProperty("taper", &LSystemModuleData::_taper);
  clazz->directProperty("tropism", &LSystemModuleData::_tropism);
  clazz->directProperty("jitter", &LSystemModuleData::_jitter);
  clazz->directProperty("apical", &LSystemModuleData::_apical);
  clazz->directProperty("jit_azimuth", &LSystemModuleData::_jit_azimuth);
  clazz->directProperty("jit_pitch", &LSystemModuleData::_jit_pitch);
  clazz->directProperty("jit_length", &LSystemModuleData::_jit_length);
  clazz->directProperty("jit_spacing", &LSystemModuleData::_jit_spacing);
  clazz->directProperty("jit_drop", &LSystemModuleData::_jit_drop);
  clazz->directProperty("jit_wave", &LSystemModuleData::_jit_wave);
}

} // namespace ork::lev2::hypermesh
