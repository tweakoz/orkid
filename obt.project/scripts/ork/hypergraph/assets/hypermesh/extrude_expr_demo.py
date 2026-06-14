###############################################################################
# ExtrudeExprDemo — per-face extrude EXPRESSIONS. A uvsphere where every face is
# extruded as a beveled stud, but the studs are driven per-face by SelExpr fields:
#   distance  = latitude gradient  -> short studs at the south pole, tall at the north
#   inset     = 0.30 (constant)    -> each stud is beveled (cap shrunk toward its centroid)
#   direction = the face normal    -> studs point outward (the default; a sea-urchin)
# onUpdate animates the global `distance` plug -> the whole stud field breathes on top of
# the per-face profile (the per-face fields recompute on the GPU each frame -> free).
#
#   ./ork.hypermesh.viewer.py extrude_expr_demo
###############################################################################
import math
from orkengine.core import vec3
from ork.hypergraph.dflow.hypermesh import Hypermesh, S, sl_smoothstep, isolate, group, POLY

RADIUS = 1.6


class ExtrudeExprDemo(Hypermesh):
  def __init__(self):
    super().__init__()
    n = self.uvsphere(radius=RADIUS, segments=64, rings=64)
    # select EVERY face into group 0 (area is always > -1) so the whole sphere studs up
    n = self.select(n, S.area > -1.0, domain=POLY, op=isolate(group(0)))
    # per-face distance: a latitude gradient (short at the bottom, tall at the top)
    height = S.P.dot(vec3(0, 1, 0))                       # y of the face centroid, in [-R, R]
    dist   = 0.04 + 0.55 * sl_smoothstep(-RADIUS, RADIUS, height)
    self._ext = self.extrude_faces(
      n,
      distance=S.id*dist*0.001,        # per-face SelExpr (float)
      inset=(S.area*1.5),           # beveled studs (constant; try inset=(S.area*5.0) for a per-face expr)
      slot=0,
    )                       # direction omitted -> lift along each face normal (outward)
    self.output(self._ext)

  def onUpdate(self, updinfo):
    # global multiplier on the per-face distance field -> the whole stud field breathes
    t = updinfo.absolutetime
    self._ext.inputs.distance = 0.55 + 0.45 * math.sin(t * 1.6)


__all__ = ["ExtrudeExprDemo"]
