////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
//
// LRuleSet — reflection (describeX) for the six grammar-as-data types (GR1.a).
// Schema in lruleset.h. No evaluator here — derive()/rewrite/turtle land in GR1.b.
//
// Reflection template (asset_gen.cpp HeightFieldGenData/ScatterSinkData):
//   directObjectVectorProperty  for object vectors  (_args/_children/_rhs/...)
//   directObjectProperty        for a single object (_guard/_value, nullable)
//   directMapProperty           for string->float   (_defaults)
//   directVectorProperty        for vector<float>    (_weights)
//   directProperty              for scalars          (int/uint32_t/float/string)
//
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/reflect/properties/registerX.inl>
#include <ork/lev2/gfx/hypermesh/lruleset.h>
#include <ork/lev2/gfx/hypermesh/hmdflow.h>          // GR1.b: LSystemModuleData (PARAM env) + XfNode/XfSlot
#include <ork/reflect/properties/DirectTypedMap.hpp> // _defaults (string->float)
#include <ork/reflect/properties/ITypedMap.hpp>
#include <cmath>
#include <cstdio>
#include <set>
#include <algorithm>

ImplementReflectionX(ork::lev2::hypermesh::LExpr,         "hypermesh::LExpr");
ImplementReflectionX(ork::lev2::hypermesh::LSymbolDef,    "hypermesh::LSymbolDef");
ImplementReflectionX(ork::lev2::hypermesh::LTurtleOp,     "hypermesh::LTurtleOp");
ImplementReflectionX(ork::lev2::hypermesh::LParamBinding, "hypermesh::LParamBinding");
ImplementReflectionX(ork::lev2::hypermesh::LRuleDef,      "hypermesh::LRuleDef");
ImplementReflectionX(ork::lev2::hypermesh::LRuleSet,      "hypermesh::LRuleSet");

namespace ork::lev2::hypermesh {

///////////////////////////////////////////////////////////////////////////////

void LExpr::describeX(object::ObjectClass* clazz) {
  clazz->directProperty("kind", &LExpr::_kind);
  clazz->directProperty("const", &LExpr::_const);
  clazz->directProperty("ref", &LExpr::_ref);
  clazz->directProperty("op", &LExpr::_op);
  clazz->directObjectVectorProperty("args", &LExpr::_args);
}

///////////////////////////////////////////////////////////////////////////////

void LSymbolDef::describeX(object::ObjectClass* clazz) {
  clazz->directProperty("name", &LSymbolDef::_name);
  clazz->directMapProperty("defaults", &LSymbolDef::_defaults);
}

///////////////////////////////////////////////////////////////////////////////

void LTurtleOp::describeX(object::ObjectClass* clazz) {
  clazz->directProperty("kind", &LTurtleOp::_kind);
  clazz->directObjectVectorProperty("params", &LTurtleOp::_params);
  clazz->directProperty("gid", &LTurtleOp::_gid);
  clazz->directProperty("symbol", &LTurtleOp::_symbol);
  clazz->directObjectVectorProperty("children", &LTurtleOp::_children);
  clazz->directVectorProperty("weights", &LTurtleOp::_weights);
  clazz->directObjectProperty("guard", &LTurtleOp::_guard);
}

///////////////////////////////////////////////////////////////////////////////

void LParamBinding::describeX(object::ObjectClass* clazz) {
  clazz->directProperty("key", &LParamBinding::_key);
  clazz->directObjectProperty("value", &LParamBinding::_value);
}

///////////////////////////////////////////////////////////////////////////////

void LRuleDef::describeX(object::ObjectClass* clazz) {
  clazz->directProperty("lhs", &LRuleDef::_lhs);
  clazz->directObjectProperty("guard", &LRuleDef::_guard);
  clazz->directProperty("weight", &LRuleDef::_weight);
  clazz->directObjectVectorProperty("rhs", &LRuleDef::_rhs);
}

///////////////////////////////////////////////////////////////////////////////

void LRuleSet::describeX(object::ObjectClass* clazz) {
  clazz->directObjectVectorProperty("symbols", &LRuleSet::_symbols);
  clazz->directObjectVectorProperty("axiom", &LRuleSet::_axiom);
  clazz->directObjectVectorProperty("rules", &LRuleSet::_rules);
  clazz->directProperty("depth", &LRuleSet::_depth);
  clazz->directProperty("segment_budget", &LRuleSet::_segmentBudget);
  clazz->directProperty("seed", &LRuleSet::_seed);
}

///////////////////////////////////////////////////////////////////////////////
// GR1.b — the EVALUATOR. derive() is two CPU bake-time passes (boundary 5):
//   pass 1 (rewrite):   axiom --_depth iterations--> a flat terminal op stream. Rules / CHOOSE
//                       branches are picked by a STATELESS counter-hash RNG (A3#1 / T10) and every
//                       LExpr param is evaluated EAGERLY in the producing instance's environment
//                       (A8: a rule's NUMBERS flow through the PARAM env = the module's live-pokeable
//                       reflected scalars — never constant-folded into the grammar).
//   pass 2 (interpret): walk the terminal ops with a turtle FRAME + bracket STACK, emitting one XfNode
//                       per SEGMENT (parent = the frame's current node, _attrs[0]=radius, _tags gid band
//                       per A1). FORK pushes N independently-rewritten child frames; SLOT emits an XfSlot.
// The caller applies the shared _buildFrames post-pass + GPU upload, so this stays PURE CPU.
//
// NOTE (post-GR1.d): the legacy archetype procedures are DELETED; this evaluator is the only
// turtle. _len3/_rot/_perp still exist as static copies in hmdflow_module_lsystem.cpp because
// the shared _buildFrames post-pass (which _buildSkeleton runs on the derived nodes) needs them
// there; _tropismBend lives only here. Both sets are TU-local statics — no ODR concern.
///////////////////////////////////////////////////////////////////////////////

namespace {

using ork::fvec3;
using ork::hyper::XfNode;
using ork::hyper::XfSlot;
using ork::hyper::xfnode_vect;
using ork::hyper::xfslot_vect;

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
// a vector perpendicular to d.
static fvec3 _perp(const fvec3& d) {
  fvec3 ref = (std::fabs(d.y) < 0.99f) ? fvec3(0, 1, 0) : fvec3(1, 0, 0);
  return ref.crossWith(d).normalized();
}
// gravitropism: bend heading toward +Y (s>0) / -Y (s<0) by |s| rad, CLAMPED so it never overshoots
// vertical (no ping-pong). Identical to the v2 Turtle helper.
static fvec3 _tropismBend(const fvec3& dir, float s) {
  if (std::fabs(s) < 1e-6f)
    return dir;
  fvec3 t(0.0f, (s >= 0.0f) ? 1.0f : -1.0f, 0.0f);
  fvec3 axis = dir.crossWith(t);
  float m = _len3(axis);
  if (m < 1e-5f)
    return dir;
  float toTarget = std::acos(std::max(-1.0f, std::min(1.0f, dir.dotWith(t))));
  float ang      = std::min(std::fabs(s), toTarget);
  return _rot(dir, axis * (1.0f / m), ang);
}

// the STATELESS counter-hash (A3#1) — the terrain-scatter / leafscatter `_lhash` family verbatim:
// deterministic per (a,b,c), no accumulator state, no time dependence (T10).
static inline uint32_t _ghash(uint32_t a, uint32_t b, uint32_t c) {
  uint32_t x = a * 2654435761u + b * 2246822519u + c * 3266489917u + 374761393u;
  x ^= x >> 15; x *= 2246822519u; x ^= x >> 13; x *= 3266489917u; x ^= x >> 16;
  return x;
}

// PARAM fallback env (A8): a PARAM expr whose name is not a symbol-instance param resolves to the
// module's reflected scalar of that name (live-pokeable). Unknown name -> 0 (like the ENV stub).
static float moduleParam(const LSystemModuleData* P, const std::string& n) {
  if (n == "depth")        return float(P->_depth);
  if (n == "budget")       return float(P->_budget);
  if (n == "children")     return float(P->_children);
  if (n == "internodes")   return float(P->_internodes);
  if (n == "seed")         return float(P->_seed);
  if (n == "seg_len")      return P->_seg_len;
  if (n == "base_radius")  return P->_base_radius;
  if (n == "branch_angle") return P->_branch_angle;
  if (n == "roll")         return P->_roll;
  if (n == "len_decay")    return P->_len_decay;
  if (n == "rad_decay")    return P->_rad_decay;
  if (n == "taper")        return P->_taper;
  if (n == "tropism")      return P->_tropism;
  if (n == "jitter")       return P->_jitter;
  if (n == "apical")       return P->_apical;
  if (n == "jit_azimuth")  return P->_jit_azimuth;
  if (n == "jit_pitch")    return P->_jit_pitch;
  if (n == "jit_length")   return P->_jit_length;
  if (n == "jit_spacing")  return P->_jit_spacing;
  if (n == "jit_drop")     return P->_jit_drop;
  if (n == "jit_wave")     return P->_jit_wave;
  return 0.0f;
}

using Env = std::map<std::string, float>;

static inline bool  hasp(const Env& m, const char* k) { return m.find(k) != m.end(); }
static inline float getp(const Env& m, const char* k, float dflt) {
  auto it = m.find(k);
  return (it != m.end()) ? it->second : dflt;
}

// a resolved op: the rewrite output pass 2 walks. Terminal/structural kinds only (CHOOSE/WHEN/CALL are
// resolved away in pass 1). FORK carries N INDEPENDENTLY-rewritten child bodies (distinct RNG each).
struct ROp {
  int                            kind = 0;
  uint32_t                       gid  = 0;
  Env                            params;   // eagerly evaluated (A8)
  std::vector<std::vector<ROp>>  branches; // FORK only
};

// the turtle pose: a full orthonormal frame (needed so PITCH/YAW/ROLL compose correctly across ops)
// + the radius accumulator (TAPER scales it) + the current parent node index (SEGMENT re-parents).
struct Frame {
  fvec3    pos{0, 0, 0};
  fvec3    H{0, 1, 0}; // heading / forward (the +Y column in the _emit convention)
  fvec3    R{1, 0, 0}; // right  (+X)
  fvec3    U{0, 0, 1}; // up     (+Z)
  float    rad    = 0.08f;
  uint32_t parent = 0xffffffffu;
};

static void reortho(Frame& fr) {
  fr.H      = fr.H.normalized();
  fr.R      = fr.R - fr.H * fr.H.dotWith(fr.R);
  float rl  = _len3(fr.R);
  fr.R      = (rl > 1e-5f) ? fr.R * (1.0f / rl) : _perp(fr.H);
  fr.U      = fr.H.crossWith(fr.R).normalized();
}

// emit one node: only heading (Y col) + position + parent + attrs + tags are load-bearing — the shared
// _buildFrames post-pass rebuilds R/U (rotation-minimizing) from the heading. gid packs into [20:32) (A1).
static uint32_t emitNode(xfnode_vect& nodes, const fvec3& pos, const fvec3& H,
                         float rad, float gen, uint32_t gid, uint32_t parent) {
  fvec3 h = H.normalized();
  XfNode n;
  float* m = n._xform;
  m[0] = 1;    m[1] = 0;    m[2] = 0;    m[3] = 0;  // R (provisional; _buildFrames overwrites)
  m[4] = h.x;  m[5] = h.y;  m[6] = h.z;  m[7] = 0;  // heading (Y col) — the frame input _buildFrames reads
  m[8] = 0;    m[9] = 0;    m[10] = 1;   m[11] = 0; // U (provisional)
  m[12] = pos.x; m[13] = pos.y; m[14] = pos.z; m[15] = 1;
  n._parent   = parent;
  n._attrs[0] = std::max(rad, 0.002f);
  n._attrs[1] = gen;
  n._attrs[2] = 0;
  n._attrs[3] = 0;
  n._tags     = (gid & 0xFFFu) << 20u; // A1: gid band [20:32)
  uint32_t idx = uint32_t(nodes.size());
  nodes.push_back(n);
  return idx;
}

// the evaluator — one instance per derive() (fresh counters => deterministic, order-only state).
struct LEval {
  const LRuleSet*          G;
  const LSystemModuleData* P;
  uint32_t seed;
  uint32_t counter    = 0; // monotonic RNG stream index (deterministic; NOT time-dependent, T10)
  uint32_t segCount   = 0;
  uint32_t opCount    = 0;
  uint32_t segBudget;
  uint32_t opCap;
  bool     budgetHit  = false;
  std::map<std::string, uint32_t> symOrd;

  LEval(const LRuleSet* g, const LSystemModuleData* p) : G(g), P(p) {
    seed      = g->_seed;
    segBudget = std::max<uint32_t>(1u, g->_segmentBudget);
    opCap     = segBudget * 64u; // T7: hard cap on total rewrite ops vs non-terminating grammars
    for (uint32_t i = 0; i < uint32_t(g->_symbols.size()); ++i)
      if (g->_symbols[i])
        symOrd[g->_symbols[i]->_name] = i;
  }

  uint32_t symOrdinal(const std::string& s) const {
    auto it = symOrd.find(s);
    if (it != symOrd.end())
      return it->second + 1u;
    uint32_t h = 2166136261u; // FNV-1a on the name for symbols outside the declared alphabet
    for (char c : s) { h ^= uint8_t(c); h *= 16777619u; }
    return h;
  }

  // stateless counter-hash draws in [0,1). `key` folds the decision context (symbol/depth/salt) so
  // sibling decisions at the same counter still diverge; `counter++` guarantees per-draw uniqueness.
  float draw(uint32_t key) { return float(_ghash(seed ^ key, counter++, 0x9e3779b9u)) * (1.0f / 4294967296.0f); }
  float draw01()           { return draw(0u); }
  float srnd()             { return draw01() * 2.0f - 1.0f; } // [-1,1)

  const LSymbolDef* findSymbol(const std::string& name) const {
    for (auto& s : G->_symbols)
      if (s && s->_name == name)
        return s.get();
    return nullptr;
  }

  // ---- LExpr eval: CONST / PARAM(+module fallback) / ENV(stub) / RNG / BINOP / CMP. Trees only (T6):
  // depth-cap 64 + a visited-set that fires loud on any shared/cyclic node. ----
  float evalExpr(const LExpr* e, const Env& env, int depth, std::set<const LExpr*>& seen) {
    OrkAssertIFMT(depth <= kLExprMaxDepth,
      "[lsystem grammar] LExpr recursion exceeded depth %d — cyclic or pathologically deep expr (T6: trees only).",
      kLExprMaxDepth);
    bool fresh = seen.insert(e).second; // insert ALWAYS (not inside the assert arg — that drops in NDEBUG)
    OrkAssertIFMT(fresh,
      "[lsystem grammar] LExpr node revisited during eval (kind=%d) — the expr graph is SHARED/CYCLIC, not a tree (T6).",
      e->_kind);
    switch (e->_kind) {
      case LExprKind::CONST:
        return e->_const;
      case LExprKind::PARAM: {
        auto it = env.find(e->_ref);
        if (it != env.end())
          return it->second;
        return moduleParam(P, e->_ref); // A8 fallback
      }
      case LExprKind::ENV:
        return 0.0f; // GR-6 stub (parse+eval-to-default; the field bridge is deferred)
      case LExprKind::RNG: {
        float a = (e->_args.size() > 0 && e->_args[0]) ? evalExpr(e->_args[0].get(), env, depth + 1, seen) : 0.0f;
        float b = (e->_args.size() > 1 && e->_args[1]) ? evalExpr(e->_args[1].get(), env, depth + 1, seen) : 1.0f;
        return a + draw01() * (b - a);
      }
      case LExprKind::BINOP: {
        float a = (e->_args.size() > 0 && e->_args[0]) ? evalExpr(e->_args[0].get(), env, depth + 1, seen) : 0.0f;
        float b = (e->_args.size() > 1 && e->_args[1]) ? evalExpr(e->_args[1].get(), env, depth + 1, seen) : 0.0f;
        switch (e->_op) {
          case LExprOp::ADD: return a + b;
          case LExprOp::SUB: return a - b;
          case LExprOp::MUL: return a * b;
          case LExprOp::DIV: return (std::fabs(b) > 1e-12f) ? (a / b) : 0.0f;
        }
        return 0.0f;
      }
      case LExprKind::CMP: {
        float a = (e->_args.size() > 0 && e->_args[0]) ? evalExpr(e->_args[0].get(), env, depth + 1, seen) : 0.0f;
        float b = (e->_args.size() > 1 && e->_args[1]) ? evalExpr(e->_args[1].get(), env, depth + 1, seen) : 0.0f;
        bool r = false;
        switch (e->_op) {
          case LExprOp::LT: r = a <  b; break;
          case LExprOp::LE: r = a <= b; break;
          case LExprOp::GT: r = a >  b; break;
          case LExprOp::GE: r = a >= b; break;
          case LExprOp::EQ: r = a == b; break;
          case LExprOp::NE: r = a != b; break;
        }
        return r ? 1.0f : 0.0f;
      }
    }
    return 0.0f;
  }
  float eval(const lexpr_ptr_t& e, const Env& env) {
    if (not e)
      return 0.0f;
    std::set<const LExpr*> seen;
    return evalExpr(e.get(), env, 0, seen);
  }

  void evalParams(const std::vector<lparam_binding_ptr_t>& binds, const Env& env, Env& out) {
    for (auto& b : binds)
      if (b and b->_value)
        out[b->_key] = eval(b->_value, env);
  }

  // ---- pass 1: rewrite `ops` in `env` at `depth` remaining iterations, appending resolved ops. ----
  void expand(const std::vector<lturtleop_ptr_t>& ops, const Env& env, int depth, std::vector<ROp>& out) {
    for (auto& opp : ops) {
      if (not opp)
        continue;
      opCount++; // increment ALWAYS (not inside the assert arg — that drops in NDEBUG)
      OrkAssertIFMT(opCount <= opCap,
        "[lsystem grammar] NON-TERMINATING: rewrite exceeded %u ops (64x segment_budget=%u). The grammar "
        "spins producing non-terminals (zero-segment recursion?). Add a terminating rule/guard or lower depth.",
        opCap, segBudget);
      const LTurtleOp* op = opp.get();
      switch (op->_kind) {
        case LTurtleKind::SEGMENT: {
          if (segCount >= segBudget) {
            if (not budgetHit) {
              budgetHit = true;
              printf("[lsystem grammar] segment_budget=%u reached — halting rewrite (grammar exceeds the bake-cost "
                     "bound; raise segment_budget or lower depth).\n", segBudget);
              fflush(stdout);
            }
            break;
          }
          ROp r;
          r.kind = LTurtleKind::SEGMENT;
          r.gid  = op->_gid;
          evalParams(op->_params, env, r.params);
          out.push_back(std::move(r));
          segCount++;
          break;
        }
        case LTurtleKind::CALL: {
          if (depth <= 0 or budgetHit)
            break; // depth exhausted or over budget: stop scheduling non-terminals
          Env childEnv;
          if (auto sd = findSymbol(op->_symbol))
            for (auto& kv : sd->_defaults)
              childEnv[kv.first] = kv.second; // symbol defaults ...
          evalParams(op->_params, env, childEnv); // ... overridden by evaluated CALL params (in PARENT env)
          // gather rules for this symbol, filtered by guard (in the child env), weighted-choose one
          std::vector<const LRuleDef*> cands;
          std::vector<float>           ws;
          float total = 0.0f;
          for (auto& rd : G->_rules) {
            if (not rd or rd->_lhs != op->_symbol)
              continue;
            if (rd->_guard and eval(rd->_guard, childEnv) < 0.5f)
              continue;
            float w = std::max(0.0f, rd->_weight);
            cands.push_back(rd.get());
            ws.push_back(w);
            total += w;
          }
          if (cands.empty())
            break; // no production (terminal non-terminal): drop
          const LRuleDef* chosen = cands.front();
          if (cands.size() > 1 and total > 0.0f) {
            float pick = draw(symOrdinal(op->_symbol) ^ (uint32_t(depth) * 0x85ebca6bu)) * total;
            float acc  = 0.0f;
            chosen     = cands.back();
            for (size_t i = 0; i < cands.size(); ++i) {
              acc += ws[i];
              if (pick < acc) { chosen = cands[i]; break; }
            }
          }
          expand(chosen->_rhs, childEnv, depth - 1, out);
          break;
        }
        case LTurtleKind::FORK: {
          ROp r;
          r.kind = LTurtleKind::FORK;
          r.gid  = op->_gid;
          evalParams(op->_params, env, r.params);
          int N = std::max(1, int(std::lround(getp(r.params, "count", 1.0f))));
          for (int k = 0; k < N; k++) {
            std::vector<ROp> branch;
            expand(op->_children, env, depth, branch); // each branch rewrites independently (distinct RNG)
            r.branches.push_back(std::move(branch));
          }
          out.push_back(std::move(r));
          break;
        }
        case LTurtleKind::CHOOSE: {
          int n = int(op->_children.size());
          if (n == 0)
            break;
          int idx = 0;
          if (n > 1) {
            float total = 0.0f;
            for (int i = 0; i < n; i++)
              total += (i < int(op->_weights.size())) ? std::max(0.0f, op->_weights[i]) : 1.0f;
            float pick = draw(0x00c0ffeeu ^ (uint32_t(depth) * 0x27d4eb2fu)) * total;
            float acc  = 0.0f;
            idx        = n - 1;
            for (int i = 0; i < n; i++) {
              acc += (i < int(op->_weights.size())) ? std::max(0.0f, op->_weights[i]) : 1.0f;
              if (pick < acc) { idx = i; break; }
            }
          }
          std::vector<lturtleop_ptr_t> one{op->_children[idx]};
          expand(one, env, depth, out);
          break;
        }
        case LTurtleKind::WHEN: {
          bool ok = (not op->_guard) or (eval(op->_guard, env) >= 0.5f);
          if (ok)
            expand(op->_children, env, depth, out);
          break;
        }
        default: { // PITCH / ROLL / YAW / TAPER / SLOT — terminal, params-only
          ROp r;
          r.kind = op->_kind;
          r.gid  = op->_gid;
          evalParams(op->_params, env, r.params);
          out.push_back(std::move(r));
          break;
        }
      }
    }
  }

  // ---- pass 2: interpret the resolved op stream with a turtle frame (Frame passed by value = the
  // bracket-stack push/pop). Emits nodes + slots; heading rotations keep the frame orthonormal. ----
  void interpret(const std::vector<ROp>& ops, Frame fr, xfnode_vect& nodes, xfslot_vect& slots) {
    for (auto& r : ops) {
      switch (r.kind) {
        case LTurtleKind::SEGMENT: {
          float len = getp(r.params, "len", P->_seg_len);
          float rad = hasp(r.params, "rad")    ? r.params.at("rad")
                    : hasp(r.params, "radius") ? r.params.at("radius")
                                               : fr.rad;
          float gen = getp(r.params, "gen", 0.0f);
          // tropism + heading jitter — the module's tropism/jit_* scalars stay live across ALL grammars.
          fr.H = _tropismBend(fr.H, P->_tropism);
          if (P->_jitter > 0.0f) {
            fvec3 ax(srnd(), srnd(), srnd());
            if (_len3(ax) > 1e-4f)
              fr.H = _rot(fr.H, ax.normalized(), P->_jitter * P->_jit_wave * std::fabs(srnd()));
          }
          reortho(fr);
          fr.pos    = fr.pos + fr.H * len;
          fr.parent = emitNode(nodes, fr.pos, fr.H, rad, gen, r.gid, fr.parent);
          fr.rad    = rad;
          break;
        }
        case LTurtleKind::PITCH: { float a = getp(r.params, "angle", 0.0f) * kDeg2Rad; fr.H = _rot(fr.H, fr.R, a); fr.U = _rot(fr.U, fr.R, a); reortho(fr); break; }
        case LTurtleKind::YAW:   { float a = getp(r.params, "angle", 0.0f) * kDeg2Rad; fr.H = _rot(fr.H, fr.U, a); fr.R = _rot(fr.R, fr.U, a); reortho(fr); break; }
        case LTurtleKind::ROLL:  { float a = getp(r.params, "angle", 0.0f) * kDeg2Rad; fr.R = _rot(fr.R, fr.H, a); fr.U = _rot(fr.U, fr.H, a); reortho(fr); break; }
        case LTurtleKind::TAPER: { fr.rad *= getp(r.params, "amount", 1.0f); break; }
        case LTurtleKind::SLOT: {
          XfSlot s;
          s._node = fr.parent;
          for (int i = 0; i < 16; i++)
            s._local[i] = (i % 5 == 0) ? 1.0f : 0.0f; // identity local xform
          s._tag = r.gid;
          slots.push_back(s);
          break;
        }
        case LTurtleKind::FORK: {
          int   N      = int(r.branches.size());
          float lean   = getp(r.params, "lean", 0.0f) * kDeg2Rad;
          float spread = (N > 1) ? (6.2831853f / float(N)) : (P->_roll * kDeg2Rad);
          for (int k = 0; k < N; k++) {
            Frame child = fr; // push
            // v2 fork math: roll about heading to the child's azimuth, then pitch off it by `lean`.
            float az = float(k) * spread + P->_jitter * P->_jit_azimuth * srnd();
            child.R  = _rot(child.R, child.H, az);
            child.U  = _rot(child.U, child.H, az);
            float p  = lean * (1.0f + P->_jit_pitch * P->_jitter * srnd());
            child.H  = _rot(child.H, child.R, p);
            child.U  = _rot(child.U, child.R, p);
            reortho(child);
            interpret(r.branches[k], child, nodes, slots);
          }
          break;
        }
        default: break;
      }
    }
  }
};

} // anonymous namespace

///////////////////////////////////////////////////////////////////////////////

void deriveLRuleSet(const LRuleSet* grammar, const LSystemModuleData* env,
                    ork::hyper::xfnode_vect& out_nodes, ork::hyper::xfslot_vect& out_slots) {
  OrkAssert(grammar and env);
  // axiom-overflow (T7 companion): if the SEED STRING alone can't fit the budget the grammar is
  // misconfigured — fail loud (distinct from the normal over-budget REWRITE, which stops + logs).
  uint32_t axiomSegs = 0;
  for (auto& op : grammar->_axiom)
    if (op and op->_kind == LTurtleKind::SEGMENT)
      axiomSegs++;
  OrkAssertIFMT(axiomSegs <= std::max<uint32_t>(1u, grammar->_segmentBudget),
    "[lsystem grammar] axiom-overflow: the axiom holds %u literal SEGMENT ops but segment_budget=%u — even the "
    "seed string does not fit. Raise segment_budget or shrink the axiom.",
    axiomSegs, grammar->_segmentBudget);

  LEval ev(grammar, env);
  // root node at origin, heading +Y (matches the legacy archetype path's root).
  Frame root;
  root.rad    = env->_base_radius;
  root.parent = emitNode(out_nodes, fvec3(0, 0, 0), fvec3(0, 1, 0), env->_base_radius, 0.0f, 0u, 0xffffffffu);

  std::vector<ROp> stream;
  ev.expand(grammar->_axiom, Env{}, int(grammar->_depth), stream); // pass 1
  ev.interpret(stream, root, out_nodes, out_slots);                // pass 2
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::lev2::hypermesh
