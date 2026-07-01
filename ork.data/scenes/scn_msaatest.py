###############################################################################
# scn_msaatest.py — MSAA validation scene. A hard-silhouette model on a skybox;
# the MSAA level comes from the MSAATEST env var (0=off,1=2x,2=4x,...) so the same
# scene can be rendered with and without multisampling for an A/B edge comparison.
#
#   MSAATEST=0 ork.scene.viewer.py scn_msaatest
#   MSAATEST=2 ork.scene.viewer.py scn_msaatest
###############################################################################
import os

from orkengine.core import vec3, lev2_pyexdir

from ork.hypergraph.ecs.scene import Scene, Transform

lev2_pyexdir.addToSysPath()


class MsaaTestScene(Scene):

  def __init__(self):
    super().__init__()

    lvl = int(os.environ.get("MSAATEST", "0"))

    SG = self.scenegraph(
        preset      = "ForwardPBR",
        skybox_path = "<ork_envmaps2>/blender_courtyard.xir",
        msaa        = lvl)

    self.entity(
        "calib",
        transform=Transform(translation=vec3(0, 0, 0)),
        components=[SG.component(nodes={
            "c": {"drawable": SG.drawables.model("data://tests/pbr_calib.glb")},
        })])


__all__ = ["MsaaTestScene"]
