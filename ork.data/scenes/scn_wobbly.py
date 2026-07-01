###############################################################################
# scn_wobbly.py — the `wobbly` CurlNoise particle hyperasset as a HYPERECS scene.
#
# A STANDALONE particle emitter (ParticlesGlobalSystem + a ParticlesComponent entity)
# rendering the streak-based CurlNoise system from ork.data/particles/wobbly.py. The
# particle graph (pool / emitter / curl-noise / streak renderer + gradient material) is
# the asset's — nothing restated here. emitter_entity binds the emission to this entity's
# published transform, so the system sits where the entity is placed.
#
# Plain Scene → runs in a window via the viewer OR in VR via `./scene.py --vr scn_wobbly`
# (the VR runtime wraps any scene into the stereo present + head pose + free-fly locomotion).
# Additive streaks (depthtest OFF), so it glows against the skybox.
#
#   ork.scene.viewer.py scn_wobbly
#   ./scene.py --vr scn_wobbly
###############################################################################

from orkengine.core import vec3, lev2_pyexdir

from ork.hypergraph.ecs.scene import Scene, Transform

lev2_pyexdir.addToSysPath()


class WobblyScene(Scene):

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

    wobbly = self.asset.ParticleSystem(
        "wobbly_ptc",
        dsl_file       = "wobbly" )
       # emitter_entity = "wobbly_ptc0")

    self.entity(
        "particles",
        transform  = Transform(translation=vec3(0.0, 0.0, 0.0)),
        publish_xf = "wobbly_ptc",
        components = [self.declare_component(
            "ParticlesComponent",
            drawabledata = wobbly,
            pool_size    = 1,
            duration     = 0.0)])


__all__ = ["WobblyScene"]
