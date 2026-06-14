from ork.hypergraph.dflow.terrain import HeightField
from ork.hypergraph.dflow import terrain as T
class BasinTest(HeightField):
    def __init__(self, frequency=4.0, octaves=5, deepen=0.5):
        super().__init__()
        base = T.Fbm(frequency=frequency, octaves=octaves) * 0.5 + 0.5
        conc = T.curvature(base, mode="concave", radius_m=160)
        deep = base + conc*(-deepen)        # lower concave valleys -> closed basins
        self.capture(deep, "raw")
        self.capture(T.basin_fill(deep), "height")
