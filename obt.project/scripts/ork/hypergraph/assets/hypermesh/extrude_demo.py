###############################################################################
# ExtrudeDemo — the first selection consumer, on a LIVE-refining mesh. An icosphere whose +Y cap is
# SELECTED (a face group in bit 0 of __tags) and EXTRUDED. onUpdate animates BOTH simultaneously:
#   * the icosphere `subdivisions` ping-pong 1 <-> 6 over a 10s loop (the source TOPOLOGY changes live,
#     so ExtrudeFaces re-derives its boundary each level step — a 1-frame passthrough at each step),
#   * the extrude `distance` pumps continuously (positions only -> free).
# So you watch the cap rise and fall on a sphere that refines and coarsens underneath it.
#
#   ./ork.hypermesh.viewer.py extrude_demo --faces
###############################################################################
import math
from orkengine.core import vec3
from ork.hypergraph.dflow.hypermesh import (Hypermesh, sel_normal_dir, replace, isolate, group, POLY)


class ExtrudeDemo(Hypermesh):
  def __init__(self):
    super().__init__()
    self._sph = self.icosphere(radius=1.6, subdivisions=1)
    cap = sel_normal_dir(n=vec3(0, 1, 0), t=0.55, soft=0.05)             # +Y cap faces
    sel = self.select(self._sph, cap, domain=POLY, op=replace(group(0))) # group 0 := the cap
    # extrude tags the new geometry per partition (view with --groups): the raised cap -> group 1,
    # the new side walls -> group 2; untouched base faces inherit (stay unselected).
    ext = self.extrude_faces(sel, distance=0.0, slot=0,
                                  mask_cap=isolate(group(1)), mask_wall=isolate(group(2)))
    self.output(ext)

    self.ext = ext

  def onUpdate(self, updinfo):
    t     = updinfo.absolutetime
    phase = (t % 30.0) / 30.0
    tri   = 1.0 - abs(2.0 * phase - 1.0)                          # 0 -> 1 -> 0 over 10s
    #self._sph.inputs.subdivisions = int(round(1 + 5 * tri))      # subdivisions 1 <-> 6
    self.ext.inputs.distance     = 0.45 * (0.5 + 0.5 * math.sin(t * 1.8))  # pump the cap


__all__ = ["ExtrudeDemo"]
