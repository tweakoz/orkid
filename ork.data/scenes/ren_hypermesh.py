###############################################################################
# ren_hypermesh.py — HYPERECS D.5 (+hypermesh host stage): an ECS scene hosting an
# ANIMATED GPU mesh graph through HypermeshComponent. The hypermesh DSL runs ONCE
# here at authoring (model B); the scene JSON carries the embedded graph + the
# ptex3d material BY NAME; at load the C++ host materializes everything with zero
# Python (ork.ecs.player.exe) — the same .ecs also plays in ecsplay/ecsedit.
#
#   ork.scene.tojson.py -i ren_hypermesh -o /tmp/hm.ecs && ork.ecs.player.exe /tmp/hm.ecs
#   ork.scene.viewer.py ren_hypermesh
###############################################################################

from orkengine.core import vec3, vec4, CrcStringProxy, lev2_pyexdir

from ork.hypergraph.ecs.scene import Scene, Transform
from ork.hypergraph.dflow.hypermesh import (Hypermesh as HypermeshDSL, S,
                                            isolate, group, POLY,
                                            GpuMeshRenderSource)
from ork.hypergraph.assets.materials.terrain.solid import Solid

lev2_pyexdir.addToSysPath()

tokens = CrcStringProxy()


class AnimMesh(HypermeshDSL):
  """A box whose top faces breathe via an S.time-driven extrude — the
  C++-clocked animated path (no Python onUpdate)."""

  def __init__(self):
    super().__init__()
    n = self.box(size=2.0)
    n = self.select(n, S.N.dot(vec3(0, 1, 0)) > 0.6, domain=POLY, op=isolate(group(2)))
    n = self.extrude_faces(n, distance=0.5 + 0.25 * S.sin(S.time), slot=2)
    self.output(n)


class HypermeshScene(Scene):

  def __init__(self):
    super().__init__()

    ##########################
    # SceneGraph
    ##########################

    SG = self.scenegraph(
        preset             = "ForwardPBR",
        skybox_path        = "<ork_envmaps2>/blender_courtyard.xir",
        SkyboxIntensity    = 2.0,
        DiffuseIntensity   = 1.0,
        SpecularIntensity  = 1.0,
        AmbientLight       = vec3(0.10))

    # the hosting system — components only LINK to systems the scene declares
    self.system_data("HypermeshSystem")

    ##########################
    # Assets — the material BY NAME (the component's drawable resolves it from
    # the AssetSystem registry at load), the mesh graph EMBEDDED (model B).
    ##########################

    hm_mat = self.asset.Ptex3d(
        "hm_mat",
        dsl_class     = Solid,
        vertex_source = GpuMeshRenderSource(),
        albedo        = vec3(0.75, 0.35, 0.15),
        roughness     = 0.4)

    hm_mesh = self.asset.Hypermesh(
        "hm_mesh",
        dsl_class = AnimMesh)

    ##########################
    # The hypermesh entity
    ##########################

    self.entity(
        "hypermesh0",
        components = [self.declare_component(
            "HypermeshComponent",
            drawabledata = hm_mesh.drawable_data(material=hm_mat),
            layername    = "std_forward",
            nodename     = "hm0")])


__all__ = ["HypermeshScene"]
