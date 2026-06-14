###############################################################################
# TransformDemo — the GPU-native Transform op. A cube whose +Y face is tagged (group 0); the transform
# lifts + twists ONLY those verts, animated every frame by re-setting the module's `matrix` PARAM (no
# shader recompile — the matrix is uploaded as rows to an SSBO each frame). The selection mask is itself
# rebuilt on the GPU from the live __tags. Nothing reads back to the CPU.
#
#   ./ork.hypermesh.viewer.py transform_demo     # the top face rises/sinks + spins; the rest stays put
###############################################################################
import math
from orkengine.core import vec3, quat, mtx4
from ork.hypergraph.dflow.hypermesh import Hypermesh, sel_normal_dir, add, group, POLY


class TransformDemo(Hypermesh):
  def __init__(self):
    super().__init__()
    n = self.box(size=1.0)
    n = self.select(n, sel_normal_dir(n=vec3(0, 1, 0), t=0.5, soft=0.05), domain=POLY, op=add(group(0)))
    self._xf = self.transform(n, slot=0)          # identity to start; animated below
    self.output(self._xf)

  def onUpdate(self, updinfo):
    t    = updinfo.absolutetime
    lift = 0.6 + 0.6 * math.sin(t * 2.0)          # 0..1.2 up/down
    spin = quat(vec3(0, 1, 0), t * 1.5)           # continuous yaw of the lifted face
    # compose translate * rotate about the face center (pivot ~ top of the cube), uploaded as a PARAM.
    p   = vec3(0, 1, 0)
    Tt  = mtx4(); Tt.compose(vec3(0, lift, 0), quat(), 1.0)
    Tp  = mtx4(); Tp.compose(p, quat(), 1.0)
    R   = mtx4(); R.compose(vec3(0, 0, 0), spin, 1.0)
    Tpi = mtx4(); Tpi.compose(p * -1.0, quat(), 1.0)
    self._xf.matrix = Tt * Tp * R * Tpi


__all__ = ["TransformDemo"]
