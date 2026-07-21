#!/usr/bin/env python3
###############################################################################
# GR1.a gate — LRuleSet grammar-as-data SERIALIZATION round-trip (no GPU baked; pure serdes).
# The keystone GR-1 acceptance: a grammar authored as reflected C++ objects (LExpr / LSymbolDef /
# LTurtleOp / LParamBinding / LRuleDef / LRuleSet) must survive JSON round-trip byte-identical, and
# the LSystemModuleData._grammar directObjectProperty must ride it (nullable — a legacy module with
# NO grammar round-trips unchanged too).
#
# Oracle (mirrors test_hypermesh_reflection.py): js1 = serializeJson(); obj2 = deserializeJson(js1);
# js2 = obj2.serializeJson(); assert js1 == js2 (uuids are stored + read back, so a faithful
# round-trip reproduces them verbatim — no stripping). PLUS: grep js1 for '"class": ""' -> zero hits
# (T1: an untouched sub-object class serializes its class name as "" and null-deserializes SILENTLY).
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys
from orkengine import core   # core before lev2
from orkengine import lev2
from orkengine import ecs    # full class registration (bare imports serialize EMPTY)
from orkengine.core import Object
from orkengine.core import dataflow

hm = lev2.hypermesh

# named int constants — mirror lruleset.h (GR1.c will expose these through the DSL front-end)
CONST, PARAM, ENV, RNG, BINOP, CMP = 0, 1, 2, 3, 4, 5
ADD, SUB, MUL, DIV = 0, 1, 2, 3
LT, LE, GT, GE, EQ, NE = 10, 11, 12, 13, 14, 15
SEGMENT, PITCH, ROLL, YAW, TAPER, SLOT, FORK, CHOOSE, WHEN, CALL = 0, 1, 2, 3, 4, 5, 6, 7, 8, 9


# ---- LExpr builders (fresh nodes every call -> TREE, no sharing/cycles; T6) ----
def konst(v):          return hm.LExpr(kind=CONST, constant=float(v))
def param(name):       return hm.LExpr(kind=PARAM, ref=name)
def binop(op, a, b):   return hm.LExpr(kind=BINOP, op=op, args=[a, b])
def cmp(op, a, b):     return hm.LExpr(kind=CMP, op=op, args=[a, b])
def rng(a, b):         return hm.LExpr(kind=RNG, args=[konst(a), konst(b)])
def bind(k, expr):     return hm.LParamBinding(key=k, value=expr)


def build_grammar():
  # alphabet: two non-terminals with default param environments (string->float map)
  symbols = [
      hm.LSymbolDef(name="A", defaults={"len": 1.0, "gen": 0.0}),
      hm.LSymbolDef(name="B", defaults={"len": 0.5}),
  ]
  # axiom: instantiate A with len bound to an expression (BINOP with two children)
  axiom = [
      hm.LTurtleOp(kind=CALL, symbol="A",
                   params=[bind("len", binop(MUL, konst(1.0), konst(1.0)))]),
  ]
  # rule A -> segment ; CHOOSE{ FORK{pitch, recurse A}, CALL B } ; WHEN(gen>=11){ slot }
  #   guarded by (gen < 8); exercises nested structural ops + guards + weights + param exprs
  rule_a = hm.LRuleDef(
      lhs="A",
      guard=cmp(LT, param("gen"), konst(8)),
      weight=1.0,
      rhs=[
          hm.LTurtleOp(kind=SEGMENT, gid=1,
                       params=[bind("radius", binop(MUL, param("len"), konst(0.1)))]),
          hm.LTurtleOp(kind=CHOOSE,
                       weights=[0.7, 0.3],
                       children=[
                           hm.LTurtleOp(kind=FORK, children=[
                               hm.LTurtleOp(kind=PITCH, params=[bind("angle", konst(25.0))]),
                               hm.LTurtleOp(kind=CALL, symbol="A",
                                            params=[bind("len", binop(MUL, param("len"), konst(0.96)))]),
                           ]),
                           hm.LTurtleOp(kind=CALL, symbol="B",
                                        params=[bind("len", binop(MUL, param("len"), rng(0.4, 0.6)))]),
                       ]),
          hm.LTurtleOp(kind=WHEN,
                       guard=cmp(GE, param("gen"), konst(11)),
                       children=[hm.LTurtleOp(kind=SLOT, gid=5)]),
      ])
  # rule B -> segment ; taper
  rule_b = hm.LRuleDef(
      lhs="B",
      weight=1.0,
      rhs=[
          hm.LTurtleOp(kind=SEGMENT, gid=2),
          hm.LTurtleOp(kind=TAPER, params=[bind("amount", konst(0.8))]),
      ])
  return hm.LRuleSet(symbols=symbols, axiom=axiom, rules=[rule_a, rule_b],
                     depth=6, segment_budget=2048, seed=12345)


def _no_empty_class(js, label):
  # T1: an untouched sub-object class name serializes as "" (both pretty + compact forms guarded)
  assert '"class": ""' not in js and '"class":""' not in js, \
      f"{label}: found '\"class\": \"\"' — a grammar sub-object class is NOT touched in the ClassToucher"


def _roundtrip(obj, label):
  js1 = obj.serializeJson()
  _no_empty_class(js1, label)
  obj2 = Object.deserializeJson(js1)
  assert obj2 is not None, f"{label}: deserializeJson returned null"
  js2 = obj2.serializeJson()
  assert js1 == js2, (f"{label}: round-trip NOT byte-identical ({len(js1)}B vs {len(js2)}B) — "
                      "a grammar field is written-but-not-read or read-but-not-written")
  return js1, obj2


def main():
  # PURE serdes — NO GPU. Class registration runs in Lev2AppInit's ClassToucher (unconditional,
  # not gated on graphics), so a graphics-free lev2appinit registers all six grammar types. The
  # CI node is a headless Mac session where a GPU/COCOA context SIGSEGVs; this test must not open one.
  lev2.lev2appinit(use_subsystems=['opq', 'core', 'lev2'])
  try:
    return _body()
  finally:
    core.coreappexit()   # ALWAYS — a skipped coreappexit hangs teardown + masks the real traceback


def _body():
  # ---- gate (a)+(b): the grammar itself, standalone ----
  grammar = build_grammar()
  js1, rs2 = _roundtrip(grammar, "LRuleSet")
  print(f"LRuleSet JSON bytes={len(js1)}", flush=True)
  # verify the schema actually populated (not an empty stub round-tripping trivially) + the six
  # class names are all present (proves each sub-object serialized WITH its touched class name)
  for needle in ('"hypermesh::LRuleSet"', '"hypermesh::LSymbolDef"', '"hypermesh::LTurtleOp"',
                 '"hypermesh::LParamBinding"', '"hypermesh::LRuleDef"', '"hypermesh::LExpr"'):
    assert needle in js1, f"expected class {needle} missing from grammar JSON (unregistered?)"
  # deep field survival: navigate the deserialized tree
  assert len(rs2.symbols) == 2 and rs2.symbols[0].name == "A"
  assert abs(rs2.symbols[0].defaults["len"] - 1.0) < 1e-6
  assert rs2.depth == 6 and rs2.segment_budget == 2048 and rs2.seed == 12345
  assert len(rs2.rules) == 2
  ra = rs2.rules[0]
  assert ra.lhs == "A" and ra.guard is not None and ra.guard.kind == CMP and ra.guard.op == LT
  choose = ra.rhs[1]
  assert choose.kind == CHOOSE and abs(choose.weights[0] - 0.7) < 1e-6
  fork = choose.children[0]
  assert fork.kind == FORK and len(fork.children) == 2
  recur = fork.children[1]
  assert recur.kind == CALL and recur.symbol == "A"
  assert recur.params[0].key == "len" and recur.params[0].value.kind == BINOP
  when = ra.rhs[2]
  assert when.kind == WHEN and when.guard is not None and len(when.children) == 1
  print("grammar deep-field survival: OK", flush=True)

  # ---- module integration: LSystemModuleData WITH grammar (inside a GraphData, the proven path) ----
  g = dataflow.GraphData.createShared()
  ls = hm.LSystemModule.createShared()
  ls.grammar = build_grammar()
  g.addModule(ls, "lsystem_grammar")
  jsg, g2 = _roundtrip(g, "LSystemModule+grammar")
  ls2 = g2.findModule("lsystem_grammar")
  assert ls2 is not None and ls2.grammar is not None, "grammar lost through module round-trip"
  assert len(ls2.grammar.rules) == 2, "grammar rules lost through module round-trip"
  print(f"LSystemModule+grammar JSON bytes={len(jsg)}", flush=True)

  # ---- gate (e): a module with NO grammar still SERIALIZES null-safely (activation asserts
  #      loud post-GR1.d, but serdes of a null grammar must not crash or fabricate one) ----
  gl = dataflow.GraphData.createShared()
  lsl = hm.LSystemModule.createShared()   # _grammar stays null (serdes-only; never activated)
  lsl.depth = 7
  lsl.children = 3
  gl.addModule(lsl, "lsystem_nogrammar")
  jsl, gl2 = _roundtrip(gl, "LSystemModule-nogrammar")
  lsl2 = gl2.findModule("lsystem_nogrammar")
  assert lsl2 is not None and lsl2.grammar is None, "null grammar did not survive as null"
  assert lsl2.children == 3 and lsl2.depth == 7, "scalar params drifted"
  print(f"LSystemModule-nogrammar JSON bytes={len(jsl)}", flush=True)

  print("LRULESET_ROUNDTRIP_RESULT=PASS", flush=True)
  return 0


if __name__ == "__main__":
  sys.exit(main())
