###############################################################################
# MirrorDemo — the model-half-then-MIRROR symmetry workflow. A box is shoved to X[0,2] so its -X face sits
# ON the x=0 plane; the far (+X) face is extruded into a bump (an asymmetric feature authored on ONE half).
# mirror(axis=x, weld=True) reflects + appends the reversed-winding copy and WELDS the on-plane seam, giving
# a symmetric whole spanning [-2.6, 2.6] with a bump on each end. GPU-native (the welded mirror-vert indices
# are compacted by the shared scan). This is the symmetry path for the racer hull.
#
#   ./ork.hypermesh.viewer.py mirror_demo     # [W] wireframe to see the welded x=0 seam
###############################################################################
from orkengine.core import vec3
from ork.hypergraph.dflow.hypermesh import Hypermesh, sel_normal_dir, add, group, POLY


class MirrorDemo(Hypermesh):
  def __init__(self):
    super().__init__()
    n = self.box(size=1.0)
    n = self.transform(n, translate=(1, 0, 0))                 # X[0,2]; the -X face lies on x=0
    n = self.select(n, sel_normal_dir(n=vec3(1, 0, 0), t=0.5, soft=0.05), domain=POLY, op=add(group(0)))
    n = self.extrude_faces(n, distance=0.6, slot=0)            # bump the far face (asymmetric, on one half)
    self.output(self.mirror(n, axis="x", weld=True))           # reflect + weld the x=0 seam -> symmetric


__all__ = ["MirrorDemo"]
