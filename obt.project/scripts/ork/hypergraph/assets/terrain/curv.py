###############################################################################
# Curv — visualizes the CURVATURE mask of fbm hills (Mask by Feature: curvature).
# Captures the mask itself as the height channel so Preview shows the structure:
#   mode="convex"    -> bright on ridges / peaks
#   mode="concave"   -> bright in valleys / pits
#   mode="magnitude" -> bright wherever the terrain bends (ridges + valleys)
# Try:  ork.terrain.view.py curv -p mode=concave -p scale=0.08
###############################################################################
from ork.hypergraph.dflow.terrain import HeightField
from ork.hypergraph.dflow import terrain as T


class Curv(HeightField):
    # radius_m is in METERS (resolution-independent): at the default 4096 m extent
    # baked at 1024, 96 m == 24 texels (the landform-scale curvature).
    def __init__(self, octaves=6, scale=0.5, mode="convex", radius_m=96.0):
        super().__init__()
        base = T.Fbm(frequency=3.0, octaves=octaves) * 0.5 + 0.5
        self.capture(T.curvature(base, scale=scale, mode=mode, radius_m=radius_m), "height")
