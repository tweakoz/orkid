#!/usr/bin/env python3
###############################################################################
# GR-2 gate — SLOT CONSUMPTION (instance_at_slots). The grammar has emitted XfSlots since
# GR-1 (the cholla's areole + bloom SLOT ops) with nothing reading them; LeafScatter's
# source=SLOTS mode consumes them: one card cluster per slot, placed at the slot's WORLD
# frame (owning node xform * slot local).
#
# Gates (cholla exemplar, GPU bake + OBJ readback — the geometry, not a promise):
#   A. consumed == emitted  — the CPU derive's slot count (294) equals the baked card count.
#   B. transforms match     — every card's petiole midpoint IS its slot's world position
#                             (slot locals are identity, so = the owning node's origin), and
#                             the blade runs `size` long (jitter=0, pitch=0 -> L = heading).
#   C. off by default       — LeafScatterModule's default source is NODES, and a NODES-mode
#                             bake of the same skeleton places on the 253 NODES, not the 294
#                             slots (the two modes are distinct; nothing existing switches).
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys, tempfile
from orkengine import core   # core before lev2
from orkengine import lev2
from orkengine import ecs    # full class registration

from ork.testing import headless_app, verdict
from ork.hypergraph.dflow.hypermesh import Hypermesh, LeafStyle, LeafSource
from ork.hypergraph.dflow.lsystem.examples import CaneCholla

hm = lev2.hypermesh

CHOLLA_NODES = 253   # the GR1.d locked exemplar counts (test_lsystem_presets gate C)
CHOLLA_SLOTS = 294
LEAF_SIZE    = 0.05
EPS          = 2.0e-4


# the scene-faithful cholla skeleton (scn_cane_cholla params) + one organ placer on it. The
# organ is deliberately jitter/pitch-free so each card's frame IS the placement frame.
def _cholla(source, min_gen):
  h = Hypermesh()
  h.lsystem(
      grammar     = CaneCholla,
      seg_len     = 0.16,
      base_radius = 0.020,
      sides       = 7,
      jitter      = 0.12,
      jit_azimuth = 0.6,
      jit_pitch   = 0.35,
      tropism     = 0.02)
  h.output(h.leaves(
      style    = LeafStyle.SINGLE,
      source   = source,
      per_node = 1,
      min_gen  = min_gen,
      size     = LEAF_SIZE,
      jitter   = 0.0,
      pitch    = 0.0))
  return h


def _verts(mesh, ctx, path):
  # OBJ readback: `v` lines are emitted in mesh vertex order, so card k owns verts 4k..4k+3.
  lev2.hypermesh.dump_obj(mesh, ctx, path)
  out = []
  for line in open(path).read().splitlines():
    if line.startswith("v "):
      x, y, z = line.split()[1:4]
      out.append((float(x), float(y), float(z)))
  return out


def run(ctx, tmp):
  fails = []

  # --- the CPU mirror of the baked skeleton: the SAME module object the bake derives from,
  #     so node frames and slot->node references are exactly what LeafScatter consumed.
  slotted = _cholla(LeafSource.SLOTS, 0.0)
  lsmod   = slotted._skeleton._module
  d       = hm._deriveLRuleSet(lsmod.grammar, lsmod)
  slots   = d["slots"]
  pos     = d["positions"]
  if len(slots) != CHOLLA_SLOTS or d["count"] != CHOLLA_NODES:
    fails.append("cholla drifted: %d nodes / %d slots (locked %d/%d)"
                 % (d["count"], len(slots), CHOLLA_NODES, CHOLLA_SLOTS))

  # --- gate A: every emitted slot became a card ---
  mesh = slotted.materialize(ctx)
  if mesh.num_faces != len(slots):
    fails.append("[A] slots emitted=%d but cards baked=%d (slot consumption dropped placements)"
                 % (len(slots), mesh.num_faces))
  if mesh.num_verts != len(slots) * 4:
    fails.append("[A] expected %d card verts, got %d" % (len(slots) * 4, mesh.num_verts))
  print("[gate A] slot consumption: %d slots -> %d cards / %d verts"
        % (len(slots), mesh.num_faces, mesh.num_verts), flush=True)

  # --- gate B: each card sits ON its slot's world transform ---
  V    = _verts(mesh, ctx, os.path.join(tmp, "slots.obj"))
  worst_p, worst_l = 0.0, 0.0
  if len(V) < len(slots) * 4:
    fails.append("[B] OBJ readback short: %d verts for %d slots" % (len(V), len(slots)))
  else:
    for k, (node, _tag) in enumerate(slots):
      b = [(V[4 * k][i] + V[4 * k + 1][i]) * 0.5 for i in range(3)]   # petiole midpoint
      t = [(V[4 * k + 2][i] + V[4 * k + 3][i]) * 0.5 for i in range(3)]  # tip midpoint
      s = [pos[3 * node + i] for i in range(3)]                       # slot world origin
      worst_p = max(worst_p, max(abs(b[i] - s[i]) for i in range(3)))
      worst_l = max(worst_l, abs(sum((t[i] - b[i]) ** 2 for i in range(3)) ** 0.5 - LEAF_SIZE))
    if worst_p > EPS:
      fails.append("[B] card base != slot transform (worst axis error %.6f)" % worst_p)
    if worst_l > EPS:
      fails.append("[B] blade length off by %.6f (placement frame is not the slot frame)" % worst_l)
  print("[gate B] transforms match slots: worst base err %.7f, worst blade err %.7f"
        % (worst_p, worst_l), flush=True)

  # --- gate C: OFF by default; NODES mode still places on nodes ---
  if hm.LeafScatterModule.createShared().source != int(LeafSource.NODES):
    fails.append("[C] LeafScatter default source is not NODES — existing graphs would switch")
  nodal = _cholla(LeafSource.NODES, 0.0).materialize(ctx)
  if nodal.num_faces != CHOLLA_NODES:
    fails.append("[C] NODES mode baked %d cards, expected %d nodes" % (nodal.num_faces, CHOLLA_NODES))
  print("[gate C] default source=NODES; NODES mode -> %d cards (nodes), SLOTS -> %d (slots)"
        % (nodal.num_faces, mesh.num_faces), flush=True)

  return fails


def main():
  tmp = tempfile.mkdtemp(prefix="lsys_slots_")
  fails = ["test never ran"]
  with headless_app() as app:
    try:
      fails = run(app.ctx, tmp)
    except Exception as e:
      fails = ["exception: %r" % (e,)]
    rc = verdict(not fails, "slot instancing (%s)" % ("; ".join(fails) if fails else "A+B+C OK"))
  sys.exit(rc)


main()
