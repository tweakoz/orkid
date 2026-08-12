###############################################################################
# ren_section_bake_scatter.py — GATE SCENE for the INSTANCING x BAKE BRIDGE:
# per-section texture-ARRAY bake (section_bake=True) composed with SCATTER-SSBO
# instancing (instance_source=(asset, sink, type_id)) — the shape the forest
# trees use. ren_section_bake.py proves the same bake against EXPLICIT instance
# matrices; nothing before this combined a bake with a scatter-pulled set.
#
# ONE gid-partitioned hypermesh (SdfBaked: box -> mesh_to_sdf -> clean remesh ->
# gid partition {0=sides/bottom, 1=top} -> section_unwrap = 2 sections/layers) is
# drawn ONCE with the STORED SAMPLER material (SectionArrayPBR: one sampler2DArray
# per capture target, sampled at the section's layer), instanced across every point
# of the terrain's "props" scatter sink. The per-gid BAKE MAP (materials={0:
# AdobeStored, 1: TimberStored}) bakes each section-layer with THAT gid's capture
# technique. The BAKE ITSELF STAYS NON-INSTANCED (one prototype, instanceCount=1) —
# the shape Track 0 ratified, and the reason ctx.P_object stays object-space.
#
# The scene is authored so a broken bridge is VISIBLE: the ground is cool blue-gray
# and the skybox DRAW is off, so every warm (R>B) pixel in the frame is a prop sampling
# its baked section array. Arrays that never bind read black (measured: the props go
# black-and-glossy), which collapses the warm population to a specular fringe.
#
#   ork.scene.tojson.py -i ren_section_bake_scatter -o /tmp/secbake_scatter.ecs
#   ork.ecs.player.exe /tmp/secbake_scatter.ecs --offscreen -S /tmp/ss.png -F 30
#   ork.scene.viewer.py ren_section_bake_scatter      # windowed
###############################################################################

import os

from orkengine.core import vec3, lev2_pyexdir

from ork.hypergraph.ecs.scene import Scene
from ork.hypergraph.dflow.hypermesh import GpuMeshRenderSource
from ork.hypergraph.assets.hypermesh.sdf_baked import SdfBaked, GID_REST, GID_TOP
from ork.hypergraph.assets.materials.hypermesh.section_array import SectionArrayPBR
from ork.hypergraph.assets.materials.adobe import AdobeStored
from ork.hypergraph.assets.materials.timber import TimberStored
from ork.hypergraph.assets.terrain.secbake_scatter import GRID_N, SINK, SOLO as SOLO_SINK

lev2_pyexdir.addToSysPath()

TERRAIN  = "secbake_terra"
BAKE_RES = 256
TYPE_ID  = 0                  # the sink's single declared type ("prop")
N_PROPS  = GRID_N * GRID_N    # the sink's deterministic placed count (what the gate expects to see)

# MUTANT KNOBS (the gate's negative proofs — each disarms ONE half of the bridge):
#   NOBAKE: drop section_bake + the bake map. The stored sampler still draws, instanced, but
#           nothing ever binds its arrays -> the props lose all baked content -> the CONTENT
#           oracle must FAIL. This is what "baked textures not bound under instancing" looks like.
#   SOLO:   pull the SOLO sink (the same placement recipe capped at ONE point) instead of the
#           full grid — same instanced technique, same bake, one instance — so the MULTIPLICITY
#           oracle must FAIL while the content oracle still passes.
NOBAKE = os.environ.get("SECBAKE_SCATTER_NOBAKE", "0") == "1"
SOLO   = os.environ.get("SECBAKE_SCATTER_SOLO", "0") == "1"


class SectionBakeScatterScene(Scene):

  def __init__(self):
    super().__init__()

    # skybox DRAW off (IBL lighting is unaffected — _diffuseLevel/_specularLevel stay 1.0):
    # the sky is the one other warm-capable surface in frame, and the warmth mask is the
    # gate's whole discriminator.
    self.scenegraph(
        preset            = "ForwardPBR",
        skybox_path       = "<ork_envmaps2>/blender_courtyard.xir",
        enable_skybox     = False,
        SkyboxIntensity   = 1.0,
        DiffuseIntensity  = 1.0,
        SpecularIntensity = 1.0,
        AmbientLight      = vec3(0.0))

    self.system_data("HypermeshSystem")

    # TERRAIN FIRST — it bakes the "props" placement the drawable resolves
    # (declaration order = dependency order).
    self.terrain(TERRAIN,
                 dsl_file         = "secbake_scatter",
                 chunk            = 128,
                 render_dimension = 256,
                 bake_dimension   = 256)

    ##########################
    # STORED SAMPLER (material=) — drawn once per scattered instance, sampling the baked
    # arrays at ctx.layer. INSTANCED vertex source -> FWD_SSBO_CUSTOM_INSTANCED.
    # The per-gid BAKE MAP materials stay NON-instanced: the bake draws the prototype
    # once (instanceCount=1), so their captures see object space.
    ##########################

    sampler = self.asset.Ptex3d(
        "secsc_sampler",
        dsl_class     = SectionArrayPBR,
        vertex_source = GpuMeshRenderSource(instanced=True))

    adobe = self.asset.Ptex3d(
        "secsc_adobe",
        dsl_class     = AdobeStored,
        vertex_source = GpuMeshRenderSource())

    timber = self.asset.Ptex3d(
        "secsc_timber",
        dsl_class     = TimberStored,
        vertex_source = GpuMeshRenderSource())

    mesh = self.asset.Hypermesh(
        "secsc_mesh",
        dsl_class = SdfBaked)

    inst_kw = {"instance_source": (TERRAIN, SOLO_SINK if SOLO else SINK, TYPE_ID)}
    bake_kw = {} if NOBAKE else dict(
        materials    = {GID_REST: adobe, GID_TOP: timber},   # the per-gid BAKE MAP
        section_bake = True,
        bake_res     = BAKE_RES)

    self.entity(
        "secsc0",
        components = [self.declare_component(
            "HypermeshComponent",
            drawabledata = mesh.drawable_data(
                material = sampler,
                **bake_kw,
                **inst_kw),
            layername = "std_forward",
            nodename  = "secsc0")])


__all__ = ["SectionBakeScatterScene"]
