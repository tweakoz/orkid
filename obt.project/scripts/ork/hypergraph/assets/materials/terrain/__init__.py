###############################################################################
# materials.terrain — TERRAIN-coupled ptex3d materials.
#
# Unlike the generic materials, these are authored to be driven by a terrain
# HeightField: the terrain asset wrapper forwards `height_scale` (and any
# MATERIAL_PARAMS) into the material ctor, so elevation-based shading is natural
# here. height_scale-carrying materials belong in THIS subpackage, not in the
# generic materials root.
#
#   from ork.hypergraph.assets.materials.terrain import Solid, Fbm, WorleyF1, Voronoi
###############################################################################
from .solid    import Solid
from .fbm      import Fbm
from .worleyf1 import WorleyF1
from .voronoi  import Voronoi

__all__ = ["Solid", "Fbm", "WorleyF1", "Voronoi"]
