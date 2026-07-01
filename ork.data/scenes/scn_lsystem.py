###############################################################################
# scn_lsystem.py — first L-system tree (M1): an LSystemModule produces the
# XfNodeGraph branch skeleton, an LSweepModule skins it to a swept-tube GpuMesh,
# rendered through the stock generated PBR material (GpuMeshRenderSource).
# Hardcoded bracketed parametric grammar for now (reflected LRuleSet is next).
#
#   ork.scene.viewer.py scn_lsystem
#   ork.scene.tojson.py -i scn_lsystem -o /tmp/lsys.ecs && ork.ecs.player.exe /tmp/lsys.ecs
###############################################################################

from orkengine.core import vec3, lev2_pyexdir

from ork.hypergraph.ecs.scene import Scene
from ork.hypergraph.dflow.hypermesh import Hypermesh as HypermeshDSL, GpuMeshRenderSource, Wind
from ork.hypergraph.assets.materials.terrain.solid import Solid

lev2_pyexdir.addToSysPath()


class TreeMesh(HypermeshDSL):
  """A swept-tube tree: lsystem() = LSystem (XfNodeGraph) -> LSweep (GpuMesh)."""

  def __init__(self):
    super().__init__()
    n = self.lsystem(
        depth        = 8,
        seg_len      = 4.0,
        base_radius  = 0.96,
        sides        = 6,
        children     = 2,
        internodes   = 3,    # curved branch segments (so the wind bends them smoothly)
        branch_angle = 38,
        tropism      = 0.05,
        jitter       = 0.35,
        apical       = 0.3,
        budget       = 16000)   # NO cpu wind: the mesh is static + cacheable; wind is VS-side (below)
    self.output(n)


class TreeScene(Scene):

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

    bark = self.asset.Ptex3d(
        "bark",
        dsl_class     = Solid,
        # VS wind: a DSL-authored vertex displacement that GENERATES the sway into the pull VS;
        # the mesh stays static, the displace reads the standard Time uniform (RCFD_TIME provider,
        # fed per-frame by the engine — no host code), so it's stable + cheap.
        vertex_source = GpuMeshRenderSource(
          vtx_displace=Wind(
            amp=0.007, 
            freq=1.7, 
            dir=(1, 0, 0))),
        albedo        = vec3(0.35, 0.22, 0.12),
        metallic      = 0.0,
        roughness     = 0.85)

    tree = self.asset.Hypermesh(
        "tree",
        dsl_class = TreeMesh)

    self.entity(
        "tree0",
        components = [self.declare_component(
            "HypermeshComponent",
            drawabledata = tree.drawable_data(material=bark),
            layername    = "std_forward",
            nodename     = "tree0")])


__all__ = ["TreeScene"]
