###############################################################################
# scn_ls_anim.py — the `ls_anim` tree (baked trunk+leaves, gid-partitioned 3 ways)
# rendered in a full scene. ls_anim is a STATIC mesh asset (built once, cacheable);
# wind is NOT on the mesh — it's a DSL vertex displacement on each MATERIAL
# (GpuMeshRenderSource(vtx_displace=...)) reading the standard RCFD_TIME clock fed
# per-frame by fx_pipeline (zero host code). gid 0 bark / gid 1 branch / gid 2 leaf;
# the leaf adds LeafFlutter so the canopy shimmers relative to the branches.
#
#   ork.scene.viewer.py scn_ls_anim
###############################################################################

from orkengine.core import vec3, lev2_pyexdir

from ork.hypergraph.ecs.scene import Scene
from ork.hypergraph.dflow.hypermesh import GpuMeshRenderSource, Wind, LeafFlutter
from ork.hypergraph.assets.materials.terrain.solid import Solid
from ork.hypergraph.assets.materials.bark import Bark
from ork.hypergraph.assets.materials.leaf import LeafProc
from ork.hypergraph.assets.hypermesh.ls_anim import LsAnim
from ork.hypergraph.colors import hsv

lev2_pyexdir.addToSysPath()


class LsAnimScene(Scene):

  def __init__(self):
    super().__init__()

    self.scenegraph(
        preset            = "ForwardPBR",
        skybox_path       = "<ork_envmaps2>/blender_forest.xir",
        SkyboxIntensity   = 1.0,
        DiffuseIntensity  = 1.5,
        SpecularIntensity = 1.0,
        AmbientLight      = vec3(0.0),
        msaa = 2,                       # A2C leaves need an MSAA RtGroup
        ssaa = 1)

    self.system_data("HypermeshSystem")

    _wind = Wind(amp=0.04, freq=1.5, dir=(1, 0, 0))   # shared bulk sway (one RCFD_TIME clock)

    bark = self.asset.Ptex3d(                              # procedural furrowed bark (colors HSV, internal)
        "bark",
        dsl_class     = Bark,
        vertex_source = GpuMeshRenderSource(vtx_displace=_wind))

    branch = self.asset.Ptex3d(
        "branch",
        dsl_class     = Solid,
        vertex_source = GpuMeshRenderSource(vtx_displace=_wind),
        albedo        = hsv(96, 0.46, 0.40),     # younger, greener bark
        metallic      = 0.0,
        roughness     = 0.7)

    leaf = self.asset.Ptex3d(
        "leaf",
        dsl_class     = LeafProc,
        vertex_source = GpuMeshRenderSource(vtx_displace=[_wind, LeafFlutter()]),
        albedo        = hsv(102, 0.66, 0.52),
        roughness     = 0.45)

    tree = self.asset.Hypermesh("ls_anim", dsl_class=LsAnim)

    self.entity(
        "tree0",
        components = [self.declare_component(
            "HypermeshComponent",
            drawabledata = tree.drawable_data(material=bark, materials={1: branch, 2: leaf}),
            layername    = "std_forward",
            nodename     = "tree0")])


__all__ = ["LsAnimScene"]
