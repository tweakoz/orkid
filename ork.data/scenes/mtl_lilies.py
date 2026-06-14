###############################################################################
# geov2_phase4c_lilies.py — round lily-pad / pillar relief (GEOV2 ptex3d POM).
#
# A full-hemisphere voronoi dome (radial .f1 profile) reads as round pads /
# pillars rather than cobblestones — saved here as its own look. Procedural
# parallax occlusion (EXPENSIVE/TEMPORARY, GEOV2 §18.5).
#
#   ork.scene.viewer.py geov2_phase4c_lilies
###############################################################################

from orkengine.core import lev2_pyexdir, vec3
from ork.hypergraph.ecs.scene import Scene
from ork.hypergraph.assets.materials import LilyPads

lev2_pyexdir.addToSysPath()


class LiliesScene(Scene):

  def __init__(self):
    super().__init__()                       # self.SG = default ForwardPBR scenegraph
    A = self.asset

    mat = A.Ptex3d("lily_mtl",
                   dsl_class=LilyPads,
                   cell_scale=1.0,
                   radius=0.6,
                   relief=0.15,
                   pom_steps=16)
    drw = A.IcoSphere("lily_sphere",
                      radius=2.5,
                      subdivisions=5,
                      material=mat)

    self.entity(
      "lilyball",
      transform  = {"translation": vec3(0, 0, 0)},
      components = [self.SG.component(nodes={"n": {"drawable": drw}})])
