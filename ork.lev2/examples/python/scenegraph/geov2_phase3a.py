#!/usr/bin/env ork.python
################################################################################
# GEOV2 Phase 3a — cracked mud, authored in the DSL, on the UvSphere ASSET.
#
# Two things this demo establishes:
#   1. A second procedural-surface CLASS (cellular/cracked) on top of the same
#      voronoi primitive — earthy plates, recessed cracks (.fwedge), per-plate
#      tone/roughness variation, dirt grain. The bring-up TARGET for the GEOV2
#      displacement phase (§18): right now the cracks are albedo/roughness only
#      (flat-shaded); Phase 4 (parallax occlusion) makes them recede + occlude.
#   2. The proper MESH ASSET path — UvSphere(material=...) bakes a .ogeo
#      chunkfile + returns a DrawableData (vs the inline make_uvsphere() the
#      earlier phase demos used to feed a raw RigidPrimitive). The material is
#      the ptex3d asset wrapper, so the whole thing is round-trip-capable.
#
#   ./geov2_phase3a.py [--cell_scale N] [--still]
################################################################################

import math, sys, argparse
from orkengine.core import *
from orkengine.lev2 import *
sys.path.append((lev2exdir() / "python").normalized.as_string)
from lev2utils.cameras import setupUiCamera
from ork.hypergraph.ptex3d import Ptex3d, P, rgb
from ork.hypergraph.assets.mesh.uvsphere import UvSphere
import ork.hypergraph.ecs.scene.assets as hgassets

_ap = argparse.ArgumentParser(description="GEOV2 Phase 3a cracked-mud render test")
_ap.add_argument("--cell_scale", type=float, default=5.0)
_ap.add_argument("--still", action="store_true", help="freeze the live crack-width param")
_args = _ap.parse_args()

################################################################################
# Cracked mud — cellular plates with recessed cracks. All bindable params are
# uniforms (base tone / crack width / plate irregularity), so the look is
# tweakable + animatable at runtime without regenerating the shader.
################################################################################

class CrackedMud(Ptex3d):
  def __init__(self, ctx, *, cell_scale=5.0):
    base  = ctx.param("base_color", vec3(0.50, 0.33, 0.19))   # mud tone (bindable)
    crack = ctx.param("crack", 0.05)                          # crack width, cell units
    warp  = ctx.param("warp", 0.30)                           # plate irregularity

    # domain-warp the cell coordinate so plates are organic, not lattice-regular
    pc    = ctx.P_object * cell_scale
    wv    = P.vec3(P.fbm(pc * 0.6), P.fbm(pc * 0.6 + 17.0), P.fbm(pc * 0.6 + 41.0))
    cell  = P.voronoi(pc + warp * wv)                          # .edge .fwedge .cell .cell2
    plate = P.smoothstep(0.0, crack, cell.fwedge)             # 0 in crack, 1 on plate

    # per-plate earthy tone + fine dirt grain
    tint  = P.mix(rgb(0.70, 0.55, 0.34), rgb(1.05, 0.95, 0.74), cell.cell)
    mud   = base * tint
    grain = P.fbm(ctx.P_object * cell_scale * 5.0, 4)
    mud   = mud * P.mix(0.82, 1.15, grain)
    # plates dip / darken toward the crack lips (fakes the recess for now)
    mud   = mud * P.mix(0.50, 1.0, P.smoothstep(0.0, crack * 3.0, cell.fwedge))

    crackcol = rgb(0.05, 0.035, 0.022)                        # deep crack bottom
    self.surface(
      albedo    = P.mix(crackcol, mud, plate),
      metallic  = 0.0,                                        # dielectric mud
      roughness = P.mix(0.95, 0.86, cell.cell2 * plate),     # matte; crack rougher
    )

################################################################################

class Phase3aApp:
  def __init__(self):
    self.ezapp = OrkEzApp.create(self, left=100, top=100, width=1280, height=720, ssaa=0)
    self.ezapp.setRefreshPolicy(RefreshFastest, 0)
    setupUiCamera(app=self, eye=vec3(0, 0, 8), constrainZ=True, up=vec3(0, 1, 0), fov_deg=60)
    self.time = 0.0

  def _crack_width(self):
    # mud drying — cracks slowly widen/narrow. LIVE bindable-uniform proof.
    if _args.still:
      return 0.05
    return 0.045 + 0.03 * (0.5 + 0.5 * math.sin(self.time * 0.4))

  def onGpuInit(self, ctx):
    sg_params = VarMap()
    sg_params.SkyboxIntensity  = 1.0
    sg_params.DiffuseIntensity = 1.0
    sg_params.SpecularIntensity = 1.0
    sg_params.AmbientLevel     = vec3(0.06)
    sg_params.SkyboxTexPathStr = "<ork_envmaps2>/blender_forest.xir"
    sg_params.preset           = "ForwardPBR"
    self.scenegraph = self.ezapp.createScene(sg_params)
    self.layer = self.scenegraph.createLayer("std_forward")

    # author the cracked-mud surface (DSL) -> ptex3d asset wrapper (the material)
    ptex = hgassets.Ptex3d(dsl_class=CrackedMud, cell_scale=_args.cell_scale)
    print("shaderpath      :", ptex.gendata.shaderpath)
    print("bindable params :", list(ptex.gendata.shader_params.keys()))

    # build the sphere via the UvSphere ASSET (bakes <staging>/geocache/*.ogeo,
    # references the material) -> a DrawableData. NOT an inline mesh.
    # RETAIN the wrapper + DrawableData on self: the DrawableData owns the
    # RigidPrimitive (_primitive), and the node's render callback captures the
    # prim by raw pointer — so if the DrawableData is GC'd, the prim is freed and
    # renderEML null-derefs its cluster (intermittent under free-threaded GC).
    # (The hypergraph Scene keeps these in scene._assets; standalone we keep them.)
    self.ptex   = ptex
    self.sphere = UvSphere(radius=2.5, segments_u=96, segments_v=72, material=ptex)
    self.sphere.gendata.asset_name = "mudball"
    self.drawable = self.sphere.build()
    print("baked geometry  :", self.sphere.gendata.geometry_path)
    self.node = self.layer.createDrawableNodeFromData("mudball", self.drawable)

    # the same built material — drive a param live (mud drying)
    self.material = ptex.as_gfx_material
    self.material.bindParam("crack", self._crack_width)

    self.scenegraph.lightingmanager.gpuInit(ctx)

  def onUpdate(self, updinfo):
    self.time += updinfo.deltatime
    self.scenegraph.updateScene(self.cameralut)

  def onUiEvent(self, uievent):
    res = ui.HandlerResult()
    if self.uicam.uiEventHandler(uievent):
      self.camera.copyFrom(self.uicam.cameradata)
    return res

################################################################################

Phase3aApp().ezapp.mainThreadLoop()
