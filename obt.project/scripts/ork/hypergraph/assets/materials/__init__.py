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
from .brick          import Brick
from .marble         import Marble
from .foil           import Foil
from .wood           import Wood, Teak, Oak, Pine
from .plywood        import Plywood
from .animal_skin    import Cheetah, Leopard, Zebra, Reptile
from .ground         import Dirt, Grass, Gravel, Sand
from .geological     import Rock, Snow, Mud          # generic geological surfaces (pre-dflow)
from .solid          import Solid                     # plain solid-color PBR material (live; instancing)
from .asphalt        import Asphalt, ROAD_PRESETS, ROAD_LANE_COUNTS, road_preset  # R-family road surface
from . import terrain                                 # terrain/ subpackage self-registers its materials
__all__ = ["CrackedMud", "CobbleStone", "LilyPads", "BeachBall", "BasketBall",
           "SoccerBall", "HoneyComb", "CarbonFiber", "SpaceshipHull", "Brick",
           "Marble", "Foil", "Wood", "Teak", "Oak", "Pine", "Plywood",
           "Cheetah", "Leopard", "Zebra", "Reptile", "Dirt", "Grass", "Gravel", "Sand", "Rock", "Snow", "Mud",
           "Solid", "Asphalt", "ROAD_PRESETS", "ROAD_LANE_COUNTS", "road_preset", "terrain"]
