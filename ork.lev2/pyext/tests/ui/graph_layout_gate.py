#!/usr/bin/env ork.python
################################################################################
# graph_layout_gate — headless, pure-CPU oracles for the layered DAG auto-layout
# (ork.editor.graph_layout). NO GPU, fast. Verdict-first output; asserts loudly.
#
# Oracles:
#   CROSSING   — exact rendered-wire crossing count (count_crossings) on the ROUTED
#                polylines (layout_ex waypoints), reported AFTER layout for every
#                graph vs a naive-order baseline; chains/diamonds MUST be 0, tangles
#                MUST be <= naive AND <= the quoted slice-A number (no regression).
#   BODY       — the owner's metric (count_body_crossings): edge segments (routed
#                polylines) that pass THROUGH a non-incident node's body. The fireball
#                bus + chain graph has BODY>0 under slice-A (quoted) and MUST be 0
#                after (side-rail + channel routing); the whole battery reaches 0.
#   GRID       — every final coordinate is an exact multiple of the grid.
#   NO-OVERLAP — every pair of node rects (inflated by a gutter) is disjoint.
#   STRAIGHT   — a fireball-shaped chain lays out on ONE rail coordinate.
#   NO-REGRESS — chains/diamonds (no bus) lay out BYTE-IDENTICAL to slice-A (golden).
#   DETERMINISM— 3x re-layout is byte-identical; permuting the input node/edge
#                enumeration order yields the IDENTICAL layout (order independence).
#   STABILITY  — adding one leaf moves the unchanged nodes a bounded amount
#                (reported; no hard assert — optimality can trump minimal-motion).
#   TRANSPOSE  — the Sugiyama refinement A/B: the owner's loop_1/comb_8 diamond with an
#                asymmetric dummy-chain weighting lays out with 1 wire crossing under
#                barycenter alone (transpose OFF) and 0 with the refinement ON; the whole
#                battery's transpose count is <= median-only and <= slice-A2 (no regress).
#   PORT       — display-level plug ordering: a node fed by two sources in crossed
#                declaration order scores 1 canonical port crossing, 0 after the pass; a
#                node already monotone keeps canonical order EXACTLY (strict-improve only).
#                Per-graph canonical/refined port crossings reported; determinism + order-
#                invariance held. Node positions/wire-crossings are UNAFFECTED by it.
#   ALAP       — feeder demotion (A5): a const wired to a deep consumer parks on layer 0
#                under longest-path layering (the owner's const_0/const_1 'lala land'). The
#                A/B (ALAP_FEEDER_DEMOTE knob) proves each const slides to consumer_layer-1
#                (layer gap 5/6 -> 1, euclidean at least halved, lateral within one node-
#                pitch); a single-path const->scale->consumer prefix demotes as a UNIT into
#                three consecutive layers. Battery-wide crossing/body/port never regress.
#
# Test graphs: real saved dflow graph (test6.orj, whose GLOB module IS a bus) via the
# engine-free .orj adapter; a LIVE terrain graph (voronoi) via the dflow adapter
# (proves the live path); the fireball bus (POOL feeding every operator) + fireball
# chain + roads-layout topologies (their live assets need a GPU context this headless
# session lacks — see the report); and synthetic tangles (binary tree, diamond, K3,3,
# butterfly, and a seeded layered DAG).
#
#   run:  ork.python ork.lev2/pyext/tests/ui/graph_layout_gate.py
################################################################################

import os
os.environ.setdefault("PYTHONUNBUFFERED", "1")
import json
import random
import sys

# Resolve THIS worktree's script tree so the module under test is the one edited here.
_HERE = os.path.dirname(os.path.abspath(__file__))
_WORKTREE = os.path.abspath(os.path.join(_HERE, "..", "..", "..", ".."))
_SCRIPTS = os.path.join(_WORKTREE, "obt.project", "scripts")
if _SCRIPTS not in sys.path:
  sys.path.insert(0, _SCRIPTS)

from ork.editor import graph_layout as gl

GRID = 32.0
_ORJ = os.path.join(_WORKTREE, "ork.data", "tests", "newrefl", "particles")

# ALAP aesthetic bound: a demoted feeder sharing its layer with the through-chain node
# that occupies its consumer's cross column can get no closer than ONE node-pitch (it
# lands in the immediately-adjacent cross column). NODE_W(150)+gutter quantises to 7*grid
# (224px); 8 grid (256px) is that floor plus one grid of slack.
ALAP_LATERAL_MAX_GRID = 8

_FAILS = []


def _euclid(a, b):
  return ((a[0] - b[0]) ** 2 + (a[1] - b[1]) ** 2) ** 0.5


def _check(cond, msg):
  if not cond:
    _FAILS.append(msg)
    print("  FAIL:", msg)
  return cond


def _freeze(pos):
  """Canonical byte form of a position dict for identity comparison."""
  return json.dumps({str(k): [round(v[0], 6), round(v[1], 6)] for k, v in sorted(pos.items(), key=lambda kv: str(kv[0]))})


def _freeze_po(port_orders):
  """Canonical byte form of a port-order map ({node:{inputs:[..],outputs:[..]}})."""
  return json.dumps({str(k): {s: [str(x) for x in v.get(s, [])] for s in ("inputs", "outputs")}
                     for k, v in sorted((port_orders or {}).items(), key=lambda kv: str(kv[0]))},
                    sort_keys=True)


################################################################################
# Graph builders
################################################################################

def fireball_chain():
  chain = ["POOL", "EMIT", "BUOY", "DRAG", "TURB", "CURL", "SPRI"]
  return gl.graph_from_edges(chain, list(zip(chain, chain[1:])))


def fireball_bus():
  # The owner's actual complaint shape: a particle POOL referenced by EVERY operator
  # (a pool feeds every module) plus the operator chain and the SPRI sink. POOL's
  # out-edges span all 6 downstream layers => a structural BUS. Under slice-A its
  # straight wires cut across the operator bodies; the A2 side-rail + channel routing
  # lifts POOL onto a right rail and runs its wires down a clear channel.
  chain = ["EMIT", "BUOY", "DRAG", "TURB", "CURL", "SPRI"]
  edges = list(zip(chain, chain[1:])) + [("POOL", c) for c in chain]
  return gl.graph_from_edges(["POOL"] + chain, edges)


# --- slice-A reference (measured on the parent commit's graph_layout, quoted so the
#     A2 gate can prove no-regression / the before->after body-crossing drop) -------
#     tag -> (body_crossings, wire_crossings) under slice-A straight-wire rendering.
SLICE_A = {
    "test6.orj":       (0, 0),
    "fireball-bus":    (6, 2),
    "fireball-chain":  (0, 0),
    "roads-layout":    (0, 0),
    "diamond":         (0, 0),
    "bintree4":        (0, 0),
    "butterfly":       (0, 0),
    "K3,3":            (9, 9),
    "layered-dag-big": (2, 1),
}

# byte-identical golden positions from slice-A for the no-bus regression proof.
GOLDEN = {
    "diamond": {"A": (192.0, 64.0), "B": (96.0, 192.0),
                "C": (320.0, 192.0), "D": (96.0, 320.0)},
    "fireball-chain": {"POOL": (96.0, 64.0), "EMIT": (96.0, 192.0),
                       "BUOY": (96.0, 320.0), "DRAG": (96.0, 448.0),
                       "TURB": (96.0, 576.0), "CURL": (96.0, 704.0),
                       "SPRI": (96.0, 832.0)},
}


def diamond():
  return gl.graph_from_edges(["A", "B", "C", "D"],
                             [("A", "B"), ("A", "C"), ("B", "D"), ("C", "D")])


def binary_tree(depth=4):
  n = (1 << depth) - 1
  nodes = ["n%d" % i for i in range(1, n + 1)]
  edges = []
  for i in range(1, n + 1):
    if 2 * i <= n:
      edges.append(("n%d" % i, "n%d" % (2 * i)))
    if 2 * i + 1 <= n:
      edges.append(("n%d" % i, "n%d" % (2 * i + 1)))
  return gl.graph_from_edges(nodes, edges)


def k33():
  srcs, dsts = ["s0", "s1", "s2"], ["t0", "t1", "t2"]
  return gl.graph_from_edges(srcs + dsts, [(s, t) for s in srcs for t in dsts])


def butterfly():
  # connected tangle whose naive input order crosses; the optimizer untangles it.
  return gl.graph_from_edges(["R", "A", "B", "C", "D"],
                             [("R", "A"), ("R", "B"), ("A", "D"), ("B", "C")])


def roads_layout_topology():
  # the build_roads_layout() wiring (ork.hypergraph.assets.terrain.roadshills): a real
  # roads DAG shape. The LIVE asset needs a GPU context (terrain loop / roads C++
  # modules) that this headless session lacks, so we exercise its TOPOLOGY here.
  nodes = ["fbm", "slope", "route_spine", "roadbed_mask", "keepout_mask",
           "parcelize", "building_seeds", "road_mesh"]
  edges = [("fbm", "slope"), ("fbm", "route_spine"), ("slope", "route_spine"),
           ("route_spine", "roadbed_mask"), ("roadbed_mask", "keepout_mask"),
           ("route_spine", "parcelize"), ("parcelize", "building_seeds"),
           ("route_spine", "road_mesh")]
  return gl.graph_from_edges(nodes, edges)


# --- ALAP feeder-demotion fixtures ------------------------------------------------
# erodeflow_echo: a pure-CPU ECHO of the owner's erodeflow pipeline shape (the live DSL
# uses T.loop, whose LoopModule reshape needs the post-appinit GPU thread — see
# real_erodeflow_check). A long height chain (fbm -> ... -> captures, the 'normal source
# chain') plus TWO constant params (const_0/const_1, real DSL literals like an lpf cutoff
# / a normalize bound) wired to operators DEEP in that chain. Under longest-path (ASAP)
# layering both consts park on layer 0 — the 'lala land' the owner saw, a long lonely wire
# reaching down to a layer-6/7 consumer. ALAP slides each to consumer_layer-1 (adjacent).
_ERODE_SPINE = ["fbm", "basin_fill", "terrace", "flow_erode", "erode_thermal",
                "lpf_hi", "lpf_lo", "normalize", "flow3d", "captures"]
# const -> consumer wiring: (const id, consumer id, ASAP consumer layer for reference)
_ERODE_CONSTS = [("const_0", "lpf_lo", 6), ("const_1", "normalize", 7)]


def erodeflow_echo():
  edges = list(zip(_ERODE_SPINE, _ERODE_SPINE[1:]))
  edges += [(c, cons) for (c, cons, _l) in _ERODE_CONSTS]
  nodes = [c for (c, _cons, _l) in _ERODE_CONSTS] + list(_ERODE_SPINE)
  return gl.graph_from_edges(nodes, edges)


def feeder_chain():
  # const -> scale -> deep consumer m5, a PURE single-path prefix hanging off a normal
  # chain m0..m5. ASAP parks const at 0, scale at 1; ALAP slides the whole prefix down as
  # a UNIT so const/scale/m5 occupy three consecutive layers (3,4,5).
  chain = ["m0", "m1", "m2", "m3", "m4", "m5"]
  edges = list(zip(chain, chain[1:])) + [("const", "scale"), ("scale", "m5")]
  return gl.graph_from_edges(["const", "scale"] + chain, edges)


def bus_deep_feeder():
  # interplay (a): a broadcast source (spans BUS_LAYER_SPAN deep layers) that ALAP wants
  # to demote. Bus detection keys off the CONSUMER layers, so BUSN stays a bus and keeps
  # its side-rail; the demotion just lowers its flow position so the rail wire runs shorter.
  chain = ["a0", "a1", "a2", "a3", "a4", "a5", "a6"]
  edges = list(zip(chain, chain[1:])) + [("BUSN", "a4"), ("BUSN", "a5"), ("BUSN", "a6")]
  return gl.graph_from_edges(["BUSN"] + chain, edges)


def seeded_layered_dag(seed=1234, layers=6, per=4, span=2):
  """A deterministic layered DAG (fixed seed => fixed structure) for the tangle
  battery + the visual artifact. Random only shapes the FIXTURE; the LAYOUT is
  deterministic regardless."""
  rng = random.Random(seed)
  ln = [["L%d_%d" % (l, i) for i in range(per)] for l in range(layers)]
  nodes = [n for layer in ln for n in layer]
  edges = []
  for l in range(layers - 1):
    for src in ln[l]:
      k = rng.randint(1, 2)
      for _ in range(k):
        dl = min(layers - 1, l + rng.randint(1, span))
        edges.append((src, rng.choice(ln[dl])))
  edges = sorted(set(edges))
  return gl.graph_from_edges(nodes, edges)


def load_orj(name):
  return gl.graph_from_orj(os.path.join(_ORJ, name))


# --- TRANSPOSE trap: the owner's loop_1/comb_8 diamond, plus the asymmetric dummy-chain
#     weighting that makes barycenter STALL. The pure 5-node diamond is solved by median
#     alone (median freely reorders the dummy layer), so — per the brief — it is reshaped
#     with weighting taps (gain_5->mix_3 chain; the loop_1->tap_7 long edge is a second
#     dummy chain) that PIN the layers into a median fixpoint one crossing above optimal.
#     Median-only lays this out with 1 wire crossing; the TRANSPOSE refinement's single
#     adjacent swap removes it -> 0 (the A/B below is the proof). Diamond layering is
#     preserved (loop_1:0, {lpf_2,remap_11}:1, {remap_12,...}:2, comb_8:3) and the fixed
#     layout carries 0 body crossings.
def owner_diamond_trap():
  nodes = ["loop_1", "lpf_2", "remap_11", "remap_12", "comb_8", "gain_5", "mix_3", "tap_7"]
  edges = [("loop_1", "lpf_2"), ("loop_1", "remap_11"),          # fork: L / R branches
           ("lpf_2", "remap_12"), ("remap_12", "comb_8"),        # L branch (len 2)
           ("remap_11", "comb_8"),                               # R branch (len 1, long edge -> dummy)
           ("loop_1", "gain_5"), ("gain_5", "mix_3"),            # weighting tap chain
           ("loop_1", "tap_7"), ("lpf_2", "tap_7")]              # loop_1->tap_7 = 2nd dummy chain
  return gl.graph_from_edges(nodes, edges)


# the owner's verbatim diamond region (a pure-CPU echo of the xxx3 fragment) — used by the
# real-graph check to assert the loop_1/comb_8 region reaches 0 crossings.
_XXX3_DIAMOND_EDGES = [("loop_1", "lpf_2"), ("lpf_2", "remap_12"),
                       ("remap_12", "comb_8"), ("loop_1", "remap_11"),
                       ("remap_11", "comb_8")]


# --- PORT-ordering fixtures. crossed: preds p<q force source `b` to the LEFT of `a`
#     (a<b), so J's CANONICAL (id-sorted) input order [a,b] is crossed at J's input row
#     (1 port crossing); the pass reorders to [b,a] -> 0. noimprove: p->a q->b leave a
#     LEFT of b, so canonical [a,b] already matches the wires (0) and MUST be kept exactly
#     (strict-improvement-only rule; scanability).
def port_crossed():
  return gl.graph_from_edges(["p", "q", "a", "b", "J"],
                             [("p", "b"), ("q", "a"), ("a", "J"), ("b", "J")])


def port_noimprove():
  return gl.graph_from_edges(["p", "q", "a", "b", "J"],
                             [("p", "a"), ("q", "b"), ("a", "J"), ("b", "J")])


################################################################################
# Per-graph oracle battery
################################################################################

def run_graph(tag, graph, expect_zero=False, expect_chain=False,
              expect_body_zero=False, kind="synthetic"):
  pos, wps, ports = gl.layout_ex(graph, grid=GRID)                         # transpose ON
  mpos, mwps, _mp = gl.layout_ex(graph, grid=GRID, transpose=False)        # median-only A/B
  plain = gl.layout(graph, grid=GRID)
  npos = gl.layout_naive(graph, grid=GRID)
  cr = gl.count_crossings(pos, graph.edges, graph.sizes, waypoints=wps)      # routed
  mcr = gl.count_crossings(mpos, graph.edges, graph.sizes, waypoints=mwps)   # median-only
  ncr = gl.count_crossings(npos, graph.edges, graph.sizes)
  bc = gl.count_body_crossings(pos, graph.edges, graph.sizes, waypoints=wps)
  nbc = gl.count_body_crossings(npos, graph.edges, graph.sizes)
  pc_can = gl.count_port_crossings(pos, graph.edges, graph.sizes)                    # canonical plug order
  pc_ref = gl.count_port_crossings(pos, graph.edges, graph.sizes, port_orders=ports)  # refined plug order
  gv = gl.grid_violations(pos, GRID)
  ov = gl.overlaps(pos, graph.sizes, gutter=1.0)
  bb = gl.bounding_box(pos, graph.sizes)
  ncomp = len(gl._components(graph))
  buses = gl._detect_buses(graph.nodes, graph.edges, gl.assign_layers(graph))
  sa = SLICE_A.get(tag)
  sa_txt = f" sliceA(body={sa[0]},wire={sa[1]})" if sa else ""
  print(f"[{kind:7s}] {tag:16s} n={len(graph.nodes):3d} e={len(graph.edges):3d} comps={ncomp} "
        f"bus={buses} WIRE tpose={cr} median={mcr} naive={ncr} BODY auto={bc} naive={nbc}{sa_txt} "
        f"PORT canon={pc_can} refined={pc_ref} grid_bad={len(gv)} overlap={len(ov)} "
        f"bbox=({bb[0]:.0f},{bb[1]:.0f},{bb[2]:.0f},{bb[3]:.0f})")

  # layout() must return exactly layout_ex()'s positions (public-API contract).
  _check(_freeze(plain) == _freeze(pos), f"{tag}: layout() != layout_ex() positions")
  _check(not gv, f"{tag}: {len(gv)} coords not on grid {GRID} (e.g. {gv[:2]})")
  _check(not ov, f"{tag}: {len(ov)} node-rect overlaps (e.g. {ov[:2]})")
  _check(cr <= ncr, f"{tag}: auto crossings {cr} > naive baseline {ncr}")
  _check(cr <= mcr, f"{tag}: transpose {cr} worse than median-only {mcr}")   # refinement never regresses
  _check(pc_ref <= pc_can, f"{tag}: refined ports {pc_ref} worse than canonical {pc_can}")
  if sa:                                            # A2 must not regress beyond slice-A
    _check(cr <= sa[1], f"{tag}: A2 wire {cr} regressed beyond slice-A {sa[1]}")
  if tag in GOLDEN:                                 # no-bus graphs byte-identical to slice-A
    _check(_freeze(pos) == _freeze(GOLDEN[tag]),
           f"{tag}: layout drifted from slice-A golden")
  if expect_zero:
    _check(cr == 0, f"{tag}: expected 0 wire crossings, got {cr}")
  if expect_body_zero:
    _check(bc == 0, f"{tag}: expected 0 BODY crossings, got {bc} (sliceA {sa[0] if sa else '?'})")
  if expect_chain:                                  # the operator chain lands on one rail
    chain_rail = sorted(set(pos[n][0] for n in graph.nodes if n not in buses))
    _check(len(chain_rail) == 1, f"{tag}: chain not straight — {len(chain_rail)} rails {chain_rail}")

  # determinism (3x byte-identical) — positions AND port orders
  b0 = _freeze(pos)
  p0 = _freeze_po(ports)
  _check(all(_freeze(gl.layout(graph, grid=GRID)) == b0 for _ in range(3)),
         f"{tag}: layout is non-deterministic across repeats")
  _check(all(_freeze_po(gl.layout_ex(graph, grid=GRID)[2]) == p0 for _ in range(3)),
         f"{tag}: port order is non-deterministic across repeats")
  # order independence (permute node + edge enumeration) — positions AND port orders
  perm_nodes = list(graph.original_order)
  random.Random(101).shuffle(perm_nodes)
  perm_edges = list(graph.edges)
  random.Random(202).shuffle(perm_edges)
  g2 = gl.graph_from_edges(perm_nodes, perm_edges, graph.sizes)
  _check(_freeze(gl.layout(g2, grid=GRID)) == b0,
         f"{tag}: layout depends on input enumeration order")
  _check(_freeze_po(gl.layout_ex(g2, grid=GRID)[2]) == p0,
         f"{tag}: port order depends on input enumeration order")
  return cr, mcr, ncr, pos


def stability_smoke(graph, tag):
  """Add one leaf onto an existing node; report max displacement of unchanged nodes."""
  base = gl.layout(graph, grid=GRID)
  anchor = graph.nodes[len(graph.nodes) // 2]
  nn = list(graph.original_order) + ["__added__"]
  ee = list(graph.edges) + [(anchor, "__added__")]
  g2 = gl.graph_from_edges(nn, ee, graph.sizes)
  after = gl.layout(g2, grid=GRID)
  moved = [((after[n][0] - base[n][0]) ** 2 + (after[n][1] - base[n][1]) ** 2) ** 0.5
           for n in graph.nodes if n in after]
  maxd = max(moved) if moved else 0.0
  nmoved = sum(1 for d in moved if d > 1e-6)
  print(f"[stab   ] {tag:16s} add-one-leaf: max_disp={maxd:.0f}px moved_nodes={nmoved}/{len(graph.nodes)} "
        f"(no hard assert — layered optimality can trump minimal motion)")
  return maxd


################################################################################
# TRANSPOSE A/B proof + real-graph diamond check
################################################################################

def run_transpose_ab(tag, graph, expect_off=1, expect_on=0):
  """The refinement's proof: lay the graph out with the TRANSPOSE pass OFF (pure
  barycenter) and ON, and assert the strict before/after crossing counts. Also proves
  the fixed layout is grid/overlap-clean and deterministic + input-order-independent
  with the pass on, and that the loop_1/comb_8 diamond region itself is untangled."""
  off_pos, off_wps, _ = gl.layout_ex(graph, grid=GRID, transpose=False)
  on_pos, on_wps, on_ports = gl.layout_ex(graph, grid=GRID, transpose=True)
  cr_off = gl.count_crossings(off_pos, graph.edges, graph.sizes, waypoints=off_wps)
  cr_on = gl.count_crossings(on_pos, graph.edges, graph.sizes, waypoints=on_wps)
  bc_on = gl.count_body_crossings(on_pos, graph.edges, graph.sizes, waypoints=on_wps)
  gv = gl.grid_violations(on_pos, GRID)
  ov = gl.overlaps(on_pos, graph.sizes, gutter=1.0)
  # diamond-region crossings (the loop_1/comb_8 core, weighting edges excluded)
  dia = [e for e in graph.edges if e in _XXX3_DIAMOND_EDGES]
  dia_on = gl.count_crossings(on_pos, dia, graph.sizes, waypoints=on_wps)
  print(f"[TPOSE  ] {tag:16s} n={len(graph.nodes)} e={len(graph.edges)} "
        f"WIRE median-only={cr_off} -> transpose={cr_on} (diamond-region={dia_on}) "
        f"BODY={bc_on} grid_bad={len(gv)} overlap={len(ov)}")
  _check(cr_off == expect_off, f"{tag}: median-only wire {cr_off} != expected {expect_off} (fixture no longer traps)")
  _check(cr_on == expect_on, f"{tag}: transpose wire {cr_on} != expected {expect_on} (refinement failed)")
  _check(cr_on < cr_off, f"{tag}: transpose {cr_on} did not strictly beat median-only {cr_off}")
  _check(dia_on == 0, f"{tag}: loop_1/comb_8 diamond region has {dia_on} crossings post-transpose")
  _check(not gv, f"{tag}: {len(gv)} coords off grid post-transpose")
  _check(not ov, f"{tag}: {len(ov)} overlaps post-transpose")
  # determinism + order invariance of the transposed layout
  b0 = _freeze(on_pos)
  _check(all(_freeze(gl.layout(graph, grid=GRID)) == b0 for _ in range(3)),
         f"{tag}: transposed layout non-deterministic")
  for si in range(4):
    pn = list(graph.original_order); random.Random(si * 9 + 1).shuffle(pn)
    pe = list(graph.edges); random.Random(si * 9 + 3).shuffle(pe)
    g2 = gl.graph_from_edges(pn, pe, graph.sizes)
    _check(_freeze(gl.layout(g2, grid=GRID)) == b0,
           f"{tag}: transposed layout order-dependent (perm {si})")
    off2p, off2w, _ = gl.layout_ex(g2, grid=GRID, transpose=False)
    _check(gl.count_crossings(off2p, g2.edges, g2.sizes, waypoints=off2w) == expect_off,
           f"{tag}: median-only crossings order-dependent (perm {si})")
  return cr_off, cr_on


def real_xxx3_check():
  """Real-graph check. The live xxx3 dflow graph elaborates only post-appinit on the GPU
  thread (LoopModule boundary reshape), so it cannot load in this pure-CPU headless
  session — we attempt the live dflow adapter (reporting the skip reason honestly) and
  fall back to the sanctioned pure-CPU ECHO of the owner's verbatim loop_1/comb_8 diamond
  region, asserting it lays out at 0 crossings under the shipped (transpose-on) layout."""
  try:
    from orkengine import core            # noqa
    from orkengine import lev2            # noqa
    from orkengine import ecs             # noqa
    from ork.hypergraph.assets.terrain.xxx3 import XXX3
    gd = XXX3().generatedflow()
    g = gl.graph_from_dflow(gd)
    pos, wps, _ = gl.layout_ex(g, grid=GRID)
    dia = [e for e in g.edges if e in _XXX3_DIAMOND_EDGES]
    dia_cr = gl.count_crossings(pos, dia, g.sizes, waypoints=wps)
    print(f"[REAL   ] xxx3(dflow LIVE)  n={len(g.nodes)} e={len(g.edges)} "
          f"diamond-region crossings={dia_cr}")
    _check(dia_cr == 0, f"xxx3 live: loop_1/comb_8 diamond has {dia_cr} crossings")
    return True
  except Exception as ex:
    print(f"[REAL   ] xxx3(dflow LIVE)  SKIPPED — elaborate() needs post-appinit GPU "
          f"thread headless: {ex!r}")
  # pure-CPU echo of the owner's verbatim diamond fragment
  nodes = sorted({n for e in _XXX3_DIAMOND_EDGES for n in e})
  g = gl.graph_from_edges(nodes, list(_XXX3_DIAMOND_EDGES))
  pos, wps, _ = gl.layout_ex(g, grid=GRID)
  cr = gl.count_crossings(pos, g.edges, g.sizes, waypoints=wps)
  bc = gl.count_body_crossings(pos, g.edges, g.sizes, waypoints=wps)
  print(f"[REAL   ] xxx3-diamond(echo) n={len(g.nodes)} e={len(g.edges)} "
        f"WIRE={cr} BODY={bc}  (loop_1->[lpf_2->remap_12 | remap_11]->comb_8)")
  _check(cr == 0, f"xxx3-diamond echo: {cr} crossings (owner's diamond must reach 0)")
  return False


################################################################################
# ALAP feeder-demotion oracle (A5) + real erodeflow check
################################################################################

def run_alap_gate():
  """The A5 proof: ALAP feeder demotion hugs stranded consts to their deep consumers.
  A/B via the ALAP_FEEDER_DEMOTE knob — pre-fix (off) parks the consts on layer 0 (a
  long lonely wire to a layer-6/7 node), post-fix (on) slides each to consumer_layer-1.
  Quotes the layer + euclidean distance before/after and asserts the post-fix hug (layer
  gap exactly 1; euclidean at least halved; lateral within one node-pitch). Then the
  feeder-CHAIN proof: const->scale->consumer demotes as a UNIT into three consecutive
  layers. Positions are grid/overlap-clean either way."""
  g = erodeflow_echo()
  asap = gl.assign_layers(g)
  alap = dict(asap)
  gl.alap_demote_feeders(g.nodes, g.edges, alap)                 # demoted layer map
  saved = gl.ALAP_FEEDER_DEMOTE
  try:
    gl.ALAP_FEEDER_DEMOTE = False
    pre = gl.layout(g, grid=GRID)                                # pre-fix: consts parked
  finally:
    gl.ALAP_FEEDER_DEMOTE = saved
  post = gl.layout(g, grid=GRID)                                 # post-fix: consts hugged
  gv = gl.grid_violations(post, GRID)
  ov = gl.overlaps(post, g.sizes, gutter=1.0)
  print(f"[ALAP   ] erodeflow-echo   spine={len(_ERODE_SPINE)} consts={len(_ERODE_CONSTS)} "
        f"grid_bad={len(gv)} overlap={len(ov)}")
  _check(not gv, f"erodeflow-echo: {len(gv)} coords off grid post-demotion")
  _check(not ov, f"erodeflow-echo: {len(ov)} overlaps post-demotion")
  for (c, cons, _l) in _ERODE_CONSTS:
    ed_pre, ed_post = _euclid(pre[c], pre[cons]), _euclid(post[c], post[cons])
    latx = abs(post[c][0] - post[cons][0])
    print(f"           {c:8s} PRE  layer={asap[c]} (gap={alap[cons]-asap[c]}) euclid={ed_pre:.0f}"
          f"   ->  POST layer={alap[c]} (gap=1) euclid={ed_post:.0f} lateral={latx:.0f}={latx/GRID:.1f}grid"
          f"   consumer={cons}(L{alap[cons]})")
    _check(alap[c] == alap[cons] - 1,
           f"{c}: not demoted to consumer_layer-1 ({alap[c]} vs {alap[cons]-1})")
    _check(ed_post <= 0.5 * ed_pre,
           f"{c}: post euclid {ed_post:.0f} not <= half of pre {ed_pre:.0f}")
    _check(latx <= ALAP_LATERAL_MAX_GRID * GRID,
           f"{c}: lateral {latx:.0f} > {ALAP_LATERAL_MAX_GRID}*grid={ALAP_LATERAL_MAX_GRID*GRID:.0f}")

  fc = feeder_chain()
  fa = gl.assign_layers(fc)
  fd = dict(fa)
  gl.alap_demote_feeders(fc.nodes, fc.edges, fd)
  lc, ls, lm = fd["const"], fd["scale"], fd["m5"]
  print(f"[ALAP   ] feeder-chain     const L{lc} -> scale L{ls} -> m5 L{lm}  "
        f"(ASAP const L{fa['const']} scale L{fa['scale']} m5 L{fa['m5']}) — demote-as-unit")
  _check(lc + 1 == ls and ls + 1 == lm,
         f"feeder-chain: single-path prefix not consecutive ({lc},{ls},{lm})")

  # interplay (a): a deep-feeding BUS demotes but keeps its rail (detection reads consumer
  # layers), and the demotion only shortens the rail wire — rail packing is not fought.
  bg = bus_deep_feeder()
  ba = gl.assign_layers(bg)
  bd = dict(ba)
  gl.alap_demote_feeders(bg.nodes, bg.edges, bd)
  buses = gl._detect_buses(bg.nodes, bg.edges, bd)
  bp, bw, _ = gl.layout_ex(bg, grid=GRID)
  saved2 = gl.ALAP_FEEDER_DEMOTE
  try:
    gl.ALAP_FEEDER_DEMOTE = False
    bp_off, _, _ = gl.layout_ex(bg, grid=GRID)
  finally:
    gl.ALAP_FEEDER_DEMOTE = saved2
  bov = gl.overlaps(bp, bg.sizes, gutter=1.0)
  bbc = gl.count_body_crossings(bp, bg.edges, bg.sizes, waypoints=bw)
  rail_off = sum(_euclid(bp_off["BUSN"], bp_off[c]) for c in ("a4", "a5", "a6"))
  rail_on = sum(_euclid(bp["BUSN"], bp[c]) for c in ("a4", "a5", "a6"))
  print(f"[ALAP   ] bus-deep-feeder  BUSN ASAP L{ba['BUSN']} -> ALAP L{bd['BUSN']}  bus={buses}  "
        f"rail wire len {rail_off:.0f} -> {rail_on:.0f} (shorter)  overlap={len(bov)} body={bbc}")
  _check("BUSN" in buses, "bus-deep-feeder: BUSN lost its bus/rail status after demotion")
  _check(bd["BUSN"] == min(bd["a4"], bd["a5"], bd["a6"]) - 1,
         f"bus-deep-feeder: BUSN not at min-consumer-1 ({bd['BUSN']})")
  _check(rail_on < rail_off,
         f"bus-deep-feeder: demotion did not shorten the rail wire ({rail_on:.0f} !< {rail_off:.0f})")
  _check(not bov and bbc == 0, "bus-deep-feeder: demotion fought rail packing (overlap/body-cross)")


def real_erodeflow_check():
  """Real-graph check. The live erodeflow DSL elaborates only post-appinit on the GPU
  thread (T.loop / LoopModule reshape), so it cannot load in this pure-CPU headless
  session — attempt the live dflow adapter (reporting the skip reason honestly) and fall
  back to the sanctioned pure-CPU ECHO of erodeflow's pipeline, asserting each const lands
  ADJACENT (exactly one layer above) its deep consumer and quoting positions."""
  try:
    from orkengine import core            # noqa
    from orkengine import lev2            # noqa
    from orkengine import ecs             # noqa
    from ork.hypergraph.dflow.terrain.resolve import resolve_dsl_file, load_dsl_class
    cls = load_dsl_class(resolve_dsl_file("erodeflow"))
    gd = cls().generatedflow()
    g = gl.graph_from_dflow(gd)
    pos, wps, _ = gl.layout_ex(g, grid=GRID)
    print(f"[REAL   ] erodeflow(dflow LIVE) n={len(g.nodes)} e={len(g.edges)} (live elaboration ok)")
    return True
  except Exception as ex:
    print(f"[REAL   ] erodeflow(dflow LIVE) SKIPPED — elaborate() needs post-appinit GPU "
          f"thread headless: {ex!r}")
  g = erodeflow_echo()
  alap = gl.assign_layers(g)
  gl.alap_demote_feeders(g.nodes, g.edges, alap)
  pos = gl.layout(g, grid=GRID)
  for (c, cons, _l) in _ERODE_CONSTS:
    adj = (alap[c] == alap[cons] - 1)
    print(f"[REAL   ] erodeflow-echo   {c:8s} @ ({pos[c][0]:.0f},{pos[c][1]:.0f}) L{alap[c]}  "
          f"consumer {cons} @ ({pos[cons][0]:.0f},{pos[cons][1]:.0f}) L{alap[cons]}  adjacent={adj}")
    _check(adj, f"erodeflow-echo real: {c} not adjacent to {cons} (L{alap[c]} vs L{alap[cons]})")
  return False


################################################################################
# PORT-ordering oracle battery
################################################################################

def run_port_gate():
  """Prove the display-level port (plug) ordering pass: the crossed fixture drops from 1
  canonical port crossing to 0; the no-improve fixture keeps canonical order EXACTLY
  (strict-improvement-only). Both checked for determinism + input-order independence."""
  gcx = port_crossed()
  p, w, po = gl.layout_ex(gcx, grid=GRID)
  can = gl.count_port_crossings(p, gcx.edges, gcx.sizes)
  ref = gl.count_port_crossings(p, gcx.edges, gcx.sizes, port_orders=po)
  print(f"[PORT   ] crossed          canonical={can} -> refined={ref}  J={po.get('J')}")
  _check(can == 1, f"port_crossed: canonical port crossings {can} != 1 (fixture not crossed)")
  _check(ref == 0, f"port_crossed: refined port crossings {ref} != 0 (pass failed)")

  gni = port_noimprove()
  p2, w2, po2 = gl.layout_ex(gni, grid=GRID)
  can2 = gl.count_port_crossings(p2, gni.edges, gni.sizes)
  ref2 = gl.count_port_crossings(p2, gni.edges, gni.sizes, port_orders=po2)
  jin = po2.get("J", {}).get("inputs")
  print(f"[PORT   ] no-improve       canonical={can2} refined={ref2}  J.inputs={jin} (canonical kept)")
  _check(can2 == 0, f"port_noimprove: canonical already {can2} != 0")
  _check(ref2 == 0, f"port_noimprove: refined {ref2} != 0")
  _check(jin == ["a", "b"], f"port_noimprove: canonical order not preserved exactly -> {jin}")

  # determinism + order invariance of the port order map (crossed fixture)
  base = _freeze_po(po)
  _check(all(_freeze_po(gl.layout_ex(gcx, grid=GRID)[2]) == base for _ in range(3)),
         "port order non-deterministic")
  for si in range(4):
    pn = list(gcx.original_order); random.Random(si * 3 + 1).shuffle(pn)
    pe = list(gcx.edges); random.Random(si * 3 + 2).shuffle(pe)
    g2 = gl.graph_from_edges(pn, pe, gcx.sizes)
    _check(_freeze_po(gl.layout_ex(g2, grid=GRID)[2]) == base,
           f"port order input-order-dependent (perm {si})")


################################################################################
# Live dflow adapter demonstration (voronoi terrain) — guarded / optional
################################################################################

def live_voronoi():
  try:
    from orkengine import core            # noqa: core before lev2
    from orkengine import lev2            # noqa
    from orkengine import ecs             # noqa: full class registration
    from ork.hypergraph.dflow.terrain.resolve import resolve_dsl_file, load_dsl_class
    cls = load_dsl_class(resolve_dsl_file("voronoi"))
    graphdata = cls().generatedflow()
    g = gl.graph_from_dflow(graphdata)
    pos = gl.layout(g, grid=GRID)
    cr = gl.count_crossings(pos, g.edges, g.sizes)
    gv = gl.grid_violations(pos, GRID)
    ov = gl.overlaps(pos, g.sizes, gutter=1.0)
    print(f"[LIVE   ] voronoi(dflow)   n={len(g.nodes)} e={len(g.edges)} CROSS auto={cr} "
          f"grid_bad={len(gv)} overlap={len(ov)}  (graph_from_dflow on a live GraphData)")
    _check(not gv and not ov and cr == 0, "voronoi live-adapter: grid/overlap/crossing check")
    return True
  except Exception as ex:
    print(f"[LIVE   ] voronoi(dflow)   SKIPPED — live load unavailable headless: {ex!r}")
    return False


################################################################################
# Visual artifact: three panels of the fireball BUS case — naive / slice-A / A2.
################################################################################

def _draw_panel(ax, Rectangle, graph, pos, waypoints, buses, title, bc, wc):
  for (u, v) in graph.edges:
    poly = gl._edge_polyline(u, v, pos, graph.sizes, "vertical", waypoints)
    routed = bool((waypoints or {}).get((u, v)))
    col = "#e08a2a" if (u in buses or v in buses) else "#3a7ad0"
    ax.plot([p[0] for p in poly], [p[1] for p in poly], "-", color=col,
            lw=1.4 if routed else 1.1, alpha=0.85, zorder=1)
  for n in graph.nodes:
    x, y = pos[n]
    w, h = graph.sizes[n]
    face = "#4a3320" if n in buses else "#30343f"
    edge = "#e0a050" if n in buses else "#8fb4ff"
    ax.add_patch(Rectangle((x, y), w, h, facecolor=face, edgecolor=edge, lw=1.2, zorder=2))
    ax.text(x + w * 0.5, y + h * 0.5, str(n), ha="center", va="center",
            color="#dfe6f2", fontsize=8, zorder=3)
  ax.set_title(f"{title}\n{bc} body crossings, {wc} wire crossings", fontsize=12)
  ax.set_aspect("equal")
  ax.invert_yaxis()                          # graph flows top->bottom
  ax.axis("off")


def render_fireball_artifact(graph, tag, out_path):
  try:
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from matplotlib.patches import Rectangle
  except Exception as ex:
    print(f"[visual ] SKIPPED (matplotlib unavailable: {ex!r})")
    return None

  layer = gl.assign_layers(graph)
  buses = set(gl._detect_buses(graph.nodes, graph.edges, layer))

  naive = gl.layout_naive(graph, grid=GRID)
  # slice-A equivalent: A2 algorithm with the bus side-rail suppressed (bus stays in
  # the flow band, straight wires cut through the operator bodies) — the pre-A2 shape.
  saved = gl.BUS_LAYER_SPAN
  try:
    gl.BUS_LAYER_SPAN = 1 << 30
    slicea = gl.layout(graph, grid=GRID)
  finally:
    gl.BUS_LAYER_SPAN = saved
  a2, wps, _po = gl.layout_ex(graph, grid=GRID)

  panels = [
      (naive, None, "NAIVE (input order, bus above)"),
      (slicea, None, "slice-A (bus in band, wires cross bodies)"),
      (a2, wps, "A2 (bus railed, channel-routed)"),
  ]
  fig, axes = plt.subplots(1, 3, figsize=(18, 9))
  for ax, (pos, wp, title) in zip(axes, panels):
    bc = gl.count_body_crossings(pos, graph.edges, graph.sizes, waypoints=wp)
    wc = gl.count_crossings(pos, graph.edges, graph.sizes, waypoints=wp)
    _draw_panel(ax, Rectangle, graph, pos, wp, buses, title, bc, wc)
  fig.suptitle(f"graph_layout: {tag}  (bus={sorted(buses)})", fontsize=15)
  fig.tight_layout(rect=(0, 0, 1, 0.96))
  fig.savefig(out_path, dpi=90)
  plt.close(fig)
  nb = gl.count_body_crossings(naive, graph.edges, graph.sizes)
  sb = gl.count_body_crossings(slicea, graph.edges, graph.sizes)
  ab = gl.count_body_crossings(a2, graph.edges, graph.sizes, waypoints=wps)
  print(f"[visual ] wrote {out_path}  (body crossings: naive {nb} / slice-A {sb} / A2 {ab})")
  return out_path


def render_transpose_artifact(graph, tag, out_path):
  """Before/after panels of the owner's diamond trap: median-only (barycenter stalled
  one crossing above optimal) vs the TRANSPOSE refinement (0)."""
  try:
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from matplotlib.patches import Rectangle
  except Exception as ex:
    print(f"[visual ] SKIPPED transpose artifact (matplotlib unavailable: {ex!r})")
    return None

  buses = set()   # no bus in this diamond; keep the shared panel drawer happy
  off_pos, off_wps, _ = gl.layout_ex(graph, grid=GRID, transpose=False)
  on_pos, on_wps, _ = gl.layout_ex(graph, grid=GRID, transpose=True)
  panels = [
      (off_pos, off_wps, "median-only (TRANSPOSE off)"),
      (on_pos, on_wps, "TRANSPOSE refinement (on)"),
  ]
  fig, axes = plt.subplots(1, 2, figsize=(13, 8))
  for ax, (pos, wp, title) in zip(axes, panels):
    bc = gl.count_body_crossings(pos, graph.edges, graph.sizes, waypoints=wp)
    wc = gl.count_crossings(pos, graph.edges, graph.sizes, waypoints=wp)
    _draw_panel(ax, Rectangle, graph, pos, wp, buses, title, bc, wc)
  fig.suptitle(f"graph_layout TRANSPOSE: {tag}", fontsize=15)
  fig.tight_layout(rect=(0, 0, 1, 0.95))
  fig.savefig(out_path, dpi=90)
  plt.close(fig)
  wc_off = gl.count_crossings(off_pos, graph.edges, graph.sizes, waypoints=off_wps)
  wc_on = gl.count_crossings(on_pos, graph.edges, graph.sizes, waypoints=on_wps)
  print(f"[visual ] wrote {out_path}  (wire crossings: median-only {wc_off} -> transpose {wc_on})")
  return out_path


def render_alap_artifact(graph, tag, out_path):
  """Before/after panels of the erodeflow-shaped fixture: ALAP off (the two consts parked
  on layer 0 with long lonely wires down to their deep consumers) vs ALAP on (each const
  slid to consumer_layer-1, hugging it). The consts are highlighted (reusing the bus/
  highlight colour) so the wire-length collapse reads at a glance."""
  try:
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from matplotlib.patches import Rectangle
  except Exception as ex:
    print(f"[visual ] SKIPPED ALAP artifact (matplotlib unavailable: {ex!r})")
    return None

  hl = {c for (c, _cons, _l) in _ERODE_CONSTS}      # highlight the const feeders
  saved = gl.ALAP_FEEDER_DEMOTE
  try:
    gl.ALAP_FEEDER_DEMOTE = False
    off_pos, off_wps, _ = gl.layout_ex(graph, grid=GRID)
  finally:
    gl.ALAP_FEEDER_DEMOTE = saved
  on_pos, on_wps, _ = gl.layout_ex(graph, grid=GRID)
  panels = [
      (off_pos, off_wps, "ALAP off (consts parked on layer 0 — lala land)"),
      (on_pos, on_wps, "ALAP on (consts demoted to consumer_layer-1)"),
  ]
  fig, axes = plt.subplots(1, 2, figsize=(14, 9))
  for ax, (pos, wp, title) in zip(axes, panels):
    bc = gl.count_body_crossings(pos, graph.edges, graph.sizes, waypoints=wp)
    wc = gl.count_crossings(pos, graph.edges, graph.sizes, waypoints=wp)
    _draw_panel(ax, Rectangle, graph, pos, wp, hl, title, bc, wc)
  fig.suptitle(f"graph_layout ALAP feeder demotion: {tag}", fontsize=15)
  fig.tight_layout(rect=(0, 0, 1, 0.95))
  fig.savefig(out_path, dpi=90)
  plt.close(fig)

  def wlen(p):                                       # total const->consumer wire length
    return sum(_euclid(p[c], p[cons]) for (c, cons, _l) in _ERODE_CONSTS)
  print(f"[visual ] wrote {out_path}  (const wire length: off {wlen(off_pos):.0f} -> on {wlen(on_pos):.0f})")
  return out_path


################################################################################
# main
################################################################################

def main():
  print("=" * 78)
  print("graph_layout_gate — layered DAG auto-layout oracles (grid=%.0f)" % GRID)
  print("=" * 78)

  # --- real saved dflow graph via the engine-free .orj adapter --------------
  #     test6.orj's GLOB module is a real bus (feeds 3 distinct layers) -> rail.
  orj = None
  try:
    orj = load_orj("test6.orj")
    run_graph("test6.orj", orj, expect_body_zero=True, kind="REAL")
  except Exception as ex:
    _FAILS.append(f"test6.orj adapter failed: {ex!r}")
    print("  FAIL test6.orj:", repr(ex))

  # --- live dflow adapter (voronoi) -----------------------------------------
  live_voronoi()

  # --- real-topology fixtures (live assets need GPU headless) ---------------
  # the fireball BUS is the owner's complaint case (POOL crosses node bodies);
  # the fireball CHAIN is the no-bus regression baseline (stays one rail).
  run_graph("fireball-bus", fireball_bus(), expect_body_zero=True,
            expect_chain=True, kind="REAL*")
  run_graph("fireball-chain", fireball_chain(), expect_zero=True, expect_chain=True,
            expect_body_zero=True, kind="REAL*")
  run_graph("roads-layout", roads_layout_topology(), expect_body_zero=True, kind="REAL*")

  # --- synthetic battery ----------------------------------------------------
  run_graph("diamond", diamond(), expect_zero=True, expect_body_zero=True)
  run_graph("bintree4", binary_tree(4), expect_zero=True, expect_body_zero=True)
  run_graph("butterfly", butterfly(), expect_body_zero=True)
  run_graph("K3,3", k33(), expect_body_zero=True)
  big = seeded_layered_dag()
  run_graph("layered-dag-big", big, expect_body_zero=True)

  # --- ALAP feeder-demotion fixtures (A5) run through the full battery --------
  # the erodeflow ECHO is the owner's complaint case (const_0/const_1 stranded on layer 0);
  # the feeder-chain proves a single-path prefix demotes as a unit. Both must stay body-0.
  run_graph("erodeflow-echo", erodeflow_echo(), expect_body_zero=True, kind="REAL*")
  run_graph("feeder-chain", feeder_chain(), expect_body_zero=True)

  # --- TRANSPOSE A/B proof (owner's diamond trap) + real-graph diamond check --
  print("-" * 78)
  run_transpose_ab("owner-diamond", owner_diamond_trap(), expect_off=1, expect_on=0)
  real_xxx3_check()

  # --- ALAP A/B proof (erodeflow-shaped) + real erodeflow check ---------------
  print("-" * 78)
  run_alap_gate()
  real_erodeflow_check()

  # --- PORT (plug) ordering oracle ------------------------------------------
  run_port_gate()

  # --- stability smoke ------------------------------------------------------
  print("-" * 78)
  stability_smoke(fireball_chain(), "fireball-chain")
  if orj is not None:
    stability_smoke(orj, "test6.orj")

  # --- visual artifacts -----------------------------------------------------
  art = os.path.join(_WORKTREE, "graph_layout_naive_vs_auto.png")
  render_fireball_artifact(fireball_bus(), "fireball bus (POOL feeds every operator)", art)
  art2 = os.path.join(_WORKTREE, "graph_layout_transpose_diamond.png")
  render_transpose_artifact(owner_diamond_trap(),
                            "owner diamond (loop_1->[lpf_2->remap_12 | remap_11]->comb_8)", art2)
  art3 = os.path.join(_WORKTREE, "graph_layout_alap_erodeflow.png")
  render_alap_artifact(erodeflow_echo(),
                       "erodeflow echo (const_0/const_1 feed deep operators)", art3)

  print("=" * 78)
  if _FAILS:
    print(f"GRAPH_LAYOUT_GATE: FAIL ({len(_FAILS)} check(s))")
    for f in _FAILS:
      print("   -", f)
    sys.exit(1)
  print("GRAPH_LAYOUT_GATE: PASS")


if __name__ == "__main__":
  main()
