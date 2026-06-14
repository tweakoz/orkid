###############################################################################
# CompactDemo — delete + COMPACT. An icosphere with its upper hemisphere deleted (delete_faces leaves the
# now-orphaned upper verts in the buffer), then compact() garbage-collects those orphan verts and remaps
# vidx. Same visual as delete_demo (a bowl), but the vertex count is tightened — compare num_verts in the
# [O] OBJ dump against delete_demo. GPU-native (shared MeshScan); compact reads back one uint (the new
# vert count).
#
#   ./ork.hypermesh.viewer.py compact_demo     # [O] dump OBJ -> fewer verts than delete_demo
###############################################################################
from orkengine.core import vec3
from ork.hypergraph.dflow.hypermesh import Hypermesh, sel_normal_dir, add, group, POLY


class CompactDemo(Hypermesh):
  def __init__(self):
    super().__init__()
    n = self.icosphere(radius=1.6, subdivisions=3)
    n = self.select(n, sel_normal_dir(n=vec3(0, 1, 0), t=0.0, soft=0.12), domain=POLY, op=add(group(0)))
    n = self.delete_faces(n, slot=0)              # drop the upper hemisphere -> orphan verts up top
    self.output(self.compact(n))                  # gc the orphans + remap vidx


__all__ = ["CompactDemo"]
