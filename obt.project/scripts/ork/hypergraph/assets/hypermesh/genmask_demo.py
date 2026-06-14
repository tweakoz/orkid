###############################################################################
# GenMaskDemo — generator face-tagging (the upstream-free analogue of select()), rendered with
# GroupView. The cone is BORN tagged: the generator's mask= puts every face in group 0. A downstream
# Select then ADDs group 1 to the base cap (its normal points -Y) — so selection composes on top of the
# tags the generator created. onUpdate animates `sides` (3 <-> 48): the generator re-tags every level
# and the select re-derives the base, all live.
#   side faces -> group 0 (one color)      base cap -> group 0+1 (another color)
#
#   ./ork.hypermesh.viewer.py genmask_demo
###############################################################################
from orkengine.core import vec3
from ork.hypergraph.dflow.hypermesh import (Hypermesh, sel_normal_dir, isolate, add, group, POLY)
from ork.hypergraph.assets.materials.hypermesh import GroupView


class GenMaskDemo(Hypermesh):
  MATERIAL_CLASS = GroupView   # color faces by selection-group bits

  def __init__(self):
    super().__init__()
    self._cone = self.cone(radius=1.6, height=2.6, sides=24, mask=isolate(group(0)))  # whole cone -> grp 0
    base = sel_normal_dir(n=vec3(0, -1, 0), t=0.5, soft=0.05)                          # the -Y base cap
    self._sel = self.select(self._cone, base, domain=POLY, op=add(group(1)))          # base -> + group 1
    self.output(self._sel)

  def onUpdate(self, updinfo):
    t = updinfo.absolutetime
    phase = (t % 16.0) / 16.0
    tri   = 1.0 - abs(2.0 * phase - 1.0)
    self._cone.inputs.sides = int(round(3 + 45 * tri))    # 3 <-> 48: generator re-tags each level


__all__ = ["GenMaskDemo"]
