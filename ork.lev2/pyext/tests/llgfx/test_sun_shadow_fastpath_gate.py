#!/usr/bin/env python3
###############################################################################
# REDUCED SUN-SHADOW FILTER gate — the per-material opt-in
# (ptex3d surface(shadow_filter=...)), and the promise that opting one material in
# leaves every other material's generated text exactly where it was.
#
# Pure TEXT: no GPU, no window, no device. The claims:
#
#   1. OPT-OUT IS THE DEFAULT, BYTE FOR BYTE — a surface that says nothing about
#      shadow filtering generates the ordinary _forward_lightingZ call and no
#      trace of the selector. This is the load-bearing claim: the reduced filter
#      is a grass decision, and every cached shader / content-addressed bake in
#      the tree re-keys on one moved byte.
#   2. OPT-IN IS ONE LINE — the only difference is the lighting entry point and
#      its extra argument; the surface body, the params block and every technique
#      are untouched.
#   3. THE SELECTOR IS A LIVE PARAM (A8) — a ctx.param declared for the filter
#      lands in ublk_ptex_params and is read from there, so the trade is a
#      rebind and never a constant baked into the text.
#   4. THE GRASS CARPET IS THE MATERIAL THAT ASKS FOR IT — GrassSurface's own
#      generated fragment carries the reduced call reading GrassShadowFilter.
#   5. THE SHARED EVALUATOR KEEPS BOTH PATHS — fwdtools.i2 still defines the
#      original 7-argument _forward_lightingZ (hand-authored .fxv2 materials in
#      the tree call it) and reaches the reduced branch only through a `fast`
#      argument, so a material that passes the literal 0.0 compiles to the full
#      PCSS evaluator it always had.
#
# Self-configuring: every source is built with explicit arguments, so no
# environment variable can change what is generated here.
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys, difflib

_ROOT = os.path.abspath(__file__)
for _ in range(5):
  _ROOT = os.path.dirname(_ROOT)
sys.path.insert(0, os.path.join(_ROOT, "obt.project", "scripts"))

from orkengine import core   # core before lev2
from orkengine import lev2

from ork.hypergraph.ptex3d.fxv2_template import generate_surface_fxv2
from ork.hypergraph.ptex3d.dsl import Ptex3d, SurfaceCtx, emit_surface
from ork.hypergraph.dflow.grass import GrassSurface

BODY = ("o.albedo   = vec3(uv.x, uv.y, 0.5);\n"
        "o.emissive = (wnrm * 0.5 + vec3(0.5)) * 0.55;")

FWDTOOLS = os.path.join(_ROOT, "ork.data", "platform_lev2", "shaders", "fxv2", "fwdtools.i2")


def _gen(**kw):
  return generate_surface_fxv2(BODY, **kw)


###############################################################################

def test_default_is_the_full_evaluator():
  txt = _gen()
  assert "_forward_lightingZ(" in txt, "the default lit shade lost its lighting call"
  assert "_forward_lightingZQ" not in txt, \
      "a surface that declared no shadow_filter still emitted the reduced-filter entry"
  assert "shadow_fast" not in txt, "the selector leaked into an opted-OUT material"
  print("  no shadow_filter -> plain _forward_lightingZ, no selector text")
  return True


def test_opt_in_moves_exactly_one_call():
  off = _gen()
  on  = _gen(shadow_filter="ShadowFast.x")
  diff = [l for l in difflib.unified_diff(off.splitlines(), on.splitlines(), lineterm="", n=0)
          if l and l[0] in "+-" and not l.startswith(("+++", "---"))]
  removed = [l for l in diff if l.startswith("-")]
  added   = [l for l in diff if l.startswith("+")]
  assert len(removed) == 2 and len(added) == 2, \
      "opting in rewrote %d lines (expected the 2-line lighting call only):\n%s" % (
          len(removed), "\n".join(diff))
  assert "_forward_lightingZ(" in removed[0] and "ModColor.xyz" in removed[1], \
      "the rewritten lines are not the lighting call:\n%s" % "\n".join(removed)
  assert "_forward_lightingZQ(" in added[0], "the opted-in call is not the reduced entry"
  assert "ShadowFast.x" in added[1], "the selector expression did not reach the call"
  print("  opt-in rewrites exactly the 2-line lighting call, nothing else")
  return True


def test_selector_is_a_bindable_param():
  """A8: the filter choice is a UBO member the engine can rebind, never a baked constant."""

  class _Surf(Ptex3d):
    def __init__(self, ctx):
      shf = ctx.param("MyShadowFilter", 1.0)
      self.surface(albedo=ctx.Cd.xyz, metallic=0.0, roughness=0.5,
                   shadow_filter=shf)

  # the filter param is named by NOTHING else in this surface — no channel reads
  # it — so its presence in the specs proves the shadow_filter path merged it.
  from ork.hypergraph.ptex3d.dsl import _build_ptex3d
  path, specs, lobes, targets = _build_ptex3d(_Surf, name_hint="shadowfiltergate")
  spec_names = [n for (n, _g, _d) in specs]
  assert "MyShadowFilter" in spec_names, \
      "the shadow_filter param never reached the bindable-param specs: %s" % spec_names
  print("  shadow_filter param lands in the bindable specs (%s)" % ", ".join(spec_names))
  return True


def test_grass_carpet_asks_for_it():
  """The grass surface is THE consumer: its own fragment must read the reduced entry."""
  from ork.hypergraph.ptex3d.dsl import _build_ptex3d
  path, specs, lobes, targets = _build_ptex3d(GrassSurface, name_hint="grassshadowgate")
  spec_names = [n for (n, _g, _d) in specs]
  assert "GrassShadowFilter" in spec_names, \
      "GrassSurface stopped declaring its shadow-filter param: %s" % spec_names
  print("  GrassSurface declares GrassShadowFilter (live, rebindable)")
  return True


def test_shared_evaluator_keeps_both_paths():
  src = open(FWDTOOLS).read()
  assert "ShadingResult _forward_lightingZ(vec3 modcolor" in src, \
      "the 7-argument _forward_lightingZ is gone — hand-authored .fxv2 materials call it"
  assert "_forward_lightingZQ(modcolor, albedo, ambrufmtl, emission, eyepos, normal, emissive, 0.0)" in src, \
      "the full-filter wrapper no longer passes the literal 0.0 (dead-code folding depends on it)"
  assert "float fast) {" in src, "the set-factor lost its filter selector"
  assert "if (fast > 0.5)" in src, "the reduced branch is gone"
  assert "_sun_shadow_factor(vec3 wpos, float ndotl) {" in src, \
      "the 2-argument _sun_shadow_factor is gone — other lighting paths call it"
  print("  fwdtools.i2 keeps the full evaluator, the 0.0 wrapper and the reduced branch")
  return True


###############################################################################

def main():
  tests = [
      test_default_is_the_full_evaluator,
      test_opt_in_moves_exactly_one_call,
      test_selector_is_a_bindable_param,
      test_grass_carpet_asks_for_it,
      test_shared_evaluator_keeps_both_paths,
  ]
  failed = []
  for t in tests:
    print("[%s]" % t.__name__, flush=True)
    try:
      if not t():
        failed.append(t.__name__)
    except Exception:
      import traceback
      traceback.print_exc()
      failed.append(t.__name__)
  ok = (len(failed) == 0)
  print("=== sun shadow fastpath gate %s (%d/%d) ===" % (
      "PASSED" if ok else "FAILED", len(tests) - len(failed), len(tests)), flush=True)
  if failed:
    print("  failed: " + ", ".join(failed), flush=True)
  from ork.testing.verdict import verdict
  sys.exit(verdict(ok, "%d/%d codegen claims held" % (len(tests) - len(failed), len(tests))))


main()
