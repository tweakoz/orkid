###############################################################################
# mtl_onyx.py — translucent veined onyx (GEOV2 ptex3d + PBR transmission lobe).
#
# Shows that ptex3d materials get full access to the stock PBR lobes: the veined
# procedural surface (Marble) is the per-pixel base, and transmission / ior /
# attenuation are set per-instance on the Ptex3d asset (material-level UBO
# uniforms the forward-PBR lighting already consumes). No shader change.
#
#   ork.scene.viewer.py mtl_onyx
###############################################################################

from orkengine.core import lev2_pyexdir, vec3
from ork.hypergraph.ecs.scene import Scene
from ork.hypergraph.assets.materials import Marble

lev2_pyexdir.addToSysPath()


class TransparentMarbleScene(Scene):

  def __init__(self):
    super().__init__()                       # self.SG = default ForwardPBR scenegraph
    A = self.asset

    mat = A.Ptex3d("marble",
                   dsl_class=Marble,
                   base_color=vec3(0.5),    
                   vein_color=vec3(0.2, 0.2, 0.4),
                   vein_freq=0.16,
                   roughness=0.08,
                   # --- PBR lobes (pass straight through to the material) ---
                   transmission_factor=0.15,
                   transmission_roughness=0.5,
                   ior=1.5,
                   attenuation_color=vec3(0.80, 0.55, 0.22),
                   attenuation_distance=1.2,
                   volume_thickness_factor=1.0)
    drw = A.IcoSphere("marble_sphere",
                      radius=2.5,
                      subdivisions=6,
                      material=mat)

    self.entity("marble",
                transform={"translation": vec3(0, 0, 0)},
                components=[self.spinner(), self.SG.component(nodes={"n": {"drawable": drw}})])
