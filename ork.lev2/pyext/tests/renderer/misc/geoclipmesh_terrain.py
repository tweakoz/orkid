#!/usr/bin/env python3

################################################################################
# lev2 sample which renders a scenegraph to a window
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import math, sys, os, random, numpy, argparse
from obt import path
from pathlib import Path
from orkengine.core import *
from orkengine.lev2 import *
lev2_pyexdir.addToSysPath()
from common.cameras import *
from common.scenegraph import createSceneGraph
from signal import signal, SIGINT

tokens = CrcStringProxy()

sys.path.append(str(thisdir()/".."/"particles"))
from _ptc_harness import *

################################################################################
parser = argparse.ArgumentParser(description='scenegraph particles example')

args = vars(parser.parse_args())

################################################################################

class WaterApp(object):

  def __init__(self):
    super().__init__()
    self.ezapp = OrkEzApp.create(self,ssaa=4)
    self.ezapp.setRefreshPolicy(RefreshFastest, 0)
    self.curtime = 0.0

    setupUiCamera( app=self, #
                   near = 0.1, #
                   far = 10000, #
                   eye = vec3(0,1000,0), #
                   tgt = vec3(0,1000,1), #
                   constrainZ=True, #
                   up=vec3(0,1,0))
    
    self.view_vel = vec2(0,0)
    self.zdir = vec3(0,0,1)
    self.key=None
    self.move_dir = 0.0

  ################################################
  # gpu data init:
  #  called on main thread when graphics context is
  #   made available
  ##############################################

  def onGpuInit(self,ctx):

    ###################################
    # create scenegraph
    ###################################
    sceneparams = VarMap() 
    sceneparams.preset = "ForwardPBR"
    sceneparams.SkyboxIntensity = float(0.0)
    sceneparams.SpecularIntensity = float(1.0)
    sceneparams.DiffuseIntensity = float(1.0)
    sceneparams.AmbientLight = vec3(0.1)
    sceneparams.DepthFogDistance = float(10000)
    sceneparams.DepthFogPower = float(2)
    sceneparams.SkyboxTexPathStr = "src://envmaps/tozenv_nebula.png"
    ###################################
    # post fx node
    ###################################

    postNode = DecompBlurPostFxNode()
    postNode.threshold = 0.99
    postNode.blurwidth = 16.0
    postNode.blurfactor = 0.1
    postNode.amount = 0.4
    postNode.gpuInit(ctx,8,8);
    postNode.addToVarMap(sceneparams,"PostFxNode")

    ###################################
    # create scene
    ###################################

    self.scene = self.ezapp.createScene(sceneparams)
    self.layer_donly = self.scene.createLayer("depth_prepass")
    self.layer_fwd = self.scene.createLayer("std_forward")
    self.fwd_layers = [self.layer_fwd,self.layer_donly]

    #######################################
    # ground material (water)
    #######################################

    gmtl = PBRMaterial() 
    gmtl.texColor = Texture.load("src://effect_textures/white.dds")
    gmtl.texNormal = Texture.load("src://effect_textures/default_normal.dds")
    gmtl.texMtlRuf = Texture.load("src://effect_textures/white.dds")
    gmtl.metallicFactor = 1
    gmtl.roughnessFactor = 1
    gmtl.doubleSided = True
    gmtl.shaderpath = str(thisdir()/"geoclipmesh_terrain.glfx")
    #gmtl.addLightingLambda()
    gmtl.gpuInit(ctx)
    gmtl.blending = tokens.ALPHA
    freestyle = gmtl.freestyle
    assert(freestyle)
    param_m= freestyle.param("m")
    gmtl.bindParam(param_m,tokens.RCFD_M )

    #######################################
    # ground drawable
    #######################################

    gdata = GeoClipMapDrawable()
    gdata.pbrmaterial = gmtl
    gdata.numLevels = 8
    gdata.ringSize = 512
    gdata.baseQuadSize = 1
    self.gdata = gdata
    self.drawable_ground = gdata.createSGDrawable(self.scene)
    self.groundnode = self.scene.createDrawableNodeOnLayers(self.fwd_layers,"partgroundicle-node",self.drawable_ground)
    self.groundnode.worldTransform.translation = vec3(0,0,0)
    self.groundnode.worldTransform.scale = 1
    #self.groundnode.viewRelative = True

  ################################################

  def onUpdate(self,updinfo):
    
    self.scene.updateScene(self.cameralut) # update and enqueue all scenenodes
    self.curtime = updinfo.absolutetime
    DT = updinfo.deltatime
    self.zdir = self.uicam.zDir
    self.zdir.y = 0
    self.zdir.normalize()
    UP = vec3(0,1,0)
    xdir = self.zdir.cross(UP)
       
    
    wasd_dir = vec3(self.view_vel.x,0,self.view_vel.y)
    # rotate wasd_dir by self.move_dir (a scalar representing rotation on y)
    #wasd_dir.roty(self.move_dir)
    view_vel = self.zdir*wasd_dir.z 
    view_vel += xdir*wasd_dir.x
    
    view_vel *= 20.0
    
    self.uicam.positionOffset += vec3(view_vel.x,0,view_vel.z)*DT*100.0
    self.uicam.updateMatrices()
    self.camera.copyFrom( self.uicam.cameradata )

  ##############################################

  def onUiEvent(self,uievent):
    handled = self.uicam.uiEventHandler(uievent)
    if not handled:
      code = uievent.code
      if code == 2634741946: # key down
        keycode = uievent.keycode
        #print("keydown keycode<%d>"%(keycode))
        
        if keycode == ord('W'):
          self.key = keycode
          self.view_vel = vec2(0,1)
        elif keycode == ord('A'):
          self.key = keycode
          #
          #self.view_vel = vec2(-1,0)
          self.move_dir += 1.0
        elif keycode == ord('S'):
          self.key = keycode
          self.view_vel = vec2(0,-1)
        elif keycode == ord('D'):
          self.key = keycode
          #self.view_vel = vec2(1,0)
          self.move_dir -= 1.0
        handled = True

      elif code == 957111669: # key up
        keycode = uievent.keycode
        if keycode == self.key:
          self.key = None
          self.view_vel = vec2(0,0)
        if keycode == ord('W'):
          pass
        elif keycode == ord('A'):
          pass
        elif keycode == ord('S'):
          pass
        elif keycode == ord('D'):
          pass
        handled = True
        #print("keyup keycode<%d>"%(keycode))
      
    return ui.HandlerResult()

###############################################################################

def sig_handler(signal_received, frame):
  print('SIGINT or CTRL-C detected. Exiting gracefully')
  sys.exit(0)

###############################################################################

signal(SIGINT, sig_handler)

WaterApp().ezapp.mainThreadLoop()
