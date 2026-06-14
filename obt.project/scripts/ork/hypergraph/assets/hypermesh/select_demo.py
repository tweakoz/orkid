###############################################################################
# SelectDemo — the SELECTION + GroupView showcase. An icosphere tagged into three disjoint named
# groups via the SelExpr DSL + MaskOp factories, rendered with GroupView (faces colored by their
# __tags working bits) so the groups read at a glance — no --groups flag needed (it's the asset's
# MATERIAL_CLASS, like a terrain material):
#   group 0 (add) = the +Y cap        (normal points up)
#   group 1 (add) = the -Y cap        (normal points down)
#   group 2 (add) = the equator belt  (centroid height near 0)
# Each select chains onto the previous (COW __tags band), so the bits accumulate. onUpdate animates the
# icosphere `subdivisions` (1 <-> 4) — the selections RE-DERIVE against the changing tessellation each
# frame, proving selection participates in the live animation.
#
#   ./ork.hypermesh.viewer.py select_demo
###############################################################################
from orkengine.core import vec3
from ork.hypergraph.dflow.hypermesh import (Hypermesh, sel_normal_dir, sel_height_band,
                                            add, group, POLY)
from ork.hypergraph.assets.materials.hypermesh import GroupView


class SelectDemo(Hypermesh):
  MATERIAL_CLASS = GroupView   # color faces by selection group (the default render; --groups not needed)

  def __init__(self):
    super().__init__()
    self._sph = self.icosphere(radius=1.8, subdivisions=2)
    up   = sel_normal_dir(n=vec3(0,  1, 0), t=0.6, soft=0.05)    # +Y cap    -> group 0
    dn   = sel_normal_dir(n=vec3(0, -1, 0), t=0.6, soft=0.05)    # -Y cap    -> group 1
    belt = sel_height_band(-0.35, 0.35, soft=0.06)               # equator   -> group 2
    s0 = self.select(self._sph, up,   domain=POLY, op=add(group(0)))
    s1 = self.select(s0,        dn,   domain=POLY, op=add(group(1)))
    s2 = self.select(s1,        belt, domain=POLY, op=add(group(2)))
    self.output(s2)

  def onUpdate(self, updinfo):
    t     = updinfo.absolutetime
    phase = (t % 16.0) / 16.0
    tri   = 1.0 - abs(2.0 * phase - 1.0)                          # 0 -> 1 -> 0 over 8s
    self._sph.inputs.subdivisions = int(round(1 + 3 * tri))      # 1 <-> 4: groups re-derive each level


__all__ = ["SelectDemo"]
