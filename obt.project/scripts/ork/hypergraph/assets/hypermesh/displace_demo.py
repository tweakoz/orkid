###############################################################################
# DisplaceDemo — E.1: the first CROSS-FAMILY graph. Terrain expression modules
# (T.fbm / T.terrace / the TerrainNode algebra) and hypermesh modules compose in
# ONE dflow graph: the fbm field computes on the GPU at displace's field_dim and
# feeds the mesh displace through the family-neutral GpuComputeImage2D plug —
# no EXR handoff, no separate bake, one serialized artifact.
#
# E.1b — the field is ANIMATED: the base fbm pans over time (offset_vel, fed by
# the SAME env clock hypermesh uses — B.4; pause/resume hold it), so the terraced
# relief rolls across the grid live. All fbm params are RUNTIME (params SSBO) —
# poke m.inputs.* between frames to edit the field with no recompile.
#
#   ./ork.hypermesh.viewer.py displace_demo
###############################################################################

from ork.hypergraph.dflow.hypermesh import Hypermesh
from ork.hypergraph.dflow import terrain as T

EXTENT = 8.0    # grid XZ span (and the field's mapping extent)
HEIGHT = 1.6    # displacement amplitude (mesh units)

class DisplaceDemo(Hypermesh):
  def __init__(self):
    super().__init__()
    n = self.ripple(grid=1024, amp=1.0, freq=1.0, extent=EXTENT)   # flat shared-vert grid
    # the field — authored with the SAME terrain vocabulary as a HeightField bake:
    # PANNING fbm relief (offset_vel -> rolls over time), softly terraced, with
    # finer counter-panning fbm detail on top.
    h = T.fbm(frequency=5.0, offset_vel=(0.35, 0.12))
    h = T.terrace(h, step_m=1.0/32.0, sharpness=2.5) * 0.8 \
        + T.fbm(frequency=18.0, offset_vel=(-0.06, 0.025)) * 0.05
    h = T.lpf(h, cutoff_m=10)
    n = self.displace(n, field=h, amount=HEIGHT, extent=EXTENT, mode="y", field_dim=1024)
    n = self.smooth_normals(n)                                    # displace moves P only; refresh shading
    self.output(n)
