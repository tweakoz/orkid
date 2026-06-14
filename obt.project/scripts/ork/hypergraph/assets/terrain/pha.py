###############################################################################
# Pha — fbm terrain run through the PROCEDURAL "phacelle" erosion FILTER
# (Rune Skovbo Johansen's "fast and gorgeous erosion", MPL-2.0). Single-pass,
# resolution-independent, speckle-free, fast, predictable — a different paradigm
# from the iterative Mei sim in erox.py.
#   ork.terrain.view.py pha --dim 512
#   ork.terrain.view.py pha -p strength=0.3 -p gully_weight=0.7
###############################################################################
from ork.hypergraph.dflow.terrain import HeightField
from ork.hypergraph.dflow import terrain as T


class Pha(HeightField):
    def __init__(self,
                 frequency=3.0,
                 octaves=2,
                 # --- phacelle erosion filter ---
                 strength=0.12,         # overall erosion magnitude
                 gully_weight=0.5,      # gully magnitude [0,1] (0=sharpen, 1=full gullies)
                 detail=1.5,            # higher-freq gullies restricted to steeper slopes when lower
                 scale=0.15,            # horizontal+vertical scale of the erosion
                 cell_scale=0.7,        # phacelle cell size relative to scale
                 normalization=0.5,     # phacelle magnitude normalization [0,1]
                 lacunarity=2.0,        # per-octave frequency multiplier
                 gain=0.5,              # per-octave magnitude multiplier
                 default_height=0.5,    # mid-height reference (fadeTarget 0)
                 pha_octaves=5):        # gully-octave count (baked)
        super().__init__()
        base = T.Fbm(frequency=frequency, octaves=octaves) * 0.5 + 0.5
        self.capture(
            T.pha(base*4.0,
                  strength=strength,
                  gully_weight=gully_weight,
                  detail=detail,
                  scale=scale,
                  cell_scale=cell_scale,
                  normalization=normalization,
                  lacunarity=lacunarity,
                  gain=gain,
                  default_height=default_height,
                  octaves=pha_octaves),
            "height")
