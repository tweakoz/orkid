from ork.hypergraph.dflow.terrain import HeightField
from ork.hypergraph.dflow import terrain as T
class LpfTest(HeightField):
    def __init__(self, cutoff=8.0, units='texels', frequency=24.0, octaves=7):
        super().__init__()
        AMPLITUDE_M = 4000.0   # authored vertical relief in meters (natural units)
        base = (T.Fbm(frequency=frequency, octaves=octaves) * 0.5 + 0.5) * AMPLITUDE_M
        self.capture(T.lpf(base, cutoff=cutoff, units=units), "height")
