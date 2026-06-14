# TwistTest — one box face extruded as a multi-segment column with a per-ring TWIST about the axis
# (a 90deg total twist) + a slight taper. Exercises the Rodrigues twist + absolute scale in cs_face_seg.
from orkengine.core import vec3
from ork.hypergraph.dflow.hypermesh import Hypermesh, S, isolate, group, POLY
import math
class TwistTest(Hypermesh):
  def __init__(self):
    super().__init__()
    n = self.box(size=1.0)
    n = self.select(n, S.N.dot(vec3(0,1,0)) > 0.6, domain=POLY, op=isolate(group(0)))   # +Y face
    n = self.extrude_faces(n, distance=0.25, segments=8, slot=0,
                           twist=(0.5*math.pi)*S.t,        # 0 -> 90deg over the column
                           scale=(1.0 - 0.4*S.t))          # taper to 60%
    n = self.face_normals(n)
    self.output(n)
__all__ = ["TwistTest"]
