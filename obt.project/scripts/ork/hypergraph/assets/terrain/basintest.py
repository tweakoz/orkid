from ork.hypergraph.dflow.terrain import HeightField
from ork.hypergraph.dflow import terrain as T
class BasinTest(HeightField):
    def __init__(self, frequency=4.0, octaves=5, deepen=2000.0):
        super().__init__()
        AMPLITUDE_M = 4000.0   # authored vertical relief in meters (natural units)
        base = (T.Fbm(frequency=frequency, octaves=octaves) * 0.5 + 0.5) * AMPLITUDE_M
        conc = T.curvature(base, mode="concave", radius_m=160)
        deep = base + conc*(-deepen)        # lower concave valleys (deepen METERS) -> closed basins
        self.capture(deep, "raw")
        self.capture(T.basin_fill(deep), "height")
