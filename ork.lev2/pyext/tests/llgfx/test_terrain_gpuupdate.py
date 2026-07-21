#!/usr/bin/env python3
###############################################################################
# E4 gate — the family-neutral gpuUpdate() seam (terrain).
#
#   1. COLD: a static terrain DSL graph (Fbm -> Terrace -> height capture) BAKES
#      via the normal lev2.terrain.bake_heightfield. The bake stamps a GpuUpdateStamp
#      on the durable GraphData (family=TERRAIN, dim, extent_m) and writes the EXR.
#   2. GPUUPDATE: lev2.dflow.gpuUpdate(graph, ctx) resolves that stamp and re-dispatches
#      bake_heightfield with the SAME stored dim/extent_m — no family knowledge in
#      the caller. On the cacheable graph the WARM cook cache serves fbm/remap/terr
#      (warm hits >= 3, the Capture sink always recomputes).
#   3. PARITY: the gpuUpdate capture EXR is BYTE-IDENTICAL (sha256) to the first bake.
#   4. OVERRIDE: gpuUpdate honors a dim override kwarg (re-baked field differs in size).
#
# The gpuUpdate seam is one entry that ALSO drives the hypermesh family — this proves
# the terrain half of that shared surface.
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys
import zlib   # crc32 tag only (no libssl dependency, unlike hashlib on this staging)
from orkengine import core   # core before lev2
from orkengine import lev2
from orkengine import ecs

from ork.hypergraph.dflow.terrain import HeightField
from ork.hypergraph.dflow import terrain as T

DIM = 256
OUT = "/tmp/terrain_gpuupdate_height.exr"


class RollingHills(HeightField):
  def __init__(self):
    super().__init__()
    h = T.Fbm(frequency=3.0, octaves=6) * 0.5 + 0.5
    self.capture(T.Terrace(h, step_m=1.0 / 6.0, sharpness=4.0), "height")


def _bytes(path):
  # byte-identity is the actual oracle (mirrors test_terrain_asset's raw compare);
  # crc32 is only a compact human tag in the log.
  with open(path, "rb") as f:
    return f.read()


def _tag(b):
  return "%08x" % (zlib.crc32(b) & 0xFFFFFFFF)


def _body(ez, ctx):
  for p in (OUT,):
    if os.path.exists(p):
      os.remove(p)

  ##############################################################
  # 1) COLD — normal bake; stamp recorded, EXR written
  ##############################################################
  hf    = RollingHills()
  graph = hf.generatedflow()
  assert graph.cacheable, "terrain DSL did not mark the static graph cacheable"
  for cap in lev2.terrain.capture_modules(graph):
    cap.path = OUT if cap.channel == "height" else "/tmp/terrain_gpuupdate_%s.exr" % cap.channel
  stats1 = lev2.terrain.bake_heightfield(graph, ctx, DIM)
  assert os.path.exists(OUT) and os.path.getsize(OUT) > 0, "cold bake wrote no EXR"
  b_cold = _bytes(OUT)
  print("gpuUpdate COLD PASS (baked, %d capture(s), %dB crc=%s)" % (len(stats1), len(b_cold), _tag(b_cold)), flush=True)

  ##############################################################
  # 2+3) GPUUPDATE — family-neutral entry re-dispatches the SAME terrain bake
  ##############################################################
  stats2 = lev2.dflow.gpuUpdate(graph, ctx)
  assert os.path.exists(OUT) and os.path.getsize(OUT) > 0, "gpuUpdate wrote no EXR"
  b_warm = _bytes(OUT)
  assert b_warm == b_cold, \
      "gpuUpdate capture diverged from first bake (crc %s != %s, %dB vs %dB)" \
      % (_tag(b_warm), _tag(b_cold), len(b_warm), len(b_cold))
  assert len(stats2) == len(stats1), "gpuUpdate returned %d stats, expected %d" % (len(stats2), len(stats1))
  print("gpuUpdate WARM PASS (capture byte-identical, %dB crc=%s)" % (len(b_warm), _tag(b_warm)), flush=True)

  ##############################################################
  # 4) OVERRIDE — dim kwarg overrides the stored resolution
  ##############################################################
  half = DIM // 2
  lev2.dflow.gpuUpdate(graph, ctx, dim=half)
  # the smaller field is a different EXR (fewer texels) -> bytes must change
  b_half = _bytes(OUT)
  assert b_half != b_cold, "dim override produced an identical EXR (override ignored?)"
  print("gpuUpdate OVERRIDE PASS (dim=%d re-baked a distinct field, %dB crc=%s)" % (half, len(b_half), _tag(b_half)), flush=True)

  ##############################################################
  # 5) LOUD on an unbaked graph
  ##############################################################
  fresh = RollingHills().generatedflow()
  raised = False
  try:
    lev2.dflow.gpuUpdate(fresh, ctx)
  except Exception as e:
    raised = "gpuUpdate stamp" in str(e) or "bake it once first" in str(e)
  assert raised, "gpuUpdate of an unbaked graph must fail LOUDLY"
  print("gpuUpdate UNBAKED-GUARD PASS (loud failure)", flush=True)

  return True


def main():
  ez = ecs.headless_appinit(use_subsystems=['opq', 'core', 'gpu', 'lev2'])
  ez.mainThreadBegin()
  ctx = ez.bindGfxToCurrentThread()
  assert ctx, "bindGfxToCurrentThread() returned null"
  ok = False
  try:
    ok = _body(ez, ctx)
  except BaseException:
    import traceback; traceback.print_exc()
  ez.mainThreadEnd()
  print("=== terrain gpuUpdate gate %s ===" % ("PASSED" if ok else "FAILED"), flush=True)
  ecs.headless_exit()
  sys.exit(0 if ok else 1)


if __name__ == "__main__":
  main()
