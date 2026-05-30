###############################################################################
# materials — reusable ptex3d DSL surface classes (GEOV2 procedural materials).
#
# Each is a Ptex3d subclass authored from the ptex3d DSL; instantiate one through
# the ptex3d asset wrapper:
#     from ork.hypergraph.assets.materials import CrackedMud
#     mat = self.asset.Ptex3d("mud", dsl_class=CrackedMud, cell_scale=5.0)
###############################################################################
from .cracked_mud import CrackedMud, CrackedMudPOM
from .cobblestone import CobblestonePOM
from .lily_pads   import LilyPadsPOM
__all__ = ["CrackedMud", "CrackedMudPOM", "CobblestonePOM", "LilyPadsPOM"]
