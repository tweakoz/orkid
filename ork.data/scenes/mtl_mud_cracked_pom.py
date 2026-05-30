###############################################################################
# mtl_mud_cracked_pom.py — cracked mud with PROCEDURAL PARALLAX OCCLUSION.
#
# The same CrackedMud material as mtl_mud_cracked, but with `pom_steps` > 0: the
# height field is RAY-MARCHED along the view ray so the cracks show real depth +
# self-occlusion, and the surface is evaluated at the displaced hit point — not
# merely normal-perturbed.
#
#   *** TEMPORARY / EXPENSIVE ***
#   The march re-evaluates the procedural height (warp + voronoi) `pom_steps`x per
#   fragment (GEOV2 §18.5 cost trap). A baked-height texture sample replaces the
#   procedural march once auto-uv lands. Lower `pom_steps` / SSAA if it's heavy.
#
# Bindable: base_color / crack / warp / bump_scale / relief (parallax depth).
#
# Run:
#   ork.scene.viewer.py mtl_mud_cracked_pom
###############################################################################

from orkengine.core import lev2_pyexdir, vec3
from ork.hypergraph.ecs.scene import Scene
from ork.hypergraph.assets.materials import CrackedMud

lev2_pyexdir.addToSysPath()


###############################################################################

class CrackedMudPOMScene(Scene):

  def __init__(self):
    super().__init__()                       # self.SG = default ForwardPBR scenegraph
    A = self.asset

    mat = A.Ptex3d("mud_mtl",
                   dsl_class=CrackedMud,
                   cell_scale=5.0,
                   pom_steps=12)
    drw = A.IcoSphere("mud_sphere",
                      radius=2.5,
                      subdivisions=5,
                      material=mat)

    self.entity(
      "mudball",
      transform  = {"translation": vec3(0, 0, 0)},
      components = [self.SG.component(nodes={"n": {"drawable": drw}})])
