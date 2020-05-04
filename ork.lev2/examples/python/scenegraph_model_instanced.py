#!/usr/bin/env python3
################################################################################
# lev2 sample which renders a scenegraph
# Copyright 1996-2020, Michael T. Mayers.
# Distributed under the Boost Software License - Version 1.0 - August 17, 2003
# see http://www.boost.org/LICENSE_1_0.txt
################################################################################
import math, random, argparse
from orkengine.core import *
from orkengine.lev2 import *
################################################################################
parser = argparse.ArgumentParser(description='scenegraph example')
parser.add_argument('--numinstances', metavar="numinstances", help='number of mesh instances' )
parser.add_argument('--vrmode', action="store_true", help='run in vr' )
################################################################################
args = vars(parser.parse_args())
vrmode = (args["vrmode"]==True)
if args["numinstances"]==None:
  numinstances = 1
else:
  numinstances = int(args["numinstances"])
################################################################################
class instance_set(object):
  def __init__(self,model,num_instances,layer):
    super().__init__()
    self.model = model
    self.sgnode = model.createInstancedNode(num_instances,"node1",layer)
    Z = random.uniform(-2.5,-125)
    self.pos = vec3(random.uniform(-1,1)*Z,
                    random.uniform(-1,1)*Z,
                    Z)
    self.rot = quat(vec3(0,1,0),0)
    incraxis = vec3(random.uniform(-1,1),
                    random.uniform(-1,1),
                    random.uniform(-1,1)).normal()
    incrmagn = random.uniform(-0.01,0.01)
    self.rotincr = quat(incraxis,incrmagn)
    self.scale = random.uniform(0.5,0.7)
  def update(self,deltatime):
    self.rot = self.rot*self.rotincr
    self.sgnode\
        .worldMatrix\
        .compose( self.pos, # pos
                  self.rot, # rot
                  self.scale) # scale
################################################################################
class SceneGraphApp(object):
  ################################################
  def __init__(self):
    super().__init__()
    self.sceneparams = VarMap()
    self.sceneparams.preset = "PBRVR" if vrmode else "PBR"
    self.qtapp = OrkEzQtApp.create(self)
    self.qtapp.setRefreshPolicy(RefreshFastest, 0)
    self.instancesets=[]
  ##############################################
  def onGpuInit(self,ctx):
    layer = self.scene.createLayer("layer1")
    models = []
    models += [Model("data://tests/pbr1/pbr1")]
    models += [Model("data://tests/pbr_calib.gltf")]
    #models += [Model("src://environ/objects/misc/headwalker.obj")]
    models += [Model("src://environ/objects/misc/ref/torus.glb")]
    ###################################
    for model in models:
      self.instancesets += [instance_set(model,numinstances,layer)]
    ###################################
    self.camera = CameraData()
    self.cameralut = CameraDataLut()
    self.cameralut.addCamera("spawncam",self.camera)
    ###################################
    self.camera.perspective(0.1, 150.0, 45.0)
    self.camera.lookAt(vec3(0,0,5), # eye
                       vec3(0, 0, 0), # tgt
                       vec3(0, 1, 0)) # up
  ################################################
  def onUpdate(self,updinfo):
    ###################################
    #for minst in self.instancesets:
     # minst.update(updinfo.deltatime)
    ###################################
    self.scene.updateScene(self.cameralut) # update and enqueue all scenenodes
################################################
app = SceneGraphApp()
app.qtapp.exec()
