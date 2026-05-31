###############################################################################
# Tilted — a diagonal gradient ramp blended with fbm detail. Exercises the
# vec2 "dir" plug: T.Gradient takes dir=(dir_x, dir_y) carried as a single
# vec2 plug (GradientModule), so the ramp runs along a non-axis-aligned
# direction. Authored in the expression-first terrain DSL.
###############################################################################
from ork.hypergraph.dflow.terrain import HeightField
from ork.hypergraph.dflow import terrain as T


class Tilted(HeightField):
    def __init__(self, dir_x=0.6, dir_y=0.8, scale=0.5, detail=0.25, octaves=5):
        super().__init__()
        ramp   = T.Gradient(dir_x=dir_x, dir_y=dir_y, scale=scale, bias=0.0)
        noise  = T.Fbm(frequency=4.0, octaves=octaves) * detail
        self.capture(T.Clamp(ramp + noise, 0.0, 1.0), "height")
