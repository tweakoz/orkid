###############################################################################
# HF1 — a basic terrain heightfield: terraced fbm. Authored in the expression-
# first terrain DSL; the HeightField asset wrapper runs this once to produce the
# graph, then embeds the serialized graph in the scene (loads with no Python).
###############################################################################
from ork.hypergraph.dflow.terrain import HeightField
from ork.hypergraph.dflow import terrain as T


class HF1(HeightField):
    def __init__(self, octaves=6, steps=6):
        super().__init__()
        AMPLITUDE_M = 4000.0   # authored vertical relief in meters (natural units)
        h = (T.Fbm(frequency=3.0, octaves=octaves) * 0.5 + 0.5) * AMPLITUDE_M
        self.capture(T.Terrace(h, step_m=AMPLITUDE_M / steps, sharpness=4.0), "height")
