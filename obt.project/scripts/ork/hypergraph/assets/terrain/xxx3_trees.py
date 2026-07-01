###############################################################################
# xxx3_trees — the xxx3 erosion terrain (reused verbatim) + a "trees" SCATTER sink:
# 16 mutually-exclusive types (8 broadleaf "bl0..7", 8 conifer "cf0..7") placed by
# a TREELINE + SLOPE mask. Broadleaf favors the low band, conifer the mid-high band
# (up to the treeline); nothing above it or on steep faces. Each placed point gets a
# type_id = its index in the types dict; the 16 tree hypermesh graphs each filter to
# their type_id via instance_source(scatter_asset="<terrain>", sink="trees", type_id).
###############################################################################
from ork.hypergraph.dflow import terrain as T
from ork.hypergraph.assets.terrain.xxx3 import XXX3

NUM_SEEDS = 8   # variants per species -> 2 species x 8 = 16 types


class XXX3Trees(XXX3):
  def __init__(self, iters=16):
    super().__init__(iters=iters)                      # builds the relief + exposes self._height
    elev   = T.normalize(self._height)                 # [0,1] elevation over the whole range
    slope  = T.slope(self._height, radius_m=1.0)      # 0 flat .. 1 steep
    gentle = slope #1.0 - T.band(slope, 0.05, 0.10, soft=0.10)   # 1 on gentle ground, 0 on cliffs
    # elevation zones (overlapping at the transition so the two species mingle there):
    broadleaf = gentle * T.band(elev, 0.0,  0.22, soft=0.0)   # valleys / lower slopes
    conifer   = gentle * T.band(elev, 0.16, 0.38, soft=0.0)   # mid-high, up to the treeline

    types = {}
    for i in range(NUM_SEEDS):
      types["bl%d" % i] = broadleaf      # species A — type_id 0..7
    for i in range(NUM_SEEDS):
      types["cf%d" % i] = conifer        # species B — type_id 8..15

    self.scatter("trees",
        count  = 150000,                 # total trees over the plantable band (sparse forest)
        seed   = 1337,
        align  = "up",               # base sits on the slope, trunk along the surface normal
        scale  = (30.0, 15.0),           # per-tree size (LsAnim is ~6 units tall -> ~60..120 m)
        cutoff = 0.05,                   # reject where the total weight is tiny (off-band)
        jitter = 1.6,
        types  = types)


__all__ = ["XXX3Trees"]
