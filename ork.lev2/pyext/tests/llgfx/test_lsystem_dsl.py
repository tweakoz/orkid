#!/usr/bin/env python3
###############################################################################
# GR1.c gates — the Python DSL front-end for the reflected LRuleSet grammar. PURE CPU (no GPU):
# authoring lowers to the six reflected types the C++ evaluator runs; class registration is in
# Lev2AppInit's (graphics-free) ClassToucher, so lev2appinit(use_subsystems=['opq','core','lev2'])
# suffices — the CI node is a headless Mac session where a GPU/COCOA context SIGSEGVs.
#
# Gates (JUL05_GR1.md §7 GR1.c):
#   A. combinator == string  — DesertBroom authored BOTH ways -> IDENTICAL uuid-stripped LRuleSet JSON.
#   B. __bool__ trap         — a rule body with `if S.gen > 3:` raises LSystemAuthorError naming the rule.
#   D. determinism           — the cholla grammar derives byte-identically twice (reuse the GR1.b seam),
#                              and hypermeshModuleIdentityHash is stable.
#   (+ combinator smoke: the cholla + DesertBroom lower to a sane reflected LRuleSet shape.)
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import re, sys
from orkengine import core   # core before lev2
from orkengine import lev2
from orkengine import ecs    # full class registration (bare imports serialize EMPTY)

from ork.hypergraph.dflow import lsystem as L
from ork.hypergraph.dflow.lsystem.examples import CaneCholla, DesertBroomString, DesertBroomCombinator

hm = lev2.hypermesh

_UUID = re.compile(r'("uuid"\s*:\s*")[^"]*(")')


def _strip_uuids(js):
  # normalize every per-object uuid VALUE to "*": two independently-built grammars mint DIFFERENT
  # uuids, so structural (grammar) equality is the uuid-stripped byte comparison (== the cook hash's).
  return _UUID.sub(r'\1*\2', js)


###############################################################################
# Gate A — combinator vs string dual form: identical uuid-stripped JSON.
###############################################################################
def gate_a_dualform():
  gs = DesertBroomString().derive()
  gc = DesertBroomCombinator().derive()
  js_s = _strip_uuids(gs.serializeJson())
  js_c = _strip_uuids(gc.serializeJson())
  if js_s != js_c:
    # print the first divergence to make a mismatch debuggable
    for i, (a, b) in enumerate(zip(js_s.splitlines(), js_c.splitlines())):
      if a != b:
        print("  string  : %s" % a, flush=True)
        print("  combinat: %s" % b, flush=True)
        print("  (line %d)" % i, flush=True)
        break
    raise AssertionError("[gate A] string-form and combinator-form JSON differ (%dB vs %dB)"
                         % (len(js_s), len(js_c)))
  # not-vacuous: the grammar actually populated (one rule, the fork/segment structure survived)
  assert len(gc.rules) == 1 and gc.rules[0].lhs == "A", "DesertBroom didn't lower to one rule 'A'"
  assert len(gc.rules[0].rhs) == 6, f"DesertBroom rule A should have 6 ops, got {len(gc.rules[0].rhs)}"
  assert '"class": ""' not in gs.serializeJson(), "empty class name in string-form JSON (untouched class)"
  print(f"[gate A] dual-form identical uuid-stripped JSON OK ({len(js_c)}B, 6-op rule A)", flush=True)


###############################################################################
# Gate B — the serializable-tree trap: a python `if S.x > 3:` in a production raises LOUD (T4).
###############################################################################
def gate_b_bool_trap():

  class BadGrammar(L.Lsystem):
    def grammar(self):
      Shoot = self.symbol("Shoot", gen=0)

      @L.rule(Shoot)
      def grow(S):
        if S.gen > 3:            # <-- the LOAD-BEARING authoring error: an expr in a python `if`
          return [L.bloom()]
        return [L.segment(len=0.1)]

      self.axiom(Shoot())
      self.iterate(depth=3, segment_budget=10)

  raised = None
  try:
    BadGrammar().derive()
  except L.LSystemAuthorError as e:
    raised = e
  assert raised is not None, "[gate B] `if S.gen > 3:` did NOT raise — the __bool__ trap is broken"
  msg = str(raised)
  assert "grow" in msg, f"[gate B] the trap message must name the offending rule 'grow', got: {msg!r}"
  # the sibling traps (list(expr) / for x in expr) also fire
  for trap in (lambda: bool(L.Expr.param("x") < 1),
               lambda: list(L.Expr.param("x")),
               lambda: len(L.Expr.param("x"))):
    try:
      trap(); ok = False
    except L.LSystemAuthorError:
      ok = True
    assert ok, "[gate B] an Expr python-truthiness/iteration/len trap did not fire"
  print("[gate B] __bool__/__iter__/__len__ traps fire loud (naming the rule) OK", flush=True)


###############################################################################
# Gate D — determinism: the cholla derives byte-identically twice + stable identity hash.
###############################################################################
def _cholla_env():
  m = hm.LSystemModule.createShared()
  m.base_radius = 0.020
  m.seg_len     = 0.16
  m.jitter      = 0.12
  m.jit_azimuth = 0.6
  m.jit_pitch   = 0.35
  m.tropism     = 0.02
  return m


def gate_d_determinism():
  g = CaneCholla().derive()
  env = _cholla_env()
  r1 = hm._deriveLRuleSet(g, env)
  r2 = hm._deriveLRuleSet(g, env)
  assert r1["count"] > 20, f"[gate D] cholla barely grew ({r1['count']} nodes) — determinism gate is vacuous"
  assert r1["bytes"] == r2["bytes"], "[gate D] cholla derive NON-deterministic: node buffers differ (memcmp)"
  assert r1["parents"] == r2["parents"], "[gate D] cholla derive NON-deterministic: parent topology differs"
  # spine + flower SLOT ops were emitted (GR-2 consumes; GR-1 proves they ride the derive)
  assert len(r1["slots"]) > 0, "[gate D] cholla emitted no XfSlots (spines/blooms lost)"
  # identity hash (the cook-cache surface) stable across two independently-derived+attached grammars
  m1 = hm.LSystemModule.createShared(); m1.grammar = CaneCholla().derive()
  m2 = hm.LSystemModule.createShared(); m2.grammar = CaneCholla().derive()
  h1, h2 = hm._moduleIdentityHash(m1), hm._moduleIdentityHash(m2)
  assert h1 == h2, f"[gate D] identity hash unstable across re-derives: {h1} != {h2}"
  print(f"[gate D] determinism OK: {r1['count']} nodes, {len(r1['slots'])} slots, memcmp x2, identityHash={h1}", flush=True)


###############################################################################
# combinator smoke — the cholla lowers to a sane reflected shape (1 symbol, 2 rules, guarded).
###############################################################################
def smoke_cholla_shape():
  g = CaneCholla().derive()
  assert len(g.symbols) == 1 and g.symbols[0].name == "Shoot"
  assert abs(g.symbols[0].defaults["len"] - 0.16) < 1e-6
  assert len(g.rules) == 2, f"cholla should have grow+cap rules, got {len(g.rules)}"
  grow, cap = g.rules
  assert grow.lhs == "Shoot" and grow.guard is not None and grow.guard.kind == L.CMP
  assert cap.guard is not None and cap.guard.kind == L.CMP
  # the grow rhs: segment ; spines(slot) ; choose(3)
  seg, spn, cho = grow.rhs
  assert seg.kind == L.SEGMENT and spn.kind == L.SLOT and cho.kind == L.CHOOSE
  assert len(cho.children) == 3 and abs(cho.weights[0] - 0.50) < 1e-6
  # gids sorted-by-name within the grammar: cane=1, flower=2, spine=3
  assert seg.gid == 1, f"cane gid should be 1 (sorted), got {seg.gid}"
  assert g.depth == 11 and g.segment_budget == 700
  # resolve_grammar accepts a class, an instance, and a raw LRuleSet
  assert isinstance(L.resolve_grammar(CaneCholla), hm.LRuleSet)
  assert isinstance(L.resolve_grammar(CaneCholla()), hm.LRuleSet)
  assert isinstance(L.resolve_grammar(g), hm.LRuleSet)
  print("[smoke] cholla reflected shape OK (1 symbol, grow+cap, choose(3), gids sorted)", flush=True)


###############################################################################
def main():
  lev2.lev2appinit(use_subsystems=['opq', 'core', 'lev2'])
  try:
    smoke_cholla_shape()
    gate_a_dualform()
    gate_b_bool_trap()
    gate_d_determinism()
    print("LSYSTEM_DSL_RESULT=PASS", flush=True)
    return 0
  finally:
    core.coreappexit()


if __name__ == "__main__":
  sys.exit(main())
