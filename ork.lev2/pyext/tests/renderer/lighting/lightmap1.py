#!/usr/bin/env python3

################################################################################
# lev2 sample which renders a scenegraph, optionally in VR mode
# Copyright 1996-2020, Michael T. Mayers.
# Distributed under the Boost Software License - Version 1.0 - August 17, 2003
# see http://www.boost.org/LICENSE_1_0.txt
################################################################################

import math, random, argparse, sys, os
from obt import path
from orkengine.core import *
from orkengine.lev2 import *

################################################################################
lev2pyex_dir = lev2pyexdir()
sys.path.append(str(lev2pyex_dir)) # add parent dir to path
################################################################################

thisdir = path.directoryOfInvokingModule()

modelpath = "data://tests/environ/roomtest_lightmaps.glb"
lightintens = float(1)
specuintens = float(1)
diffuintens = float(1)
ambiuintens = float(0)
camdist = 1.0
envmap = "white"
oshader = None
ssaa = 0
ssao = 0
ocolor = None
#ssaa = args["ssaa"]
#ssao = args["ssao"]
lightmap = "a"
rendermodel = "forward"
showgrid = False

################################################################################

# make sure env vars are set before importing the engine...

def trace_imports(frame, event, arg):
    if event == "import":
        module_name = arg
        print(f"Importing module: {module_name}")
    return trace_imports

sys.settrace(trace_imports) 
from lev2utils.cameras import *
from lev2utils.shaders import *
from lev2utils.primitives import createGridData
from lev2utils.scenegraph import createSceneGraph

################################################################################

#assert(False)

class SceneGraphApp(object):

  def __init__(self):
    super().__init__()
    self.ezapp = OrkEzApp.create(self,ssaa=ssaa)
    self.ezapp.setRefreshPolicy(RefreshFastest, 0)
    self.materials = set()
    setupUiCamera(app=self,eye=vec3(0,0.5,3))
    self.modelinsts=[]
    self.ssaamode = False
    if ssao>0:
      self.ssaamode = True
  ##############################################

  def onGpuInit(self,ctx):

    params_dict = {
      "SkyboxIntensity": float(lightintens),
      "AmbientLight": vec3(ambiuintens),
      "DiffuseIntensity": diffuintens,
      "SpecularIntensity": specuintens,
      "depthFogDistance": float(10000),
      "SSAONumSamples": ssao,
      "SSAONumSteps": 2,
      "SSAOBias": -1.0e-5,
      "SSAORadius": 2.0*25.4/1000.0,
      "SSAOWeight": 0.75,
      "SSAOPower": 0.75,
    }

    if envmap != "":
      params_dict["SkyboxTexPathStr"] = envmap

    #rendermodel = "DeferredPBR"
    global rendermodel
    if rendermodel == "deferred":
      rendermodel = "DeferredPBR"
    elif rendermodel == "forward":
      rendermodel="ForwardPBR"


    createSceneGraph( app=self,
                      params_dict=params_dict,
                      rendermodel=rendermodel )

    self.model = XgmModel(modelpath)
    self.sgnode = self.model.createNode("node",self.layer1)
    self.pbr_common = self.scene.pbr_common
    
    ######################
    # override shader ?
    ######################

    self.modelinst = self.sgnode.user.pyext_retain_modelinst

    ######################

    center = self.model.boundingCenter
    radius = self.model.boundingRadius*2.5

    if camdist!=0.0:
      radius = camdist

    self.uicam.lookAt( center-vec3(0,0,radius), 
                       center, 
                       vec3(0,1,0) )

    #self.uicam.base_zmoveamt = radius*0.01 

    self.camera.copyFrom( self.uicam.cameradata )

    ###################################

    if showgrid:
      self.grid_data = createGridData()
      if rendermodel == "ForwardPBR":
        self.grid_data.shader_suffix = "_V3"
      self.grid_node = self.layer1.createGridNode("grid",self.grid_data)
      self.grid_node.sortkey = 1

  ##############################################

  def onUiEvent(self,uievent):
    res = ui.HandlerResult()
    if uievent.code == tokens.KEY_DOWN.hashed:
      if uievent.keycode == ord("A"):
        if self.ssaamode == True:
          self.ssaamode = False
        else:
          self.ssaamode = True
        print("SSAO MODE",self.ssaamode)
        return res
    handled = self.uicam.uiEventHandler(uievent)
    if handled:
      self.camera.copyFrom( self.uicam.cameradata )
    else:
      handled = ui.HandlerResult()
    return res

  ################################################

  def onUpdate(self,updinfo):

    if self.ssaamode:
      self.pbr_common.ssaoNumSamples = ssao 
    else:
      self.pbr_common.ssaoNumSamples = 0 
    self.scene.updateScene(self.cameralut) 

    self.abstime = updinfo.absolutetime
    
  ################################################

  def onGpuUpdate(self,ctx):
    
    for m in self.model.meshes:
      for s in m.submeshes:
        mtl = s.material
        phia = 0.5 + 0.5*math.sin(self.abstime*10)
        phib = 0.5 - 0.5*math.cos(self.abstime*7)
        mtl.setActiveLightMapA("a",vec3(phia,0,0))
        mtl.setActiveLightMapB("b",vec3(0,phib,0))
        print(phia,phib)

###############################################################################

print("XXXX")
SceneGraphApp().ezapp.mainThreadLoop()
