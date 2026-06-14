###############################################################################
# InstanceDemo — Phase 1 of hypermesh INSTANCING: one base mesh (a rounded cube) drawn 16x in a SINGLE
# indirect draw call, each copy placed by a per-instance matrix from `self.instances`. The geometry is
# computed ONCE (one graph eval + one triangulate); the 16 draws share it (FWD_SSBO_CUSTOM_INSTANCED).
#
# Per-instance data rides in the matrix BOTTOM ROW (m[0..2].w, unused by the affine transform) -> frg_clr,
# here a per-instance hue (visible with a material that reads the Cd/frg_clr selector).
#
#   ./ork.hypermesh.viewer.py instance_demo
###############################################################################
import numpy as np
from ork.hypergraph.dflow.hypermesh import Hypermesh


def _inst_mtx(tx, ty, tz, s, d0, d1, d2):
  # COLUMN-MAJOR mat4: each ROW below IS a glm COLUMN, so .reshape(-1) yields column-major order.
  # columns 0-2 = scaled basis (+ data in .w); column 3 = translation (.w must stay 1).
  return np.array([[s,  0.0, 0.0, d0],
                   [0.0, s,  0.0, d1],
                   [0.0, 0.0, s,  d2],
                   [tx, ty,  tz,  1.0]], dtype="float32")

class InstanceDemo(Hypermesh):
  def __init__(self):
    super().__init__()
    n = self.box(size=0.5)
    n = self.subdivide(n, smooth=True, level=2)        # a rounded cube (cheap geometry, shared by all instances)
    self.subdiv = n
    n = self.face_normals(n)
    self.output(n)
    # 16 instances in a 4x4 grid; idata (matrix bottom row) = a per-instance hue ramp -> frg_clr.
    mats = []
    for i in range(16):
      gx, gz = i % 4, i // 4
      tx, tz = (gx - 1.5) * 1.6, (gz - 1.5) * 1.6
      h = i / 16.0                                       # per-instance debug data
      mats.append(_inst_mtx(tx, 0.0, tz, 1.0, h, 1.0 - h, 0.5))
    self.instances = mats

  def onUpdate(self, updinfo):
    self.subdiv.inputs.level = 1+int(updinfo.absolutetime)%5   # LIVE plug: spins the nozzle ring (runtime, no rebuild)

__all__ = ["InstanceDemo"]
