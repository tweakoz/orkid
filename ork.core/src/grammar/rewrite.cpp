////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
//
// LRuleSet rewrite — pass 1 of the grammar evaluator (GR1.b). Family-neutral: no mesh,
// no transform, no lev2 type appears here. See ork/grammar/rewrite.h for the pass contract
// (notably: pass 2 must CONTINUE this pass's _rng, never construct its own).
//
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/grammar/rewrite.h>
#include <cmath>
#include <cstdio>
#include <algorithm>

namespace ork::grammar {

///////////////////////////////////////////////////////////////////////////////

uint32_t ghash(uint32_t a, uint32_t b, uint32_t c) {
  uint32_t x = a * 2654435761u + b * 2246822519u + c * 3266489917u + 374761393u;
  x ^= x >> 15; x *= 2246822519u; x ^= x >> 13; x *= 3266489917u; x ^= x >> 16;
  return x;
}

float LRng::draw(uint32_t key) {
  return float(ghash(_seed ^ key, _counter++, 0x9e3779b9u)) * (1.0f / 4294967296.0f);
}
float LRng::draw01() {
  return draw(0u);
}
float LRng::srnd() {
  return draw01() * 2.0f - 1.0f;
}

///////////////////////////////////////////////////////////////////////////////

LRewriter::LRewriter(const LRuleSet* g, param_resolver_t resolver)
    : _grammar(g)
    , _resolver(std::move(resolver)) {
  OrkAssert(_grammar);
  _rng._seed     = g->_seed;
  _countedBudget = std::max<uint32_t>(1u, g->_countedBudget);
  _opCap         = _countedBudget * 64u; // T7: hard cap on total rewrite ops vs non-terminating grammars
  for (uint32_t i = 0; i < uint32_t(g->_symbols.size()); ++i)
    if (g->_symbols[i])
      _symOrd[g->_symbols[i]->_name] = i;
}

///////////////////////////////////////////////////////////////////////////////

uint32_t LRewriter::symOrdinal(const std::string& s) const {
  auto it = _symOrd.find(s);
  if (it != _symOrd.end())
    return it->second + 1u;
  uint32_t h = 2166136261u; // FNV-1a on the name for symbols outside the declared alphabet
  for (char c : s) { h ^= uint8_t(c); h *= 16777619u; }
  return h;
}

const LSymbolDef* LRewriter::findSymbol(const std::string& name) const {
  for (auto& s : _grammar->_symbols)
    if (s && s->_name == name)
      return s.get();
  return nullptr;
}

///////////////////////////////////////////////////////////////////////////////
// LExpr eval — trees only (T6): depth-cap kLExprMaxDepth + a visited-set that fires loud on
// any shared/cyclic node.
///////////////////////////////////////////////////////////////////////////////

float LRewriter::evalExpr(const LExpr* e, const Env& env, int depth, std::set<const LExpr*>& seen) {
  OrkAssertIFMT(depth <= kLExprMaxDepth,
    "[lsystem grammar] LExpr recursion exceeded depth %d — cyclic or pathologically deep expr (T6: trees only).",
    kLExprMaxDepth);
  bool fresh = seen.insert(e).second; // insert ALWAYS (not inside the assert arg — that drops in NDEBUG)
  OrkAssertIFMT(fresh,
    "[lsystem grammar] LExpr node revisited during eval (kind=%d) — the expr graph is SHARED/CYCLIC, not a tree (T6).",
    int(e->_kind));
  switch (e->_kind) {
    case LExprKind::CONST:
      return e->_const;
    case LExprKind::PARAM: {
      auto it = env.find(e->_ref);
      if (it != env.end())
        return it->second;
      return _resolver ? _resolver(e->_ref) : 0.0f; // A8 fallback
    }
    case LExprKind::ENV:
      return 0.0f; // GR-6 stub (parse+eval-to-default; the field bridge is deferred)
    case LExprKind::RNG: {
      float a = (e->_args.size() > 0 && e->_args[0]) ? evalExpr(e->_args[0].get(), env, depth + 1, seen) : 0.0f;
      float b = (e->_args.size() > 1 && e->_args[1]) ? evalExpr(e->_args[1].get(), env, depth + 1, seen) : 1.0f;
      return a + _rng.draw01() * (b - a);
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

float LRewriter::eval(const lexpr_ptr_t& e, const Env& env) {
  if (not e)
    return 0.0f;
  std::set<const LExpr*> seen;
  return evalExpr(e.get(), env, 0, seen);
}

void LRewriter::evalParams(const std::vector<lparam_binding_ptr_t>& binds, const Env& env, Env& out) {
  for (auto& b : binds)
    if (b and b->_value)
      out[b->_key] = eval(b->_value, env);
}

///////////////////////////////////////////////////////////////////////////////

void LRewriter::expand(const std::vector<lturtleop_ptr_t>& ops, const Env& env, int depth, rop_vect& out) {
  auto& vocabulary = LVocabularyRegistry::instance();
  for (auto& opp : ops) {
    if (not opp)
      continue;
    _opCount++; // increment ALWAYS (not inside the assert arg — that drops in NDEBUG)
    OrkAssertIFMT(_opCount <= _opCap,
      "[lsystem grammar] NON-TERMINATING: rewrite exceeded %u ops (64x segment_budget=%u). The grammar "
      "spins producing non-terminals (zero-segment recursion?). Add a terminating rule/guard or lower depth.",
      _opCap, _countedBudget);
    const LTurtleOp* op = opp.get();
    switch (op->_kind) {
      case LOpCode::CALL: {
        if (depth <= 0 or _budgetHit)
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
        for (auto& rd : _grammar->_rules) {
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
          float pick = _rng.draw(symOrdinal(op->_symbol) ^ (uint32_t(depth) * 0x85ebca6bu)) * total;
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
      case LOpCode::FORK: {
        ROp r;
        r.kind = LOpCode::FORK;
        r.gid  = op->_gid;
        evalParams(op->_params, env, r.params);
        int N = std::max(1, int(std::lround(getp(r.params, "count", 1.0f))));
        for (int k = 0; k < N; k++) {
          rop_vect branch;
          expand(op->_children, env, depth, branch); // each branch rewrites independently (distinct RNG)
          r.branches.push_back(std::move(branch));
        }
        out.push_back(std::move(r));
        break;
      }
      case LOpCode::CHOOSE: {
        int n = int(op->_children.size());
        if (n == 0)
          break;
        int idx = 0;
        if (n > 1) {
          float total = 0.0f;
          for (int i = 0; i < n; i++)
            total += (i < int(op->_weights.size())) ? std::max(0.0f, op->_weights[i]) : 1.0f;
          float pick = _rng.draw(0x00c0ffeeu ^ (uint32_t(depth) * 0x27d4eb2fu)) * total;
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
      case LOpCode::WHEN: {
        bool ok = (not op->_guard) or (eval(op->_guard, env) >= 0.5f);
        if (ok)
          expand(op->_children, env, depth, out);
        break;
      }
      default: {
        // a family VOCABULARY op: opaque terminal, params-only. The registry supplies the only two
        // facts core acts on — which vocabulary claims the code, and whether it is the COUNTED op.
        const LOpDesc& desc = vocabulary.requireOp(int32_t(op->_kind));
        if (desc._counted and (_countedCount >= _countedBudget)) {
          if (not _budgetHit) {
            _budgetHit = true;
            printf("[lsystem grammar] segment_budget=%u reached — halting rewrite (grammar exceeds the bake-cost "
                   "bound; raise segment_budget or lower depth).\n", _countedBudget);
            fflush(stdout);
          }
          break;
        }
        ROp r;
        r.kind  = op->_kind;
        r.vocab = desc._vocab;
        r.gid   = op->_gid;
        evalParams(op->_params, env, r.params);
        out.push_back(std::move(r));
        if (desc._counted)
          _countedCount++;
        break;
      }
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

void preflightAxiomBudget(const LRuleSet* grammar) {
  OrkAssert(grammar);
  auto&    vocabulary   = LVocabularyRegistry::instance();
  uint32_t axiomCounted = 0;
  for (auto& op : grammar->_axiom)
    if (op and (not isControlOp(op->_kind)) and vocabulary.requireOp(int32_t(op->_kind))._counted)
      axiomCounted++;
  OrkAssertIFMT(axiomCounted <= std::max<uint32_t>(1u, grammar->_countedBudget),
    "[lsystem grammar] axiom-overflow: the axiom holds %u literal counted ops but segment_budget=%u — even the "
    "seed string does not fit. Raise segment_budget or shrink the axiom.",
    axiomCounted, grammar->_countedBudget);
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::grammar
