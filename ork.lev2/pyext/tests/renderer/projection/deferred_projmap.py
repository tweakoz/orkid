#!/usr/bin/env python3

################################################################################
# lev2 sample which renders a UI with four views to the same scenegraph to a window
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import sys, math, random, signal, numpy, obt.path, os
from orkengine.core import *
from orkengine.lev2 import *
from PIL import Image

################################################################################

lev2_pyexdir.addToSysPath()
this_dir = obt.path.directoryOfInvokingModule()
from lev2utils.cameras import *
from lev2utils.shaders import *
from lev2utils.primitives import createGridData, createCubePrim
from lev2utils.scenegraph import createSceneGraph

################################################################################

def defaultSceneGraphParams():
  sg_params_def = VarMap()
  sg_params_def.SkyboxIntensity = 3.0
  sg_params_def.DiffuseIntensity = 1.0
  sg_params_def.SpecularIntensity = 1.0
  sg_params_def.AmbientLevel = vec3(.125)
  sg_params_def.DepthFogDistance = 10000.0
  return sg_params_def

################################################################################

save_images = False 
do_offscreen = False 

class Panel:

  def __init__(self,parent,index):
    #
    self.parent = parent
    self.index = index
    self.camname = "Camera%d"%index
    self.use_event = False

    #

    self.cameralut = CameraDataLut()
    self.cur_eye = vec3(0,0,0)
    self.cur_tgt = vec3(0,0,1)
    self.dst_eye = vec3(0,0,0)
    self.dst_tgt = vec3(0,0,0)
    self.counter = 0
                

    griditem = parent.griditems[index]        
    self.cameralut = CameraDataLut()
    self.camera, self.uicam = setupUiCameraX( cameralut=self.cameralut, 
                                              fov_deg = 90,
                                              near = 0.03,
                                              far = 100.0,
                                              eye = vec3(0,5,20),
                                              tgt = vec3(0,5,0),
                                              up = vec3(0,1,0),
                                              camname=self.camname )

    sg_params_def = defaultSceneGraphParams()

    #################################################
    # scenegraph 0 gets standard deferred pbr
    #################################################

    if index==0:
      sg_params_def.preset = "DeferredPBR"
      self.scenegraph = scenegraph.Scene(sg_params_def)

    #################################################
    # scenegraph 1 gets deferred pbr node with 
    #  custom environmentlighting shader 
    #################################################

    else:

      # custom compositor node

      comp_tek = NodeCompositingTechnique()
      comp_tek.renderNode = DeferredPbrRenderNode()
      #comp_tek.outputNode = ScreenOutputNode()
      comp_tek.renderNode.overrideShader(str(this_dir/"deferred_projmap.glfx"))
      self.comp_tek = comp_tek

      comp_data = CompositingData()
      comp_scene = comp_data.createScene("scene1")
      comp_sceneitem = comp_scene.createSceneItem("item1")
      comp_sceneitem.technique = comp_tek
      sg_params_def.preset = "USER"
      sg_params_def.compositordata = comp_data

      # custom pbr params

      pbr_common = comp_tek.renderNode.pbr_common
      pbr_common.requestSkyboxTexture("src://envmaps/tozenv_hellscape")

      # 
      self.scenegraph = scenegraph.Scene(sg_params_def)

    #################################################
    # cache output and render nodes for scenegraph
    #################################################

    self.output_node = self.scenegraph.compositoroutputnode
    self.render_node = self.scenegraph.compositorrendernode

    #################################################
    # scenegraph 1 gets custom shader parameter bindings
    #  to feed custom environmentlighting shader 
    #################################################

    if index==1:
      self.deftex = Texture.load("src://effect_textures/white.dds")
      self.deferred_ctx = self.render_node.context
      self.depthtex_binding = self.deferred_ctx.createAuxBinding("MapShadowDepth")
      self.projtex_binding = self.deferred_ctx.createAuxBinding("ProjectionTexture")
      self.projmtx_binding = self.deferred_ctx.createAuxBinding("ProjectionTextureMatrix")
      self.projcam_eye = self.deferred_ctx.createAuxBinding("ProjectionEyePostion")
      self.nearfar_binding = self.deferred_ctx.createAuxBinding("NearFar")
      self.deferred_ctx.lightAccumFormat = tokens.RGBA32F
      self.projtex_binding.texture = self.deftex
      self.depthtex_binding.texture = self.deftex

    #################################################
    # both scenes will render the same content
    #  but with different cameras and environment lighting
    #################################################

    self.use_event = True
    self.layer = self.scenegraph.createLayer("std_deferred")

    self.sgnode = parent.model.createNode("modelnode",self.layer)

    self.anim_inst = XgmAnimInst(parent.anim)
    self.anim_inst.mask.enableAll()
    self.anim_inst.use_temporal_lerp = True
    self.anim_inst.bindToSkeleton(parent.model.skeleton)

    self.modelinst = self.sgnode.user.pyext_retain_modelinst
    self.modelinst.enableSkinning()
    self.modelinst.enableAllMeshes()
    self.localpose = self.modelinst.localpose
    self.worldpose = self.modelinst.worldpose
    griditem.widget.cameraName = self.camname
    griditem.widget.scenegraph = self.scenegraph

    #################################################
    # gpu udpate per panel
    #################################################

    def gpu_update_handler(context):

      # update skeleton pose from animation

      self.localpose.bindPose()
      self.anim_inst.currentFrame = parent.abstime*30.0
      self.anim_inst.weight = 1.0
      self.anim_inst.applyToPose(self.localpose)
      self.localpose.blendPoses()
      self.localpose.concatenate()
      self.worldpose.fromLocalPose(self.localpose,mtx4())

    # register update handler

    parent.gpu_update_handlers += [gpu_update_handler]

    #################################################
    # fork rendering database per panel
    #################################################

    griditem.widget.forkDB()

    ########################################### 
    # route events to panels ui camera ?
    ########################################### 

    def onPanelEvent(index, event):
      if event.code == tokens.KEY_DOWN.hashed or event.code == tokens.KEY_REPEAT.hashed:
        if event.keycode == 32: # spacebar
          self.use_event = not self.use_event

      if self.use_event:
        self.uicam.uiEventHandler(event)
      
      return ui.HandlerResult()

    griditem.widget.evhandler = lambda ev: onPanelEvent(index,ev)

  ###

  def update(self):
    self.uicam.updateMatrices()
    self.camera.copyFrom( self.uicam.cameradata )
    self.scenegraph.updateScene(self.cameralut)


################################################################################

class UiSgQuadViewTestApp(object):

  def __init__(self):
    super().__init__()

    self.ezapp = OrkEzApp.create(self, offscreen=do_offscreen)
    self.ezapp.setRefreshPolicy(RefreshFixedFPS, 30)

    # enable UI draw mode
    self.ezapp.topWidget.enableUiDraw()

    # make a grid of scenegraph viewports

    lg_group = self.ezapp.topLayoutGroup
    self.griditems = lg_group.makeGrid( width = 2,
                                        height = 1,
                                        margin = 1,
                                        uiclass = ui.SceneGraphViewport,
                                        args = ["box",vec4(1,0,1,1)] )

    self.gpu_update_handlers = []
    self.abstime = 0.0

    def onCtrlC(signum, frame):
      print("signalling EXIT to ezapp")
      self.ezapp.signalExit()

    signal.signal(signal.SIGINT, onCtrlC)

  ##############################################

  def onGpuInit(self,ctx):

    self.dbufcontext = self.ezapp.vars.dbufcontext
    self.cameralut = self.ezapp.vars.cameras
    self.uicontext = self.ezapp.uicontext

    ########################################################
    # shared geometry
    ########################################################
    
    self.model = XgmModel("data://tests/chartest/char_mesh")
    self.anim = XgmAnim("data://tests/chartest/char_testanim1")

    ########################################################
    # create scenegraph / panels
    ########################################################

    self.panels = [
      Panel(self, 0),
      Panel(self, 1),
    ]
    
    ##########################################################################

    #self.grid_data = createGridData()
    #self.grid_node = self.panels[0].layer.createGridNode("grid",self.grid_data)
    #self.grid_node.sortkey = 1

    #self.panels[0].griditem.widget.decoupleFromUiSize(4096,4096)
    #self.panels[0].griditem.widget.aspect_from_rtgroup = True

  ################################################

  def onGpuUpdate(self,context):

    #####################
    # invoke per panel gpu update handlers
    #####################

    for handler in self.gpu_update_handlers:
      handler(context)

    #####################

    panel_0 = self.panels[0]
    panel_1 = self.panels[1]

    #####################
    # compute camera frustum pixel length data
    #####################
    
    camdata0 = panel_0.camera
    w0 = self.griditems[0].widget
    #print(w0.aspect,w0.width,w0.height)
    vmatrix = camdata0.vMatrix()
    pmatrix = camdata0.pMatrix(w0.aspect)
    # generate frustum from view and projection matrices
    camfrustum = frustum(vmatrix,pmatrix)
    # compute pixel length vectors for near plane (using frustum corners)
    near_horiz = camfrustum.nearCorners[1] - camfrustum.nearCorners[0]
    near_verti = camfrustum.nearCorners[2] - camfrustum.nearCorners[1]
    upp_horiz = near_horiz * (float(1)/float(w0.width)) # units per pixel
    upp_verti = near_verti * (float(1)/float(w0.height)) # units per pixel
    #print(near_horiz, near_verti)
    #print(upp_horiz, upp_verti)

    far_horiz = camfrustum.farCorners[1] - camfrustum.farCorners[0]
    far_verti = camfrustum.farCorners[2] - camfrustum.farCorners[1]
    upp_horiz = far_horiz * (float(1)/float(w0.width)) # units per pixel
    upp_verti = far_verti * (float(1)/float(w0.height)) # units per pixel
    #print(upp_horiz, upp_verti)
    
    # get pixel length vectors given a position in world space and a viewport
    pxeyezn = camdata0.pixelLengthVectors( camdata0.eye+(camdata0.znormal*-10), # position at which to measure
                                           vec2(w0.width,w0.height)) # viewport
    
    print(pxeyezn)

    #####################
    # fetch from panel 0
    #  1. deferred context
    #  2. light accumulation buffer
    #####################

    scenegraph_0 = panel_0.scenegraph
    rendernode_0 = panel_0.render_node
    deferred_context = rendernode_0.context
    light_accum_buffer_0 = deferred_context.lbuffer

    #####################
    # Bind Auxiliary shader parameters
    #####################

    if light_accum_buffer_0!=None:
      self.griditems[0].widget.outputnode.supersample = 2
      self.griditems[1].widget.outputnode.supersample = 2
      #
      gbuffer0 = deferred_context.gbuffer
      zbuffer0 = gbuffer0.depth_buffer
      L = light_accum_buffer_0.mrt_buffer(0).texture
      Z = zbuffer0.texture
      panel_1.projtex_binding.texture = L
      panel_1.depthtex_binding.texture = Z
      # we need an aspect ratio to compute the vp matrix
      aspect = float(L.width)/float(L.height)
      vp_matrix = panel_0.camera.vpMatrix(aspect)
      #
      panel_1.projmtx_binding.mtx4 = vp_matrix
      panel_1.nearfar_binding.vec2 = vec2(panel_0.camera.near,panel_0.camera.far)
      panel_1.projcam_eye.vec3 = panel_0.camera.eye

  ################################################

  def onUpdate(self,updinfo):
    self.abstime = updinfo.absolutetime
    for panel in self.panels:
      panel.update()
    # in UI mode we have to mark widgets dirty so the texture suface can repaint.
    for g in self.griditems:
      g.widget.setDirty()

###############################################################################

def onRunLoopIteration():
  # we just need this in-python runloop iteration 
  #  in order to catch ctrl-c from python
  #  so the python signal handler can trigger it's designated callback
  pass

UiSgQuadViewTestApp().ezapp.mainThreadLoop(on_iter=onRunLoopIteration)
