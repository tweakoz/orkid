#!/usr/bin/env python3
###############################################################################
# G5 gate — the grammar CORE is family-neutral (ork::grammar in ork.core).
#
# The LRuleSet schema + the rewrite pass were lifted out of lev2's hypermesh into ork.core so a
# second consumer (the audio family) can share them. This gate proves the extraction is REAL, not
# cosmetic, on two independent axes:
#
#   A. VOCABULARY-FREE grammars work. A toy grammar built from CONTROL ops only (CALL / CHOOSE /
#      WHEN + params + symbol defaults, ZERO turtle/vocabulary ops) round-trips byte-identical and
#      DERIVES: the rewrite resolves the whole control chain and emits no vocabulary op at all
#      (derive returns the bare root node, zero slots). A breadth-doubling WHEN/CHOOSE/CALL grammar
#      then fires the core op-cap assert LOUD — proof the control machinery really expanded, rather
#      than dropping the ops on the floor and returning an empty stream.
#   B. LINK NEUTRALITY. The compiled ork.core grammar objects name no lev2/gfx symbol: their
#      UNDEFINED-symbol lists (nm -u) are grepped for lev2/hypermesh/hyper. Backed by a source scan
#      of the ork/grammar headers + TUs for any lev2 include or namespace token, so the gate still
#      says something precise when only sources are at hand.
#   C. A SECOND VOCABULARY. A family beyond mesh registers its own op alphabet — private codes, its
#      own COUNTED op — entirely from here, with NO ork.core edit, and a grammar written in that
#      alphabet round-trips by name and derives through the core rewrite with exact counting
#      (including the budget stop). This is the MusicGrammar reusability claim, executed.
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys, glob, subprocess
from pathlib import Path
from orkengine import core   # core before lev2
from orkengine import lev2
from orkengine import ecs    # full class registration
from orkengine.core import Object

hm = lev2.hypermesh

# named int constants — expression selectors mirror ork/grammar/lruleset.h; op codes 6..9 are core's
# CONTROL ops (ork/grammar/vocabulary.h) and 0..5 the mesh family's registered alphabet (lev2 LMeshOp).
CONST, PARAM, ENV, RNG, BINOP, CMP = 0, 1, 2, 3, 4, 5
ADD, SUB, MUL, DIV = 0, 1, 2, 3
LT, LE, GT, GE, EQ, NE = 10, 11, 12, 13, 14, 15
SEGMENT, PITCH, ROLL, YAW, TAPER, SLOT, FORK, CHOOSE, WHEN, CALL = 0, 1, 2, 3, 4, 5, 6, 7, 8, 9

# the ops the MESH family owns. NONE of them may appear in gates A/C.
VOCABULARY_KINDS = {SEGMENT, PITCH, ROLL, YAW, TAPER, SLOT}

REPO = Path(__file__).resolve().parents[4]


def konst(v):        return hm.LExpr(kind=CONST, constant=float(v))
def param(name):     return hm.LExpr(kind=PARAM, ref=name)
def binop(op, a, b): return hm.LExpr(kind=BINOP, op=op, args=[a, b])
def cmp(op, a, b):   return hm.LExpr(kind=CMP, op=op, args=[a, b])
def bind(k, e):      return hm.LParamBinding(key=k, value=e)


def _env(**kw):
  m = hm.LSystemModule.createShared()
  for k, v in kw.items():
    setattr(m, k, v)
  return m


def _assert_no_vocabulary(ops, where):
  # a vocabulary op sneaking in would make gate A vacuous (the turtle, not the core, did the work)
  for op in ops:
    assert op.kind not in VOCABULARY_KINDS, f"{where}: vocabulary op kind={op.kind} in a control-only grammar"
    _assert_no_vocabulary(op.children, where)


###############################################################################
# Gate A1 — a control-only grammar round-trips through the core schema
###############################################################################
def _control_only_grammar(depth, guard_bound):
  # alphabet A/B with default param envs; A recurses through CHOOSE{ CALL A , CALL B } under a
  # WHEN(gen < guard_bound) gate, B terminates. Every op is CALL / CHOOSE / WHEN.
  symbols = [
      hm.LSymbolDef(name="A", defaults={"gen": 0.0, "mass": 1.0}),
      hm.LSymbolDef(name="B", defaults={"gen": 0.0}),
  ]
  axiom = [hm.LTurtleOp(kind=CALL, symbol="A", params=[bind("mass", konst(3.0))])]
  recur = hm.LTurtleOp(kind=CALL, symbol="A", params=[
      bind("gen",  binop(ADD, param("gen"), konst(1.0))),
      bind("mass", binop(MUL, param("mass"), konst(0.5))),
  ])
  rule_a = hm.LRuleDef(lhs="A", guard=cmp(LT, param("gen"), konst(float(guard_bound))), weight=1.0, rhs=[
      hm.LTurtleOp(kind=WHEN, guard=cmp(GT, param("mass"), konst(0.01)), children=[
          hm.LTurtleOp(kind=CHOOSE, weights=[0.6, 0.4],
                       children=[recur, hm.LTurtleOp(kind=CALL, symbol="B")]),
      ]),
  ])
  rule_b = hm.LRuleDef(lhs="B", weight=1.0, rhs=[])  # terminal: produces nothing
  return hm.LRuleSet(symbols=symbols, axiom=axiom, rules=[rule_a, rule_b],
                     depth=depth, segment_budget=64, seed=20260727)


def gate_a1_roundtrip():
  g = _control_only_grammar(depth=6, guard_bound=4)
  _assert_no_vocabulary(g.axiom, "axiom")
  for r in g.rules:
    _assert_no_vocabulary(r.rhs, f"rule {r.lhs}")
  js1 = g.serializeJson()
  assert '"class": ""' not in js1 and '"class":""' not in js1, \
      "found '\"class\": \"\"' — a grammar sub-object class is not touched in the core ClassToucher"
  g2 = Object.deserializeJson(js1)
  assert g2 is not None, "control-only grammar: deserializeJson returned null"
  js2 = g2.serializeJson()
  assert js1 == js2, f"control-only grammar round-trip NOT byte-identical ({len(js1)}B vs {len(js2)}B)"
  # the schema classes are registered from ork.core now — the saved names are unchanged (data contract)
  for needle in ('"hypermesh::LRuleSet"', '"hypermesh::LSymbolDef"', '"hypermesh::LTurtleOp"',
                 '"hypermesh::LParamBinding"', '"hypermesh::LRuleDef"', '"hypermesh::LExpr"'):
    assert needle in js1, f"expected class {needle} missing from the grammar JSON (unregistered?)"
  # no vocabulary op means no vocabulary enum NAME may appear in the wire form either
  for name in ("segment", "pitch", "roll", "yaw", "taper", "slot"):
    assert f'"kind": "{name}"' not in js1, f'vocabulary op "{name}" leaked into a control-only grammar'
  for name in ("call", "choose", "when"):
    assert f'"kind": "{name}"' in js1, f'control op "{name}" missing from the grammar JSON'
  assert g2.symbols[0].name == "A" and abs(g2.symbols[0].defaults["mass"] - 1.0) < 1e-6
  assert g2.rules[0].rhs[0].kind == WHEN and g2.rules[0].rhs[0].children[0].kind == CHOOSE
  print(f"[gate A1] control-only round-trip OK ({len(js1)}B, no vocabulary op)", flush=True)


###############################################################################
# Gate A2 — a control-only grammar DERIVES (core rewrite resolves it; nothing is emitted)
###############################################################################
def gate_a2_derive():
  env = _env(jitter=0.0, tropism=0.0, seg_len=0.5, base_radius=0.05)
  r = hm._deriveLRuleSet(_control_only_grammar(depth=6, guard_bound=4), env)
  assert r["count"] == 1, f"control-only grammar emitted {r['count']} nodes (expected only the root)"
  assert list(r["slots"]) == [], f"control-only grammar emitted slots: {list(r['slots'])}"
  print("[gate A2] control-only derive OK: root node only, zero slots", flush=True)


###############################################################################
# Gate A3 — the control machinery really expands (unbounded control-only -> loud op cap)
###############################################################################
def _doubling_control_grammar():
  # A -> WHEN(true){ CHOOSE[1,0]{ CALL A , CALL A } } , CALL A    (breadth-doubling, zero vocabulary
  # ops, and it routes through ALL THREE control ops). Nothing can ever hit segment_budget, so the
  # only thing that can stop it is the core rewrite's 64x op cap.
  recur = lambda: hm.LTurtleOp(kind=CALL, symbol="A")
  rule_a = hm.LRuleDef(lhs="A", weight=1.0, rhs=[
      hm.LTurtleOp(kind=WHEN, guard=cmp(GT, konst(1.0), konst(0.0)), children=[
          hm.LTurtleOp(kind=CHOOSE, weights=[1.0, 0.0], children=[recur(), recur()]),
      ]),
      recur(),
  ])
  return hm.LRuleSet(symbols=[hm.LSymbolDef(name="A")], axiom=[recur()], rules=[rule_a],
                     depth=30, segment_budget=64, seed=7)


def _nonterminating_control_case():
  hm._deriveLRuleSet(_doubling_control_grammar(), _env())  # expected: LOUD assert + crash


def gate_a3_expansion_is_real():
  cp = subprocess.run([sys.executable, os.path.abspath(__file__), "--case", "nonterminating_control"],
                      capture_output=True, timeout=120)
  marker = b"NON-TERMINATING"
  assert cp.returncode != 0, "[gate A3] unbounded WHEN/CHOOSE/CALL grammar did NOT crash — the core pass " \
                             "silently dropped the control ops instead of expanding them"
  assert marker in cp.stderr or marker in cp.stdout, \
      f"[gate A3] crashed but did NOT print the named marker {marker!r}"
  print(f"[gate A3] control expansion is real OK (rc={cp.returncode}, op-cap marker seen)", flush=True)


###############################################################################
# Gate C — a SECOND vocabulary, registered test-side, derives through core
#
# The toy alphabet stands in for the audio family: TOY_BEAT is the counted op (audio's NOTE),
# TOY_REST is not. Codes live in a private band; core knows neither of them at compile time.
###############################################################################
TOY_BEAT, TOY_REST = 4200, 4201


def _toy_grammar(budget):
  # M(n) -> beat(dur = 0.5*(n+1)) rest  WHEN(n < 3){ M(n+1) }.  No RNG anywhere: the derived stream
  # is a closed form — M instantiates at n=0,1,2,3 => 4 beats + 4 rests, 4 counted.
  recur = hm.LTurtleOp(kind=CALL, symbol="M", params=[bind("n", binop(ADD, param("n"), konst(1.0)))])
  rule = hm.LRuleDef(lhs="M", weight=1.0, rhs=[
      hm.LTurtleOp(kind=TOY_BEAT, params=[bind("dur", binop(MUL, konst(0.5),
                                                            binop(ADD, param("n"), konst(1.0))))]),
      hm.LTurtleOp(kind=TOY_REST),
      hm.LTurtleOp(kind=WHEN, guard=cmp(LT, param("n"), konst(3.0)), children=[recur]),
  ])
  return hm.LRuleSet(symbols=[hm.LSymbolDef(name="M", defaults={"n": 0.0})],
                     axiom=[hm.LTurtleOp(kind=CALL, symbol="M", params=[bind("n", konst(0.0))])],
                     rules=[rule], depth=8, segment_budget=budget, seed=1)


def gate_c_second_vocabulary():
  vid = hm._grammarRegisterVocabulary("toytest", [("toy_beat", TOY_BEAT, True),
                                                  ("toy_rest", TOY_REST, False)])
  assert vid not in (0, 0xffffffff), f"toy vocabulary got a bogus id {vid}"
  # a vocabulary is fixed at first registration; re-registering the same alphabet is a no-op
  assert hm._grammarRegisterVocabulary("toytest", [("toy_beat", TOY_BEAT, True),
                                                   ("toy_rest", TOY_REST, False)]) == vid, \
      "re-registering an identical vocabulary did not return the same id"

  # -- serdes: the toy ops go out by NAME through the same reflected scalar the mesh ops use
  g = _toy_grammar(budget=64)
  js1 = g.serializeJson()
  assert '"class": ""' not in js1, "a grammar sub-object class is not touched in the core ClassToucher"
  js2 = Object.deserializeJson(js1).serializeJson()
  assert js1 == js2, f"toy-vocabulary grammar round-trip NOT byte-identical ({len(js1)}B vs {len(js2)}B)"
  for name in ('"kind": "toy_beat"', '"kind": "toy_rest"'):
    assert name in js1, f"{name} missing — the toy vocabulary did not reach the LOpCode name table"
  for name in ("segment", "pitch", "roll", "yaw", "taper", "slot"):
    assert f'"kind": "{name}"' not in js1, f'mesh op "{name}" leaked into a toy-vocabulary grammar'

  # -- derive: core resolves the control chain and counts the toy COUNTED op, no mesh anything
  r = hm._grammarDerive(g)
  kinds = [op["kind"] for op in r["ops"]]
  assert kinds == [TOY_BEAT, TOY_REST] * 4, f"toy derive produced {kinds}"
  assert all(op["vocab"] == vid for op in r["ops"]), "resolved toy ops carry the wrong vocabulary id"
  assert r["counted"] == 4 and not r["budget_hit"], f"toy derive counted={r['counted']} hit={r['budget_hit']}"
  durs = [op["params"]["dur"] for op in r["ops"] if op["kind"] == TOY_BEAT]
  assert durs == [0.5, 1.0, 1.5, 2.0], f"toy param env did not flow through the CALL chain: {durs}"

  # -- the budget is the vocabulary's counted op, not SEGMENT: budget=2 stops after 2 beats, and the
  #    rest that follows the rejected beat still emits (only non-terminal scheduling halts).
  r2 = hm._grammarDerive(_toy_grammar(budget=2))
  assert [op["kind"] for op in r2["ops"]] == [TOY_BEAT, TOY_REST, TOY_BEAT, TOY_REST, TOY_REST], \
      f"toy budget-stop stream wrong: {[op['kind'] for op in r2['ops']]}"
  assert r2["counted"] == 2 and r2["budget_hit"], f"toy budget-stop counted={r2['counted']} hit={r2['budget_hit']}"
  print(f"[gate C] second vocabulary OK: id={vid}, {len(js1)}B round-trip, 4 counted / budget-stop at 2",
        flush=True)


###############################################################################
# Gate B — link neutrality of the compiled ork.core grammar TUs
###############################################################################
_FORBIDDEN_TOKENS = ("lev2", "hypermesh", "ork6hyper", "ork5hyper")


def gate_b_source_neutrality():
  srcs = sorted(glob.glob(str(REPO / "ork.core/inc/ork/grammar/*.h")) +
                glob.glob(str(REPO / "ork.core/src/grammar/*.cpp")))
  assert len(srcs) >= 4, f"expected the grammar-core headers + TUs under {REPO}, found {srcs}"
  for path in srcs:
    for lineno, line in enumerate(open(path).read().splitlines(), 1):
      code = line.split("//")[0]
      # NOT forbidden: the "hypermesh::LExpr" style SAVED CLASS NAMES. Those are string literals —
      # the on-disk contract of every already-serialized grammar — not a dependency on lev2.
      for tok in ("ork/lev2", "ork::lev2", "ork::hyper", "#include <ork/gfx"):
        assert tok not in code, f"{path}:{lineno}: grammar-core source names '{tok}' — not family-neutral"
  print(f"[gate B1] grammar-core sources are lev2-free ({len(srcs)} files)", flush=True)


def gate_b_link_neutrality():
  builddir = Path(os.environ.get("ORKID_BUILD_DIR", str(REPO / ".build")))
  objs = sorted(glob.glob(str(builddir / "**/ork_core.dir/src/grammar/*.o"), recursive=True))
  assert objs, (f"no compiled grammar objects under {builddir} — build first, or point ORKID_BUILD_DIR "
                "at the build tree (the link-neutrality proof needs the objects)")
  total_undef = 0
  for obj in objs:
    cp = subprocess.run(["nm", "-u", obj], capture_output=True, text=True, timeout=120)
    assert cp.returncode == 0, f"nm -u failed on {obj}: {cp.stderr.strip()}"
    lines = [l.strip() for l in cp.stdout.splitlines() if l.strip()]
    assert lines, f"nm -u produced NO undefined symbols for {obj} — the scan would pass vacuously"
    total_undef += len(lines)
    for l in lines:
      for tok in _FORBIDDEN_TOKENS:
        assert tok not in l, f"{Path(obj).name}: undefined symbol references '{tok}': {l}"
  print(f"[gate B2] link neutrality OK: {len(objs)} objects, {total_undef} undefined symbols, "
        "none naming lev2/hypermesh/hyper", flush=True)


###############################################################################
def main():
  # PURE serdes + CPU derive, NO GPU (the CI node is a headless Mac session where a GPU/COCOA
  # context SIGSEGVs). Registration of the six grammar classes now runs in ork::CoreAppInit,
  # which lev2appinit invokes first.
  lev2.lev2appinit(use_subsystems=['opq', 'core', 'lev2'])
  try:
    if "--case" in sys.argv:
      case = sys.argv[sys.argv.index("--case") + 1]
      {"nonterminating_control": _nonterminating_control_case}[case]()
      return 0  # unreachable if the assert fires (that IS the pass condition for the parent)
    gate_a1_roundtrip()
    gate_a2_derive()
    gate_a3_expansion_is_real()
    gate_c_second_vocabulary()
    gate_b_source_neutrality()
    gate_b_link_neutrality()
    print("GRAMMAR_CORE_NEUTRAL_RESULT=PASS", flush=True)
    return 0
  finally:
    core.coreappexit()


if __name__ == "__main__":
  sys.exit(main())
