////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
//
// LRuleSet — the TURTLE half of the grammar evaluator (GR1.b), i.e. everything the
// mesh family owns: its OP VOCABULARY (LMeshOp, registered into ork::grammar's registry
// below) and the pass-2 interpreter that turns resolved ops into geometry. The schema
// (six reflected types + their describeX) and the generic rewrite pass live in ork.core
// (ork/grammar/lruleset.h, ork/grammar/rewrite.h) so the audio family can share them.
//
// derive() is two CPU bake-time passes (boundary 5):
//   pass 1 (rewrite, ork.core): axiom --_depth iterations--> a flat resolved op stream. Rules / CHOOSE
//                       branches are picked by a STATELESS counter-hash RNG (A3#1 / T10) and every
//                       LExpr param is evaluated EAGERLY in the producing instance's environment
//                       (A8: a rule's NUMBERS flow through the PARAM env = the module's live-pokeable
//                       reflected scalars — never constant-folded into the grammar).
//   pass 2 (interpret, HERE): walk the resolved ops with a turtle FRAME + bracket STACK, emitting one XfNode
//                       per SEGMENT (parent = the frame's current node, _attrs[0]=radius, _tags gid band
//                       per A1). FORK pushes N independently-rewritten child frames; SLOT emits an XfSlot.
// The two passes SHARE one LRng: pass 2's jitter/azimuth draws continue pass 1's counter stream, so the
// turtle takes the rewriter's _rng BY REFERENCE — a second RNG would shift every stochastic decision.
// The caller applies the shared _buildFrames post-pass + GPU upload, so this stays PURE CPU.
//
// NOTE (post-GR1.d): the legacy archetype procedures are DELETED; this evaluator is the only
// turtle. _len3/_rot/_perp still exist as static copies in hmdflow_module_lsystem.cpp because
// the shared _buildFrames post-pass (which _buildSkeleton runs on the derived nodes) needs them
// there; _tropismBend lives only here. Both sets are TU-local statics — no ODR concern.
//
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/grammar/rewrite.h>
#include <ork/lev2/gfx/hypermesh/hmdflow.h> // LSystemModuleData (PARAM env) + XfNode/XfSlot
#include <cmath>
#include <algorithm>

namespace ork::lev2::hypermesh {

namespace {

using ork::fvec3;
using ork::hyper::XfNode;
using ork::hyper::XfSlot;
using ork::hyper::xfnode_vect;
using ork::hyper::xfslot_vect;
using ork::grammar::Env;
using ork::grammar::getp;
using ork::grammar::hasp;
using ork::grammar::LOpDesc;
using ork::grammar::LRewriter;
using ork::grammar::LRng;
using ork::grammar::LVocabularyRegistry;
using ork::grammar::rop_vect;

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

// PARAM fallback env (A8): a PARAM expr whose name is not a symbol-instance param resolves to the
// module's reflected scalar of that name (live-pokeable). Unknown name -> 0 (like the ENV stub).
// This lambda-able lookup is the ONLY channel by which family parameters reach the core rewrite pass.
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

// the pass-2 interpreter: the mesh family's reading of the resolved vocabulary ops. `rng` is a
// REFERENCE to the rewriter's stream (see file header).
struct LTurtle {
  const LSystemModuleData* P;
  LRng&                    rng;

  void interpret(const rop_vect& ops, Frame fr, xfnode_vect& nodes, xfslot_vect& slots) {
    for (auto& r : ops) {
      if (r.vocab == ork::grammar::kVocabControl) {
        interpretControl(r, fr, nodes, slots);
        continue;
      }
      // a resolved stream is authored per family; an op from someone else's alphabet means the
      // grammar was pointed at the wrong interpreter — say so instead of skipping it.
      OrkAssertIFMT(r.vocab == meshVocabId(),
        "[lsystem turtle] resolved op code %d belongs to vocabulary '%s' — the mesh turtle interprets only the "
        "'mesh' alphabet.",
        int32_t(r.kind), LVocabularyRegistry::instance().vocabularyName(r.vocab).c_str());
      switch (LMeshOp(int32_t(r.kind))) {
        case LMeshOp::SEGMENT: {
          float len = getp(r.params, "len", P->_seg_len);
          float rad = hasp(r.params, "rad")    ? r.params.at("rad")
                    : hasp(r.params, "radius") ? r.params.at("radius")
                                               : fr.rad;
          float gen = getp(r.params, "gen", 0.0f);
          // tropism + heading jitter — the module's tropism/jit_* scalars stay live across ALL grammars.
          fr.H = _tropismBend(fr.H, P->_tropism);
          if (P->_jitter > 0.0f) {
            fvec3 ax(rng.srnd(), rng.srnd(), rng.srnd());
            if (_len3(ax) > 1e-4f)
              fr.H = _rot(fr.H, ax.normalized(), P->_jitter * P->_jit_wave * std::fabs(rng.srnd()));
          }
          reortho(fr);
          fr.pos    = fr.pos + fr.H * len;
          fr.parent = emitNode(nodes, fr.pos, fr.H, rad, gen, r.gid, fr.parent);
          fr.rad    = rad;
          break;
        }
        case LMeshOp::PITCH: { float a = getp(r.params, "angle", 0.0f) * kDeg2Rad; fr.H = _rot(fr.H, fr.R, a); fr.U = _rot(fr.U, fr.R, a); reortho(fr); break; }
        case LMeshOp::YAW:   { float a = getp(r.params, "angle", 0.0f) * kDeg2Rad; fr.H = _rot(fr.H, fr.U, a); fr.R = _rot(fr.R, fr.U, a); reortho(fr); break; }
        case LMeshOp::ROLL:  { float a = getp(r.params, "angle", 0.0f) * kDeg2Rad; fr.R = _rot(fr.R, fr.H, a); fr.U = _rot(fr.U, fr.H, a); reortho(fr); break; }
        case LMeshOp::TAPER: { fr.rad *= getp(r.params, "amount", 1.0f); break; }
        case LMeshOp::SLOT: {
          XfSlot s;
          s._node = fr.parent;
          for (int i = 0; i < 16; i++)
            s._local[i] = (i % 5 == 0) ? 1.0f : 0.0f; // identity local xform
          s._tag = r.gid;
          slots.push_back(s);
          break;
        }
      }
    }
  }

  // the one CONTROL op that survives pass 1. `fr` is the parent frame — FORK pushes copies of it,
  // it never advances the parent.
  void interpretControl(const ork::grammar::ROp& r, const Frame& fr, xfnode_vect& nodes, xfslot_vect& slots) {
    OrkAssertIFMT(r.kind == LOpCode::FORK,
      "[lsystem turtle] control op code %d survived the rewrite — only FORK reaches an interpreter.",
      int32_t(r.kind));
    int   N      = int(r.branches.size());
    float lean   = getp(r.params, "lean", 0.0f) * kDeg2Rad;
    float spread = (N > 1) ? (6.2831853f / float(N)) : (P->_roll * kDeg2Rad);
    for (int k = 0; k < N; k++) {
      Frame child = fr; // push
      // v2 fork math: roll about heading to the child's azimuth, then pitch off it by `lean`.
      float az = float(k) * spread + P->_jitter * P->_jit_azimuth * rng.srnd();
      child.R  = _rot(child.R, child.H, az);
      child.U  = _rot(child.U, child.H, az);
      float p  = lean * (1.0f + P->_jit_pitch * P->_jitter * rng.srnd());
      child.H  = _rot(child.H, child.R, p);
      child.U  = _rot(child.U, child.R, p);
      reortho(child);
      interpret(r.branches[k], child, nodes, slots);
    }
  }
};

} // anonymous namespace

///////////////////////////////////////////////////////////////////////////////
// the mesh family's alphabet, handed to ork.core. Codes and saved tokens are PINNED (see
// LMeshOp): they are what every serialized grammar and the python DSL already speak.
///////////////////////////////////////////////////////////////////////////////

static ork::grammar::lvocab_id_t _mesh_vocab_id = ork::grammar::kVocabInvalid;

void registerMeshVocabulary() {
  const std::vector<LOpDesc> ops{
      {int32_t(LMeshOp::SEGMENT), "segment", true}, // the COUNTED op — segment_budget bounds the bake
      {int32_t(LMeshOp::PITCH),   "pitch",   false},
      {int32_t(LMeshOp::ROLL),    "roll",    false},
      {int32_t(LMeshOp::YAW),     "yaw",     false},
      {int32_t(LMeshOp::TAPER),   "taper",   false},
      {int32_t(LMeshOp::SLOT),    "slot",    false},
  };
  _mesh_vocab_id = LVocabularyRegistry::instance().registerVocabulary("mesh", ops);
}

ork::grammar::lvocab_id_t meshVocabId() {
  if (_mesh_vocab_id == ork::grammar::kVocabInvalid)
    registerMeshVocabulary(); // self-defend: never classify mesh ops against an unregistered alphabet
  return _mesh_vocab_id;
}

///////////////////////////////////////////////////////////////////////////////

void deriveLRuleSet(const LRuleSet* grammar, const LSystemModuleData* env,
                    ork::hyper::xfnode_vect& out_nodes, ork::hyper::xfslot_vect& out_slots) {
  OrkAssert(grammar and env);
  meshVocabId(); // the rewrite pass classifies every op through the registry — register first
  ork::grammar::preflightAxiomBudget(grammar);

  LRewriter ev(grammar, [env](const std::string& n) { return moduleParam(env, n); });
  // root node at origin, heading +Y (matches the legacy archetype path's root).
  Frame root;
  root.rad    = env->_base_radius;
  root.parent = emitNode(out_nodes, fvec3(0, 0, 0), fvec3(0, 1, 0), env->_base_radius, 0.0f, 0u, 0xffffffffu);

  rop_vect stream;
  ev.expand(grammar->_axiom, Env{}, int(grammar->_depth), stream); // pass 1
  LTurtle turtle{env, ev._rng};
  turtle.interpret(stream, root, out_nodes, out_slots);            // pass 2
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::lev2::hypermesh
