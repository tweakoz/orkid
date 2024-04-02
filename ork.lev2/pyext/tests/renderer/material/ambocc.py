#!/usr/bin/env python3

################################################################################
# lev2 sample which renders a scenegraph, optionally in VR mode
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import math, random, argparse, sys
from orkengine.core import *
from orkengine.lev2 import *

################################################################################

sys.path.append((thisdir()/".."/".."/".."/"examples"/"python").normalized.as_string) # add parent dir to path
from common.cameras import *
from common.shaders import *
from common.primitives import createGridData
from common.scenegraph import createSceneGraph

################################################################################

parser = argparse.ArgumentParser(description='scenegraph example')
parser.add_argument("-e", "--envmap", type=str, default="", help='environment map')
parser.add_argument("-a", "--ambient", type=float, default=0.0, help='ambient intensity')
parser.add_argument("-s", "--specular", type=float, default=1.0, help='specular intensity')
parser.add_argument("-d", "--diffuse", type=float, default=1.0, help='diffuse intensity')
parser.add_argument("-i", "--skybox", type=float, default=2.0, help='skybox envlight intensity')

################################################################################

args = vars(parser.parse_args())
envmap = args["envmap"]
ambient = args["ambient"]
specular = args["specular"]
diffuse = args["diffuse"]
skybox = args["skybox"]

################################################################################

class NODE(object):

  def __init__(self,model,layer, index):

    super().__init__()
    self.model = model
    self.sgnode = model.createNode("node%d"%index,layer)
    self.modelinst = self.sgnode.user.pyext_retain_modelinst
    self.sgnode.worldTransform.scale = 1

################################################################################

class SceneGraphApp(object):

  def __init__(self):
    super().__init__()
    self.ezapp = OrkEzApp.create(self)
    self.ezapp.setRefreshPolicy(RefreshFastest, 0)
    self.materials = set()
    self.node = None
    self.cameralut = CameraDataLut()
    self.vizcamera, self.uicam = setupUiCameraX( cameralut=self.cameralut, 
                                              near = 0.01,
                                              far = 100.0,
                                              eye = vec3(0,5,20),
                                              tgt = vec3(0,5,0),
                                              up = vec3(0,1,0),
                                              camname="spawncam" )
    self.shadow_camera = CameraData()
    self.cameralut.addCamera("shadow_camera",self.shadow_camera)
    self.seed = 12
    self.ambient = ambient
    self.specular = specular
    self.diffuse = diffuse

  ##############################################

  def onGpuInit(self,ctx):

    sg_params_def = VarMap()
    sg_params_def.SkyboxIntensity = skybox
    sg_params_def.DiffuseIntensity = diffuse
    sg_params_def.SpecularIntensity = specular
    sg_params_def.AmbientLevel = vec3(ambient)
    sg_params_def.preset = "DeferredPBR"
    sg_params_def.DepthFogDistance = 10000.0
    sg_params_def.SkyboxTexPathStr = "src://envmaps/blender_night.dds"
    self.params = sg_params_def 
    
    ###################################    
    
    comp_tek = NodeCompositingTechnique()
    comp_tek.renderNode = DeferredPbrRenderNode()
    comp_tek.outputNode = ScreenOutputNode()
    comp_tek.renderNode.overrideShader(str(thisdir()/"ambocc_render.glfx"))
    self.comp_tek = comp_tek

    comp_data = CompositingData()
    comp_scene = comp_data.createScene("scene1")
    comp_sceneitem = comp_scene.createSceneItem("item1")
    comp_sceneitem.technique = comp_tek

    sg_params_def.preset = "USER"
    sg_params_def.compositordata = comp_data

    self.scene = self.ezapp.createScene(sg_params_def)
    self.layer1 = self.scene.createLayer("layer1")
    self.output_node = self.scene.compositoroutputnode
    self.render_node = self.scene.compositorrendernode
    self.pbr_common = self.render_node.pbr_common
    self.deferred_ctx = self.render_node.context
    self.deferred_ctx.lightAccumFormat = tokens.RGBA32F
    self.depthtex_binding = self.deferred_ctx.createAuxBinding("MapShadowDepth")
    self.projtex_binding = self.deferred_ctx.createAuxBinding("ProjectionTexture")
    self.projmtx_binding = self.deferred_ctx.createAuxBinding("ProjectionTextureMatrix")
    self.projcam_eye = self.deferred_ctx.createAuxBinding("ProjectionEyePostion")
    self.nearfar_binding = self.deferred_ctx.createAuxBinding("NearFar")
    self.pbr_common.requestSkyboxTexture(sg_params_def.SkyboxTexPathStr)

    ###################################

    model = XgmModel("data://tests/monkey_pbr.glb")
    comp_model = meshutil.Mesh()
    comp_model.readFromWavefrontObj("data://tests/monkey_pbr.obj")
    computeAmbientOcclusion(1024, comp_model,ctx)
    
    self.node = model.createNode("modelnode",self.layer1)
    self.modelinst = self.node.user.pyext_retain_modelinst
    
  ################################################

  def onGpuUpdate(self,context):
    light_accum_buffer_0 = self.deferred_ctx.lbuffer
    if light_accum_buffer_0!=None:
      gbuffer0 = self.deferred_ctx.gbuffer
      zbuffer0 = gbuffer0.depth_buffer
      L = light_accum_buffer_0.mrt_buffer(0).texture
      Z = zbuffer0.texture
      self.projtex_binding.texture = L
      self.depthtex_binding.texture = Z
      # we need an aspect ratio to compute the vp matrix
      aspect = float(L.width)/float(L.height)
      v_matrix = self.shadow_camera.vMatrix(aspect)
      vp_matrix = self.shadow_camera.vpMatrix(aspect)
      #
      self.projmtx_binding.mtx4 = vp_matrix
      near = self.shadow_camera.near
      far = self.shadow_camera.far
      
      self.nearfar_binding.vec2 = vec2(near,far)
      self.projcam_eye.vec3 = self.shadow_camera.eye
    pass 

  ################################################

  def onUpdate(self,updinfo):
    phase = updinfo.absolutetime * 0.4
    x =  math.sin(phase)*10    
    z = -math.cos(phase)*10    
    ###################################
    self.uicam.updateMatrices()
    self.vizcamera.copyFrom( self.uicam.cameradata )
    self.shadow_camera.perspective(0.1, 50.0, 35.0*constants.DTOR)
    self.shadow_camera.lookAt(vec3(x,0,z)*2, # eye
                       vec3(0, 0, 0), # tgt
                       vec3(0, 1, 0)) # up
    self.scene.updateScene(self.cameralut) 

  ##############################################

  def onUiEvent(self,uievent):
    handled = self.uicam.uiEventHandler(uievent)    
    if not handled:
      if uievent.code in [tokens.KEY_DOWN.hashed, tokens.KEY_REPEAT.hashed]:
        handled = True
    return ui.HandlerResult()

###############################################################################

SceneGraphApp().ezapp.mainThreadLoop()
