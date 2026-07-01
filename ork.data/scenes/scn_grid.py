###############################################################################
# scn_grid.py — a minimal HYPERECS reference scene: one GridDrawableData ground
# grid (the standard CAD-style XZ reference grid) on the default ForwardPBR
# scenegraph. GridDrawableData is self-shaded (the "_V4" grid shader), so it
# needs no PBR material — it attaches straight to a SceneGraph node and, being
# reflected, round-trips into the zero-Python player.
#
#   ork.scene.viewer.py scn_grid
#   ork.scene.tojson.py -i scn_grid -o /tmp/grid.ecs && ork.ecs.player.exe /tmp/grid.ecs
###############################################################################

from orkengine.core import vec3
from orkengine import lev2

from ork.hypergraph.ecs.scene import Scene

# Grid geometry / appearance (the proven std_grid values).
EXTENT     = 10.0     # half-size of the grid, in world units
MAJOR_DIM  = 1.0      # spacing of the bright major lines
MINOR_DIM  = 0.1      # spacing of the faint minor lines
LINE_WIDTH = 0.025
# Minor-grid eye-distance fade (world units): the 0.1-spaced minor lines go
# sub-Nyquist quickly, so dissolve them between these distances from the camera.
MINOR_FADE_BEGIN = 4.0
MINOR_FADE_END   = 10.0


def _make_grid():
  grid = lev2.GridDrawableData()
  grid.shader_suffix = "_V4"
  grid.modcolor      = vec3(1.0)
  grid.intensityA    = 1.5
  grid.intensityB    = 0.75
  grid.intensityC    = 0.5
  grid.intensityD    = 0.25
  grid.lineWidth     = LINE_WIDTH
  grid.extent        = EXTENT
  grid.majorTileDim  = MAJOR_DIM
  grid.minorTileDim  = MINOR_DIM
  grid.minor_fade_begin = MINOR_FADE_BEGIN
  grid.minor_fade_end   = MINOR_FADE_END
  return grid


class GridScene(Scene):

  def __init__(self):
    super().__init__()

    self.scenegraph(
        preset            = "ForwardPBR",
        skybox_path       = "<ork_envmaps2>/blender_forest.xir",
        SkyboxIntensity   = 1.0,
        DiffuseIntensity  = 2.0,
        SpecularIntensity = 0.0,
        AmbientLight      = vec3(0.00),
        msaa = 2,
        ssaa = 0)

    ##########################
    # Entity: the ground grid (default ForwardPBR scenegraph via self.SG)
    ##########################

    self.entity(
        "grid",
        transform=Transform(translation=vec3(0,-1.7,0)),
        components = [self.SG.component(nodes = {
            "grid": {"drawable": _make_grid()}})])


__all__ = ["GridScene"]
