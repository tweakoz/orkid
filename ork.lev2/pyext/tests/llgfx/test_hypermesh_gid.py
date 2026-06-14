#!/usr/bin/env python3
###############################################################################
# E.3 gate — gid multi-material partition (the A1 tag contract's persistent
# gid band, __tags[20:32)).
#
#   1. assign_gid(all): every face carries the gid; the __tags channel is
#      CREATED on an untagged mesh.
#   2. select -> assign_gid(slot): only matched faces get the gid; the free
#      region [0:20) is preserved (the select group bit survives).
#   3. a SECOND select after assign_gid cannot clobber the gid (the masked-
#      write contract).
#   4. serdes: the graph with GidAssign round-trips byte-identical, and the
#      clone recomputes to the same tags.
#   5. HypermeshDrawableData.gid_materials round-trips.
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys
from orkengine import core   # core MUST be imported before lev2
from orkengine import lev2
from orkengine import ecs

from ork.hypergraph.dflow.hypermesh import (Hypermesh, sel_normal_dir, replace, group)
from orkengine.core import vec3

GID = 7


class GidAll(Hypermesh):
  def __init__(self):
    super().__init__()
    b = self.box(size=1.0)
    g = self.assign_gid(b, gid=GID)              # ALL faces; creates __tags
    self.output(g)


class GidTop(Hypermesh):
  def __init__(self):
    super().__init__()
    b = self.box(size=1.0)
    s = self.select(b, sel_normal_dir(n=vec3(0, 1, 0), t=0.5), op=replace(group(0)))
    g = self.assign_gid(s, gid=GID, slot=0)      # only the +Y face(s)
    # a select AFTER the gid write must not clobber it (masked writes)
    s2 = self.select(g, sel_normal_dir(n=vec3(1, 0, 0), t=0.5), op=replace(group(1)))
    self.output(s2)


def gids(tags):
  return [(t >> 20) & 0xFFF for t in tags]


def main():
  ezapp = ecs.headless_appinit(use_subsystems=['opq', 'core', 'gpu', 'lev2'])
  ezapp.mainThreadBegin()
  ctx = ezapp.bindGfxToCurrentThread()
  assert ctx, "bindGfxToCurrentThread() returned null"
  hm = lev2.hypermesh
  try:
    _body(ezapp, ctx, hm)
  except SystemExit:
    raise                       # _body already tore down; don't double-teardown
  except BaseException:
    import traceback; traceback.print_exc()
    # ALWAYS tear down — a skipped headless_exit spins the process forever
    # (the coreappexit teardown trap)
    ezapp.mainThreadEnd()
    ecs.headless_exit()
    sys.exit(1)


def _body(ezapp, ctx, hm):

  ##############################################################
  # 1) assign-to-ALL creates + fills the gid band
  ##############################################################
  g_all = GidAll().generatedflow()
  live  = hm.materialize_live(g_all, ctx)
  tags  = hm.read_face_tags(live.mesh, ctx)
  assert len(tags) == 6, "box face count %d != 6" % len(tags)
  assert all(g == GID for g in gids(tags)), "assign_gid(all) gids: %s" % gids(tags)
  print("GID all-faces PASS (%d faces @ gid %d)" % (len(tags), GID), flush=True)

  ##############################################################
  # 2+3) selected-slot assignment + gid survives later selects
  ##############################################################
  g_top = GidTop().generatedflow()
  live2 = hm.materialize_live(g_top, ctx)
  tags2 = hm.read_face_tags(live2.mesh, ctx)
  gl    = gids(tags2)
  n_gid = sum(1 for g in gl if g == GID)
  n_z   = sum(1 for g in gl if g == 0)
  assert n_gid == 1 and n_z == len(gl) - 1, "top-face gid partition wrong: %s" % gl
  # the matched face still carries its select bit 0; the second select wrote bit 1
  top = [i for i, g in enumerate(gl) if g == GID][0]
  assert (tags2[top] >> 0) & 1 == 1, "free-region select bit lost on the gid face"
  assert any((t >> 1) & 1 for t in tags2), "second select wrote nothing"
  print("GID slot-selected PASS (%s)" % gl, flush=True)

  ##############################################################
  # 4) serdes: byte-identical round-trip + clone recompute parity
  ##############################################################
  js    = g_top.serializeJson()
  clone = core.Object.deserializeJson(js)
  assert clone.serializeJson() == js, "GidAssign graph re-serialization diverged"
  live3 = hm.materialize_live(clone, ctx)
  tags3 = hm.read_face_tags(live3.mesh, ctx)
  assert list(tags3) == list(tags2), "clone recompute tags diverged"
  print("GID serdes PASS (byte-identical + tag parity)", flush=True)

  ##############################################################
  # 5) drawable-data gid_materials round-trip
  ##############################################################
  dd = lev2.HypermeshDrawableData(
      graph          = g_top,
      material_asset = "default_mat",
      gid_materials  = {GID: "hot_mat", 12: "cold_mat"})
  djs = dd.serializeJson()
  dclone = core.Object.deserializeJson(djs)
  assert dclone.serializeJson() == djs, "HypermeshDrawableData re-serialization diverged"
  assert dclone.gid_materials == {GID: "hot_mat", 12: "cold_mat"}, \
      "gid_materials lost on round-trip: %s" % dclone.gid_materials
  print("GID drawabledata PASS", flush=True)

  ezapp.mainThreadEnd()
  print("=== hypermesh gid gate PASSED ===", flush=True)
  ecs.headless_exit()
  sys.exit(0)


if __name__ == "__main__":
  main()
