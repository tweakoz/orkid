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
    self.num_instances = num_instances
    self.model = model
    self.sgnode = model.createInstancedNode(num_instances,"node1",layer)

  def updateInstance(self,deltatime,instance_id):
    Z = random.uniform(-2.5,-125)
    pos = vec3(random.uniform(-2.5,2.5)*Z,
               random.uniform(-2.5,2.5)*Z,
               Z)
    incraxis = vec3(random.uniform(-1,1),
                    random.uniform(-1,1),
                    random.uniform(-1,1)).normal()
    incrmagn = random.uniform(-1,1)
    rot = quat(incraxis,incrmagn)
    scale = random.uniform(0.1,0.2)

    mtx = mtx4()
    mtx.compose( pos, rot, scale )
    self.sgnode.setInstanceMatrix(instance_id,mtx)

  def update(self,deltatime):
    for i in range(100):
      instance_id = random.randint(0,numinstances-1)
      self.updateInstance(deltatime,instance_id)

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
    #models += [Model("data://tests/pbr1/pbr1")]
    #models += [Model("data://tests/pbr_calib.gltf")]
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
    for minst in self.instancesets:
      minst.update(updinfo.deltatime)
    ###################################
    self.scene.updateScene(self.cameralut) # update and enqueue all scenenodes
################################################
app = SceneGraphApp()
app.qtapp.exec()
