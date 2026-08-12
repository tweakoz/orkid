#!/usr/bin/env ork.python
###############################################################################
# LEAF CARD ANCHORING — the mesh-level gate on WHERE LeafScatter roots a card.
#
# Two defects this pins, both read from the baked geometry (no render):
#   * needles on the BOLE. Node admission was generation-only, and a monopodial
#     conifer leader carries ONE generation from its thick base to its thin apex,
#     so no min_gen can clothe the apex without also clothing the trunk. The
#     radius gate (max_radius) cuts on segment thickness instead.
#   * cards FLOATING beside the wood. The base edge sat on the skeleton
#     CENTERLINE while the bark surface is centerline + radius * radial (the
#     LSweep ring), so a needle hung in the air next to the twig it grew from.
#     embed anchors the base edge under the bark; the blade emerges through it.
#
# Legs (each with the control that proves the check can FAIL):
#   0  cook identity — each new knob moves the module's cook hash (arming one on a
#                     shipped graph must RECOOK, not replay the cached mesh).
#   A  radius gate  — armed: ZERO cards anchored on nodes fatter than the
#                     threshold. Positive control: filter off -> cards ARE there.
#   B  embedding    — armed: every base corner of an EMBEDDED card lies inside the
#                     wood cylinder (distance from the segment axis <= the local
#                     radius) and a card too wide to contain stays exactly where
#                     the legacy anchor left it; the anchor offset matches the
#                     closed form to the float.
#                     Negative control: embed=0 -> every base edge back on the
#                     centerline (the legacy placement, to the float).
#   C  determinism  — jitter_deg/twist/up_bias armed, cook cache OFF, two cooks
#                     of one graph are byte-identical (hash-seeded, not rand()).
#   D  orientation  — armed knobs measurably re-aim the cards (vs the unarmed
#                     cook) WITHOUT lifting any base edge out of the wood.
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import math, sys, tempfile
from orkengine import core   # core before lev2
from orkengine import lev2
from orkengine import ecs

from ork.hypergraph.dflow.hypermesh import Hypermesh, Archetype, LeafStyle, materialize_graph

hm = lev2.hypermesh

SEED     = 7
SIZE     = 0.08                  # blade length (small enough that most segments are wider than the
                                 # card -> the containment clamp is not what is being measured)
ASPECT   = 0.5
HW       = SIZE * ASPECT * 0.5   # base half-width (card `jitter` is 0, so this is exact)
RADMAX   = 0.05                  # metres of segment radius above which a node is bole, not twig
EMBED    = 0.35                  # base edge recess, as a fraction of the local radius
TOL      = 1e-3                  # metres — node heading vs. parent->node chord slack

LEAF = dict(style=LeafStyle.SINGLE, per_node=4, min_gen=0.0,
            size=SIZE, aspect=ASPECT, pitch=25.0, roll=90.0, jitter=0.0, seed=3)

TMP = tempfile.mkdtemp(prefix="leafanchor_")


class _Spruce(Hypermesh):
  """conifer skeleton (leader gen 0 base->apex, laterals gen 1) + the leaf cards ALONE:
  the output carries no bark, so every dumped face is a card."""
  def __init__(self, **leafkw):
    super().__init__()
    self.lsystem(archetype=Archetype.CONIFER, depth=4, children=2, internodes=2,
                 seg_len=0.5, base_radius=0.12, sides=5, branch_angle=22.0,
                 jitter=0.0, apical=0.6, seed=SEED, budget=4000)
    self.output(self.leaves(**dict(LEAF, **leafkw)))


def _sub(a, b): return (a[0]-b[0], a[1]-b[1], a[2]-b[2])
def _dot(a, b): return a[0]*b[0] + a[1]*b[1] + a[2]*b[2]
def _cross(a, b): return (a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0])
def _len(a): return math.sqrt(_dot(a, a))
def _norm(a):
  m = _len(a) or 1.0
  return (a[0]/m, a[1]/m, a[2]/m)
def _mid(a, b): return tuple((a[k]+b[k]) * 0.5 for k in range(3))


def _cook(ctx, tag, **leafkw):
  """bake the skeleton + cards, return (cards, nodes, obj_bytes).
  cards: [(base_l, base_r, tip_r, tip_l)] in LeafScatter's winding.
  nodes: [(pos, radius, parent)] from the SAME derive the LSystem module runs."""
  h = _Spruce(**leafkw)
  h._close_trace()
  h.graphdata.cacheable = False    # cook cache OFF: leg C must exercise the KERNEL, not a blob replay
  mesh = materialize_graph(h.graphdata, ctx)
  path = os.path.join(TMP, tag + ".obj")
  lev2.hypermesh.dump_obj(mesh, ctx, path)
  raw = open(path, "rb").read()
  V, cards = [], []
  for line in raw.decode().splitlines():
    if line.startswith("v "):
      p = line.split(); V.append((float(p[1]), float(p[2]), float(p[3])))
    elif line.startswith("f "):
      f = [int(t.split("/")[0]) - 1 for t in line.split()[1:]]
      cards.append(tuple(V[i] for i in f))
  ls = h._skeleton._module
  d  = hm._deriveLRuleSet(ls.grammar, ls)
  P, R, PA = d["positions"], d["radii"], d["parents"]
  nodes = [((P[i*3], P[i*3+1], P[i*3+2]), R[i], PA[i]) for i in range(d["count"])]
  return cards, nodes, raw


def _anchor(card, nodes):
  """the node a card grew from = the nearest one to its base-edge midpoint (the anchor offset is
  bounded by the local radius, an order below the internode spacing). Returns (index, distance)."""
  b = _mid(card[0], card[1])
  best, bd = -1, 1e30
  for i, (p, _r, _pa) in enumerate(nodes):
    d = _len(_sub(b, p))
    if d < bd:
      best, bd = i, d
  return best, bd


def _axis_dist(pt, nodes, ni):
  """distance from `pt` to the INFINITE segment axis (parent -> node), or None at a root."""
  pos, _r, pa = nodes[ni]
  if pa >= len(nodes):
    return None
  a = _norm(_sub(pos, nodes[pa][0]))
  v = _sub(pt, pos)
  return _len(_sub(v, tuple(a[k] * _dot(v, a) for k in range(3))))


def _expect_offset(rad):
  """the closed form the kernel anchors with: min(rad*(1-embed), sqrt(rad^2 - hw^2)), 0 if the
  card is wider than its segment (no containable anchor -> legacy centerline)."""
  if rad <= HW:
    return 0.0
  return min(rad * (1.0 - EMBED), math.sqrt(rad * rad - HW * HW))


def _corner_excess(cards, nodes):
  """worst distance by which a BASE corner overshoots its containment bound: the wood radius on a
  segment wider than the card (the embedded case), else the half-width the legacy centerline anchor
  already spent (a card wider than its twig cannot be contained by any anchor — and must not end up
  further out than it was before embedding)."""
  worst = 0.0
  for c in cards:
    ni, _off = _anchor(c, nodes)
    bound = max(nodes[ni][1], HW)
    for corner in (c[0], c[1]):
      d = _axis_dist(corner, nodes, ni)
      if d is not None:
        worst = max(worst, d - bound)
  return worst


def _card_normal(c):
  return _norm(_cross(_sub(c[1], c[0]), _sub(c[2], c[0])))


###############################################################################
def leg_0_cook_identity(fails):
  """every new knob must enter the COOK KEY, or arming it would replay a stale cached mesh."""
  base = hm.LeafScatterModule.createShared()
  h0   = hm._moduleIdentityHash(base)
  for name, val in (("max_radius", RADMAX), ("embed", EMBED), ("twist", 30.0),
                    ("up_bias", 0.4), ("jitter_deg", 12.0)):
    m = hm.LeafScatterModule.createShared()
    setattr(m, name, val)
    if hm._moduleIdentityHash(m) == h0:
      fails.append("[leg 0] %s does not change the cook identity — arming it would hit a stale cook" % name)
  print("[leg 0] cook identity moves for all 5 new knobs: %s"
        % (not [f for f in fails if f.startswith("[leg 0]")]), flush=True)


def leg_a_radius_gate(ctx, fails):
  off,   nodes, _ = _cook(ctx, "radius_off")
  armed, _,     _ = _cook(ctx, "radius_armed", max_radius=RADMAX)
  thick = {i for i, (_p, r, _pa) in enumerate(nodes) if r > RADMAX}
  n_off   = sum(1 for c in off   if _anchor(c, nodes)[0] in thick)
  n_armed = sum(1 for c in armed if _anchor(c, nodes)[0] in thick)
  print("[leg A] nodes=%d thick(r>%.3f)=%d | cards off=%d (on thick %d) armed=%d (on thick %d)"
        % (len(nodes), RADMAX, len(thick), len(off), n_off, len(armed), n_armed), flush=True)
  if not thick:
    fails.append("[leg A] the skeleton has NO node fatter than %.3f — the gate cannot see the filter" % RADMAX)
  if n_off == 0:
    fails.append("[leg A] positive control: filter OFF placed no card on a thick node (nothing to fix)")
  if n_armed:
    fails.append("[leg A] %d cards still anchored on nodes fatter than %.3f" % (n_armed, RADMAX))
  if len(armed) >= len(off):
    fails.append("[leg A] armed card count %d not below the unfiltered %d" % (len(armed), len(off)))


def leg_b_embedding(ctx, fails):
  armed, nodes, _ = _cook(ctx, "embed_armed", embed=EMBED)
  plain, _,     _ = _cook(ctx, "embed_off")
  # negative control: embed off -> the base edge midpoint IS the node position, to the float
  worst_plain = max(_anchor(c, nodes)[1] for c in plain)
  if worst_plain > 1e-5:
    fails.append("[leg B] embed=0 moved a base edge %.6fm off the centerline (legacy placement broken)" % worst_plain)
  n_emb, worst_off, worst_out = 0, 0.0, _corner_excess(armed, nodes)
  for c in armed:
    ni, off = _anchor(c, nodes)
    exp = _expect_offset(nodes[ni][1])
    worst_off = max(worst_off, abs(off - exp))
    if exp > 0.0:
      n_emb += 1
  print("[leg B] cards=%d embedded=%d (rest: card wider than its segment -> legacy anchor) | "
        "anchor vs closed form %.2em | worst base corner past the containment bound %.2em"
        % (len(armed), n_emb, worst_off, worst_out), flush=True)
  if n_emb == 0:
    fails.append("[leg B] embed armed but NO card anchored under the bark")
  if worst_off > 1e-5:
    fails.append("[leg B] anchor offset deviates %.6fm from the closed form" % worst_off)
  if worst_out > TOL:
    fails.append("[leg B] a base corner sits %.6fm past its containment bound (poking through sideways)" % worst_out)


def leg_c_determinism(ctx, fails):
  kw = dict(embed=EMBED, max_radius=RADMAX, jitter_deg=12.0, twist=30.0, up_bias=0.4)
  _c1, _n1, raw1 = _cook(ctx, "det_1", **kw)
  _c2, _n2, raw2 = _cook(ctx, "det_2", **kw)
  print("[leg C] two cache-free cooks: %d bytes each, identical=%s"
        % (len(raw1), raw1 == raw2), flush=True)
  if raw1 != raw2:
    fails.append("[leg C] jitter/twist/up_bias cooks differ — the per-card randomization is not hash-seeded")


def leg_d_oriented_still_embedded(ctx, fails):
  base, nodes, _ = _cook(ctx, "orient_off", embed=EMBED, max_radius=RADMAX)
  arm,  _,     _ = _cook(ctx, "orient_armed", embed=EMBED, max_radius=RADMAX,
                         jitter_deg=12.0, twist=30.0, up_bias=0.4)
  if len(base) != len(arm):
    fails.append("[leg D] the orientation knobs changed the card COUNT (%d -> %d)" % (len(base), len(arm)))
    return
  angs = [math.degrees(math.acos(max(-1.0, min(1.0, abs(_dot(_card_normal(a), _card_normal(b)))))))
          for a, b in zip(base, arm)]
  moved = sum(1 for a in angs if a > 1.0)
  worst_out = _corner_excess(arm, nodes)
  print("[leg D] cards=%d re-aimed>1deg=%d mean|dnormal|=%.1fdeg | worst base corner past the bound %.2em"
        % (len(arm), moved, sum(angs) / len(angs), worst_out), flush=True)
  if moved < 0.9 * len(arm):
    fails.append("[leg D] only %d/%d cards re-aimed — the orientation knobs are inert" % (moved, len(arm)))
  if sum(angs) / len(angs) < 5.0:
    fails.append("[leg D] mean re-aim %.1fdeg is below the authored twist/bias" % (sum(angs) / len(angs)))
  if worst_out > TOL:
    fails.append("[leg D] armed orientation pushed a base corner %.6fm past its containment bound" % worst_out)


###############################################################################
def main():
  ezapp = ecs.headless_appinit(use_subsystems=['opq', 'core', 'gpu', 'lev2'])
  ezapp.mainThreadBegin()
  ctx = ezapp.bindGfxToCurrentThread()
  assert ctx, "bindGfxToCurrentThread() returned null"

  fails = []
  leg_0_cook_identity(fails)
  leg_a_radius_gate(ctx, fails)
  leg_b_embedding(ctx, fails)
  leg_c_determinism(ctx, fails)
  leg_d_oriented_still_embedded(ctx, fails)

  ezapp.mainThreadEnd()
  ecs.headless_exit()
  for f in fails:
    print("FAIL: " + f, flush=True)
  print("LEAF_CARD_ANCHOR_RESULT=%s" % ("PASS" if not fails else "FAIL"), flush=True)
  sys.exit(0 if not fails else 1)


main()
