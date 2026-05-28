###############################################################################
# hello.py — HYPERECS M0 acid test.
#
# The smallest possible Tier 3 scene: one entity, one component, visible
# on screen. Hosted by ork.scene.viewer.py.
#
# Usage:
#   ork.scene.viewer.py hello
###############################################################################

from orkengine.core import vec3
from ork.ecs.scene import Scene, Transform


class HelloScene(Scene):

  def __init__(self):
    super().__init__()
    SG = self.scenegraph(preset="ForwardPBR",
                         skybox_path = "<ork_envmaps2>/pillars4k.xir"
    )
    self.entity("cube",
      transform=Transform(translation=vec3(0, 0, 0)),
      components=[SG.component(nodes={
        "c": {"drawable": SG.drawables.model("data://tests/pbr_calib.glb")},
      })])
