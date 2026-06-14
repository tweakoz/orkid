#!/usr/bin/env python3
###############################################################################
# A.1 gate — hypermesh op-state REFLECTION round-trip (no GPU; pure serialization).
# Builds a graph exercising every op whose authored state was pyext-only (and therefore
# silently RESET on reload): Select (predicate GLSL + 6 MaskOp uints + domain), Transform
# (matrix), ExtrudeFaces (5 predicate GLSL strings + expr_params + part_masks + segments),
# Inset (part_masks), Box/primitives (mask), BitOp (dst/a/b/op/width), SortTest (n).
# Asserts: (1) the serialized JSON CONTAINS each authored value; (2) field-level equality
# after Object.deserializeJson (via the pyext getters); (3) serialize(deserialize(js)) == js
# (idempotence — catches fields written but not read and vice versa).
# Companion of the terrain precedent test_terrain_asset.py; the A.2 BAKE-compare gate
# builds on this.
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
os.environ["ORK_DFLOW_ENFORCE_TYPED_CONNECT"] = "1"   # gates ENFORCE typed connections (runtime default = WARN)
import sys
from orkengine import core   # core before lev2
from orkengine import lev2
from orkengine import ecs    # headless_appinit (full class registration — bare imports serialize EMPTY)
from orkengine.core import Object, vec3

from ork.hypergraph.dflow.hypermesh import (Hypermesh, S, isolate, group, POLY, param, vexpr)


class A1Asset(Hypermesh):
  def __init__(self):
    super().__init__()
    self._amp = param("amp", 0.25)                       # -> extrude expr_params vec4 slot
    n = self.box(size=1.0, mask=isolate(group(7)))       # generator whole-mesh mask triple
    n = self.select(n, (S.N.dot(vec3(0, 1, 0)) > 0.6) & ~S.tag(3),
                    domain=POLY, op=isolate(group(2)))   # predicate GLSL + both MaskOp triples
    n = self.transform(n, translate=(1.0, 2.0, 3.0), slot=2)
    n = self.extrude_faces(n,
                           distance=S.t * -0.15 + self._amp,
                           segments=3,
                           twist=0.5 * S.t,
                           scale=1.0 - 0.3 * S.t - 0.01 * S.time,   # S.time -> time_slot (the C++ clock bridge)
                           slot=2,
                           mask_cap=isolate(group(4)))
    n = self.inset(n, amount=0.1, sides=8, slot=4, mask_inner=isolate(group(5)))
    self.output(n)


def main():
  # the terrain-precedent init recipe (test_terrain_asset.py): serialization walks reflected class
  # descriptions, which only exist after the full subsystem class registration.
  ezapp = ecs.headless_appinit(use_subsystems=['opq', 'core', 'gpu', 'lev2'])
  ezapp.mainThreadBegin()
  ezapp.bindGfxToCurrentThread()

  try:
    return _body(ezapp)
  finally:
    ezapp.mainThreadEnd()
    core.coreappexit()         # ALWAYS exit cleanly — a skipped coreappexit hangs teardown and
                               # masks the real traceback as a timeout


def _body(ezapp):
  a = A1Asset()
  g = a.graphdata

  # standalone module-data coverage for ops the DSL flow above doesn't route through
  bit = lev2.hypermesh.BitOp.createShared()
  bit.dst = 10; bit.a = 0; bit.b = 5; bit.op = 4; bit.width = 5
  g.addModule(bit, "bitop_x")
  srt = lev2.hypermesh.SortTest.createShared()
  srt.n = 777
  g.addModule(srt, "sorttest_x")

  js1 = g.serializeJson()
  print(f"hypermesh graph JSON bytes={len(js1)}", flush=True)

  # (1) the JSON must CONTAIN the authored state (the silent-drop class of bug)
  for needle, why in [
      ("_sel =",            "Select predicate GLSL"),
      ("_dist =",           "Extrude distance predicate GLSL"),
      ("_twist =",          "Extrude twist predicate GLSL"),
      ("_scale =",          "Extrude scale predicate GLSL"),
      ("time_slot",         "Extrude S.time slot (C++ clock bridge)"),
      ("exprp0",            "expression-param PLUGS (B.4: params are plug values)"),
      ("part_masks",        "Extrude/Inset part_masks"),
      ("predicate_abi",     "predicate ABI version"),
      ("matrix",            "Transform matrix"),
      ("sorttest_x",        "module names (name-keyed round-trip)"),
  ]:
    assert needle in js1, f"serialized JSON missing {why} ({needle!r})"

  # (2) deserialize WITHOUT any DSL re-run; field-level equality via the pyext getters
  g2 = Object.deserializeJson(js1)
  sel  = g2.findModule("select_1")
  assert "_sel =" in sel.predicate, "predicate GLSL lost"
  assert sel.domain == 0
  assert sel.sel_and == 0 and sel.sel_or == (1 << 2), \
      f"isolate(group(2)) sel triple lost: and={sel.sel_and:#x} or={sel.sel_or:#x}"
  assert sel.unsel_and == 0xFFFFFFFF and sel.unsel_or == 0
  xf = g2.findModule("transform_2")
  assert xf.slot == 2
  m1 = a.graphdata.findModule("transform_2").matrix
  assert str(xf.matrix) == str(m1), "Transform matrix lost (was silently identity before A.1)"
  ext = g2.findModule("extrude_3")
  assert ext.segments == 3
  assert ext.time_slot >= 0, "S.time slot lost (the C++ clock bridge would never fire)"
  for prop in ("dist_predicate", "twist_predicate", "scale_predicate"):
    assert len(getattr(ext, prop)) > 0, f"Extrude {prop} lost"
  bit2 = g2.findModule("bitop_x")
  assert (bit2.dst, bit2.a, bit2.b, bit2.op, bit2.width) == (10, 0, 5, 4, 5), "BitOp fields lost"
  assert g2.findModule("sorttest_x").n == 777, "SortTest n lost"

  # (3) idempotence: a second serialize of the deserialized graph must be byte-identical
  js2 = g2.serializeJson()
  assert js1 == js2, "round-trip NOT idempotent (a field is written-but-not-read or read-but-not-written)"

  print("HYPERMESH_REFLECTION_RESULT=PASS", flush=True)
  return 0


if __name__ == "__main__":
  sys.exit(main())
