from ork.hypergraph.dflow.terrain import HeightField
from ork.hypergraph.dflow import terrain as T
class LpfTest(HeightField):
    def __init__(self, cutoff_texels=8.0, cutoff_m=0.0, frequency=24.0, octaves=7):
        super().__init__()
        base = T.Fbm(frequency=frequency, octaves=octaves) * 0.5 + 0.5
        cm = cutoff_m if cutoff_m > 0 else None
        self.capture(T.lpf(base, cutoff_texels=cutoff_texels, cutoff_m=cm), "height")
