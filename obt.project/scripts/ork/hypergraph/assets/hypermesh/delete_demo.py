###############################################################################
# DeleteDemo — the GPU-native DeleteFaces op (shared count→scan→scatter, no readback). An icosphere with
# its upper hemisphere (faces whose normal points up) deleted -> an open bowl. The deleted faces become
# zero-length in the output CSR (the render skips them); the surviving corners are packed by the scan.
#
#   ./ork.hypermesh.viewer.py delete_demo
###############################################################################
from orkengine.core import vec3
from ork.hypergraph.dflow.hypermesh import Hypermesh, sel_normal_dir, add, group, POLY


class DeleteDemo(Hypermesh):
  def __init__(self):
    super().__init__()
    n = self.icosphere(radius=1.6, subdivisions=3)
    n = self.select(n, sel_normal_dir(n=vec3(0, 1, 0), t=0.0, soft=0.12), domain=POLY, op=add(group(0)))
    self.output(self.delete_faces(n, slot=0))     # drop the upper hemisphere -> a bowl


__all__ = ["DeleteDemo"]
