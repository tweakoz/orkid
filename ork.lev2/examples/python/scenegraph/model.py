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

SSAO_NUM_SAMPLES = 96

################################################################################

sys.path.append((thisdir()/"..").normalized.as_string) # add parent dir to path
from lev2utils.cameras import *
from lev2utils.shaders import *
from lev2utils.primitives import createGridData
from lev2utils.scenegraph import createSceneGraph

################################################################################

parser = argparse.ArgumentParser(description='scenegraph example')
parser.add_argument('--numinstances', metavar="numinstances", help='number of mesh instances' )
parser.add_argument('--vrmode', action="store_true", help='run in vr' )
parser.add_argument('--seed', type=int, default=57, help='run in vr' )

################################################################################

args = vars(parser.parse_args())
vrmode = (args["vrmode"]==True)
if args["numinstances"]==None:
  numinstances = 300
else:
  numinstances = int(args["numinstances"])

seed = args["seed"]
random.seed(seed)

################################################################################

class modelinst(object):

  def __init__(self,model,layer, index):

    super().__init__()

    self.model = model
    self.sgnode = model.createNode("node%d"%index,layer)
    self.pos = vec3(random.uniform(-25.0,25),
                    random.uniform(1,3),
                    random.uniform(-25.0,25))
    self.rot = quat(vec3(0,1,0),0)
    incraxis = vec3(random.uniform(-1,1),
                    random.uniform(-1,1),
                    random.uniform(-1,1)).normalized
    incrmagn = random.uniform(-0.01,0.01)
    self.rotincr = quat(incraxis,incrmagn)
    self.scale = random.uniform(0.5,0.7)
    self.sgnode.worldTransform.translation = self.pos 
    self.sgnode.worldTransform.scale = self.scale

  def update(self,deltatime):
    self.rot = self.rot*self.rotincr
    self.sgnode.worldTransform.orientation = self.rot 

################################################################################

class SceneGraphApp(object):

  def __init__(self):
    super().__init__()
    self.ezapp = OrkEzApp.create(self)
    self.ezapp.setRefreshPolicy(RefreshFastest, 0)
    self.materials = set()
    setupUiCamera(app=self,eye=vec3(0,0.5,3))
    self.modelinsts=[]
    self.ssaamode = True

  ##############################################

  def onGpuInit(self,ctx):

    params_dict = {
      "SkyboxIntensity": 1.5,
      "SpecularIntensity": float(1),
      "DiffuseIntensity": float(1),
      "AmbientLight": vec3(0),
      "SSAONumSamples": 96,
      "SSAONumSteps": 2,
      "SSAOBias": -1e-5,
      "SSAORadius": 3.0*2.54/100,
      "SSAOWeight": 1.0,
      "SSAOPower": 0.3,
    }
    
    createSceneGraph(app=self,params_dict=params_dict,rendermodel="PBRVR" if vrmode else "DeferredPBR")
    self.pbr_common = self.scene.pbr_common

    models = []
    models += [XgmModel("data://tests/pbr1/pbr1")]
    models += [XgmModel("data://tests/pbr_calib.glb")]
    models += [XgmModel("src://environ/objects/misc/ref/torus.glb")]

    ###################################

    for i in range(numinstances):
      model = models[i%len(models)]
      self.modelinsts += [modelinst(model,self.layer1,i)]

    ###################################

    self.grid_data = createGridData()
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

    if self.ssaamode == True:
      self.pbr_common.ssaoNumSamples = SSAO_NUM_SAMPLES
    else:
      self.pbr_common.ssaoNumSamples = 0

    for minst in self.modelinsts:
      minst.update(updinfo.deltatime)

    self.scene.updateScene(self.cameralut) 

###############################################################################

SceneGraphApp().ezapp.mainThreadLoop()
