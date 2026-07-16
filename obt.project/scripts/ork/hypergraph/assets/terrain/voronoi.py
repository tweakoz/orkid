###############################################################################
# voronoi — the T.voronoi noise PRIMITIVE and nothing else, shaded pure white so
# the raw noise shape reads under lighting alone (no albedo to distract). Flat
# per-cell values -> stepped cells (sharp risers at cell walls). A noise-basis
# example/test asset (cf. perlin / simplex / worleyf1).
#   ork.terrain.viewer2.py voronoi
#   ork.terrain.viewer2.py voronoi -p frequency=16
###############################################################################
from ork.hypergraph.dflow.terrain import HeightField
from ork.hypergraph.dflow import terrain as T
from ork.hypergraph.units import cycles, meters
from ork.hypergraph.assets.materials.terrain import Fbm
from orkengine.core import vec3, mtx4, quat

xf = mtx4.composed(
    vec3(0,0,0),
    quat(vec3(0,1,0),.5),
    0.01 )

class Voronoi(HeightField):
    MATERIAL_CLASS = Fbm
    MATERIAL_PARAMS = {
      "albedo": vec3(0.25),
      "roughness": 0.5,
      "inp_xf": xf,
      "octaves": 8,
      "aa": 0.5
    }

    def __init__(self, frequency=cycles(8), octaves=1, amplitude=meters(2000)):  # octaves=1 = pure primitive
        super().__init__()
        # TYPED LITERALS (E0 part 2): frequency is spatial frequency (cycles across the map),
        # amplitude IS the relief in TRUE METERS — the units ride the value, so the unit
        # suffix comes off the name (amplitude_m -> amplitude). A Terrain Parameter.
        self.capture(T.voronoi(frequency=frequency, octaves=octaves,
                            amplitude=amplitude), "height")
