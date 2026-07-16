###############################################################################
# perlin — the T.perlin noise PRIMITIVE and nothing else, shaded pure white so
# the raw noise shape reads under lighting alone (no albedo to distract).
# A noise-basis example/test asset (cf. simplex / worleyf1 / voronoi).
#   ork.terrain.viewer2.py perlin
#   ork.terrain.viewer2.py perlin -p frequency=16 -p octaves=5
###############################################################################
from ork.hypergraph.dflow.terrain import HeightField
from ork.hypergraph.dflow import terrain as T
from ork.hypergraph.assets.materials.terrain import Solid, Fbm
from orkengine.core import vec3, mtx4, quat

xf = mtx4.composed(
    vec3(0,0,0),
    quat(vec3(0,1,0),.5),
    0.01 )

class Perlin(HeightField):
    MATERIAL_CLASS = Fbm
    MATERIAL_PARAMS = {
      "albedo": vec3(0.25),
      "roughness": 0.5,
      "inp_xf": xf,
      "octaves": 8,
      "aa": 0.5
    }

    def __init__(self, frequency=8.0, octaves=1, amplitude_m=2000.0):   # octaves=1 = pure primitive
        super().__init__()
        # amplitude_m IS the relief in TRUE METERS (natural units) — a Terrain Parameter.
        self.capture(T.perlin(frequency=frequency, octaves=octaves,
                            amplitude=float(amplitude_m)), "height")
