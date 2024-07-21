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
from lev2utils.cameras import *
from lev2utils.scenegraph import createSceneGraph
from signal import signal, SIGINT
from PIL import Image as PIL_Image

tokens = CrcStringProxy()

sys.path.append(str(thisdir()/".."/"particles"))
from _ptc_harness import *

################################################################################
parser = argparse.ArgumentParser(description='scenegraph particles example')

args = vars(parser.parse_args())

def calculateWaterFlow(pos):
    return mnoise(pos * 0.005 + vec3(0.0, 1000.0, 0.0))

def simulateSediment(pos):
    return mnoise(pos * 0.02 + vec3(0.0, 500.0, 0.0))

def applyErosion(waterFlow, sediment):
    return mix(0.85, 1.0, clamp(waterFlow * 1.5 - sediment * 0.5, 0.0, 1.0))

def mix(a, b, t):
    return a + (b - a) * clamp(t, 0.0, 1.0)

def clamp(value, min_val, max_val):
    return max(min_val, min(value, max_val))

def addEnhancedDetail(pos, waterFlow, sediment, startOctave, numOctaves, baseAmp, baseFrq):
    detail = 0.0
    detailAmpModifier = mix(0.5, 2.0, sediment)  # Higher sediment, more detail
    detailFrqModifier = mix(2.0, 0.5, waterFlow)  # Higher water flow, less detail frequency

    for i in range(startOctave, numOctaves):
        amp = baseAmp * math.pow(0.5, i) * detailAmpModifier
        frq = baseFrq * math.pow(2.0, i) * detailFrqModifier
        detail += mnoise(pos * frq) * amp

    return detail
      
def computeTerrainDisplacement(pos):
    mountain = 0.0
    for i in range(8):
        amp = 500.0 * math.pow(0.5, i)
        frq = math.pow(2.0, i) * 0.0003
        mountain += mnoise(pos * frq) * amp

    waterFlow = calculateWaterFlow(pos)
    sediment = simulateSediment(pos)
    erosion = applyErosion(waterFlow, sediment)

    # Generate enhanced details based on environmental factors
    detail = addEnhancedDetail(pos, waterFlow, sediment, 5, 12, 0.025, 50.0 * 0.0003)
    
    # Apply erosion effect
    displacement = (mountain * erosion + detail * 100.0)
    return displacement
  
################################################################################

class TERRAINAPP(object):

  def __init__(self):
    super().__init__()
    self.ezapp = OrkEzApp.create(self,ssaa=0)
    self.ezapp.setRefreshPolicy(RefreshFastest, 0)
    self.curtime = 0.0

    self.height = 10000.8 # 1000.0
    setupUiCamera( app=self, #
                   near = 0.1, #
                   far = 10000, #
                   eye = vec3(0,self.height,0), #
                   tgt = vec3(0,self.height,1), #
                   constrainZ=True, #
                   up=vec3(0,1,0))
    
    self.view_vel = vec2(0,0)
    self.zdir = vec3(0,0,1)
    self.key=None
    self.move_dir = 0.0
    self.pos_offset = vec3(0,0,0)

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

    postNode = PostFxNodeDecompBlur()
    postNode.threshold = 0.99
    postNode.blurwidth = 16.0
    postNode.blurfactor = 0.1
    postNode.amount = 0.4
    postNode.gpuInit(ctx,8,8);
    postNode.addToSceneVars(sceneparams,"PostFxChain")

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
    hmap_path = path.orkid()/"ork.data"/"src"/"terrain"/"testhmap2.png"
    def on_complete():
      print("on_complete")
      assert(False)
    def on_event(loadreq,evcode,data):
      if evcode == tokens.beginLoadMainThread.hashed:
        print("beginLoadMainThread data<%s>"%(data))
      elif evcode == tokens.endLoadMainThread.hashed:
        print("endLoadMainThread data<%s>"%(data))
      elif evcode == tokens.loadComplete.hashed:
        print("loadComplete data<%s>"%(data))
      elif evcode == tokens.cacheCheck.hashed:
        data_str = data.dumpToString()
        print("cacheCheck data<%s>"%(data_str))
        #assert(False)
      elif evcode == tokens.onMipLoad.hashed:
        data_str = data.dumpToString()
        # data is a datablock
        print("onMipLoad data: %s"%(data_str))
      else:
        data_str = str(data)
        print("unhandled loadreq event<%x> data<%s>"%(evcode,data_str))
    self.hmlrq = asset.enqueueLoad(
      path = str(hmap_path),
      vars = { 
        "hello":"world"
      },
      onEvent = on_event
    )
    #print(self.hmlrq)
    #self.hmlrq.enqueueAsync(on_complete,on_event)
    # load image in python
    self.cpu_heightmap = PIL_Image.open(str(hmap_path))

    gmtl = PBRMaterial() 
    gmtl.texColor = Texture.load("src://effect_textures/white.dds")
    gmtl.texNormal = Texture.load("src://effect_textures/default_normal.dds")
    gmtl.texMtlRuf = Texture.load("src://effect_textures/white.dds")
    gmtl.metallicFactor = 1
    gmtl.roughnessFactor = 1
    gmtl.doubleSided = True
    gmtl.shaderpath = str(thisdir()/"geoclipmesh_terrain2.glfx")
    #gmtl.addLightingLambda()
    gmtl.gpuInit(ctx)
    gmtl.blending = tokens.ALPHA
    freestyle = gmtl.freestyle
    assert(freestyle)
    param_m= freestyle.param("m")
    param_hmap = freestyle.param("height_map")
    gmtl.bindParam(param_m,tokens.RCFD_M )
    

    #######################################
    # ground drawable
    #######################################

    gdata = GeoClipMapDrawable()
    gdata.pbrmaterial = gmtl
    gdata.numLevels = 8
    gdata.ringSize = 512
    gdata.baseQuadSize = 0.25
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
    
    scalar = clamp(self.uicam.loc.y,0.0001,1)
    view_vel *= 40.3 * math.pow(scalar,0.7)
    #print(scalar)
    self.pos_offset  += vec3(view_vel.x,0,view_vel.z)*DT*100.0

    displacement = 0.0
    image_coord = self.pos_offset * 0.0
    image_coord.x *= self.cpu_heightmap.width
    image_coord.z *= self.cpu_heightmap.height
    image_coord.x = image_coord.x % self.cpu_heightmap.width
    image_coord.z = image_coord.z % self.cpu_heightmap.height
    #image_coord.x = (self.cpu_heightmap.width-1) - image_coord.x
    image_coord.z = (self.cpu_heightmap.height-1) - image_coord.z
    hmap_read = 0 #self.cpu_heightmap.getpixel((int(image_coord.x), int(image_coord.z)))
    displacement = 60 * hmap_read / 65536.0 
    displaced_offset = self.pos_offset + vec3(0,displacement,0)
    
    self.uicam.positionOffset = self.uicam.positionOffset*0.9+displaced_offset*0.1
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

TERRAINAPP().ezapp.mainThreadLoop()
