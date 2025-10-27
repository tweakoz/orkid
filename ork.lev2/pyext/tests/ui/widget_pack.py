#!/usr/bin/env ork.python

################################################################################
# lev2 sample which renders a scenegraph to a window
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import math, sys, os, signal, random
from obt import path
from orkengine.core import vec2, vec3, vec4, mtx4, quat, VarMap, CrcStringProxy
from orkengine import lev2

tokens = CrcStringProxy()

################################################################################

l2exdir = (lev2.lev2exdir()/"python").normalized.as_string
sys.path.append(l2exdir) # add parent dir to path
from lev2utils.cameras import *
from lev2utils.shaders import *
from lev2utils.primitives import createGridData, createCubePrim
from lev2utils.scenegraph import createSceneGraph

################################################################################

SHADER_TEXT = """
uniform_set ub_ui {
  vec4 modcolor;
  mat4 mvp;
  float time;
}
sampler_set ss_ui (descriptor_set 0) {
  sampler2D ColorMap;
}
vertex_interface iface_vui : ub_ui {
  inputs {
    vec4 position : POSITION;
    vec4 uv0 : TEXCOORD0;
  }
  outputs {
  	vec4 frg_uv0;
  }
}
fragment_interface iface_fui 
  : iface_vui
  : ub_ui
	: ss_ui {
  outputs {
    layout(location = 0) vec4 out_color;
  }
}
vertex_shader vs_x
	: iface_vui {
	gl_Position = mvp*position;
	frg_uv0 = uv0;
}
fragment_shader fs_x
	: iface_fui {
  vec2 uvp = frg_uv0.xy - vec2(0.5,0.5);
  float tscale = 2.5;
  float timex = time*tscale;
  vec2 uvr;
  float angle = atan(uvp.y,uvp.x);
  float oradius = length(uvp);
  float radius = oradius + sin(20.0*oradius)*0.03;
  angle += timex*0.1;
  uvr.x = sin(angle)*radius;
  uvr.y = cos(angle)*(radius*-1.0);   
  uvr += vec2(0.5,0.5);
  float mask = 1.0-smoothstep(0.45,0.5,oradius);

  vec3 c = texture(ColorMap,uvr.xy).xyz*mask;
  out_color = vec4(c,1);
}
state_block sb_x : default {
  CullTest = OFF;
}
technique uix {
	fxconfig=fxcfg_default;
	vf_pass = { vs_x, fs_x, sb_x }
}
"""

################################################################################

class PackWidgets(object):

  def __init__(self):
    super().__init__()

    self.box_height = 0.0

    self.ezapp = lev2.OrkEzApp.create(self,
                                      fullscreen=False,
                                      enable_audio=True,
                                      enable_audio_output=True,
                                      enable_audio_synth=True)

    self.ezapp.setRefreshPolicy(lev2.RefreshFastest, 0)
    self.ezapp.topWidget.enableUiDraw()

    lg_group = self.ezapp.topLayoutGroup
    lg_group.clearColorGuide = vec4(0.8,0.6,0.2,1)
    self.lg_group1 = lg_group
    self.uictx1 = self.ezapp.uicontext

    MARGIN = 2
    LAYOUT_MARGIN = 2
    
    ############################################
    # start out with a 2x2 grid of boxes
    ############################################

    self.griditems = lg_group.makeGrid(
      width=2,
      height=2,
      margin = MARGIN,
      uiclass = lev2.ui.Box,
      args = ["label",vec4(0.1,0.1,0.3,1)],
    )
    
    ############################################

    self.lg_group = lg_group
    lg_group.margin = LAYOUT_MARGIN

    ############################################
    # create a vertical pack widget in the upper-left grid cell
    #  a vertical pack lays out its children vertically,
    #  filling available widdth, and using item_height property of vpack
    #  to determine height of each child
    #  if fill property is True, the last child will fill remaining space
    ############################################

    pk1 = lg_group.makeChild( uiclass=lev2.ui.VerticalPack, args=["vpack1"])
    self.lg_group.replaceChild( self.griditems[0].layout, pk1 )
    self.vpack1 = pk1.widget
    self.vpack1.margin = MARGIN
    self.vpack1.item_height = 28
    self.vpack1.fill = True

    ############################################
    # populate the vertical pack with 3 text boxes
    ############################################

    box1 = self.vpack1.makeChild( uiclass=lev2.ui.LineEdit, args=["box1  ","text",vec3(0.5,0.3,0.3)] )
    box2 = self.vpack1.makeChild( uiclass=lev2.ui.LineEdit, args=["box2  ","text",vec3(0.3,0.5,0.3)] )
    box3 = self.vpack1.makeChild( uiclass=lev2.ui.LineEdit, args=["box3  ","text",vec3(0.3,0.3,0.5)] )

    ############################################
    # populate the vertical pack with a horizontal pack
    #  a horizontal pack lays out its children horizontally,
    #  filling available height, and using item_width property of hpack
    ############################################

    self.hpack1 = self.vpack1.makeChild( uiclass=lev2.ui.HorizontalPack, args=["hpack1"])
    self.hpack1.margin = MARGIN
    self.hpack1.item_width = 192
    self.hpack1.fill = True
    
    box4 = self.hpack1.makeChild( uiclass=lev2.ui.LineEdit, args=["box1  ","text",vec3(0.5,0.5,0.5)] )
    box5 = self.hpack1.makeChild( uiclass=lev2.ui.LineEdit, args=["box2  ","text",vec3(0.5,0.0,0.5)] )

    ############################################
    # populate the vertical pack with another horizontal pack
    #  this one has uniform=True, which makes all children the same width
    ############################################

    self.hpack2 = self.vpack1.makeChild( uiclass=lev2.ui.HorizontalPack, args=["hpack2"])
    self.hpack2.margin = MARGIN
    self.hpack2.uniform = True
    chk_col = vec3(0.25)
    self.chk1 = self.hpack2.makeChild( uiclass=lev2.ui.Checkbox, args=["chk1  ",chk_col] )
    self.chk2 = self.hpack2.makeChild( uiclass=lev2.ui.Checkbox, args=["chk2  ",chk_col] )
    self.chk3 = self.hpack2.makeChild( uiclass=lev2.ui.Checkbox, args=["chk3  ",chk_col] )
    self.chk4 = self.hpack2.makeChild( uiclass=lev2.ui.Checkbox, args=["chk4  ",chk_col] )
    self.chk5 = self.hpack2.makeChild( uiclass=lev2.ui.Checkbox, args=["chk5  ",chk_col] )
    self.chk5.onToggled = lambda x: print("chk5 toggled to ",x.toggled)

    ############################################
    # populate the vertical pack with another horizontal pack
    ############################################

    self.hpack3 = self.vpack1.makeChild( uiclass=lev2.ui.HorizontalPack, args=["hpack3"])
    self.hpack3.margin = MARGIN
    self.hpack3.uniform = True
    btn_col1 = vec3(0.3,0.5,0.3)
    btn_col2 = vec3(0.3,0.3,0.5)
    btn_col3 = vec3(0.3,0.5,0.5)
    self.btn1 = self.hpack3.makeChild( uiclass=lev2.ui.Button, args=["Pause",btn_col1] )
    self.btn2 = self.hpack3.makeChild( uiclass=lev2.ui.Button, args=["Restart",btn_col1] )
    self.btn3 = self.hpack3.makeChild( uiclass=lev2.ui.Button, args=["Pause",btn_col2] )
    self.btn4 = self.hpack3.makeChild( uiclass=lev2.ui.Button, args=["Restart",btn_col2] )
    self.btn5 = self.hpack3.makeChild( uiclass=lev2.ui.Button, args=["Swap",btn_col3] )

    ############################################
    # populate the vertical pack with some sliders
    ############################################

    sli_col = vec3(0.3,0.3,0.5)
    self.sli1 = self.vpack1.makeChild( uiclass=lev2.ui.IntSlider, args=["sli1  ",sli_col,0,100,50] )
    self.sli2 = self.vpack1.makeChild( uiclass=lev2.ui.FloatSlider, args=["sli2  ",sli_col,-3.0,3.0,0.0] )

    def on_V(x): 
      self.box_height = x.value
    self.sli2.onValueChanged = on_V
    self.sli2.update_on_drag = True

    ############################################
    # create a combo box
    # this displays a text item from a list of items
    ############################################

    self.cb1 = self.vpack1.makeChild( uiclass=lev2.ui.ComboBox, args=["cbx1  ",sli_col,0,100,50] )
    self.cb1.setItems(["zero","one","two","three","four","five","six","seven","eight","nine"])
    
    ############################################
    # create a tabbed widget
    # this displays one of several child widgets, with tabs to select the active child
    ############################################

    self.tb1 = self.vpack1.makeChild( uiclass=lev2.ui.TabsWidget, args=["tab1  ",sli_col] )

    ############################################
    # add 2 text boxes to the tabbed widget
    ############################################

    self.tx1 = self.tb1.makeChild( uiclass=lev2.ui.TextBox, args=["tex1",vec4(0.6,0,0,1),"nam"] )
    self.tx2 = self.tb1.makeChild( uiclass=lev2.ui.TextBox, args=["tex2",vec4(0.6,0,0.6,1),"nam"] )
    self.tx1.setText("This is a TextBox.\n It can hold multiple lines of text.\nThe quick brown fox jumps over the lazy dog.\n0123456789")
    self.tx2.setText("This is another TextBox.\n It can hold multiple lines of text.\nThe quick brown fox jumps over the lazy dog.\n0123456789")

    ############################################
    # create 2 imageview widgets
    ############################################

    imgview1 = lg_group.makeChild( uiclass=lev2.ui.ImageView, args=["imgv1",vec4()])
    self.lg_group.replaceChild( self.griditems[1].layout, imgview1 )
    imgview1w = imgview1.widget
    self.imgview1 = imgview1w
    self.imgview1.maintain_aspect_ratio = True

    imgview2 = lg_group.makeChild( uiclass=lev2.ui.ImageView, args=["imgv2",vec4()])
    self.lg_group.replaceChild( self.griditems[3].layout, imgview2 )
    imgview2w = imgview2.widget
    self.imgview2 = imgview2w
    self.imgview2.maintain_aspect_ratio = True

    ############################################
    # create a horizontal split widget in the lower-left grid cell
    #  a split widget lays out its children horizontally or vertically,
    #  with fixed split ratio (0.0-1.0)
    ############################################

    sp2 = lg_group.makeChild( uiclass=lev2.ui.HorizontalSplit, args=["spl1"])
    self.lg_group.replaceChild( self.griditems[2].layout, sp2 )
    sp2w = sp2.widget
    self.x2 = sp2w.makeChild( uiclass=lev2.ui.Box, args=["evb1",vec4(0.3,0.3,0.3,1)] )
    self.y2 = sp2w.makeChild( uiclass=lev2.ui.EvTestBox, args=["evb2",vec4(0.3,0.3,0.4,1)] )

    self.abstime = 0.0
    self.x2.ignoreEvents = True

    ############################################
    
    def onCtrlC(signum, frame):
      print("signalling EXIT to ezapp")
      self.ezapp.signalExit()

    signal.signal(signal.SIGINT, onCtrlC)

  ##############################################

  def onSynthInit(self,synth):
    #self.audiodevice = singularity.device.instance()
    self.synth = synth
    assert(self.synth!=None)
    #self.synth.system_tempo = 120.0

  ##############################################

  def onGpuInit(self,ctx):
        
    ############################################
    # Setup movie playback for bunny.mp4
    #  assign the image provider to 1st imageview widget
    ############################################

    self.movie1 = lev2.MoviePlaybackContext()
    movie1_path = str(path.stage()/"assetcache"/"movies"/"bunny.mp4")
    self.movie1.init(movie1_path)
    provider1 = self.movie1.createImageProvider()
    self.imgview1.image = provider1
    self.movie1.play()
    #self.imgview1.invert_aspect = True
    self.prg1 = self.movie1.createAudioProgram(self.synth)
    self.voice1 = self.synth.keyOn(0, 60, self.prg1, None)
    self.voice1.gain = 0.0
    self.movie1_tgt_gain = 0.0
    ############################################
    # Override shader for 1st imageview widget
    ############################################

    imp_mtl = lev2.FreestyleMaterial()
    imp_mtl.gpuInitFromShaderText(ctx,"uix",SHADER_TEXT)
    imp_mtl.rasterstate.setBlendingMacro(tokens.OFF)
    imp_mtl.rasterstate.culltest = tokens.PASS_FRONT
    imp_mtl.rasterstate.depthtest = tokens.LEQUALS
    imp_permu = lev2.FxPipelinePermutation()
    imp_permu.technique = imp_mtl.shader.technique("uix")
    imp_pipeline = imp_mtl.fxcache.findPipeline(imp_permu)
    imp_pipeline.name = "imppasspipe"
    imp_pipeline.sharedMaterial = imp_mtl
    self.imgview1.pipeline = imp_pipeline
    p_mvp = imp_mtl.param("mvp")
    p_tex = imp_mtl.param("ColorMap")
    p_tim = imp_mtl.param("time")
    imp_pipeline.bindParam(p_mvp, tokens.RCFD_Camera_MVP_Mono )
    imp_pipeline.bindParam(p_tex, self.imgview1.texture )
    imp_pipeline.bindParam(p_tim, lambda: self.abstime )
    ############################################
    # Setup movie playback for wipeout.mp4
    #  assign the image provider to 2nd imageview widget
    ############################################

    self.movie2 = lev2.MoviePlaybackContext()
    movie2_path = str(path.stage()/"assetcache"/"movies"/"wipeout.mp4")
    self.movie2.init(movie2_path)
    provider2 = self.movie2.createImageProvider()
    self.imgview2.image = provider2
    self.movie2.play()
   
    self.prg2 = self.movie2.createAudioProgram(self.synth)
    self.voice2 = self.synth.keyOn(0, 60, self.prg2, None)
    self.voice2.gain = -36.0
    self.movie2_tgt_gain = -36.0

    ########################################################
    # assign buttons to control movie playback
    ########################################################

    self.m1_playing = True
    self.m2_playing = True
    self.m_swap = False
    def on_B1(x):
      if self.m1_playing:
        self.movie1.pause()
        self.m1_playing = False
      else:
        self.movie1.play()
        self.m1_playing = True
    def on_B2(x):
      self.movie1.restart()
    def on_B3(x):
      if self.m2_playing:
        self.movie2.pause()
        self.m2_playing = False
      else:
        self.movie2.play()
        self.m2_playing = True
    def on_B4(x):
      self.movie2.restart()
    def on_B5(x):
      if self.m_swap:
        self.imgview1.setImageProvider(provider1)
        self.imgview2.setImageProvider(provider2)
        self.m_swap = False
        self.movie1_tgt_gain = 0.0
        self.movie2_tgt_gain = -18.0
      else:
        self.imgview1.setImageProvider(provider2)
        self.imgview2.setImageProvider(provider1)
        self.m_swap = True
        self.movie1_tgt_gain = -18.0
        self.movie2_tgt_gain = 0.0

    self.btn1.onPressed = on_B1
    self.btn2.onPressed = on_B2
    self.btn3.onPressed = on_B3
    self.btn4.onPressed = on_B4
    self.btn5.onPressed = on_B5

    ########################################################
    # shared geometry (for scenegraph viewport)
    ########################################################
    
    self.grid_data = createGridData()
    cube_prim = createCubePrim(ctx=ctx,size=2.0)
    pipeline_cube = createPipeline( app = self, ctx = ctx, rendermodel="FORWARD_PBR", techname="std_mono_fwd" )
    mesh = lev2.meshutil.Mesh()
    mesh.readFromWavefrontObj("data://tests/simple_obj/cone.obj")
    submesh = mesh.submesh_list[0]
    submesh_prim = lev2.RigidPrimitive(submesh,ctx)
    pipeline_mesh = createPipeline( app = self, ctx = ctx, rendermodel="FORWARD_PBR", techname="std_mono_fwd" )

    ########################################################
    # scenegraph init data
    ########################################################

    sg_params = VarMap()
    sg_params.SkyboxIntensity = 3.0
    sg_params.DiffuseIntensity = 1.0
    sg_params.SpecularIntensity = 1.0
    sg_params.AmbientLevel = vec3(.125)
    sg_params.preset = "ForwardPBR"
    sg_params.ssaa = 4 # 4x4 SuperSample AntiAliasing

    self.scenegraph = lev2.scenegraph.Scene(sg_params)
    self.layer = self.scenegraph.createLayer("std_forward")
    self.grid_node = self.layer.createDrawableNodeFromData("grid",self.grid_data)
    self.grid_node.sortkey = 1
    self.cube_node = cube_prim.createNode("cube",self.layer,pipeline_cube)

    ########################################################
    # setup a camera for the scenegraph 
    ########################################################

    self.camname = "Camera0"

    self.cameralut = CameraDataLut()
    self.camera, self.uicam = setupUiCameraX( cameralut=self.cameralut, camname=self.camname )
    self.cur_eye = vec3(0,0,0)
    self.cur_tgt = vec3(0,0,1)
    self.dst_eye = vec3(0,0,0)
    self.dst_tgt = vec3(0,0,0)
    self.counter = 0
    self.use_event = False

    ########################################################
    # finally, add a scenegraph viewport to the tabbed widget
    ########################################################

    self.sgv = self.tb1.makeChild( uiclass=lev2.ui.SceneGraphViewport, args=["sg",vec4(0,0,0,1)] )   
    self.sgv.cameraName = self.camname
    self.sgv.scenegraph = self.scenegraph
    self.sgv.forkDB()
    self.scenegraph.lightingmanager.gpuInit(ctx)

  ################################################

  def onUpdate(self,updinfo):

    ##################################
    # update the scenegraph
    ##################################

    abstime = updinfo.absolutetime
    self.sgv.setDirty()
    self.abstime = abstime
    
    self.cube_node.worldTransform.translation = vec3(0,-self.box_height,0)

    def genpos():
      r = vec3(0)
      r.x = random.uniform(-20,20)
      r.z = random.uniform(-20,20)
      r.y = random.uniform(10,20)
      return r 
  
    if self.counter<=0:
      self.counter = int(random.uniform(1,1000))
      self.dst_eye = genpos()
      self.dst_tgt = vec3(0,0,0)

    if not self.use_event:
      self.cur_eye = self.cur_eye*0.9995 + self.dst_eye*0.0005
      self.cur_tgt = self.cur_tgt*0.9995 + self.dst_tgt*0.0005
      self.uicam.distance = 1
      self.uicam.lookAt( self.cur_eye,
                        self.cur_tgt,
                        vec3(0,1,0))

    self.counter = self.counter-1

    self.uicam.updateMatrices()
    self.camera.copyFrom( self.uicam.cameradata )
    self.scenegraph.updateScene(self.cameralut)

    self.voice1.gain = (self.voice1.gain*0.98+self.movie1_tgt_gain*0.02)
    self.voice2.gain = (self.voice2.gain*0.98+self.movie2_tgt_gain*0.02)

  ##############################################

  def onUiEvent(self,uievent):
    #print("onUiEvent: ",uievent)
    return lev2.ui.HandlerResult()

###############################################################################

PackWidgets().ezapp.mainThreadLoop()
