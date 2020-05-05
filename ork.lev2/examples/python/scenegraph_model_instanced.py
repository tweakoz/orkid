#!/usr/bin/env python3
################################################################################
# lev2 sample which renders an instanced model, optionally in VR mode
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
class AnimationState(object):
  def __init__(self):
    super().__init__()
    self.curpos = vec3()
    self.dstpos = vec3()
    self.currot = quat()
    self.cursca = 0.0
    self.dstsca = 1.0
    self.incrot = quat()
  def update(self,deltatime):
    self.lerpindex += deltatime*0.25
    if self.lerpindex > 1:
        self.lerpindex = 1
    pos = vec3()
    pos.lerp(self.curpos,self.dstpos,self.lerpindex)
    sca = self.dstsca*self.lerpindex + self.cursca*(1-self.lerpindex)
    mtx = mtx4()
    mtx.compose(pos,self.currot,sca)
    self.currot = self.currot * self.incrot
    done = self.lerpindex>=1
    if done:
      self.curpos = pos
      self.cursca = sca
    return mtx,done
################################################################################
class instance_set(object):
  ########################################################
  def __init__(self,model,num_instances,layer):
    super().__init__()
    self.num_instances = num_instances
    self.model = model
    self.sgnode = model.createInstancedNode(num_instances,"node1",layer)
    self.animated = dict()
    self.animstates = dict()
    for i in range(num_instances):
      self.animstates[i] = AnimationState()
  ########################################################
  def animateInstance(self,deltatime,instance_id):
    animstate =self.animstates[instance_id]
    self.animated[instance_id] = animstate
    ########################################
    incraxis = vec3(random.uniform(-1,1),
                    random.uniform(-1,1),
                    random.uniform(-1,1)).normal()
    incrmagn = random.uniform(-0.05,0.05)
    ########################################
    Z = random.uniform(-5,-125)
    animstate.dstpos = vec3(random.uniform(-2.5,2.5)*Z,
               random.uniform(-2.5,2.5)*Z,
               Z)
    animstate.incrot = quat(incraxis,incrmagn)
    animstate.dstsca = random.uniform(0.1,0.2)
    animstate.lerpindex = 0.0
  ########################################################
  def update(self,deltatime):
    for i in range(3):
      instance_id = random.randint(0,numinstances-1)
      self.animateInstance(deltatime,instance_id)
    keys2del = list()
    for id in self.animated.keys():
      animstate = self.animstates[id]
      matrix, done = animstate.update(deltatime)
      self.sgnode.setInstanceMatrix(id,matrix)
      if done:
        keys2del += [id]
    for id in keys2del:
      del self.animated[id]

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
