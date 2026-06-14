###############################################################################
# BitOpDemo — the __tags BIT-BANKING showcase, rendered with GroupView. Two hemisphere selections are
# saved into storage banks, then COMBINED back into the working/viz band:
#   selA = +Y hemisphere -> working -> save to bank 1
#   selB = +X hemisphere -> working -> save to bank 2   (overwrites working; that's why we banked A)
#   combine: working(bank 0) = bank 1  [OP]  bank 2
# onUpdate cycles the combine OP (OR -> AND -> XOR) every 3s — the visible region changes shape with NO
# shader recompile (the op is a runtime ctl uniform), proving banking params tweak live. The icosphere
# `subdivisions` also animate, so the whole banking pipeline re-derives against changing tessellation.
#   OR  = +Y union +X            AND = the +X+Y quadrant            XOR = the two off-axis quadrants
#
#   ./ork.hypermesh.viewer.py bitop_demo
###############################################################################
from orkengine.core import vec3
from ork.hypergraph.dflow.hypermesh import (Hypermesh, sel_normal_dir, replace, group, bank,
                                            BIT_OR, BIT_AND, BIT_XOR, POLY)
from ork.hypergraph.assets.materials.hypermesh import GroupView


class BitOpDemo(Hypermesh):
  MATERIAL_CLASS = GroupView   # color faces by their working bits (the combined region)

  _OPS = [BIT_OR, BIT_AND, BIT_XOR]

  def __init__(self):
    super().__init__()
    self._sph = self.icosphere(radius=1.8, subdivisions=2)
    hemiY = sel_normal_dir(n=vec3(0, 1, 0), t=0.0, soft=0.00)   # +Y hemisphere
    hemiX = sel_normal_dir(n=vec3(1, 0, 0), t=0.0, soft=0.00)   # +X hemisphere
    a = self.select(self._sph, hemiY, op=replace(group(0)))     # working = +Y
    a = self.save_bank(a, into=bank(1))                          # bank 1  = +Y
    b = self.select(a, hemiX, op=replace(group(1)))             # working = +X
    b = self.save_bank(b, into=bank(2))                          # bank 2  = +X
    self._combine = self.combine_banks(b, a=bank(1), b=bank(2),  # working = bank1 OP bank2
                                       op=BIT_OR, into=bank(0))
    self.output(self._combine)

  def onUpdate(self, updinfo):
    t = updinfo.absolutetime
    self._combine.op = self._OPS[int(t / 3.0) % 3]              # cycle OR/AND/XOR live (no recompile)
    phase = (t % 20.0) / 20.0
    tri   = 1.0 - abs(2.0 * phase - 1.0)
    self._sph.inputs.subdivisions = int(round(1 + 2 * tri))    # 1 <-> 3: banking re-derives each level


__all__ = ["BitOpDemo"]
