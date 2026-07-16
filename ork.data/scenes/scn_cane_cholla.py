###############################################################################
# scn_cane_cholla.py — GR1.c vertical slice: the §17.2 CaneCholla grammar authored in the
# reflected-LRuleSet Python DSL (ork.hypergraph.dflow.lsystem), attached to the LSystemModule via
# H.lsystem(grammar=...), derived by the C++ evaluator into an XfNodeGraph, and skinned by LSweep
# into a swept-tube GpuMesh — the exact same MODULE + plug + skinner contract as scn_lsystem, but
# the grammar is now DATA (no hardcoded archetype procedure).
#
#   ork.scene.viewer.py scn_cane_cholla
#   ork.scene.materialize.py scn_cane_cholla --movie /tmp/cholla.mp4   # offscreen render
###############################################################################

from orkengine.core import vec3, lev2_pyexdir

from ork.hypergraph.ecs.scene import Scene
from ork.hypergraph.dflow.hypermesh import Hypermesh as HypermeshDSL, GpuMeshRenderSource
from ork.hypergraph.dflow.lsystem.examples import CaneCholla
from ork.hypergraph.assets.materials.terrain.solid import Solid

lev2_pyexdir.addToSysPath()


class CaneChollaMesh(HypermeshDSL):
  """the cane cholla: H.lsystem(grammar=CaneCholla) -> LSystem (derive the reflected grammar to an
  XfNodeGraph) -> LSweep (GpuMesh). The joints are small (rad ~2cm) so the whole plant is scaled up
  for framing (the grammar's relative proportions are unchanged)."""

  def __init__(self):
    super().__init__()
    n = self.lsystem(
        grammar     = CaneCholla,
        seg_len     = 0.16,
        base_radius = 0.020,
        sides       = 7,        # §17.2 H.ncircle(7)
        jitter      = 0.12,
        jit_azimuth = 0.6,
        jit_pitch   = 0.35,
        tropism     = 0.02)
    n = self.transform(
        n,
        scale = (12, 12, 12))   # ~1.5m cactus -> ~18m so the auto-framed camera reads the silhouette
    self.output(n)


class CaneChollaScene(Scene):

  def __init__(self):
    super().__init__()

    self.scenegraph(
        preset            = "ForwardPBR",
        skybox_path       = "<ork_envmaps2>/blender_forest.xir",
        SkyboxIntensity   = 1.0,
        DiffuseIntensity  = 1.5,
        SpecularIntensity = 1.0,
        AmbientLight      = vec3(0.0),
        msaa = 2,
        ssaa = 1)

    self.system_data("HypermeshSystem")

    cactus_mtl = self.asset.Ptex3d(
        "cactus",
        dsl_class     = Solid,
        vertex_source = GpuMeshRenderSource(),
        albedo        = vec3(0.28, 0.42, 0.20),
        metallic      = 0.0,
        roughness     = 0.80)

    cholla = self.asset.Hypermesh(
        "cholla",
        dsl_class = CaneChollaMesh)

    self.entity(
        "cholla0",
        components = [self.declare_component(
            "HypermeshComponent",
            drawabledata = cholla.drawable_data(material=cactus_mtl),
            layername    = "std_forward",
            nodename     = "cholla0")])


__all__ = ["CaneChollaScene"]
