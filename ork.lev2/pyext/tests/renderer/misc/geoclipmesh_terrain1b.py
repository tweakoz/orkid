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

tokens = CrcStringProxy()

sys.path.append(str(thisdir()/".."/"particles"))
from _ptc_harness import *

################################################################################
parser = argparse.ArgumentParser(description='scenegraph particles example')

args = vars(parser.parse_args())

################################################################################

SCALEXZ = float(0.002)
HEIGHT = float(500) #1000.0
BIAS_Y = float(-120) #-1600.0
CAMHEIGHT = 0
SPEED = 1

class TERRAINAPP(object):

  def __init__(self):
    super().__init__()
    self.ezapp = OrkEzApp.create(self,ssaa=0)
    self.ezapp.setRefreshPolicy(RefreshFastest, 0)
    self.curtime = 0.0

    self.height = CAMHEIGHT
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

  def _sampleNoiseMap(self,uv):
    if hasattr(self,"cpu_ntex_imgdata"):
      imgdata = self.cpu_ntex_imgdata
      width = self.cpu_ntex_width
      height = self.cpu_ntex_height
      x = uv.x
      y = uv.y
      x = x % width
      y = y % height
      # invert y
      #y = (height-1) - y
      x = int(x)
      y = int(y)
      
      idx = (y*width+x)*3
      #print(uv,x,y,width,height,idx)
      r = imgdata.readByte(idx+0)
      return r/255.0
    else:
      return 0.0
      
  def _texNoise(self,pos2d):
    f = pos2d.fract
    u = f * f * (vec2(3,3) - vec2(2,2) * f)
    du = f * (vec2(1,1) - f).mul(6.0) 
    p = pos2d.floor
    a = self._sampleNoiseMap(p)
    b = self._sampleNoiseMap(p+vec2(1,0))
    c = self._sampleNoiseMap(p+vec2(0,1))
    d = self._sampleNoiseMap(p+vec2(1,1))
    
    x = a + (b - a) * u.x + (c - a) * u.y + (a - b - c + d) * u.x * u.y
    yz = du * (vec2(b - a, c - a) + u.yx.mul(a - b - c + d) )      
    return vec3(x,yz.x,yz.y)
  
  def _terrain(self,pos2d,num_octaves):
    p = pos2d
    p.x *= SCALEXZ
    p.y *= SCALEXZ
    a = 0.0
    b = 1.0
    d = vec2(0,0)
    m2 = vec4(0.8,-0.6,0.6,0.8)
    for i in range(num_octaves):
      n = self._texNoise(p)
      d += n.yz
      a += b * n.x / (1.0 + d.dot(d))
      b *= 0.5
      # treat m2 as a 2x2 matrix (but stored as a vec4)
      # and compute p = m2 * p * 2.0
      p = vec2(m2.x*p.x + m2.y*p.y, m2.z*p.x + m2.w*p.y).mul(2)
    return HEIGHT * a

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

    #postNode = PostFxNodeDecompBlur()
    #postNode.threshold = 0.99
    #postNode.blurwidth = 16.0
    #postNode.blurfactor = 0.1
    #postNode.amount = 0.4
    #postNode.gpuInit(ctx,8,8);
    #postNode.addToSceneVars(sceneparams,"PostFxChain")

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

    noisemap_path = "lev2://textures/noisekern_hot256c.png"
    def on_event(loadreq,evcode,data):
      if evcode == tokens.onMipLoad.hashed:
        data_str = data.dumpToString()
        print("onMipLoad data: %s"%(data_str))
        if data.level == 0:
          self.cpu_ntex = data
          self.cpu_ntex_imgdata = data.data
          self.cpu_ntex_width = data.width
          self.cpu_ntex_height = data.height
          self.cpu_ntex_format = data.format_string
          assert(data.format_string == "RGB8")
          numb = data.width*data.height*3
          assert(data.data.size == numb)
          print(self.cpu_ntex_imgdata)
          byte0 = self._sampleNoiseMap(vec2(0.5,0.5))
          print("byte0<%d>"%(byte0))
          #self.cpu_ntex_npy = numpy.frombuffer(data.data,dtype=numpy.uint8)
          #assert(False)
      else:
        data_str = str(data)
        print("unhandled loadreq event<%x> data<%s>"%(evcode,data_str))
    
    NOISEMAP = Texture.load(noisemap_path)
      
    self.hmlrq = asset.enqueueLoad(
      path = noisemap_path,
      vars = { 
        "hello":"world"
      },
      onEvent = on_event
    )
    
    #######################################

    gmtl = PBRMaterial() 
    gmtl.texColor = Texture.load("src://effect_textures/white.dds")
    gmtl.texNormal = Texture.load("src://effect_textures/default_normal.dds")
    gmtl.texMtlRuf = Texture.load("src://effect_textures/white.dds")
    gmtl.metallicFactor = 1
    gmtl.roughnessFactor = 1
    gmtl.doubleSided = True
    gmtl.shaderpath = str(thisdir()/"geoclipmesh_terrain1b.glfx")
    #gmtl.addLightingLambda()
    gmtl.gpuInit(ctx)
    gmtl.blending = tokens.ALPHA
    freestyle = gmtl.freestyle
    assert(freestyle)
    param_m = freestyle.param("m")
    param_SCXZ = freestyle.param("SCXZ")
    param_SCY = freestyle.param("SCY")
    param_BIASY = freestyle.param("BIAS_Y")
    param_nzemap = freestyle.param("noise_map")
    param_m2 = freestyle.param("v2")
    gmtl.bindParam(param_m,tokens.RCFD_M )
    gmtl.bindParam(param_SCXZ,SCALEXZ )
    gmtl.bindParam(param_SCY,HEIGHT )
    gmtl.bindParam(param_BIASY,BIAS_Y )
    #gmtl.bindParam(param_nzemap, NOISEMAP)
    gmtl.bindParam(param_m2,vec4(0.8,-0.6,0.6,0.8) )

    #######################################
    # ground drawable
    #######################################

    gdata = GeoClipMapDrawable()
    gdata.pbrmaterial = gmtl
    gdata.numLevels = 6
    gdata.ringSize = 512
    gdata.baseQuadSize = 0.01
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
    
    scalar = 1.0 #clamp(self.uicam.loc.y,0.0001,1)
    view_vel *= SPEED * math.pow(scalar,0.7)
    #print(scalar)
    self.pos_offset  += vec3(view_vel.x,0,view_vel.z)*DT*100.0

    displacement = self._terrain(self.pos_offset.xz(),1)+BIAS_Y
    #print(self.pos_offset,displacement)
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
