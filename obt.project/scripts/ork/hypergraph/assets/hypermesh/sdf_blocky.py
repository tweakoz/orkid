###############################################################################
# SdfBlocky — CUBERILLE companion to SdfConform. The SAME N orbiting spheres, but
# extracted as a PURE VOXEL-BLOCK surface via sdf.to_mesh(blocky=True) instead of
# conformed. This shows the field's TRUE topology — disconnected lumps that merge
# and split as they orbit — which conform (a fixed-topology shrink-wrap) cannot.
# No redistance / conform / fairing / temporal: blocky needs only the field's SIGN.
# Block size = the brick voxel (RES/extent): higher RES = finer blocks.
#
#   N orbiting spheres ─ smooth_union ─ to_mesh(blocky=True) ─ (render)
#
#   ./ork.hypermesh.viewer.py sdf_blocky
###############################################################################
from ork.hypergraph.dflow.hypermesh import Hypermesh
from ork.hypergraph.assets.hypermesh.sdf_conform import _orbit_basis   # reuse the orbit-plane basis
from ork.hypergraph.assets.materials.terrain.solid import Solid
from orkengine.core import vec3, vec4
import math


class SdfBlocky(Hypermesh):
  MATERIAL_CLASS  = Solid
  MATERIAL_PARAMS = {
      "color": vec4(0.85, 0.45, 0.2, 1.0),   # flat-shaded blocks read clearly on a mid-roughness non-metal
      "roughness": 0.5,
      "metallic": 0.0,
  }

  N            = 4       # number of orbiting spheres
  RES          = 160      # BLOCK SIZE: higher = finer blocks (cost ~dim^3 per frame)
  RADIUS       = 0.7     # each sphere's SDF radius (vs ORBIT_R sets how much they overlap/separate)
  K            = 0.25    # smooth_union blend (0 -> hard sphere intersections)
  ORBIT_R      = 1.0     # orbit radius (sphere CENTER distance from origin)
  OMEGA        = 0.6     # base angular rate (rad/sec)
  OMEGA_DETUNE = 0.15    # per-sphere rate spread

  def __init__(self):
    super().__init__()
    reach     = self.ORBIT_R + self.RADIUS
    extent    = 8.0 * reach + 2.0                          # brick contains the swept blob + margin
    sdfc      = self.sdf(dim=self.RES, extent=extent)
    self.spheres = [sdfc.sphere(self.RADIUS) for _ in range(self.N)]
    self._orbits = [_orbit_basis(i, self.N) for i in range(self.N)]

    sdf = self.spheres[0]
    for s in self.spheres[1:]:
      sdf = sdf.smooth_union(s, k=self.K)
    self.output(sdf.to_mesh(blocky=False,weld=False))                 # pure voxel-block surface — TRUE topology

  def _angle(self, i, t):
    phi = 2.0 * math.pi * i / self.N
    w   = self.OMEGA * (1.0 + self.OMEGA_DETUNE * i)
    return w * t + phi

  def onUpdate(self, updinfo):
    t = updinfo.absolutetime
    for i, s in enumerate(self.spheres):
      th     = self._angle(i, t)
      u, v   = self._orbits[i]
      ct, st = math.cos(th), math.sin(th)
      c = (self.ORBIT_R * (ct*u[0] + st*v[0]),
           self.ORBIT_R * (ct*u[1] + st*v[1]),
           self.ORBIT_R * (ct*u[2] + st*v[2]))
      s.inputs.offset = vec3(*c)
