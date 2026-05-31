###############################################################################
# Hydro — fbm hills run through HYDRAULIC (pipe-model) erosion. Unlike thermal
# (which only softens slopes), hydraulic erosion routes water downhill and lets
# the flow carve drainage channels / gullies and deposit sediment in the valleys
# — so you get branching river networks, not just smoothing.
#   ork.terrain.view.py hydro
#   ork.terrain.view.py hydro -p iterations=120 -p capacity=0.5
###############################################################################
from ork.hypergraph.dflow.terrain import HeightField
from ork.hypergraph.dflow import terrain as T


class Hydro(HeightField):
    def __init__(self, octaves=6, iterations=250,
                 rain=0.12, evaporation=0.015, capacity=0.15, erosion=0.255, deposition=0.50):
        super().__init__()
        base = T.Fbm(frequency=3.0, octaves=octaves) * 0.5 + 0.5
        h1 = T.erode_hydro(base, iterations=iterations, rain=rain, evaporation=evaporation,
                          capacity=capacity, erosion=erosion, deposition=deposition)
        self.capture(h1,"height")
