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
#
# ENUM canaries (kind/op are reflected enums serialized BY NAME):
#   (d) a 3-deep nested-op-array grammar exercising EVERY LTurtleKind / LExprKind / LExprOp value
#       round-trips byte-identical with every selector recovered, and the json carries the NAMES.
#   (e) an UNREGISTERED kind int fails LOUD at serialize time (crash subprocess — the codec must
#       never write a silent garbage/empty selector).
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys, subprocess
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


###############################################################################
# gate (d) — every selector value, nested 3+ deep in the op arrays
###############################################################################
_TURTLE_NAMES = {SEGMENT: "segment", PITCH: "pitch", ROLL: "roll", YAW: "yaw", TAPER: "taper",
                 SLOT: "slot", FORK: "fork", CHOOSE: "choose", WHEN: "when", CALL: "call"}
_EXPRKIND_NAMES = {CONST: "const", PARAM: "param", ENV: "env", RNG: "rng", BINOP: "binop", CMP: "cmp"}
_EXPROP_NAMES = {ADD: "add", SUB: "sub", MUL: "mul", DIV: "div",
                 LT: "lt", LE: "le", GT: "gt", GE: "ge", EQ: "eq", NE: "ne"}


def build_deep_grammar():
  """A -> ... FORK{ CHOOSE{ WHEN{ SEGMENT } , CALL } , ... } : op-array nesting 4 levels deep,
  carrying every LTurtleKind and (through the params/guards) every LExprKind + LExprOp."""
  cmps = [cmp(op, param("gen"), konst(i)) for i, op in enumerate((LT, LE, GT, GE, EQ, NE))]
  bins = [binop(op, param("len"), konst(0.5)) for op in (ADD, SUB, MUL, DIV)]
  deep = hm.LTurtleOp(kind=FORK, params=[bind("count", konst(2))], children=[      # depth 1
      hm.LTurtleOp(kind=CHOOSE, weights=[0.5, 0.5], children=[                     # depth 2
          hm.LTurtleOp(kind=WHEN, guard=cmps[0], children=[                        # depth 3
              hm.LTurtleOp(kind=SEGMENT, gid=3, params=[bind("rad", bins[0])]),    # depth 4
              hm.LTurtleOp(kind=SLOT, gid=4),
          ]),
          hm.LTurtleOp(kind=CALL, symbol="A", params=[bind("len", bins[1])]),
      ]),
      hm.LTurtleOp(kind=TAPER, params=[bind("amount", hm.LExpr(kind=ENV, ref="moisture"))]),
  ])
  rhs = [
      hm.LTurtleOp(kind=SEGMENT, gid=1, params=[bind("len", bins[2])]),
      hm.LTurtleOp(kind=PITCH, params=[bind("angle", rng(10.0, 20.0))]),
      hm.LTurtleOp(kind=ROLL, params=[bind("angle", bins[3])]),
      hm.LTurtleOp(kind=YAW, params=[bind("angle", konst(15.0))]),
      deep,
      hm.LTurtleOp(kind=WHEN, guard=cmps[1], children=[hm.LTurtleOp(kind=SEGMENT, gid=2)]),
  ]
  # the remaining CMP ops ride as rule guards so every LExprOp value is present somewhere
  rules = [hm.LRuleDef(lhs="A", guard=cmps[2], weight=1.0, rhs=rhs)]
  for i, c in enumerate(cmps[3:]):
    rules.append(hm.LRuleDef(lhs="A", guard=c, weight=0.5,
                             rhs=[hm.LTurtleOp(kind=SEGMENT, gid=10 + i)]))
  return hm.LRuleSet(symbols=[hm.LSymbolDef(name="A", defaults={"len": 1.0, "gen": 0.0})],
                     axiom=[hm.LTurtleOp(kind=CALL, symbol="A")],
                     rules=rules, depth=4, segment_budget=512, seed=7)


def _walk_expr(e, kinds, ops):
  if e is None:
    return
  kinds.add(e.kind)
  if e.kind in (BINOP, CMP):
    ops.add(e.op)
  for a in e.args:
    _walk_expr(a, kinds, ops)


def _walk_op(op, tkinds, kinds, ops, depth, maxdepth):
  tkinds.add(op.kind)
  maxdepth[0] = max(maxdepth[0], depth)
  _walk_expr(op.guard, kinds, ops)
  for p in op.params:
    _walk_expr(p.value, kinds, ops)
  for c in op.children:
    _walk_op(c, tkinds, kinds, ops, depth + 1, maxdepth)


def gate_deep_enums():
  g = build_deep_grammar()
  js1, g2 = _roundtrip(g, "LRuleSet-deep")
  tkinds, kinds, ops, maxdepth = set(), set(), set(), [0]
  for op in g2.axiom:
    _walk_op(op, tkinds, kinds, ops, 1, maxdepth)
  for r in g2.rules:
    _walk_expr(r.guard, kinds, ops)
    for op in r.rhs:
      _walk_op(op, tkinds, kinds, ops, 1, maxdepth)
  assert maxdepth[0] >= 4, f"deep grammar op nesting only {maxdepth[0]} levels (expected >= 4)"
  assert tkinds == set(_TURTLE_NAMES), f"turtle kinds lost through round-trip: missing {set(_TURTLE_NAMES) - tkinds}"
  assert kinds == set(_EXPRKIND_NAMES), f"expr kinds lost through round-trip: missing {set(_EXPRKIND_NAMES) - kinds}"
  assert ops == set(_EXPROP_NAMES), f"expr ops lost through round-trip: missing {set(_EXPROP_NAMES) - ops}"
  # the wire form is the registered NAME (not the int) — one spelling for json/propsheet/emitters
  for field, table in (("kind", _TURTLE_NAMES), ("kind", _EXPRKIND_NAMES), ("op", _EXPROP_NAMES)):
    for name in table.values():
      assert f'"{field}": "{name}"' in js1 or f'"{field}":"{name}"' in js1, \
          f'expected enum NAME "{field}": "{name}" in the grammar JSON (int-coded selector?)'
  print(f"deep-nested enum survival: OK (depth={maxdepth[0]}, {len(tkinds)} kinds, {len(ops)} ops)", flush=True)


###############################################################################
# gate (e) — an unregistered selector int must CRASH LOUD at serialize (never silently written)
###############################################################################
def _unregistered_kind_case():
  bad = hm.LTurtleOp(kind=99, gid=1)   # 99 is not an LTurtleKind enumerator
  g = hm.LRuleSet(symbols=[hm.LSymbolDef(name="A")], axiom=[bad], rules=[], depth=1)
  g.serializeJson()                    # expected: LOUD "NOT a registered enumerator" + crash


def gate_unregistered_kind():
  cp = subprocess.run([sys.executable, os.path.abspath(__file__), "--case", "unregistered_kind"],
                      capture_output=True, timeout=120)
  marker = b"NOT a registered"
  assert cp.returncode != 0, "[gate e] unregistered kind serialized WITHOUT crashing (silent garbage selector)"
  assert marker in cp.stderr or marker in cp.stdout, \
      f"[gate e] crashed but did NOT print the named marker {marker!r}"
  print(f"[gate e] unregistered kind asserted loud OK (rc={cp.returncode}, marker seen)", flush=True)


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
    if "--case" in sys.argv:
      case = sys.argv[sys.argv.index("--case") + 1]
      {"unregistered_kind": _unregistered_kind_case}[case]()
      return 0  # unreachable if the assert fires (that IS the pass condition for the parent)
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

  # ---- gate (d)+(e): the reflected-enum selectors ----
  gate_deep_enums()
  gate_unregistered_kind()

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
