#!/usr/bin/env python3

################################################################################
# lev2 sample which renders a scenegraph, optionally in VR mode
# Copyright 1996-2020, Michael T. Mayers.
# Distributed under the Boost Software License - Version 1.0 - August 17, 2003
# see http://www.boost.org/LICENSE_1_0.txt
################################################################################

import math, random, argparse, sys
from orkengine.core import *
from orkengine.lev2 import *

################################################################################

sys.path.append((thisdir()/"..").normalized.as_string) # add parent dir to path
from common.cameras import *
from common.shaders import *
from common.primitives import createGridData
from common.scenegraph import createSceneGraph

################################################################################

parser = argparse.ArgumentParser(description='scenegraph example')
parser.add_argument('--numinstances', metavar="numinstances", help='number of mesh instances' )
parser.add_argument('--vrmode', action="store_true", help='run in vr' )

################################################################################

args = vars(parser.parse_args())
vrmode = (args["vrmode"]==True)
if args["numinstances"]==None:
  numinstances = 100
else:
  numinstances = int(args["numinstances"])

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
                    random.uniform(-1,1)).normalized()
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

  ##############################################

  def onGpuInit(self,ctx):

    createSceneGraph(app=self,rendermodel="PBRVR" if vrmode else "DeferredPBR")

    models = []
    models += [Model("data://tests/pbr1/pbr1")]
    models += [Model("data://tests/pbr_calib.glb")]
    models += [Model("src://environ/objects/misc/ref/torus.glb")]

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
    handled = self.uicam.uiEventHandler(uievent)
    if handled:
      self.camera.copyFrom( self.uicam.cameradata )

  ################################################

  def onUpdate(self,updinfo):

    for minst in self.modelinsts:
      minst.update(updinfo.deltatime)

    self.scene.updateScene(self.cameralut) 

###############################################################################

SceneGraphApp().ezapp.mainThreadLoop()
