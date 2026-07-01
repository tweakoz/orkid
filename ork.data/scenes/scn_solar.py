###############################################################################
# scn_solar.py — the `solar` particle hyperasset as a HYPERECS scene.
#
# CurlNoise transform field streak-rendered through a GradientMaterial (ptc3 cookie),
# additive (glows on the skybox).
#
# Plain Scene → window via the viewer OR VR via `./scene.py --vr scn_solar`.
#
#   ork.scene.viewer.py scn_solar
#   ./scene.py --vr scn_solar
###############################################################################

from orkengine.core import vec3, lev2_pyexdir

from ork.hypergraph.ecs.scene import Scene, Transform

lev2_pyexdir.addToSysPath()


class SolarScene(Scene):

  def __init__(self):
    super().__init__()

    self.scenegraph(
        preset            = "ForwardPBR",
        skybox_path       = "<ork_envmaps2>/tozenv_nebula.xir",
        SkyboxIntensity   = 0.7,
        DiffuseIntensity  = 1.0,
        SpecularIntensity = 1.0,
        AmbientLight      = vec3(0.0),
        msaa              = 2)

    self.system_data("ParticlesGlobalSystem")

    solar = self.asset.ParticleSystem(
        "solar_ptc",
        dsl_file = "solar")

    self.entity(
        "particles",
        transform  = Transform(translation=vec3(0.0, 2.0, 0.0)),
        components = [self.declare_component(
            "ParticlesComponent",
            drawabledata = solar,
            pool_size    = 1,
            duration     = 0.0)])


__all__ = ["SolarScene"]
