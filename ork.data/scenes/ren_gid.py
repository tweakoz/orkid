###############################################################################
# ren_gid.py — E.3 DEMO: gid MULTI-MATERIAL render bucketing. ONE hypermesh
# draws with THREE ptex3d materials in three indirect draws: the triangulator
# partitions the index buffer into contiguous per-gid ranges (gid = the LOCKED
# __tags[20:32) band, written by assign_gid — the A1 tag contract), and the
# drawable issues one DrawIndexedIndirect per bound gid with its own material.
# Unbound gids fold to the default material. Everything authored ONCE here;
# plays zero-Python in ork.ecs.player.exe.
#
#   ork.scene.tojson.py -i ren_gid -o /tmp/gid.ecs && ork.ecs.player.exe /tmp/gid.ecs
#   ork.scene.viewer.py ren_gid
###############################################################################

from orkengine.core import vec3, vec4, CrcStringProxy, lev2_pyexdir

from ork.hypergraph.ecs.scene import Scene, Transform
from ork.hypergraph.dflow.hypermesh import (Hypermesh as HypermeshDSL, S,
                                            sel_normal_dir, replace, group, POLY,
                                            GpuMeshRenderSource)
from ork.hypergraph.assets.materials.terrain.solid import Solid
from ork.hypergraph.colors import hsv

lev2_pyexdir.addToSysPath()

tokens = CrcStringProxy()


class GidSphere(HypermeshDSL):
  """An icosphere partitioned into three material classes:
       gid 0 (default) — the equatorial band
       gid 1           — the +Y polar cap
       gid 2           — the -Y polar cap"""

  def __init__(self):
    super().__init__()
    n = self.icosphere(radius=8.0, subdivisions=4)
    n = self.select(n, sel_normal_dir(n=vec3(0, 1, 0), t=0.55), op=replace(group(0)))
    n = self.assign_gid(n, gid=1, slot=0)   # top cap
    n = self.select(n, sel_normal_dir(n=vec3(0, -1, 0), t=0.55), op=replace(group(0)))
    n = self.assign_gid(n, gid=2, slot=0)   # bottom cap
    self.output(n)


class GidScene(Scene):

  def __init__(self):
    super().__init__()

    SG = self.scenegraph(
        preset             = "ForwardPBR",
        skybox_path        = "<ork_envmaps2>/blender_courtyard.xir",
        SkyboxIntensity    = 1.0,
        DiffuseIntensity   = 1.0,
        SpecularIntensity  = 1.0,
        AmbientLight       = vec3(0.0))

    self.system_data("HypermeshSystem")

    ##########################
    # three materials — default + one per bound gid (referenced BY NAME;
    # the drawable resolves the whole set from the AssetSystem registry)
    ##########################

    mat_band = self.asset.Ptex3d(
        "mat_band",
        dsl_class     = Solid,
        vertex_source = GpuMeshRenderSource(),
        albedo        = hsv(210, 0.55, 0.65),   # steel blue chrome band
        metallic      = 1.0,
        roughness     = 0.0)

    mat_top = self.asset.Ptex3d(
        "mat_top",
        dsl_class     = Solid,
        vertex_source = GpuMeshRenderSource(),
        albedo        = hsv(30, 0.85, 0.95),    # hot orange cap
        roughness     = 0.25)

    mat_bot = self.asset.Ptex3d(
        "mat_bot",
        dsl_class     = Solid,
        vertex_source = GpuMeshRenderSource(),
        albedo        = hsv(130, 0.65, 0.55),   # green cap
        roughness     = 0.85)

    gid_mesh = self.asset.Hypermesh(
        "gid_mesh",
        dsl_class = GidSphere)

    ##########################
    # ONE entity, ONE mesh, THREE materials (E.3)
    ##########################

    self.entity(
        "gidsphere0",
        components = [self.declare_component(
            "HypermeshComponent",
            drawabledata = gid_mesh.drawable_data(
                material  = mat_band,
                materials = {1: mat_top, 2: mat_bot}),
            layername    = "std_forward",
            nodename     = "gid0")])


__all__ = ["GidScene"]
