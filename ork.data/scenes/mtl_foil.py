###############################################################################
# mtl_foil.py — crinkled gold foil on a sphere (GEOV2 ptex3d).
#
# Foil = perturbed normals + standard metallic PBR. The reusable `crumple` field
# (ridged volumetric turbulence) drives the faceted normals; metallic=1 + a gold
# reflectance tint does the rest. Volumetric, so it wraps the sphere with no
# projection seams. Change metal_color for silver / copper.
#
#   ork.scene.viewer.py mtl_foil
###############################################################################

from orkengine.core import lev2_pyexdir, vec3
from ork.hypergraph.ecs.scene import Scene
from ork.hypergraph.assets.materials import Foil

lev2_pyexdir.addToSysPath()


class FoilScene(Scene):

  def __init__(self):
    super().__init__()                       # self.SG = default ForwardPBR scenegraph
    A = self.asset

    mat = A.Ptex3d("foil_mtl",
                   dsl_class=Foil,
                   scale=6.0,
                   wrinkle=0.01)
    drw = A.IcoSphere("foil_sphere",
                      radius=2.5,
                      subdivisions=6,
                      material=mat)

    self.entity("foil",
                transform={"translation": vec3(0, 0, 0)},
                components=[self.SG.component(nodes={"n": {"drawable": drw}})])
