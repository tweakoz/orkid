#!/usr/bin/env ork.python
################################################################################
# SPVR — the CULL-ORACLE BINDINGS (cull-oracle ask 3).
#
# THE QUESTION: can a gate READ the engine's GPU-cull result counts, and can it
# prove it armed the counter before believing them?
#
# WHY THE ARMING MATTERS. Both cull sites (terrain, hypermesh) read their result
# buffers back ONLY while CullStats is enabled -- that is what keeps the
# no-readback cull path free. So an unarmed run reports all zeros, and "all
# zeros" from an unarmed counter means NOBODY COUNTED, not "nothing was culled".
# A gate that cannot tell those apart will report a dead cull path as a healthy
# one. Hence `enabled` is exposed for reading, not just for setting, and rides
# inside the snapshot dict so the reading and its arming state cannot be
# separated by a caller.
#
# LEGS (all must pass)
#   (a) DEFAULT-OFF   cullStatsEnabled is False on a fresh context: the counter
#                     costs GPU readbacks, so nothing may arm it implicitly
#   (b) ARMS          setting it True reads back True, and False returns it
#   (c) SNAPSHOT      cullStats is a dict carrying every documented field, with
#                     the per-family *_valid flags that separate "no such
#                     geometry in this scene" from "everything got culled"
#   (d) ARMED-STATE   the arming state inside the snapshot tracks the property,
#                     so a banked reading can never be read as armed when it
#                     was not
#
# This test deliberately does NOT assert nonzero counts: it boots no scene, so
# there is nothing to cull. Proving the counts are CORRECT is the oracle's job
# in the gate that drives real geometry; proving they are REACHABLE and
# honestly arming-labelled is this one's.
#
# Self-configuring, bare invocation, no environment:
#   ork.python test_spvr_cull_oracle_bindings.py
################################################################################

import os
os.environ["PYTHONUNBUFFERED"] = "1"
os.environ.setdefault("ORKID_VULKAN_VALIDATE", "2")

import sys
import time

sys.stdout.reconfigure(line_buffering=True)

_ROOT = os.path.abspath(__file__)
for _ in range(5):
  _ROOT = os.path.dirname(_ROOT)
sys.path.insert(0, os.path.join(_ROOT, "obt.project", "scripts"))
sys.path.insert(0, os.path.join(_ROOT, "obt.project", "bin"))

# every field the snapshot must carry, and the reason it exists.
REQUIRED_FIELDS = (
    "enabled",        # the arming state, inseparable from the reading
    "terrain_valid",  # did the terrain cull contribute at all this frame
    "t_total", "t_frustum", "t_visible",
    "hyper_valid",    # ...and the hypermesh cull
    "h_variants", "h_total", "h_frustum", "h_visible", "h_occluded",
)


def main():
  from orkengine import core
  from orkengine import lev2
  from ork.testing import headless_app

  t0 = time.time()
  fails = []

  with headless_app(subsystems=['opq', 'core', 'gpu', 'lev2']) as app:
    ctx = app.ctx

    # ---- leg (a): default OFF
    default_state = ctx.cullStatsEnabled
    print("LEG_DEFAULT_OFF enabled=%s" % default_state, flush=True)
    if default_state:
      fails.append("cullStatsEnabled is True on a fresh context -- the cull "
                   "readback cost must never be armed implicitly")

    # ---- leg (b): arms and disarms
    ctx.cullStatsEnabled = True
    armed = ctx.cullStatsEnabled
    ctx.cullStatsEnabled = False
    disarmed = ctx.cullStatsEnabled
    print("LEG_ARMS armed=%s disarmed=%s" % (armed, disarmed), flush=True)
    if not armed:
      fails.append("setting cullStatsEnabled=True did not take")
    if disarmed:
      fails.append("setting cullStatsEnabled=False did not take")

    # ---- leg (c): the snapshot's shape
    ctx.cullStatsEnabled = True
    snap = ctx.cullStats
    print("LEG_SNAPSHOT keys=%d" % (len(snap) if hasattr(snap, "__len__") else -1),
          flush=True)
    if not isinstance(snap, dict):
      fails.append("cullStats is %r, not a dict" % type(snap))
    else:
      for field in REQUIRED_FIELDS:
        if field not in snap:
          fails.append("cullStats is missing '%s'" % field)
      print("LEG_SNAPSHOT %s" % ", ".join(
          "%s=%s" % (k, snap[k]) for k in REQUIRED_FIELDS if k in snap), flush=True)

    # ---- leg (d): the snapshot's arming state tracks the property
    armed_in_snap = bool(snap.get("enabled", False)) if isinstance(snap, dict) else False
    ctx.cullStatsEnabled = False
    snap_off = ctx.cullStats
    disarmed_in_snap = bool(snap_off.get("enabled", True)) if isinstance(snap_off, dict) else True
    print("LEG_ARMED_STATE in_snapshot_armed=%s in_snapshot_disarmed=%s"
          % (armed_in_snap, disarmed_in_snap), flush=True)
    if not armed_in_snap:
      fails.append("the snapshot reported enabled=False while the counter was ARMED "
                   "-- a gate could not prove it armed the counter")
    if disarmed_in_snap:
      fails.append("the snapshot reported enabled=True while the counter was OFF "
                   "-- an unarmed all-zero reading would look trustworthy")

    # leave the counter as it was found; this property is process-global.
    ctx.cullStatsEnabled = default_state

    armed_validation = bool(ctx.validation_armed)
    verrs = int(ctx.validation_errors)

  print("VALIDATION armed=%s errors=%d" % (armed_validation, verrs), flush=True)
  if not armed_validation:
    fails.append("validation layer was not armed -- a zero error count proves nothing")
  elif verrs != 0:
    fails.append("validation reported %d error(s)" % verrs)

  dt = time.time() - t0
  if fails:
    print("TESTVERDICT FAIL (%d): %s" % (len(fails), "; ".join(fails)), flush=True)
    return 1
  print("TESTVERDICT PASS -- cull-oracle counters readable, default-off, and "
        "honestly arming-labelled (%.1fs)" % dt, flush=True)
  return 0


if __name__ == "__main__":
  sys.exit(main())
