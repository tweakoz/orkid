###############################################################################
# flow3d — VALIDATION asset for the continuous flow-field primitive (T.flow3d).
#
# Emits TWO outputs from one module:
#   flowdir   — RGBA: R,G = continuous downhill flow direction (-grad z, any angle, NOT D8);
#               B = slope. View the .exr as a colour image to eyeball the direction field.
#   discharge — mono MFD drainage area (the flow map), viewable as relief.
#
#   ork.terrain.viewer2.py -d 1024 flow3d        # renders the captured "height" (discharge) as relief
#   ork.image.summarize.py -i <assetcache>/terrain/flow3d/flowdir.exr   # inspect the RGBA dir field
###############################################################################
from ork.hypergraph.dflow.terrain import HeightField
from ork.hypergraph.dflow import terrain as T


class Flow3D(HeightField):
    EXTENT_M = 16384.0
    HEIGHT_M = 1000.0

    def __init__(self):
        super().__init__()
        h      = T.Fbm(frequency=6.0, octaves=8) * 0.5 + 0.5
        filled = T.basin_fill(h)               # exact CPU priority-flood (through-drainage)
        f      = T.flow3d(filled)              # .dir, .discharge, .metrics
        self.capture(f.dir, "flowdir")         # RGBA: R,G=direction  B=slope        -> flowdir.exr
        self.capture(f.discharge, ["discharge","height"]) # mono drainage-area image            -> discharge.exr
        self.capture(f.metrics, "metrics")     # RGBA: R=flatness G=curvature B=wetness -> metrics.exr
