###############################################################################
# ren_section_bake.py — O3 stage 3 GATE SCENE: the C++ player-path per-section
# texture-ARRAY bake, driven ENTIRELY by the shipping path (tojson -> player,
# HypermeshSystem, hm_drawable.cpp). Zero per-frame Python.
#
# ONE gid-partitioned hypermesh (SdfBaked: a box -> mesh_to_sdf -> clean remesh ->
# gid partition {0=sides/bottom, 1=top} -> section_unwrap = 2 sections/layers) is
# drawn ONCE with the STORED SAMPLER material (SectionArrayPBR: samples one
# sampler2DArray per capture target at the section's layer). The per-gid BAKE MAP
# (materials={0: AdobeStored, 1: TimberStored}) tells the C++ driver to bake each
# section-layer with THAT gid's material's capture technique — adobe grain into the
# sides layer, timber grain into the top layer. COLD run GPU-bakes + content-address-
# caches the arrays (placeholder -> rebind); WARM run loads the cache. The result is
# a lit render with VISIBLY DISTINCT per-gid baked surfaces from a single draw.
#
#   ork.scene.tojson.py -i ren_section_bake -o /tmp/secbake.ecs
#   ork.ecs.player.exe /tmp/secbake.ecs --offscreen -S /tmp/secbake.png -F 30
#   ork.scene.viewer.py ren_section_bake       # windowed
###############################################################################

import os

from orkengine.core import vec3, CrcStringProxy, lev2_pyexdir

from ork.hypergraph.ecs.scene import Scene
from ork.hypergraph.dflow.hypermesh import GpuMeshRenderSource
from ork.hypergraph.assets.hypermesh.sdf_baked import SdfBaked, GID_REST, GID_TOP
from ork.hypergraph.assets.materials.hypermesh.section_array import SectionArrayPBR
from ork.hypergraph.assets.materials.adobe import AdobeStored
from ork.hypergraph.assets.materials.timber import TimberStored

lev2_pyexdir.addToSysPath()

tokens = CrcStringProxy()

BAKE_RES = 256

# INSTANCED coverage knob (the C++-path instanced gate — mirrors how section_array_bake.py
# reads SECTION_ARRAY_INSTANCED to flip its make_drawable base). When REN_SECTION_BAKE_INSTANCED
# is set, the STORED sampler is authored instanced (GpuMeshRenderSource(instanced=True) -> the
# FWD_SSBO_CUSTOM_INSTANCED forward technique) and the entity draws N distinct instances via
# drawable_data(instance_matrices=...) — the FIRST coverage that routes section_bake=True AND
# instance matrices through HypermeshDrawableData/bindSectionArrays. The bake side stays NON-
# instanced (the adobe/timber capture VS is instanceCount=1, the impostor precedent). Env unset
# -> byte-identical to the shipping non-instanced gate.
INSTANCED    = os.environ.get("REN_SECTION_BAKE_INSTANCED", "0") == "1"
# NEGATIVE-PROOF knob: collapse every instance onto the origin (all translations 0). The mesh
# still draws N times but every copy overlaps -> ONE screen cluster, so the N-cluster oracle in
# the player wrapper MUST report FAIL. Proves the cluster oracle discriminates before its PASS
# is trusted. Only meaningful with INSTANCED.
INST_COLLAPSE = os.environ.get("REN_SECTION_BAKE_INST_COLLAPSE", "0") == "1"
N_INSTANCES   = 3       # distinct row of cubes proving multiplicity through the C++ driver
INST_SPACING  = 3.0     # world-X gap between instances (SdfBaked box edge = 1.2 -> clean separation)


def _instance_matrices():
  """N*16 COLUMN-MAJOR mat4 floats, translation in the 4th column (flat indices 12,13,14).
  Row-major here would drop tx into the projective bottom-row slot [3], which setupMeshRender
  extracts to per-instance attrs + ZEROES -> all instances collapse to identity (the G3 miss)."""
  out = []
  for i in range(N_INSTANCES):
    tx = 0.0 if INST_COLLAPSE else (i - (N_INSTANCES - 1) * 0.5) * INST_SPACING
    out += [1.0, 0.0, 0.0, 0.0,     # col 0
            0.0, 1.0, 0.0, 0.0,     # col 1
            0.0, 0.0, 1.0, 0.0,     # col 2
            tx,  0.0, 0.0, 1.0]     # col 3 = translation (tx,0,0)
  return out


class SectionBakeScene(Scene):

  def __init__(self):
    super().__init__()

    # INSTANCED gate: disable the skybox DRAW (IBL lighting is unaffected — _diffuseLevel /
    # _specularLevel stay 1.0) so the offscreen frame is 3 lit cubes on a black background and
    # the player-wrapper cluster oracle can ISOLATE the instances (the skybox otherwise fills the
    # frame -> one giant lit blob, indistinguishable from a collapsed draw). Mirrors the proven
    # section_array_bake.py gate (pbr_common.enable_skybox = False). Non-instanced -> untouched,
    # unless REN_SECTION_BAKE_NOSKY is set (a diagnostic lever to measure the drawn cube surface
    # in the non-instanced path with the same black-background isolation).
    nosky  = INSTANCED or os.environ.get("REN_SECTION_BAKE_NOSKY", "0") == "1"
    sky_kw = {"enable_skybox": False} if nosky else {}

    self.scenegraph(
        preset            = "ForwardPBR",
        skybox_path       = "<ork_envmaps2>/blender_courtyard.xir",
        SkyboxIntensity   = 1.0,
        DiffuseIntensity  = 1.0,
        SpecularIntensity = 1.0,
        AmbientLight      = vec3(0.0),
        **sky_kw)

    self.system_data("HypermeshSystem")

    ##########################
    # STORED SAMPLER (material=) — drawn once, samples the baked arrays at ctx.layer.
    # Per-gid BAKE MAP (materials={gid: capture material}) — the second role of the
    # E.3 materials={} map: each section-layer is baked with THAT gid's material.
    ##########################

    sampler = self.asset.Ptex3d(
        "sec_sampler",
        dsl_class     = SectionArrayPBR,
        vertex_source = GpuMeshRenderSource(instanced=INSTANCED))

    adobe = self.asset.Ptex3d(
        "sec_adobe",
        dsl_class     = AdobeStored,
        vertex_source = GpuMeshRenderSource())

    timber = self.asset.Ptex3d(
        "sec_timber",
        dsl_class     = TimberStored,
        vertex_source = GpuMeshRenderSource())

    mesh = self.asset.Hypermesh(
        "sec_mesh",
        dsl_class = SdfBaked)

    ##########################
    # ONE entity, ONE mesh, ONE stored draw — two baked sections.
    ##########################

    # instance_matrices flows through drawable_data(**viz) -> HypermeshDrawableData; empty (default)
    # keeps the shipping non-instanced draw untouched.
    inst_kw = {"instance_matrices": _instance_matrices()} if INSTANCED else {}

    self.entity(
        "secbake0",
        components = [self.declare_component(
            "HypermeshComponent",
            drawabledata = mesh.drawable_data(
                material     = sampler,                          # the stored sampler (drawn)
                materials    = {GID_REST: adobe, GID_TOP: timber},  # the per-gid BAKE MAP
                section_bake = True,
                bake_res     = BAKE_RES,
                **inst_kw),
            layername = "std_forward",
            nodename  = "secbake0")])


__all__ = ["SectionBakeScene"]
