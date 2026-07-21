################################################################################
# graph_layout — layered DAG auto-layout (Sugiyama family) for dataflow graphs.
#
# A PURE, model-agnostic graph -> positions module: no lev2, no GPU, stdlib only,
# so it is unit-testable headless and reusable by any node-editor surface. The
# node editor (ork.ui.node_editor) binds THIS module for both its initial seed and
# the interactive Shift+A relayout (through the adapters below); it is the full
# engine — long-edge virtual nodes, iterated median + TRANSPOSE crossing
# minimisation, priority-method coordinate alignment, grid quantisation, and a
# display-level port (plug) ordering pass.
#
# ORIENTATION: the live canvas is VERTICAL (Houdini network-editor style — inputs
# on a node's TOP edge, outputs on its BOTTOM edge, flow top->bottom; see
# ork.ui.node_editor_math header). We MATCH that: the FLOW axis is Y (layers stack
# downward, sources on the first/top rail), the CROSS axis is X (ordering + rail
# alignment within a layer). "horizontal" (flow along X) is supported symmetrically.
#
# The algorithm is DETERMINISTIC (house law: same graph -> byte-identical layout):
# the neutral graph is canonicalised (nodes/edges sorted by id); every tie is
# broken on node id; there is zero randomness. Input enumeration order changes
# nothing (the naive baseline is the only order-sensitive path, by design).
#
# ADAPTERS (build the neutral LayoutGraph the later canvas slice reuses):
#   graph_from_edges(...)   — trivial, from an explicit node/edge list
#   graph_from_model(...)   — from a duck-typed ork.ui.node_editor.NodeGraphModel
#   graph_from_dflow(...)   — from a LIVE orkengine dataflow.GraphData
#   graph_from_orj(...)     — from a serialized dflow .orj JSON (engine-free)
#
# Copyright 1996-2023, Michael T. Mayers. Distributed under the MIT License.
################################################################################

import json
import math

################################################################################
# Geometry defaults (graph-space pre-zoom px). Mirror ork.ui.node_editor_math so a
# layout produced here drops onto the live canvas at the same scale; every value a
# caller might tune is a keyword arg (never baked).
################################################################################

GRID_DEFAULT     = 32.0    # GRID_SPACING — final coords are multiples of this
NODE_W           = 150.0   # default tile width  (NODE_W)
NODE_H           = 46.0    # default tile height (NODE_H)
CROSS_GUTTER     = 48.0    # min clear gap between siblings within a layer
LAYER_GUTTER     = 64.0    # min clear gap added between layers along the flow axis
COMPONENT_GUTTER = 96.0    # gap between disconnected subgraphs (laid side-by-side)
LAYOUT_X0        = 96.0    # first-rail cross origin
LAYOUT_Y0        = 64.0    # first-layer flow origin

# long-edge routing: a virtual (dummy) waypoint occupies a real, grid-quantised
# CHANNEL in the cross axis (not a zero-width slot) so multi-layer wires route
# through gaps beside real tiles instead of over their bodies.
CHANNEL_W        = GRID_DEFAULT   # cross-axis footprint of a long-edge waypoint

# BUS side-rail: a node whose out-edges reach BUS_LAYER_SPAN-or-more DISTINCT
# layers is a broadcast/"bus" node (e.g. a particle POOL feeding every operator).
# Routing its long edges through the main band drags them across intervening node
# bodies; instead the node is lifted onto a dedicated RAIL column to the right of
# the flow band and its wires run DOWN that clear channel. Purely structural — no
# name-based special cases.
BUS_LAYER_SPAN   = 3       # >= this many distinct successor layers => rail the node
RAIL_GUTTER      = 64.0    # clear gap between the main band's right edge and rail 0
RAIL_PITCH       = 96.0    # clear gap between adjacent stacked rail columns

# TRANSPOSE refinement (Sugiyama): once the median ordering has converged, greedily
# swap adjacent same-layer node pairs whenever a swap STRICTLY lowers the TOTAL
# crossing count (long-edge dummies included) and re-sweep until a full pass makes no
# swap. The count is monotone-decreasing so the loop always terminates; this bound
# only guards against a float-tie pathology.
TRANSPOSE_MAX_SWEEPS = 64

# ALAP feeder demotion: longest-path (ASAP) layering parks EVERY in-degree-0 source on
# the first rail, so a constant feeding a deep consumer strands far above it (long lonely
# wires — the owner's const_0/const_1 "lala land"). After layering we slide pure-feeder
# prefixes DOWN to hug the nodes they feed (as late as possible while staying strictly
# above every consumer). Structural, deterministic, off-critical-path only. This module
# global is the gate's A/B knob (flip False to reproduce the pre-fix parked-source shape).
ALAP_FEEDER_DEMOTE = True

_BIG = 1.0e18


def _key(n):
  """Total order used for every deterministic tiebreak. Node ids are hashable;
  strings in practice — str() gives a stable, input-order-independent order."""
  return str(n)


def _q_up(v, grid):
  """Smallest multiple of `grid` >= v (grid-quantise, rounding up)."""
  return math.ceil(v / grid - 1e-9) * grid


def _q_near(v, grid):
  """Nearest multiple of `grid` to v (ties round up, deterministically)."""
  return math.floor(v / grid + 0.5) * grid


################################################################################
# Neutral graph description
################################################################################

class LayoutGraph:
  """Model-agnostic graph the layout engine consumes. Directed edges are node
  level (src_id -> dst_id); node sizes are (w, h) graph-space tiles.

  Canonicalised on construction (nodes + edges sorted by id) so layout() is
  independent of the order ids/edges were handed in; `original_order` retains the
  caller's enumeration order purely to drive the naive baseline."""

  def __init__(self, node_ids, edges, node_sizes=None, default_size=(NODE_W, NODE_H)):
    order = list(dict.fromkeys(node_ids))          # de-dup, keep first-seen order
    seen = set(order)
    norm = []
    for e in edges:
      u, v = e[0], e[1]
      for x in (u, v):
        if x not in seen:                          # endpoints referenced but not listed
          seen.add(x)
          order.append(x)
      if u != v:                                   # drop self loops (no layered meaning)
        norm.append((u, v))
    self.original_order = order
    self.nodes = sorted(order, key=_key)
    self.edges = sorted(set(norm), key=lambda e: (_key(e[0]), _key(e[1])))
    ns = dict(node_sizes or {})
    self.sizes = {n: tuple(ns.get(n, default_size)) for n in self.nodes}

  # -- derived views ---------------------------------------------------------

  def succ(self):
    s = {n: [] for n in self.nodes}
    for u, v in self.edges:
      s[u].append(v)
    return s

  def pred(self):
    p = {n: [] for n in self.nodes}
    for u, v in self.edges:
      p[v].append(u)
    return p


################################################################################
# Adapters -> LayoutGraph
################################################################################

def graph_from_edges(node_ids, edges, node_sizes=None, default_size=(NODE_W, NODE_H)):
  """Neutral constructor from an explicit node list + (src,dst) edge list."""
  return LayoutGraph(node_ids, edges, node_sizes, default_size)


def _size_for_ports(n_in, n_out, orient):
  """Default tile size from port counts, matching the canvas: VERTICAL tiles are a
  fixed NODE_W x NODE_H; HORIZONTAL tiles grow in height with the port-row count."""
  if orient == "vertical":
    return (NODE_W, NODE_H)
  rows = max(1, n_in, n_out)
  return (NODE_W, 22.0 + rows * 18.0 + 8.0)        # header + rows + pad (node_editor_math)


def graph_from_model(model, orient="vertical", size_fn=None):
  """Build from a duck-typed ork.ui.node_editor.NodeGraphModel: nodes(),
  inputs(id)/outputs(id) -> [(plug,type)..], edges() -> (src,sp,dst,dp). This is
  the adapter the later canvas slice calls to place a live editor graph."""
  ids = list(model.nodes())
  sizes = {}
  for nid in ids:
    n_in = len(model.inputs(nid))
    n_out = len(model.outputs(nid))
    sizes[nid] = size_fn(nid, n_in, n_out) if size_fn else _size_for_ports(n_in, n_out, orient)
  edges = [(s, d) for (s, _sp, d, _dp) in model.edges()]
  return LayoutGraph(ids, edges, sizes)


def graph_from_dflow(graphdata, orient="vertical", size_fn=None, default_size=None):
  """Build from a LIVE orkengine dataflow.GraphData. Nodes come from the reflection
  JSON Modules map (with any edge-only endpoints unioned in as a self-defence);
  edges come from GraphData.edges() (the engine's typed-edge introspection). Port
  counts (for HORIZONTAL sizing) come from each module's serialized plug arrays."""
  nodes, portcount = _dflow_nodes(graphdata)
  edges = []
  for e in graphdata.edges():
    om, im = e["out_module"], e["in_module"]
    for x in (om, im):
      if x not in nodes:                           # endpoint absent from Modules map
        nodes.append(x)
        portcount.setdefault(x, (0, 0))
    edges.append((om, im))
  ds = default_size or _size_for_ports(0, 0, orient)
  sizes = {}
  for n in nodes:
    ni, no = portcount.get(n, (0, 0))
    sizes[n] = size_fn(n, ni, no) if size_fn else _size_for_ports(ni, no, orient)
  return LayoutGraph(nodes, edges, sizes, ds)


def _dflow_nodes(graphdata):
  """(ordered node names, {name:(n_in,n_out)}) parsed from a GraphData's reflection
  JSON. Returns empty maps gracefully (some elaborated graphs serialize no Modules —
  callers union the edge endpoints back in)."""
  nodes, ports = [], {}
  try:
    root = json.loads(graphdata.serializeJson())
    mods = root.get("root", {}).get("object", {}).get("properties", {}).get("Modules", {})
    for name, entry in mods.items():
      props = entry.get("object", {}).get("properties", {})
      ni = len(props.get("inputs", []) or [])
      no = len(props.get("outputs", []) or [])
      nodes.append(name)
      ports[name] = (ni, no)
  except Exception:
    pass
  return nodes, ports


def graph_from_orj(path_or_obj, orient="vertical", size_fn=None):
  """Build from a serialized dflow graph (.orj) WITHOUT the engine: parse the
  Modules map (nodes + plug-array port counts) and the zzz_connections map
  (out_module -> in_module edges). Accepts a path, a JSON string, or a parsed dict."""
  if isinstance(path_or_obj, dict):
    root = path_or_obj
  elif isinstance(path_or_obj, str) and path_or_obj.lstrip().startswith("{"):
    root = json.loads(path_or_obj)
  else:
    with open(path_or_obj) as f:
      root = json.load(f)
  props = root["root"]["object"]["properties"]
  mods = props.get("Modules", {}) or {}
  nodes, ports = [], {}
  for name, entry in mods.items():
    p = entry.get("object", {}).get("properties", {})
    nodes.append(name)
    ports[name] = (len(p.get("inputs", []) or []), len(p.get("outputs", []) or []))
  edges = []
  conns = props.get("zzz_connections", {}) or {}
  n = int(conns.get("numlinks", 0))
  for i in range(n):
    c = conns.get("conn-%d" % i)
    if c:                                          # .orj link keys: out_module -> inp_module
      edges.append((c["out_module"], c["inp_module"]))
  sizes = {}
  for nd in nodes:
    ni, no = ports[nd]
    sizes[nd] = size_fn(nd, ni, no) if size_fn else _size_for_ports(ni, no, orient)
  return LayoutGraph(nodes, edges, sizes)


################################################################################
# Layering (longest-path from sources; cycle-safe)
################################################################################

def assign_layers(graph):
  """layer[n] = longest path (in edges) from any source to n. Sources (no
  predecessors) are layer 0. Cycle-safe: a back-edge contributes nothing (the
  graph is a DAG by contract; this just refuses to loop forever)."""
  pred = graph.pred()
  layer = {}
  visiting = set()

  def depth(n):
    if n in layer:
      return layer[n]
    if n in visiting:                              # back-edge -> break the cycle
      return 0
    visiting.add(n)
    d = 0
    for p in sorted(pred[n], key=_key):
      d = max(d, depth(p) + 1)
    visiting.discard(n)
    layer[n] = d
    return d

  for n in graph.nodes:
    depth(n)
  return layer


def alap_demote_feeders(nodes, edges, layer):
  """AS-LATE-AS-POSSIBLE demotion of leaf feeders + their single-path prefixes, in place.

  Longest-path (ASAP) layering parks every in-degree-0 node on layer 0, so a const wired
  to a deep consumer strands far above it. This slides such FEEDERS down to hug what they
  feed. A FEEDER is:
    - a SOURCE: in-degree 0 (a const / leaf feeder), single- OR multi-consumer; OR
    - a CHAIN node: in-degree 1 whose sole predecessor p is itself a feeder AND has
      out-degree 1 — i.e. p->n is a PRIVATE single-path edge (the 'const -> math ->
      consumer, whole prefix single-path' case). A fan-out (a source feeding several
      operators) stops the chain: the fanned-to nodes are consumers, not prefix, so a
      real join like the loop_1/comb_8 diamond is NOT swept in.
  Each feeder with a downstream consumer moves to layer = min(consumer layer) - 1 (as
  late as possible while strictly above every consumer). Processed DEEPEST-first (reverse
  ASAP), so a whole single-path prefix cascades down as a UNIT — each feeder reads its
  already-demoted successors. A feeder with no slack (a straight chain: the fireball
  chain, the diamond spine, a balanced tree) is a no-op, so those layouts stay byte-
  identical.

  BUS interplay: a broadcast source is demoted too, but _detect_buses keys off the
  CONSUMER layers (never the bus's own), so its rail treatment is untouched and its rail
  wire simply runs shorter. ALAP only ever INCREASES a layer, so it can never invert an
  edge (asserted); cycle back-edges (assign_layers already broke them) are ignored
  (forward successors only)."""
  if not ALAP_FEEDER_DEMOTE:
    return
  pred = {n: [] for n in nodes}
  succ = {n: [] for n in nodes}
  for (u, v) in edges:
    if u == v:
      continue                                       # self-loop: no layered meaning
    succ[u].append(v)
    pred[v].append(u)
  orig = dict(layer)
  # feeder set: decide in ASAP topo order so every predecessor is resolved first; a
  # not-yet-resolved pred (cycle back-edge) counts as non-feeder -> stays conservative.
  feeder = {}
  for n in sorted(nodes, key=lambda z: (orig[z], _key(z))):
    ps = pred[n]
    if not ps:
      feeder[n] = True                               # source / const
    elif len(ps) == 1:
      p = ps[0]
      feeder[n] = feeder.get(p, False) and len(succ[p]) == 1   # private single-path edge
    else:
      feeder[n] = False                              # a join is a consumer, not a feeder
  # ALAP slide: deepest feeder first (reverse ASAP) so successors are finalised first.
  for n in sorted((z for z in nodes if feeder[z]), key=lambda z: (-orig[z], _key(z))):
    below = [layer[c] for c in succ[n] if layer[c] > layer[n]]   # forward succs only
    if not below:
      continue                                       # feeder-sink / isolated -> stays put
    target = min(below) - 1
    assert target >= layer[n], \
        "ALAP would promote feeder %r (%d -> %d)" % (n, layer[n], target)
    layer[n] = target
  for (u, v) in edges:                               # invariant: no new layer inversion
    if orig[u] < orig[v]:
      assert layer[u] < layer[v], \
          "ALAP inverted edge %r->%r (layers %d,%d)" % (u, v, layer[u], layer[v])


################################################################################
# Weakly-connected components (laid out independently, then side-by-side)
################################################################################

def _components(graph):
  parent = {n: n for n in graph.nodes}

  def find(a):
    while parent[a] != a:
      parent[a] = parent[parent[a]]
      a = parent[a]
    return a

  for u, v in graph.edges:
    ru, rv = find(u), find(v)
    if ru != rv:
      parent[ru] = rv
  groups = {}
  for n in graph.nodes:
    groups.setdefault(find(n), []).append(n)
  comps = [sorted(members, key=_key) for members in groups.values()]
  comps.sort(key=lambda m: _key(m[0]))             # deterministic component order
  return comps


################################################################################
# Crossing minimisation (median ordering sweeps + transpose), on the dummy-
# expanded layered graph.
################################################################################

def _build_ordered(nodes, edges, layer, sizes, orient):
  """Insert virtual (dummy) nodes for edges spanning >1 layer and build the initial
  per-layer node ordering (by id — the deterministic seed). Returns
  (layers, up, down, width, dummies, edge_dummies) where up/down are per-node
  neighbour lists in the EXPANDED graph, width is the cross-axis size of each
  node/dummy, and edge_dummies maps each original (u,v) edge to its dummy chain in
  u->v flow order (so a caller can materialise the routed polyline)."""
  up = {n: [] for n in nodes}                      # neighbours one layer up (preds)
  down = {n: [] for n in nodes}                    # neighbours one layer down (succs)
  width = {}
  for n in nodes:
    w, h = sizes[n]
    width[n] = w if orient == "vertical" else h
  dummies = set()
  edge_dummies = {}
  dcount = [0]

  def add_dummy(ly):
    did = ("\x00dummy", dcount[0])
    dcount[0] += 1
    dummies.add(did)
    up[did] = []
    down[did] = []
    width[did] = CHANNEL_W                          # grid-quantised routing channel
    layer[did] = ly
    return did

  for (u0, v0) in sorted(edges, key=lambda e: (_key(e[0]), _key(e[1]))):
    u, v = u0, v0
    lu, lv = layer[u], layer[v]
    if lu == lv:
      continue                                     # intra-layer (cycle artifact): skip routing
    flipped = lv < lu
    if flipped:
      u, v = v, u
      lu, lv = lv, lu
    if lv - lu == 1:
      down[u].append(v)
      up[v].append(u)
    else:
      chain = []
      prev = u
      for ly in range(lu + 1, lv):                 # a dummy per crossed layer
        d = add_dummy(ly)
        chain.append(d)
        down[prev].append(d)
        up[d].append(prev)
        prev = d
      down[prev].append(v)
      up[v].append(prev)
      edge_dummies[(u0, v0)] = list(reversed(chain)) if flipped else chain

  all_nodes = list(nodes) + sorted(dummies, key=lambda d: d[1])
  max_layer = max(layer.values(), default=0)
  layers = [[] for _ in range(max_layer + 1)]
  for n in sorted(all_nodes, key=_key):            # id-sorted initial order (deterministic)
    layers[layer[n]].append(n)
  return layers, up, down, width, dummies, edge_dummies


def _order_pos(layers):
  return {n: i for layer in layers for i, n in enumerate(layer)}


def _count_layer_crossings(upper, lower, pos_lower, adjacency_down):
  """Crossings between two adjacent layers given fixed orders. `adjacency_down`
  maps an upper node -> its lower neighbours. Standard inversion count: for edge
  endpoint sequences read left-to-right across the upper layer, count pairs whose
  lower endpoints are out of order (Bubble/BIT count, O(E log E) via a simple
  O(E^2) here — layers are tiny)."""
  seq = []
  for u in upper:
    for w in sorted(adjacency_down.get(u, []), key=lambda x: pos_lower[x]):
      seq.append(pos_lower[w])
  cr = 0
  for i in range(len(seq)):
    for j in range(i + 1, len(seq)):
      if seq[i] > seq[j]:
        cr += 1
  return cr


def _total_crossings(layers, down):
  pos = _order_pos(layers)
  adj = {u: down[u] for u in down}
  total = 0
  for li in range(len(layers) - 1):
    total += _count_layer_crossings(layers[li], layers[li + 1], pos, adj)
  return total


def _median_key(node, neighbours, pos):
  ns = sorted(pos[x] for x in neighbours)
  if not ns:
    return None
  m = len(ns) // 2
  if len(ns) % 2 == 1:
    return float(ns[m])
  return 0.5 * (ns[m - 1] + ns[m])


def _reorder(layers, up, down, sweeps, transpose=True):
  """Iterated median ordering (down then up sweeps), keeping the best (min crossing)
  ordering seen, then — if `transpose` — the TRANSPOSE refinement pass on the
  converged order. Median is interleaved with the refinement each sweep (the
  canonical Sugiyama loop, so the median phase already unwinds the easy tangles) and
  the pass runs once more AFTER convergence per the refinement's contract (idempotent
  when the winning sweep already reached the transpose fixpoint). Deterministic: nodes
  with no neighbour in the reference layer keep their slot; ties break on id. With
  transpose=False this is PURE barycenter ordering — the A/B baseline the gate scores
  the refinement against (median alone can stall one crossing short of optimal)."""
  best = [list(l) for l in layers]
  best_cr = _total_crossings(layers, down)
  for it in range(sweeps):
    ref = up if (it % 2 == 0) else down            # alternate down/up sweeps
    rng = range(1, len(layers)) if (it % 2 == 0) else range(len(layers) - 2, -1, -1)
    pos = _order_pos(layers)
    for li in rng:
      keyed = []
      for slot, n in enumerate(layers[li]):
        mk = _median_key(n, ref[n], pos)
        keyed.append((n, slot if mk is None else mk, slot))
      keyed.sort(key=lambda t: (t[1], t[2], _key(t[0])))
      layers[li] = [t[0] for t in keyed]
      pos = _order_pos(layers)
    if transpose:
      _transpose_pass(layers, up, down)
    cr = _total_crossings(layers, down)
    if cr < best_cr:
      best_cr = cr
      best = [list(l) for l in layers]
      if best_cr == 0:
        break
  if transpose:                                    # refinement AFTER median convergence
    best_cr = _transpose_pass(best, up, down)
  return best, best_cr


def _transpose_pass(layers, up, down):
  """Sugiyama TRANSPOSE refinement. Sweep every layer in a stable left-to-right scan;
  for each adjacent pair tentatively swap and KEEP the swap iff it STRICTLY lowers the
  TOTAL crossing count (long-edge dummies are ordinary nodes here and count). Repeat
  full sweeps until one makes no swap. This escapes median local-optima median cannot:
  median compresses each node's neighbour set to a single key, discarding the ordering
  information a crossing count retains, so an asymmetric dummy-chain weighting (e.g.
  the owner's loop_1 -> [lpf_2->remap_12 | remap_11] -> comb_8 diamond) can leave
  median stalled one crossing above optimal that a single adjacent swap removes.
  Deterministic: fixed scan order, strict-improvement only (ties revert -> stable), no
  randomness; the crossing count is monotone-decreasing so it terminates
  (TRANSPOSE_MAX_SWEEPS guards float-tie pathology). Returns the final crossing count.
  `up` is unused here (swapping needs only the count) but kept for call symmetry."""
  cur = _total_crossings(layers, down)
  for _ in range(TRANSPOSE_MAX_SWEEPS):
    improved = False
    for li in range(len(layers)):
      row = layers[li]
      for i in range(len(row) - 1):
        row[i], row[i + 1] = row[i + 1], row[i]    # tentative swap
        cand = _total_crossings(layers, down)
        if cand < cur:                             # STRICT total-crossing improvement
          cur = cand
          improved = True
        else:
          row[i], row[i + 1] = row[i + 1], row[i]  # revert (also on ties -> stable)
    if not improved:
      break
  return cur


################################################################################
# Cross-axis coordinate assignment (priority method: straighten runs; long-edge
# dummies get top priority so multi-layer edges come out straight).
################################################################################

def _assign_cross(layers, up, down, width, dummies, gutter):
  order_index = {}
  for layer in layers:
    for i, n in enumerate(layer):
      order_index[n] = i

  def sep(a, b):
    return (width[a] + width[b]) * 0.5 + gutter

  pos = {}
  for layer in layers:                             # initial left-pack per layer
    prev = None
    for n in layer:
      pos[n] = width[n] * 0.5 if prev is None else pos[prev] + sep(prev, n)
      prev = n

  def prio(n):
    return _BIG if n in dummies else (len(up[n]) + len(down[n]))

  def align(layer, refadj):
    locked = set()
    for n in sorted(layer, key=lambda z: (-prio(z), order_index[z], _key(z))):
      refs = [r for r in refadj[n] if r in pos]
      idx = order_index[n]
      if refs:
        des = _median_key(n, refs, pos)
        lb, acc = -_BIG, 0.0                       # nearest locked node to the left
        for j in range(idx - 1, -1, -1):
          acc += sep(layer[j], layer[j + 1])
          if layer[j] in locked:
            lb = pos[layer[j]] + acc
            break
        rb, acc = _BIG, 0.0                        # nearest locked node to the right
        for j in range(idx + 1, len(layer)):
          acc += sep(layer[j - 1], layer[j])
          if layer[j] in locked:
            rb = pos[layer[j]] - acc
            break
        pos[n] = min(max(des, lb), rb)
      locked.add(n)
      for j in range(idx - 1, -1, -1):             # push unlocked left neighbours aside
        if layer[j] in locked:
          break
        need = pos[layer[j + 1]] - sep(layer[j], layer[j + 1])
        if pos[layer[j]] > need:
          pos[layer[j]] = need
        else:
          break
      for j in range(idx + 1, len(layer)):         # push unlocked right neighbours aside
        if layer[j] in locked:
          break
        need = pos[layer[j - 1]] + sep(layer[j - 1], layer[j])
        if pos[layer[j]] < need:
          pos[layer[j]] = need
        else:
          break

  rounds = 6
  for _ in range(rounds):
    for li in range(1, len(layers)):
      align(layers[li], up)
    for li in range(len(layers) - 2, -1, -1):
      align(layers[li], down)
  return pos                                       # cross-axis CENTERS


################################################################################
# Bus detection (structural side-rail candidates)
################################################################################

def _detect_buses(nodes, edges, layer):
  """Nodes whose out-edges reach >= BUS_LAYER_SPAN DISTINCT layers — broadcast /
  "bus" nodes (a pool feeding every operator). Structural only: no name tests.
  Returned id-sorted (deterministic rail stacking order)."""
  succ = {n: [] for n in nodes}
  for (u, v) in edges:
    succ[u].append(v)
  buses = []
  for n in sorted(nodes, key=_key):
    if len({layer[v] for v in succ[n]}) >= BUS_LAYER_SPAN:
      buses.append(n)
  return buses


################################################################################
# Top-level layout
################################################################################

def layout(graph, grid=GRID_DEFAULT, orient="vertical", node_sizes=None,
           cross_gutter=CROSS_GUTTER, layer_gutter=LAYER_GUTTER,
           component_gutter=COMPONENT_GUTTER, sweeps=8, x0=LAYOUT_X0, y0=LAYOUT_Y0,
           _optimize=True, transpose=True):
  """Layered auto-layout. Returns {node_id: (x, y)} graph-space top-left positions.

  Every returned coordinate is a multiple of `grid`; nodes never overlap (min
  `cross_gutter`/`layer_gutter` clearance); chains come out straight; disconnected
  subgraphs are placed side-by-side across the cross axis with `component_gutter`;
  broadcast/"bus" nodes are lifted onto right-side rail columns. Deterministic and
  input-order-independent (see module header). Positions leave room for long-edge
  routing channels; the routed waypoints themselves are exposed by layout_ex().
  `transpose` gates the crossing-minimising TRANSPOSE refinement (default on; the
  gate flips it off for the median-only A/B baseline). The RETURN SHAPE is unchanged
  — a plain positions dict — so existing callers (the node-editor seed / Shift+A)
  are unaffected."""
  positions, _wp, _po = _layout_ex(graph, grid, orient, node_sizes, cross_gutter,
                                   layer_gutter, component_gutter, sweeps, x0, y0,
                                   _optimize, transpose)
  return positions


def layout_ex(graph, grid=GRID_DEFAULT, orient="vertical", node_sizes=None,
              cross_gutter=CROSS_GUTTER, layer_gutter=LAYER_GUTTER,
              component_gutter=COMPONENT_GUTTER, sweeps=8, x0=LAYOUT_X0, y0=LAYOUT_Y0,
              transpose=True):
  """As layout(), but returns (positions, waypoints, port_orders). `positions` is
  byte-identical to layout()'s return (real nodes only). `waypoints` maps each
  long/bus edge (u,v) to the ordered list of INTERMEDIATE routing points [(x,y),..]
  between the source's output anchor and the dest's input anchor — the canvas
  materialises the routed polyline as [src_out] + waypoints + [dst_in]; straight
  (single-layer, non-bus) edges are absent. `port_orders` is the display-level plug
  ordering (compute_port_orders): {node: {"inputs":[src_id..], "outputs":[dst_id..]}}
  giving the left-to-right plug slot order for every node with a non-trivial choice
  (the canvas slice draws plugs + routes wires to these slots later). ADDITIVE: the
  third tuple member is the only signature change vs the prior (positions, waypoints)
  return — node-level positions/waypoints are identical."""
  return _layout_ex(graph, grid, orient, node_sizes, cross_gutter, layer_gutter,
                    component_gutter, sweeps, x0, y0, True, transpose)


def _layout_ex(graph, grid, orient, node_sizes, cross_gutter, layer_gutter,
               component_gutter, sweeps, x0, y0, optimize, transpose=True):
  if node_sizes:
    graph = LayoutGraph(graph.nodes, graph.edges, {**graph.sizes, **node_sizes})
  if not graph.nodes:
    return {}, {}, {}
  gutter = _q_up(cross_gutter, grid)
  comp_gutter = _q_up(component_gutter, grid)

  # uniform layer pitch (flow axis), grid-quantised for a consistent inter-layer gap
  row_h = max((graph.sizes[n][1] if orient == "vertical" else graph.sizes[n][0])
              for n in graph.nodes)
  pitch = _q_up(row_h + layer_gutter, grid)

  positions = {}
  waypoints = {}
  cross_cursor = _q_up(x0 if orient == "vertical" else y0, grid)
  flow0 = _q_up(y0 if orient == "vertical" else x0, grid)

  for members in _components(graph):
    mset = set(members)
    sub_edges = [(u, v) for (u, v) in graph.edges if u in mset and v in mset]
    sizes = {n: graph.sizes[n] for n in members}
    cross_min, cross_max = _layout_component(
        members, sub_edges, sizes, grid, orient, gutter, pitch, row_h, flow0,
        cross_cursor, positions, waypoints, optimize, sweeps, graph.original_order,
        transpose)
    cross_cursor = _q_up(cross_max + comp_gutter, grid)
  # display-level plug ordering over the FINAL placement (positions unaffected).
  port_orders = compute_port_orders(positions, graph.edges, graph.sizes, orient)
  return positions, waypoints, port_orders


def _flow_center(ly, flow0, pitch, row_h):
  """Flow-axis center of a layer's node row (waypoint flow coordinate)."""
  return flow0 + ly * pitch + row_h * 0.5


def _cross_xy(cross, flowc, orient):
  return (float(cross), float(flowc)) if orient == "vertical" else (float(flowc), float(cross))


def _route_bus_edge(u, v, layer, col, sizes, orient, flow0, pitch, row_h):
  """Intermediate waypoints for a rail-routed bus edge: the wire runs along the bus
  node's rail COLUMN (cross coord `col`) through every layer strictly between the
  endpoints, then hops off the rail into the non-bus endpoint."""
  lu, lv = layer[u], layer[v]
  lo, hi = (lu, lv) if lu < lv else (lv, lu)
  inter = list(range(lo + 1, hi))
  if lu > lv:
    inter.reverse()                                # keep u -> v flow order
  return [_cross_xy(col, _flow_center(L, flow0, pitch, row_h), orient) for L in inter]


def _layout_component(members, edges, sizes, grid, orient, gutter, pitch, row_h,
                      flow0, cross_offset, out_positions, out_waypoints, optimize,
                      sweeps, original_order, transpose=True):
  layer = assign_layers(LayoutGraph(members, edges, sizes))
  # ALAP: slide pure-feeder prefixes (consts + their single-feed chains) DOWN to hug
  # their consumers before ordering/bus-detection (the naive baseline keeps ASAP so it
  # stays the honest 'parked source' reference). Bus detection reads the demoted layers.
  if optimize:
    alap_demote_feeders(members, edges, layer)
  # bus nodes are lifted OUT of the ordered band (main_nodes/main_edges exclude them)
  # onto dedicated rail columns below, so the median + TRANSPOSE ordering never touches
  # a railed node — the refinement operates strictly within the main band, rails hold.
  buses = _detect_buses(members, edges, layer) if optimize else []
  busset = set(buses)

  def _wx(n):                                      # cross-axis extent of a real node
    w, h = sizes[n]
    return w if orient == "vertical" else h

  # main band = the graph with bus nodes (and their edges) lifted out; bus edges are
  # routed separately on the rail so they never tangle the band's ordering.
  main_nodes = [n for n in members if n not in busset]
  main_edges = [(u, v) for (u, v) in edges if u not in busset and v not in busset]
  layers, up, down, width, dummies, edge_dummies = _build_ordered(
      main_nodes, main_edges, layer, sizes, orient)

  if optimize:
    layers, _cr = _reorder(layers, up, down, sweeps, transpose)
    centers = _assign_cross(layers, up, down, width, dummies, gutter)
  else:
    # naive baseline: order each layer by the caller's enumeration order, left-pack,
    # no straightening (this is the only order-sensitive path, by design).
    rank = {n: i for i, n in enumerate(original_order)}
    for li in range(len(layers)):
      layers[li].sort(key=lambda n: (rank.get(n, 1 << 30), _key(n)))
    centers = {}
    for layer_nodes in layers:
      prev = None
      for n in layer_nodes:
        centers[n] = width[n] * 0.5 if prev is None else \
            centers[prev] + (width[prev] + width[n]) * 0.5 + gutter
        prev = n

  # snap CROSS axis: per layer, quantise the leading edge, enforce grid-aligned
  # min-separation left-to-right (order preserved => chains stay on one rail).
  cross_left = {}
  for layer_nodes in layers:
    prev_right = None
    for n in layer_nodes:
      left = _q_near(centers[n] - width[n] * 0.5, grid)
      if prev_right is not None:
        left = max(left, _q_up(prev_right + gutter, grid))
      cross_left[n] = left
      prev_right = left + width[n]
  # normalise so the band's cross-min sits at cross_offset (grid-aligned)
  base = min((cross_left[n] for n in main_nodes), default=cross_offset)
  shift = cross_offset - base

  cross_min = _BIG
  cross_max = -_BIG

  def _place(n, lx, ly):
    flow = flow0 + ly * pitch                       # top-aligned within its layer
    out_positions[n] = _cross_xy(lx, flow, orient)

  for n in main_nodes:
    lx = cross_left[n] + shift
    _place(n, lx, layer[n])
    cross_min = min(cross_min, lx)
    cross_max = max(cross_max, lx + width[n])

  if optimize:
    # main-band long-edge waypoints (routed through the dummy channels)
    for (u, v), chain in edge_dummies.items():
      out_waypoints[(u, v)] = [
          _cross_xy(cross_left[d] + shift + width[d] * 0.5,
                    _flow_center(layer[d], flow0, pitch, row_h), orient)
          for d in chain]

  # ---- BUS SIDE-RAIL -------------------------------------------------------
  # each bus gets its own dedicated column to the RIGHT of the band, packed in stable
  # order (width-safe cumulative placement — columns never overlap, whatever the
  # node widths); the bus keeps its own flow (layer) position within its column.
  if buses:
    band_right = cross_max if cross_max > -_BIG else cross_offset  # (bus-only comp)
    railcol = {}
    rail_cursor = band_right + RAIL_GUTTER
    for b in buses:
      col_left = _q_up(rail_cursor, grid)
      _place(b, col_left, layer[b])
      railcol[b] = col_left + _wx(b) * 0.5          # rail column center (wire axis)
      cross_min = min(cross_min, col_left)
      cross_max = max(cross_max, col_left + _wx(b))
      rail_cursor = col_left + _wx(b) + RAIL_PITCH
    for (u, v) in edges:                            # route every bus-incident edge
      if u in busset or v in busset:
        col = railcol[u] if u in busset else railcol[v]
        out_waypoints[(u, v)] = _route_bus_edge(
            u, v, layer, col, sizes, orient, flow0, pitch, row_h)

  return cross_min, cross_max


################################################################################
# Display-level PORT (plug) ordering — additive, node-placement-preserving. Ordering
# a node's plugs left-to-right to match the cross-positions of the nodes their wires
# reach untangles the port ROWS (where multiple wires converge on one node) without
# moving any node. The node editor's Shift+A layout is unchanged; the canvas slice
# that draws plugs + routes wires to these slots consumes this later.
################################################################################

def _cross_center(n, positions, sizes, orient):
  """Center of node n on the CROSS axis (x for vertical flow, y for horizontal) — the
  axis a node's port row runs along."""
  x, y = positions[n]
  w, h = sizes[n]
  return (x + w * 0.5) if orient == "vertical" else (y + h * 0.5)


def _inversions(seq):
  """Out-of-order pairs (i<j with seq[i] > seq[j]) — the port-row crossing count for a
  plug slot order whose neighbours sit at cross-coords `seq`."""
  c = 0
  for i in range(len(seq)):
    for j in range(i + 1, len(seq)):
      if seq[i] > seq[j]:
        c += 1
  return c


def _node_neighbors(edges):
  """(incoming, outgoing) neighbour lists per node in CANONICAL order — each edge is a
  port; canonical is id-sorted (deterministic + input-order-independent): input plugs
  by source id, output plugs by dest id."""
  inc, out = {}, {}
  for (u, v) in sorted(edges, key=lambda e: (_key(e[0]), _key(e[1]))):
    out.setdefault(u, []).append(v)                # canonical output-plug order: by dst id
  for (u, v) in sorted(edges, key=lambda e: (_key(e[1]), _key(e[0]))):
    inc.setdefault(v, []).append(u)                # canonical input-plug order: by src id
  return inc, out


def _order_ports(canonical, positions, sizes, orient):
  """Chosen plug order for one side of a node: sort the canonical (id-ordered)
  neighbour list by cross-position (ties -> canonical order) and ADOPT it only if it
  STRICTLY reduces port crossings; otherwise keep canonical."""
  if len(canonical) < 2:
    return list(canonical)
  cc = {m: _cross_center(m, positions, sizes, orient) for m in canonical if m in positions}
  key = lambda m: cc.get(m, 0.0)
  canon_inv = _inversions([key(m) for m in canonical])
  order = sorted(range(len(canonical)), key=lambda i: (key(canonical[i]), i))
  chosen = [canonical[i] for i in order]
  return chosen if _inversions([key(m) for m in chosen]) < canon_inv else list(canonical)


def compute_port_orders(positions, edges, sizes, orient="vertical"):
  """Display-level PORT (plug) ordering, computed AFTER node placement is final. For
  each node the INPUT plugs are ordered left-to-right by the cross-position of their
  edge SOURCES and the OUTPUT plugs by their CONSUMERS' cross-position, so wires meet
  the node at monotone slots instead of tangling at its port rows. CANONICAL
  (id-sorted neighbour) order is the DEFAULT and is kept unless reordering STRICTLY
  reduces that node's port crossings — ties resolve to canonical order, so an already-
  untangled node's plugs read identically in every layout (scanability). Deterministic
  + input-order-independent (canonical order and the cross-positions both come from the
  deterministic layout). Node-level positions are UNAFFECTED. Returns
  {node: {"inputs":[src_id..], "outputs":[dst_id..]}} for every node with >=2 inputs OR
  >=2 outputs (a node with <=1 of each has no ordering choice, omitted)."""
  inc, out = _node_neighbors(edges)
  result = {}
  for n in sorted(set(inc) | set(out), key=_key):
    ins, outs = inc.get(n, []), out.get(n, [])
    if len(ins) < 2 and len(outs) < 2:
      continue
    result[n] = {"inputs": _order_ports(ins, positions, sizes, orient),
                 "outputs": _order_ports(outs, positions, sizes, orient)}
  return result


def count_port_crossings(positions, edges, sizes, orient="vertical", port_orders=None):
  """Total PORT-ROW crossings over all nodes: for each node, pairs of incoming edges
  whose source cross-order conflicts with the input-plug slot order, plus the same for
  outgoing edges vs the output-plug order. With port_orders=None the CANONICAL
  (id-sorted) plug order is scored — the pre-pass baseline; pass compute_port_orders()'s
  result to score the refined order. 0 == every node's plugs are monotone with their
  wires."""
  inc, out = _node_neighbors(edges)
  po = port_orders or {}
  total = 0
  for n in set(inc) | set(out):
    ins = (po.get(n, {}).get("inputs")) or inc.get(n, [])
    outs = (po.get(n, {}).get("outputs")) or out.get(n, [])
    total += _inversions([_cross_center(m, positions, sizes, orient)
                          for m in ins if m in positions])
    total += _inversions([_cross_center(m, positions, sizes, orient)
                          for m in outs if m in positions])
  return total


def layout_naive(graph, grid=GRID_DEFAULT, orient="vertical", **kw):
  """Unoptimised baseline (input-enumeration order within each layer, no
  straightening, no bus rails) — the reference the oracles score auto-layout
  against (this is the 'wires cross node bodies' shape the owner complained of)."""
  return layout(graph, grid=grid, orient=orient, _optimize=False, **kw)


################################################################################
# Crossing oracle (exact rendered-wire crossings) + grid / overlap checks
################################################################################

def _edge_anchors(u, v, positions, sizes, orient):
  ux, uy = positions[u]
  uw, uh = sizes[u]
  vx, vy = positions[v]
  vw, vh = sizes[v]
  if orient == "vertical":
    p0 = (ux + uw * 0.5, uy + uh)                  # output plug row (bottom edge)
    p1 = (vx + vw * 0.5, vy)                       # input plug row (top edge)
  else:
    p0 = (ux + uw, uy + uh * 0.5)
    p1 = (vx, vy + vh * 0.5)
  return p0, p1


def _orient(a, b, c):
  return (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0])


def _proper_intersect(p1, p2, p3, p4):
  """True iff segments (p1,p2) and (p3,p4) cross at an interior point (strict —
  shared endpoints / collinear touching do not count)."""
  d1 = _orient(p3, p4, p1)
  d2 = _orient(p3, p4, p2)
  d3 = _orient(p1, p2, p3)
  d4 = _orient(p1, p2, p4)
  return ((d1 > 0) != (d2 > 0)) and ((d3 > 0) != (d4 > 0)) \
      and d1 != 0 and d2 != 0 and d3 != 0 and d4 != 0


def _edge_polyline(u, v, positions, sizes, orient, waypoints):
  """Ordered point list a rendered wire follows: source output anchor, any routed
  intermediate waypoints (layout_ex), dest input anchor."""
  p0, p1 = _edge_anchors(u, v, positions, sizes, orient)
  mids = (waypoints or {}).get((u, v), [])
  return [p0] + [tuple(m) for m in mids] + [p1]


def count_crossings(positions, edges, sizes, orient="vertical", waypoints=None):
  """EXACT count of wire crossings for a drawn layout: each edge is the polyline
  from its source's output-plug row through any routed waypoints to its dest's
  input-plug row; a PAIR counts once when any of their segments properly intersect.
  Edges that share a node endpoint never count (their wires meet at the node, not
  cross). With waypoints=None the polyline is the straight source->dest segment
  (backward-compatible). Model-independent — the honest aesthetic measure. O(E^2)."""
  polys = []
  for (u, v) in edges:
    if u in positions and v in positions:
      polys.append((u, v, _edge_polyline(u, v, positions, sizes, orient, waypoints)))
  cr = 0
  for i in range(len(polys)):
    u1, v1, pa = polys[i]
    for j in range(i + 1, len(polys)):
      u2, v2, pb = polys[j]
      if u1 == u2 or u1 == v2 or v1 == u2 or v1 == v2:
        continue
      if _polys_cross(pa, pb):
        cr += 1
  return cr


def _polys_cross(pa, pb):
  for i in range(len(pa) - 1):
    for j in range(len(pb) - 1):
      if _proper_intersect(pa[i], pa[i + 1], pb[j], pb[j + 1]):
        return True
  return False


def _seg_hits_rect(a, b, rx0, ry0, rx1, ry1):
  """True iff segment (a,b) touches the axis-aligned rect [rx0,ry0]-[rx1,ry1]:
  either endpoint inside, or the segment crosses any rect edge."""
  if rx0 <= a[0] <= rx1 and ry0 <= a[1] <= ry1:
    return True
  if rx0 <= b[0] <= rx1 and ry0 <= b[1] <= ry1:
    return True
  corners = [(rx0, ry0), (rx1, ry0), (rx1, ry1), (rx0, ry1)]
  for k in range(4):
    if _proper_intersect(a, b, corners[k], corners[(k + 1) % 4]):
      return True
  return False


def count_body_crossings(positions, edges, sizes, orient="vertical", waypoints=None,
                         gutter=8.0):
  """The owner's metric: how many edge SEGMENTS (straight or routed-polyline) pass
  THROUGH a non-incident node's body. Each edge's rendered polyline (source anchor +
  waypoints + dest anchor) is tested segment-by-segment against every node rect it
  is NOT incident to, inflated by `gutter`. Returns the incidence count (0 == no wire
  crosses any node body). This is what side-rail + channel routing drives to zero."""
  cnt = 0
  for (u, v) in edges:
    if u not in positions or v not in positions:
      continue
    poly = _edge_polyline(u, v, positions, sizes, orient, waypoints)
    for si in range(len(poly) - 1):
      a, b = poly[si], poly[si + 1]
      for n, (nx, ny) in positions.items():
        if n == u or n == v or n not in sizes:
          continue
        nw, nh = sizes[n]
        if _seg_hits_rect(a, b, nx - gutter, ny - gutter, nx + nw + gutter, ny + nh + gutter):
          cnt += 1
  return cnt


def grid_violations(positions, grid=GRID_DEFAULT):
  """List of (id, x, y) whose coordinates are not exact multiples of `grid`."""
  bad = []
  for nid, (x, y) in positions.items():
    if abs(x - round(x / grid) * grid) > 1e-6 or abs(y - round(y / grid) * grid) > 1e-6:
      bad.append((nid, x, y))
  return bad


def overlaps(positions, sizes, gutter=0.0):
  """List of overlapping node-rect id pairs (rects inflated by gutter*0.5 each so a
  positive `gutter` also flags nodes closer than that clearance)."""
  ids = [n for n in positions if n in sizes]
  pad = gutter * 0.5
  bad = []
  for i in range(len(ids)):
    a = ids[i]
    ax, ay = positions[a]
    aw, ah = sizes[a]
    for j in range(i + 1, len(ids)):
      b = ids[j]
      bx, by = positions[b]
      bw, bh = sizes[b]
      if (ax - pad < bx + bw + pad and bx - pad < ax + aw + pad and
          ay - pad < by + bh + pad and by - pad < ay + ah + pad):
        bad.append((a, b))
  return bad


def bounding_box(positions, sizes):
  """(minx, miny, maxx, maxy) of the placed node rects, or None if empty."""
  if not positions:
    return None
  xs0 = [positions[n][0] for n in positions]
  ys0 = [positions[n][1] for n in positions]
  xs1 = [positions[n][0] + sizes[n][0] for n in positions if n in sizes]
  ys1 = [positions[n][1] + sizes[n][1] for n in positions if n in sizes]
  return (min(xs0), min(ys0), max(xs1), max(ys1))
