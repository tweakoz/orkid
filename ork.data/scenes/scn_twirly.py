###############################################################################
# scn_twirly.py — the `elliptical_exposed` particle hyperasset as a HYPERECS scene.
#
# A STANDALONE particle emitter (ParticlesGlobalSystem + a ParticlesComponent entity)
# rendering the elliptical-attractor + vortex + turbulence streak system from
# ork.data/particles/elliptical_exposed.py — the twirly vortex look. The DSL exposes
# runtime-mutable Turb / Rate params (defaults reproduce elliptical.py); nothing about
# the graph is restated here. The DSL's emitter defines its own origin, so no
# emitter_entity binding is needed (that's only for emission that FOLLOWS a moving entity).
#
# Plain Scene → runs in a window via the viewer OR in VR via `./scene.py --vr scn_twirly`
# (the VR runtime wraps any scene into the stereo present + head pose + free-fly locomotion).
# Additive streaks (depthtest OFF), so it glows against the skybox.
#
#   ork.scene.viewer.py scn_twirly
#   ./scene.py --vr scn_twirly
###############################################################################

from orkengine.core import vec3, quat, lev2_pyexdir

from ork.hypergraph.ecs.scene import Scene, Transform

lev2_pyexdir.addToSysPath()


class TwirlyScene(Scene):

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

    twirly = self.asset.ParticleSystem(
        "twirly_ptc",
        dsl_file = "elliptical_exposed")

    self.entity(
        "particles",
        transform  = Transform(translation=vec3(0.0, 2.0, 0.0),
                               orientation=quat(vec3(1,0,0),3.14*0.5)),
        #publish_xf = "twirly_ptc",
        components = [self.declare_component(
            "ParticlesComponent",
            drawabledata = twirly,
            pool_size    = 1,
            duration     = 0.0)])


__all__ = ["TwirlyScene"]
