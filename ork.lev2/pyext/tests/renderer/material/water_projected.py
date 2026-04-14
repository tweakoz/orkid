#!/usr/bin/env ork.python

################################################################################
# projected-grid water material test
#   - ComponentizedApplication + StandardSceneGraphComponent
#   - ForwardPBR scene, nebula skybox
#   - one moving spotlight with color + depth cookies (PCF shadows)
#   - ProjectedGridDrawableData: static V12T8 grid in param space, vs bilerps
#     the 4 world-space frustum/plane corners each frame (supplied by the
#     RCFD_GROUND_FRUSTUM_CORNERS named-param provider) and displaces y via
#     water_height
#   - DamagedHelmet floating above waterline
#
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
################################################################################

import math, sys, signal
from orkengine.core import vec3, vec4, VarMap, CrcStringProxy, thisdir
from orkengine import lev2

from ork.app.application import ComponentizedApplication
from ork.app.std_scenegraph import StandardSceneGraphComponent, StdSpotLight

tokens = CrcStringProxy()

################################################################################

class WaterProjectedApp(ComponentizedApplication):

  def __init__(self):
    super().__init__()

    sg_params = {
      "SkyboxIntensity":   1.3,
      "SpecularIntensity": 1.0,
      "DiffuseIntensity":  1.0,
      "AmbientLevel":      vec3(0.1),
      "DepthFogDistance":  10000.0,
      "DepthFogPower":     2.0,
      "SkyboxTexPathStr":  "ork_envmaps|tozenv_nebula",
    }

    self.SGC = self.addComponent("std_scenegraph",
                                 StandardSceneGraphComponent,
                                 grid_variant=None,
                                 eye=vec3(0, 30, 80),
                                 tgt=vec3(0, 0, 0),
                                 sg_params=sg_params)

    self.createEzApp(ssaa=0,
                     name="WaterProjected",
                     use_subsystems=['opq', 'core', 'gpu', 'lev2'])

    self.curtime = 0.0
    self.lighttime = 0.0

  ##############################################

  def _onGpuInit(self, ctx):
    SGC = self.SGC
    SG  = SGC.scenegraph

    ###################################
    # spotlight cookies
    ###################################

    color_cookies = lev2.TextureArray(w=1024, h=1024, slices=1, fmt=tokens.RGB8, mipmapped=True)
    depth_cookies = lev2.TextureArray(w=1024, h=1024, slices=1, fmt=tokens.Z32F, mipmapped=True)
    color_cookies.needsRadianceCache = False
    cookie0 = color_cookies.load("src://effect_textures/knob2.png")
    ctx.TXI.updateTextureArray(color_cookies)
    depth0 = depth_cookies.slice(0)

    lite_model = lev2.XgmModel("data://tests/pbr_calib.glb")

    self.spotlight = StdSpotLight(
      SGC=SGC,
      index=0,
      model=lite_model,
      frq=0.17,
      color=vec3(1.0, 1.0, 0.7) * 3000.0,
      cookie=cookie0,
      depth_cookie=depth0,
      fovbase=60.0,
      fovamp=20.0,
      voffset=40,
      vscale=10,
      bias=1e-5,
      dim=2048,
      range=200,
      radius=30,
    )

    lmgr = SG.lightingmanager
    lmgr.spot_cookies_color = color_cookies
    lmgr.spot_cookies_depth = depth_cookies

    ###################################
    # water material
    ###################################

    white  = lev2.Image.createFromFile("src://effect_textures/white_64.dds")
    normal = lev2.Image.createFromFile("src://effect_textures/default_normal.dds")

    gmtl = lev2.PBRMaterial()
    gmtl.assignImages(
      ctx,
      color  = white,
      normal = normal,
      mtlruf = white,
      doConform = True,
    )
    gmtl.metallicFactor  = 1.0
    gmtl.roughnessFactor = 1.0
    gmtl.doubleSided     = True
    gmtl.shaderpath      = str(thisdir() / "water_projected.fxv2")
    gmtl.addLightingLambda()
    gmtl.gpuInit(ctx)
    gmtl.rasterstate.setBlendingMacro(tokens.ALPHA)

    freestyle = gmtl.freestyle
    assert freestyle

    param_time      = freestyle.param("Time")
    param_color     = freestyle.param("BaseColor")
    param_plightamp = freestyle.param("plightamp")
    param_corners   = freestyle.param("GroundCorners")
    assert param_time
    assert param_corners

    gmtl.bindParam(param_time,      lambda: self.curtime)
    gmtl.bindParam(param_color,     lambda: vec3(0.18, 0.30, 0.42))
    gmtl.bindParam(param_plightamp, 0.15)
    gmtl.bindParam(param_corners,   tokens.RCFD_GROUND_FRUSTUM_CORNERS)

    self.water_material = gmtl

    ###################################
    # water drawable — static projected grid
    # NOTE: added to [SGC.layer_fwd] only — not on depth_prepass.
    ###################################

    gdata = lev2.ProjectedGridDrawableData()
    gdata.pbrmaterial = gmtl
    gdata.griddim     = 128
    self.gdata = gdata

    self.drawable_water = gdata.createSGDrawable(SG)
    self.waternode = SG.createDrawableNodeOnLayers(
      [SGC.layer_fwd],
      "water-node",
      self.drawable_water,
    )
    self.waternode.worldTransform.translation = vec3(0, 0, 0)

    ###################################
    # floating helmet (both fwd + depth_prepass)
    ###################################

    self.model = lev2.XgmModel("data://tests/misc_gltf_samples/DamagedHelmet.glb")
    self.drawable_helmet = self.model.createDrawable()
    self.helmetnode = SG.createDrawableNodeOnLayers(
      SGC.fwd_layers,
      "helmet-node",
      self.drawable_helmet,
    )
    self.helmetnode.worldTransform.scale       = 12.0
    self.helmetnode.worldTransform.translation = vec3(0, 18, 0)

    ###################################

    SG.lightingmanager.gpuInit(ctx)

  ##############################################

  def _onUpdate(self, updinfo):
    self.curtime   = updinfo.absolutetime
    self.lighttime = updinfo.absolutetime

    mdl_y = 18.0 + 3.0 * math.sin(self.curtime * 1.3)
    self.helmetnode.worldTransform.translation = vec3(0, mdl_y, 0)

  ##############################################

  def _onGpuUpdate(self, ctx):
    self.spotlight.update(self.lighttime)

################################################################################

def sig_handler(signal_received, frame):
  print("SIGINT or CTRL-C detected. Exiting gracefully")
  sys.exit(0)

signal.signal(signal.SIGINT, sig_handler)

app = WaterProjectedApp()
app.ezapp.mainThreadLoop()
app.ezapp.shutdown()
