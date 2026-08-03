////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
//
// LRuleSet REWRITE — pass 1 of the grammar evaluator (GR1.b), family-neutral.
//
//   pass 1 (HERE):  axiom --_depth iterations--> a flat resolved op stream. Rules / CHOOSE
//                   branches are picked by a STATELESS counter-hash RNG (A3#1 / T10) and every
//                   LExpr param is evaluated EAGERLY in the producing instance's environment
//                   (A8: a rule's NUMBERS flow through the PARAM env = the host's live-pokeable
//                   reflected scalars — never constant-folded into the grammar).
//   pass 2 (NOT here): the consuming FAMILY walks the resolved stream. The turtle/mesh
//                   interpreter lives in lev2 (hmdflow_lruleset.cpp); the audio family gets its
//                   own. Pass 2 CONTINUES this pass's RNG stream — pass an LRewriter::_rng
//                   reference into the interpreter, do not construct a second one, or every
//                   stochastic decision downstream shifts.
//
// The pass knows only the CONTROL ops (CALL/CHOOSE/WHEN/FORK). Every other op code is resolved
// through the family vocabulary registry (grammar/vocabulary.h) and copied through opaquely as
// (kind, vocabulary, gid, evaluated params); the only family fact the pass acts on is the
// registry's COUNTED flag, which charges that op against LRuleSet::_countedBudget.
//
////////////////////////////////////////////////////////////////
#pragma once

#include <ork/grammar/lruleset.h>
#include <functional>
#include <set>

namespace ork::grammar {

///////////////////////////////////////////////////////////////////////////////

using Env = std::map<std::string, float>;

inline bool hasp(const Env& m, const char* k) {
  return m.find(k) != m.end();
}
inline float getp(const Env& m, const char* k, float dflt) {
  auto it = m.find(k);
  return (it != m.end()) ? it->second : dflt;
}

// PARAM fallback env (A8): a PARAM expr whose name is not a symbol-instance param resolves
// through the HOST's reflected scalar of that name (live-pokeable). Unknown name -> 0, like
// the ENV stub. Supplying this is how a family injects its module parameters without the
// rewrite pass ever naming a family type.
using param_resolver_t = std::function<float(const std::string&)>;

// the STATELESS counter-hash (A3#1) — the terrain-scatter / leafscatter `_lhash` family verbatim:
// deterministic per (a,b,c), no accumulator state, no time dependence (T10).
uint32_t ghash(uint32_t a, uint32_t b, uint32_t c);

// the derive RNG. `counter` is the only mutable state and it is ORDER-ONLY: identical draw
// sequence => identical output, so pass 2 must share this instance with pass 1.
struct LRng {
  uint32_t _seed    = 0;
  uint32_t _counter = 0;
  // `key` folds the decision context (symbol/depth/salt) so sibling decisions at the same
  // counter still diverge; `_counter++` guarantees per-draw uniqueness.
  float draw(uint32_t key);
  float draw01();
  float srnd(); // [-1,1)
};

// a resolved op: the rewrite output pass 2 walks. Vocabulary/FORK kinds only (CHOOSE/WHEN/CALL are
// resolved away in pass 1). FORK carries N INDEPENDENTLY-rewritten child bodies (distinct RNG each).
// `vocab` is the registry id `kind` resolved to (kVocabControl for FORK) — a family interpreter
// dispatches on it and REJECTS a foreign op instead of silently skipping it.
struct ROp {
  // the default is a control code that can never survive pass 1, so an ROp nobody filled in reads
  // as obviously wrong rather than as a plausible first vocabulary op.
  LOpCode                       kind  = LOpCode::CALL;
  lvocab_id_t                   vocab = kVocabControl;
  uint32_t                      gid   = 0;
  Env                           params;   // eagerly evaluated (A8)
  std::vector<std::vector<ROp>> branches; // FORK only
};
using rop_vect = std::vector<ROp>;

///////////////////////////////////////////////////////////////////////////////
// LRewriter — one instance per derive() (fresh counters => deterministic, order-only state).
///////////////////////////////////////////////////////////////////////////////
struct LRewriter {

  LRewriter(const LRuleSet* g, param_resolver_t resolver);

  // ---- pass 1: rewrite `ops` in `env` at `depth` remaining iterations, appending resolved ops. ----
  void expand(const std::vector<lturtleop_ptr_t>& ops, const Env& env, int depth, rop_vect& out);

  // LExpr eval: CONST / PARAM(+host fallback) / ENV(stub) / RNG / BINOP / CMP. Trees only (T6).
  float eval(const lexpr_ptr_t& e, const Env& env);
  void  evalParams(const std::vector<lparam_binding_ptr_t>& binds, const Env& env, Env& out);

  const LSymbolDef* findSymbol(const std::string& name) const;
  uint32_t          symOrdinal(const std::string& s) const;

  const LRuleSet*  _grammar;
  param_resolver_t _resolver;
  LRng             _rng; // pass 2 continues this stream (see header note)
  uint32_t         _countedCount  = 0; // emissions of the vocabulary's counted op
  uint32_t         _opCount       = 0;
  uint32_t         _countedBudget = 1;
  uint32_t         _opCap         = 1;
  bool             _budgetHit     = false;
  std::map<std::string, uint32_t> _symOrd;

private:
  float evalExpr(const LExpr* e, const Env& env, int depth, std::set<const LExpr*>& seen);
};

///////////////////////////////////////////////////////////////////////////////
// axiom-overflow preflight (T7 companion): if the SEED STRING alone cannot fit the budget the
// grammar is misconfigured — fails LOUD (distinct from the normal over-budget REWRITE, which
// stops + logs). Call before constructing the rewriter.
///////////////////////////////////////////////////////////////////////////////
void preflightAxiomBudget(const LRuleSet* grammar);

} // namespace ork::grammar
