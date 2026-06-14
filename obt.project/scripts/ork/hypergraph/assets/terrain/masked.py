###############################################################################
# Masked — rocky high-frequency detail applied ONLY on steep slopes, leaving the
# flats smooth. Demonstrates the masking story end-to-end:
#   slope(base)        -> a [0,1] mask field (Mask by Feature: slope)
#   base.masked_by(...) -> mix(base, rocky, mask)  (the MaskBlend primitive)
# This is Houdini's lerp(input, op(input), mask), expressed compositionally.
###############################################################################
from ork.hypergraph.dflow.terrain import HeightField
from ork.hypergraph.dflow import terrain as T


class Masked(HeightField):
    def __init__(self, octaves=6, detail_freq=20.0, detail_amp=0.1, sensitivity=0.15):
        super().__init__()
        base  = T.Fbm(frequency=3.0, octaves=octaves) * 0.5 + 0.5   # smooth hills, [0,1]
        steep = T.slope(base, scale=sensitivity)                    # 1 on steep faces, ~0 on flats
        rocky = base + T.Fbm(frequency=detail_freq, octaves=3) * detail_amp
        self.capture(base.masked_by(rocky, steep), "height")        # rocky only where steep
