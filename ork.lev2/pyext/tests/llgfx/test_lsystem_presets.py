#!/usr/bin/env python3
###############################################################################
# GR1.d gates — the four ARCHETYPE PRESET EMITTERS (grammar-as-data replacements for
# the legacy C++ LArchetype procedures). PURE CPU, no GPU: presets lower through the
# combinator DSL and derive through the GR1.b `_deriveLRuleSet` seam; class
# registration is in Lev2AppInit's (graphics-free) ClassToucher, so
# lev2appinit(use_subsystems=['opq','core','lev2']) suffices (headless-CI-safe).
#
# Gates:
#   A. determinism       — each preset derives byte-identically twice per (params, seed);
#                          a different seed on a stochastic param set derives DIFFERENT bytes.
#   B. structural counts — with jitter=0 the node count is closed-form per archetype
#                          (sympodial/conifer/saguaro/ocotillo formulas below) and the
#                          derived count matches EXACTLY (the legacy procedures share the
#                          same closed forms — the count-CLASS parity anchor).
#   C. cholla intact     — the GR1.c CaneCholla exemplar still derives to its locked
#                          253 nodes / 294 slots, and its lowering is unaffected by the
#                          segment(gen=) DSL addition (no "gen" param in its JSON).
#   D. gen attr contract — preset SEGMENTs stamp the generation attr (_attrs[1]) the
#                          leaves()/LeafScatter min_gen gate reads (max gen == depth-1
#                          for a sympodial of `depth` generations).
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import re
import struct
import sys
from orkengine import core   # core before lev2
from orkengine import lev2
from orkengine import ecs    # full class registration (bare imports serialize EMPTY)

from ork.hypergraph.dflow.lsystem import presets as PR
from ork.hypergraph.dflow.lsystem import resolve_grammar
from ork.hypergraph.dflow.lsystem.examples import CaneCholla

hm = lev2.hypermesh

_UUID = re.compile(r'("uuid"\s*:\s*")[^"]*(")')


def _strip_uuids(js):
  return _UUID.sub(r'\1*\2', js)


def _env(arch, over, **scalars):
  # NOTE: deliberately no m.archetype poke — the reflected archetype scalar dies with the
  # legacy path at the GR1.d deletion; presets never reference PARAM("archetype").
  m = hm.LSystemModule.createShared()
  jd = PR.PRESET_JIT[int(arch)]
  for k, v in jd.items():
    setattr(m, k, float(v))
  for k, v in scalars.items():
    setattr(m, k, v)
  for k, v in over.items():
    setattr(m, k, float(v))
  return m


def _derive(arch, structural, scalars):
  grammar, over = PR.build_preset(arch, **structural)
  rs = resolve_grammar(grammar)
  env = _env(arch, over, seed=structural.get("seed", 1),
             depth=int(structural.get("depth", 7)),
             children=int(structural.get("children", 2)),
             internodes=int(structural.get("internodes", 1)),
             budget=int(structural.get("budget", 4000)),
             tropism=float(structural.get("tropism", 0.0)), **scalars)
  return hm._deriveLRuleSet(rs, env)


###############################################################################
# Gate A — determinism (and seed sensitivity)
###############################################################################
def gate_a_determinism():
  cases = [
    (PR.SYMPODIAL, dict(depth=5, children=2, internodes=2, seed=11, budget=4000, tropism=0.07),
     dict(seg_len=0.6, base_radius=0.15, branch_angle=40.0, jitter=0.35, apical=0.3)),
    (PR.CONIFER,   dict(depth=6, children=4, internodes=1, seed=7, budget=6000, tropism=0.4),
     dict(seg_len=0.5, base_radius=0.2, branch_angle=75.0, jitter=0.3)),
    (PR.SAGUARO,   dict(depth=10, children=3, internodes=1, seed=3, budget=4000, tropism=1.0),
     dict(seg_len=0.5, base_radius=0.4, branch_angle=50.0, jitter=0.1, taper=0.5)),
    (PR.OCOTILLO,  dict(depth=10, children=10, internodes=1, seed=5, budget=4000, tropism=0.01),
     dict(seg_len=0.4, base_radius=0.06, branch_angle=30.0, jitter=0.3, taper=0.8)),
  ]
  for arch, structural, scalars in cases:
    d1 = _derive(arch, structural, scalars)
    d2 = _derive(arch, structural, scalars)
    assert d1["bytes"] == d2["bytes"], \
        f"[gate A] preset {arch} non-deterministic across two derives"
    assert d1["count"] > 1, f"[gate A] preset {arch} derived a degenerate skeleton"
    s2 = dict(structural, seed=structural["seed"] + 1)
    d3 = _derive(arch, s2, scalars)
    assert d3["bytes"] != d1["bytes"], \
        f"[gate A] preset {arch}: seed change did not change the derivation (stochastics dead?)"
    print(f"[gate A] preset {arch} deterministic ({d1['count']} nodes), seed-sensitive OK", flush=True)


###############################################################################
# Gate B — closed-form structural counts at jitter=0 (the legacy-shared formulas)
###############################################################################
def gate_b_counts():
  # sympodial: root + IN * sum_{g=0}^{D-1} C^g
  D, C, IN = 5, 2, 2
  d = _derive(PR.SYMPODIAL, dict(depth=D, children=C, internodes=IN, seed=1, budget=100000),
              dict(seg_len=0.5, base_radius=0.1, branch_angle=35.0, jitter=0.0))
  want = 1 + IN * sum(C ** g for g in range(D))
  assert d["count"] == want, f"[gate B] sympodial count {d['count']} != {want}"
  # conifer: root + W*(IN + K*(IN + C2*IN)) with W=max(3,depth), K=max(3,children), C2=max(1,children)
  Dp, Ch, IN = 6, 4, 1
  W, K, C2 = max(3, Dp), max(3, Ch), max(1, Ch)
  d = _derive(PR.CONIFER, dict(depth=Dp, children=Ch, internodes=IN, seed=1, budget=100000),
              dict(seg_len=0.5, base_radius=0.2, branch_angle=75.0, jitter=0.0))
  want = 1 + W * (IN + K * (IN + C2 * IN))
  assert d["count"] == want, f"[gate B] conifer count {d['count']} != {want}"
  # saguaro: root + T + A*NA with T=max(8,depth), NA=max(6,T//2)
  Dp, A = 12, 3
  T = max(8, Dp); NA = max(6, T // 2)
  d = _derive(PR.SAGUARO, dict(depth=Dp, children=A, internodes=1, seed=1, budget=100000),
              dict(seg_len=0.5, base_radius=0.4, branch_angle=50.0, jitter=0.0, taper=0.5))
  want = 1 + T + A * NA
  assert d["count"] == want, f"[gate B] saguaro count {d['count']} != {want}"
  # ocotillo: root + Sn*SN with Sn=max(8,children), SN=max(8,depth)
  Dp, Ch = 12, 10
  Sn, SN = max(8, Ch), max(8, Dp)
  d = _derive(PR.OCOTILLO, dict(depth=Dp, children=Ch, internodes=1, seed=1, budget=100000),
              dict(seg_len=0.4, base_radius=0.06, branch_angle=30.0, jitter=0.0))
  want = 1 + Sn * SN
  assert d["count"] == want, f"[gate B] ocotillo count {d['count']} != {want}"
  print("[gate B] all four closed-form structural counts EXACT", flush=True)


###############################################################################
# Gate C — the CaneCholla exemplar is untouched by GR1.d
###############################################################################
def gate_c_cholla_intact():
  rs = resolve_grammar(CaneCholla)
  d = hm._deriveLRuleSet(rs, None)
  assert d["count"] == 253 and len(d["slots"]) == 294, \
      f"[gate C] cholla drifted: {d['count']} nodes / {len(d['slots'])} slots (locked 253/294)"
  js1 = _strip_uuids(resolve_grammar(CaneCholla).serializeJson())
  js2 = _strip_uuids(resolve_grammar(CaneCholla).serializeJson())
  assert js1 == js2, "[gate C] cholla uuid-stripped JSON unstable across derives"
  # the segment(gen=) DSL addition must be INERT for a segment that doesn't pass gen
  # (cholla's own "gen" occurrences are CALL params — legitimate; test a minimal grammar).
  from ork.hypergraph.dflow import lsystem as L

  class _Plain(L.Lsystem):
    def grammar(self):
      A = self.symbol("A")

      @L.rule(A)
      def go(S):
        return [L.segment(len=0.1, rad=0.02)]

      self.axiom(A())
      self.iterate(depth=1, segment_budget=8)

  js_p = _Plain().derive().serializeJson()
  assert '"gen"' not in js_p, \
      "[gate C] segment() without gen= emitted a 'gen' binding (DSL addition not inert)"
  print("[gate C] cholla exemplar intact (253/294, stable stripped JSON, gen= inert)", flush=True)


###############################################################################
# Gate D — generation attr contract (leaves()/min_gen reads _attrs[1])
###############################################################################
def gate_d_gen_attr():
  D = 6
  d = _derive(PR.SYMPODIAL, dict(depth=D, children=2, internodes=1, seed=1, budget=100000),
              dict(seg_len=0.5, base_radius=0.1, branch_angle=35.0, jitter=0.0))
  raw = d["bytes"]  # 88B/node: 16f xform, u32 parent, 4f attrs, u32 tags — attrs[1]=gen @ +72
  gens = set()
  for i in range(d["count"]):
    gens.add(int(struct.unpack_from("<f", raw, i * 88 + 72)[0]))
  assert max(gens) == D - 1, \
      f"[gate D] sympodial max generation {max(gens)} != depth-1 ({D - 1}) — leaves() min_gen contract broken"
  assert gens == set(range(D)), f"[gate D] generation coverage holes: {sorted(gens)}"
  print(f"[gate D] generation attr stamped 0..{D - 1} OK", flush=True)


###############################################################################
def main():
  lev2.lev2appinit(use_subsystems=["opq", "core", "lev2"])
  try:
    gate_a_determinism()
    gate_b_counts()
    gate_c_cholla_intact()
    gate_d_gen_attr()
    print("LSYSTEM_PRESETS_RESULT=PASS", flush=True)
    return 0
  finally:
    core.coreappexit()


if __name__ == "__main__":
  sys.exit(main())
