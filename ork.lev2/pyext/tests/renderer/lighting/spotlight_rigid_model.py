#!/usr/bin/env ork.python

################################################################################
# lev2 sample which renders a scenegraph
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import math, random, argparse, sys, signal
from orkengine.core import vec3, vec4, quat, mtx4, dfrustum, dvec4, fmtx4_to_dmtx4, CrcStringProxy
from orkengine import lev2
from ork.app.application import ComponentizedApplication
from ork.app.std_scenegraph import StandardSceneGraphComponent, StdSpotLight
from ork.app.loggerui import LoggerUIComponent

################################################################################

tokens = CrcStringProxy()

################################################################################

parser = argparse.ArgumentParser(description='scenegraph example')
parser.add_argument('-S', '--stateDebugger', action="store_true", help='Graphics state debugger')
################################################################################
args = vars(parser.parse_args())
statedebug = args["stateDebugger"]
################################################################################

class SpotlightApp(ComponentizedApplication):

  def __init__(self):
    super().__init__(lui="yes")
    self.SGC = self.addComponent("std_scenegraph", 
                                 StandardSceneGraphComponent, 
                                 grid_variant="_V4",
                                 eye=vec3(0,12,15))
    self.createEzApp(name="RenderTestSpotLightRigidModel", ssaa=0, fullscreen=True, fsmouse=True)

  ##############################################

  def _onGpuInit(self,ctx):

    SGC = self.SGC
    SG = SGC.scenegraph

    ###################################
    # override grid params
    ###################################

    SGC.grid_data.modcolor = vec3(0.3)
    SGC.grid_data.intensityA = 0.1
    SGC.grid_data.intensityB = 0.2
    SGC.grid_data.intensityC = 0
    SGC.grid_data.intensityD = 0
    SGC.grid_data.lineWidth = 0.025

    ###################################

    model = lev2.XgmModel("data://tests/pbr_calib.glb")
    model.debugRenderingModel = tokens.ALL if statedebug else tokens.NONE
    model.debugPassID = tokens.PRIMARY if statedebug else tokens.NONE
    model.debugSubPassID = tokens.ALL if statedebug else tokens.NONE
    self.drawable_model = model.createDrawable()
    self.modelnode = SG.createDrawableNodeOnLayers(SGC.fwd_layers,"model-node",self.drawable_model)
    self.modelnode.worldTransform.scale = 1
    self.modelnode.worldTransform.translation = vec3(0,2,0)

    ###################################
    # setup spotlights / cookies
    ###################################


    color_cookies = lev2.TextureArray(w=1024,h=1024,slices=4,fmt=tokens.RGB8,mipmapped=True)
    depth_cookies = lev2.TextureArray(w=1024,h=1024,slices=4,fmt=tokens.Z32F,mipmapped=True)
    color_cookies.needsRadianceCache = False

    cookie1 = color_cookies.load("src://effect_textures/L0D.png")
    cookie2 = color_cookies.load("lev2://textures/transponder24.png")
    cookie3 = color_cookies.load("src://effect_textures/knob2.png")
    cookie4 = color_cookies.load("src://effect_textures/knob2.png")

    ctx.TXI.updateTextureArray(color_cookies)
    depth1 = depth_cookies.slice(0)
    depth2 = depth_cookies.slice(1)
    depth3 = depth_cookies.slice(2)
    depth4 = depth_cookies.slice(3)
    shadow_size = 2048
    shadow_bias = 1e-5
    intens_scale = 0.5
    speed_scale = 0.5
    self.spotlight1 = StdSpotLight(index=0,SGC=SGC,model=model,frq=0.17*speed_scale,color=vec3(0,5500,0)*intens_scale,cookie=cookie1,depth_cookie=depth1,fovbase=60.0,fovamp=20.0,voffset=15,vscale=13,bias=shadow_bias,dim=shadow_size,radius=12)
    self.spotlight2 = StdSpotLight(index=1,SGC=SGC,model=model,frq=0.37*speed_scale,color=vec3(5000,0,0)*intens_scale,cookie=cookie2,depth_cookie=depth2,fovbase=60.0,fovamp=20.0,voffset=15,vscale=13,bias=shadow_bias,dim=shadow_size,radius=12)
    self.spotlight3 = StdSpotLight(index=2,SGC=SGC,model=model,frq=0.57*speed_scale,color=vec3(800)*intens_scale,cookie=cookie3,depth_cookie=depth3,fovbase=60.0,fovamp=20.0,voffset=15,vscale=13,bias=shadow_bias,dim=shadow_size,radius=12)
    self.spotlight4 = StdSpotLight(index=3,SGC=SGC,model=model,frq=0.97*speed_scale,color=vec3(0,0,600)*intens_scale,cookie=cookie4,depth_cookie=depth4,fovbase=70.0,fovamp=20.0,voffset=3,vscale=2,bias=shadow_bias,dim=shadow_size,radius=7)

    lmgr = SG.lightingmanager
    lmgr.spot_cookies_color = color_cookies
    lmgr.spot_cookies_depth = depth_cookies

    cursor_data = lev2.CursorDrawableData()
    cursor_data.color = vec4(1, 1, 1, 0.9)  # white, 90% alpha
    cursor_data.size = 0.01                  # crosshair arm length in meters
    cursor_data.thickness = 0.005            # bar thickness in meters
    cursor_data.depth = 2.0                  # depth in front of camera
    cursor_data.autopos = True
    self.cursor_data = cursor_data
    
    # Create drawable and add to scenegraph layer
    cursor_drawable = cursor_data.createDrawable()
    self.cnode = SGC.layer_std.createDrawableNode("cursor", cursor_drawable)
    self.cnode.sortkey = 100


  ################################################

  def _onUpdate(self,updinfo):
    self.lighttime = updinfo.absolutetime

  ################################################

  def _onGpuUpdate(self,ctx):
    self.spotlight1.update(self.lighttime)
    self.spotlight2.update(self.lighttime)
    self.spotlight3.update(self.lighttime)
    self.spotlight4.update(self.lighttime)

###############################################################################

SpotlightApp().ezapp.mainThreadLoop()
