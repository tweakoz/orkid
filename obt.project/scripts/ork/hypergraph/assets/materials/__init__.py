###############################################################################
# materials — reusable ptex3d DSL surface classes (GEOV2 procedural materials).
#
# Each is a Ptex3d subclass authored from the ptex3d DSL; instantiate one through
# the ptex3d asset wrapper:
#     from ork.hypergraph.assets.materials import CrackedMud
#     mat = self.asset.Ptex3d("mud", dsl_class=CrackedMud, cell_scale=5.0)
###############################################################################
from .cracked_mud import CrackedMud
from .cobblestone import CobbleStone
from .lily_pads   import LilyPads
from .beachball   import BeachBall
from .basketball  import BasketBall
from .soccer      import SoccerBall
from .honeycomb   import HoneyComb
from .carbon_fiber import CarbonFiber
from .spaceship_hull import SpaceshipHull
__all__ = ["CrackedMud", "CobbleStone", "LilyPads", "BeachBall", "BasketBall",
           "SoccerBall", "HoneyComb", "CarbonFiber", "SpaceshipHull"]
