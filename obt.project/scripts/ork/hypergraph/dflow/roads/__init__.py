###############################################################################
# roads (R.) — procedural roads / streets / layout family (HyperSyn).
#
# The layout is a FUNCTION of terrain and is fed BACK into it (spec §0):
#   FORWARD  terrain fields (slope/curvature/discharge) -> RouteSpine cost field
#   BACKWARD roadbed_mask + road_elev_m -> terrain MaskBlend flatten; keepout ->
#            scatter sinks inverted. All in ONE Merkle graph (Q5 in-graph v1).
#
# C++ modules live in the hypermesh family (Q3: LSystem precedent) — R. is a DSL
# namespace over them. This package is import-light at the top level so the
# pure-python routing REFERENCE (route_ref / mask_ref — the executable spec the
# C++ mirrors, scatter.py<->hfdflow_scatter.cpp style) imports WITHOUT orkengine,
# for standalone analytic oracles. The engine-facing DSL (`Roads`) pulls
# orkengine LAZILY (see base.py) so it never breaks the standalone reference.
#
#   # standalone (no engine): analytic oracles
#   from ork.hypergraph.dflow.roads import route_ref, mask_ref
#   # engine DSL (needs the built C++ R-modules):
#   from ork.hypergraph.dflow import roads as Rd
#   town = Rd.Roads(); spine = town.route_spine(pois=[...], max_grade=0.1)
###############################################################################

# route_ref / mask_ref are imported explicitly by callers (kept out of the
# package top level so the reference stays orkengine-free / standalone-runnable).

def __getattr__(name):
  # lazy DSL surface — only touched when the engine is present.
  if name in ("Roads", "RoadProfile", "BuildTypeTable"):
    from . import base as _base
    return getattr(_base, name)
  raise AttributeError("module %r has no attribute %r" % (__name__, name))
