###############################################################################
# simplex — the T.simplex noise PRIMITIVE and nothing else, shaded pure white so
# the raw noise shape reads under lighting alone (no albedo to distract).
# A noise-basis example/test asset (cf. perlin / worleyf1 / voronoi).
#   ork.terrain.viewer2.py simplex
#   ork.terrain.viewer2.py simplex -p frequency=16 -p octaves=5
###############################################################################
from ork.hypergraph.dflow.terrain import HeightField
from ork.hypergraph.dflow import terrain as T
from ork.hypergraph.assets.materials.terrain import Fbm
from orkengine.core import vec3, mtx4, quat

xf = mtx4.composed(
    vec3(0,0,0),
    quat(vec3(0,1,0),.5),
    0.01 )

class Simplex(HeightField):
    HEIGHT_M       = 2000.0
    MATERIAL_CLASS = Fbm
    MATERIAL_PARAMS = {
      "albedo": vec3(0.25),
      "roughness": 0.5,
      "inp_xf": xf,
      "octaves": 8,
      "aa": 0.5
    }

    def __init__(self, frequency=8.0, octaves=1):   # octaves=1 = pure primitive
        super().__init__()
        self.capture(T.simplex(frequency=frequency, octaves=octaves), "height")
