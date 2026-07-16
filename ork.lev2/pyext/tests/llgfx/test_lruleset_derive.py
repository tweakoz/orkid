#!/usr/bin/env python3
###############################################################################
# GR1.b gates — the LRuleSet EVALUATOR (derive: rewrite + turtle-interpret -> XfNodeGraph).
# PURE CPU, no GPU: the hypermesh `_deriveLRuleSet` test seam runs the two-pass evaluator and returns
# the derived XfNode buffer (raw bytes + unpacked fields). Class registration runs in Lev2AppInit's
# (graphics-free) ClassToucher, so a lev2appinit(use_subsystems=['opq','core','lev2']) suffices — the
# CI node is a headless Mac session where a GPU/COCOA context SIGSEGVs, so this test must not open one.
#
# Gates (JUL05_GR1.md §7 GR1.b):
#   A. determinism   — two derives, same (grammar,seed) -> byte-identical node buffer (memcmp) AND
#                      identical hypermeshModuleIdentityHash.
#   B. hand-computable — a trivial 2-rule grammar produces the exact node count + parent topology.
#   C. budget        — an over-budget grammar STOPS at budget with a loud log; axiom-overflow +
#                      non-terminating grammars ASSERT loud (verified in crash subprocesses).
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys, subprocess
from orkengine import core   # core before lev2
from orkengine import lev2
from orkengine import ecs    # full class registration

hm = lev2.hypermesh

# named int constants — mirror lruleset.h
CONST, PARAM, ENV, RNG, BINOP, CMP = 0, 1, 2, 3, 4, 5
ADD, SUB, MUL, DIV = 0, 1, 2, 3
LT, LE, GT, GE, EQ, NE = 10, 11, 12, 13, 14, 15
SEGMENT, PITCH, ROLL, YAW, TAPER, SLOT, FORK, CHOOSE, WHEN, CALL = 0, 1, 2, 3, 4, 5, 6, 7, 8, 9

NOPARENT = 0xffffffff


# ---- LExpr / op builders (fresh nodes every call -> TREE, no sharing/cycles; T6) ----
def konst(v):        return hm.LExpr(kind=CONST, constant=float(v))
def param(name):     return hm.LExpr(kind=PARAM, ref=name)
def binop(op, a, b): return hm.LExpr(kind=BINOP, op=op, args=[a, b])
def cmp(op, a, b):   return hm.LExpr(kind=CMP, op=op, args=[a, b])
def rng(a, b):       return hm.LExpr(kind=RNG, args=[konst(a), konst(b)])
def bind(k, e):      return hm.LParamBinding(key=k, value=e)


def _env(**kw):
  m = hm.LSystemModule.createShared()
  for k, v in kw.items():
    setattr(m, k, v)
  return m


###############################################################################
# Gate A — determinism
###############################################################################
def _stochastic_grammar():
  # A recursion that exercises BOTH RNG surfaces: CHOOSE (pass-1 rewrite RNG) + FORK azimuth jitter
  # (pass-2 turtle RNG). `gen<5` guard terminates it; segment_budget backstops.
  symbols = [hm.LSymbolDef(name="A", defaults={"len": 1.0, "gen": 0.0})]
  axiom = [hm.LTurtleOp(kind=CALL, symbol="A", params=[bind("len", konst(1.0))])]
  grow_child = lambda k, dl: hm.LTurtleOp(kind=FORK, params=[bind("count", konst(k))], children=[
      hm.LTurtleOp(kind=PITCH, params=[bind("angle", konst(dl))]),
      hm.LTurtleOp(kind=CALL, symbol="A", params=[
          bind("len", binop(MUL, param("len"), konst(0.7))),
          bind("gen", binop(ADD, param("gen"), konst(1.0))),
      ]),
  ])
  rule_a = hm.LRuleDef(lhs="A", guard=cmp(LT, param("gen"), konst(5.0)), weight=1.0, rhs=[
      hm.LTurtleOp(kind=SEGMENT, gid=1, params=[bind("len", param("len"))]),
      hm.LTurtleOp(kind=CHOOSE, weights=[0.7, 0.3], children=[grow_child(2, 25.0), grow_child(3, 15.0)]),
  ])
  return hm.LRuleSet(symbols=symbols, axiom=axiom, rules=[rule_a],
                     depth=6, segment_budget=400, seed=20260709)


def gate_a_determinism():
  env = _env(jitter=0.4, jit_azimuth=0.8, jit_pitch=0.35, tropism=0.03, seg_len=0.5, base_radius=0.06)
  g = _stochastic_grammar()
  r1 = hm._deriveLRuleSet(g, env)
  r2 = hm._deriveLRuleSet(g, env)
  assert r1["count"] > 8, f"grammar barely grew ({r1['count']} nodes) — determinism gate is vacuous"
  assert r1["bytes"] == r2["bytes"], "derive() NON-deterministic: node buffers differ across two calls (memcmp)"
  assert r1["parents"] == r2["parents"], "derive() NON-deterministic: parent topology differs"
  h1 = hm._moduleIdentityHash(_grammar_module(g, env))
  h2 = hm._moduleIdentityHash(_grammar_module(g, env))
  assert h1 == h2, f"hypermeshModuleIdentityHash unstable: {h1} != {h2}"
  print(f"[gate A] determinism OK: {r1['count']} nodes, memcmp-identical x2, identityHash={h1}", flush=True)


def _grammar_module(g, env):
  # a module carrying the grammar + the same env scalars (the cook-identity surface).
  m = hm.LSystemModule.createShared()
  m.grammar = g
  for name in ("jitter", "jit_azimuth", "jit_pitch", "tropism", "seg_len", "base_radius"):
    setattr(m, name, getattr(env, name))
  return m


###############################################################################
# Gate B — hand-computable node count + parent topology
###############################################################################
def _fork_grammar():
  #  axiom: A
  #  A -> SEGMENT(len=1) ; FORK(count=2){ CALL B }      (a trunk joint, then two child branches)
  #  B -> SEGMENT(len=1)
  #  depth=2, jitter=tropism=0  =>  deterministic, straight-up geometry.
  symbols = [hm.LSymbolDef(name="A"), hm.LSymbolDef(name="B")]
  axiom = [hm.LTurtleOp(kind=CALL, symbol="A")]
  rule_a = hm.LRuleDef(lhs="A", weight=1.0, rhs=[
      hm.LTurtleOp(kind=SEGMENT, gid=1, params=[bind("len", konst(1.0))]),
      hm.LTurtleOp(kind=FORK, params=[bind("count", konst(2.0))],
                   children=[hm.LTurtleOp(kind=CALL, symbol="B")]),
  ])
  rule_b = hm.LRuleDef(lhs="B", weight=1.0, rhs=[
      hm.LTurtleOp(kind=SEGMENT, gid=2, params=[bind("len", konst(1.0))]),
  ])
  return hm.LRuleSet(symbols=symbols, axiom=axiom, rules=[rule_a, rule_b],
                     depth=2, segment_budget=64, seed=1)


def gate_b_handcomputable():
  env = _env(jitter=0.0, tropism=0.0, seg_len=1.0, base_radius=0.05)
  r = hm._deriveLRuleSet(_fork_grammar(), env)
  # Hand derivation:
  #   node0 = root (parent 0xffffffff, pos 0,0,0)
  #   node1 = A's SEGMENT       (parent 0, pos y=1)
  #   node2 = branch0 B SEGMENT (parent 1, pos y=2)
  #   node3 = branch1 B SEGMENT (parent 1, pos y=2)
  assert r["count"] == 4, f"expected 4 nodes (root + 3 SEGMENTs), got {r['count']}"
  assert r["parents"] == [NOPARENT, 0, 1, 1], f"parent topology mismatch: {r['parents']}"
  ys = r["positions"][1::3]
  assert abs(ys[0]) < 1e-5 and abs(ys[1] - 1.0) < 1e-5 and abs(ys[2] - 2.0) < 1e-5 and abs(ys[3] - 2.0) < 1e-5, \
      f"node heights not hand-computable: {ys}"
  # gid band packs into __tags[20:32) (A1)
  assert (r["tags"][1] >> 20) == 1 and (r["tags"][2] >> 20) == 2, f"gid band not written: {r['tags']}"
  print(f"[gate B] hand-computable OK: count=4, parents={r['parents']}", flush=True)


###############################################################################
# Gate C — budget: stop-at-budget (loud log, inline) + assert cases (crash subprocesses)
###############################################################################
def _overbudget_grammar():
  #  A -> SEGMENT ; CALL A     (unbounded linear growth; depth=100)  budget=10 => stops at 10 segments.
  symbols = [hm.LSymbolDef(name="A")]
  axiom = [hm.LTurtleOp(kind=CALL, symbol="A")]
  rule_a = hm.LRuleDef(lhs="A", weight=1.0, rhs=[
      hm.LTurtleOp(kind=SEGMENT, gid=1, params=[bind("len", konst(0.5))]),
      hm.LTurtleOp(kind=CALL, symbol="A"),
  ])
  return hm.LRuleSet(symbols=symbols, axiom=axiom, rules=[rule_a],
                     depth=100, segment_budget=10, seed=1)


def gate_c_budget_stop():
  env = _env(jitter=0.0, tropism=0.0, seg_len=0.5)
  r = hm._deriveLRuleSet(_overbudget_grammar(), env)  # emits the loud "segment_budget=10 reached" log
  assert r["count"] == 11, f"over-budget grammar should stop at root+10 segments, got {r['count']}"
  print(f"[gate C] budget-stop OK: capped at {r['count']} nodes (root + segment_budget=10)", flush=True)


def _axiom_overflow_case():
  # 4 literal SEGMENTs in the axiom, budget=2  =>  deriveLRuleSet asserts loud ("axiom-overflow").
  env = _env()
  axiom = [hm.LTurtleOp(kind=SEGMENT, gid=1) for _ in range(4)]
  g = hm.LRuleSet(symbols=[], axiom=axiom, rules=[], depth=1, segment_budget=2, seed=1)
  hm._deriveLRuleSet(g, env)  # expected: LOUD assert + crash


def _nonterminating_case():
  # A -> A A  (ZERO segments, breadth-doubling)  =>  never hits segment_budget; the 64x op cap asserts.
  env = _env()
  symbols = [hm.LSymbolDef(name="A")]
  axiom = [hm.LTurtleOp(kind=CALL, symbol="A")]
  rule_a = hm.LRuleDef(lhs="A", weight=1.0, rhs=[
      hm.LTurtleOp(kind=CALL, symbol="A"),
      hm.LTurtleOp(kind=CALL, symbol="A"),
  ])
  g = hm.LRuleSet(symbols=symbols, axiom=axiom, rules=[rule_a], depth=30, segment_budget=100, seed=1)
  hm._deriveLRuleSet(g, env)  # expected: LOUD assert + crash ("NON-TERMINATING")


def gate_c_assert_cases():
  # each assert case CRASHES the process (OrkAssert -> segfault), so run in a subprocess + check that
  # it died AND printed the named marker. This is the ops-self-defend / fail-loud contract (T7).
  for case, marker in (("axiom_overflow", b"axiom-overflow"), ("nonterminating", b"NON-TERMINATING")):
    cp = subprocess.run([sys.executable, os.path.abspath(__file__), "--case", case],
                        capture_output=True, timeout=120)
    died = cp.returncode != 0
    loud = marker in cp.stderr or marker in cp.stdout
    assert died, f"[gate C] {case}: expected a LOUD crash, but exited 0"
    assert loud, f"[gate C] {case}: crashed but did NOT print the named marker {marker!r}"
    print(f"[gate C] {case} asserted loud OK (rc={cp.returncode}, marker seen)", flush=True)


###############################################################################
def main():
  lev2.lev2appinit(use_subsystems=['opq', 'core', 'lev2'])
  try:
    if "--case" in sys.argv:
      case = sys.argv[sys.argv.index("--case") + 1]
      {"axiom_overflow": _axiom_overflow_case, "nonterminating": _nonterminating_case}[case]()
      return 0  # unreachable if the assert fires (that IS the pass condition for the parent)
    gate_a_determinism()
    gate_b_handcomputable()
    gate_c_budget_stop()
    gate_c_assert_cases()
    print("LRULESET_DERIVE_RESULT=PASS", flush=True)
    return 0
  finally:
    core.coreappexit()


if __name__ == "__main__":
  sys.exit(main())
