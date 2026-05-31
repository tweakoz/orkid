###############################################################################
# ork.data/terrain/rolling_hills.py — a terrain HeightField DSL file, resolvable
# by bare name ("rolling_hills") via ORK_TERRAIN_SEARCH_PATH (default
# <ork.data>/terrain). The HeightField asset wrapper runs this ONCE at authoring
# to produce the graph, then embeds the serialized graph in the scene.
###############################################################################
from ork.hypergraph.dflow.terrain import HeightField
from ork.hypergraph.dflow import terrain as T


class RollingHills(HeightField):
    def __init__(self, octaves=6, steps=6):
        super().__init__()
        h = T.Fbm(frequency=3.0, octaves=octaves) * 0.5 + 0.5
        self.capture(T.Terrace(h, steps=steps, sharpness=4.0), "height")
