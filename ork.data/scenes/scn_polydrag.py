###############################################################################
# scn_polydrag.py — the `polydrag` particle hyperasset as a HYPERECS scene.
#
# Quantized CurlNoise (Levels=3) + heavy nonlinear PolyDrag: particles get launched
# at the ψ-level boundaries, then a quadratic/cubic drag brakes the fast ones hard —
# crisp discrete violet/magenta arcs. Streak-rendered, additive (glows on the skybox).
#
# Plain Scene → window via the viewer OR VR via `./scene.py --vr scn_polydrag`.
#
#   ork.scene.viewer.py scn_polydrag
#   ./scene.py --vr scn_polydrag
###############################################################################

from orkengine.core import vec3, lev2_pyexdir

from ork.hypergraph.ecs.scene import Scene, Transform

lev2_pyexdir.addToSysPath()


class PolyDragScene(Scene):

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

    polydrag = self.asset.ParticleSystem(
        "polydrag_ptc",
        dsl_file = "polydrag")

    self.entity(
        "particles",
        transform  = Transform(translation=vec3(0.0, 2.0, 0.0)),
        components = [self.declare_component(
            "ParticlesComponent",
            drawabledata = polydrag,
            pool_size    = 1,
            duration     = 0.0)])


__all__ = ["PolyDragScene"]
