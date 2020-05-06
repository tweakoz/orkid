#!/usr/bin/env python3
################################################################################
# lev2 sample which renders an instanced model, optionally in VR mode
#  the models are animated via a numba jitified python function
# Copyright 1996-2020, Michael T. Mayers.
# Distributed under the Boost Software License - Version 1.0 - August 17, 2003
# see http://www.boost.org/LICENSE_1_0.txt
################################################################################
import math, random, argparse
import numpy as np
from scipy import linalg as la
from numba import vectorize, jit
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
#@vectorize(['float32(float32, float32)'], target='cuda')
@jit(nopython=True)
def matrix_update(curmatrices, deltas):
  for i in range(0,numinstances):
    a = curmatrices[i]
    b = deltas[i]
    curmatrices[i]=b.dot(a)
################################################################################
class instance_set(object):
  ########################################################
  def __init__(self,model,num_instances,layer):
    super().__init__()
    self.model = model
    self.sgnode = model.createInstancedNode(num_instances,"node1",layer)
    self.instancematrices = np.array(self.sgnode.instanceData, copy = False)
    self.deltas = np.zeros((num_instances,4,4),dtype=np.float32) # array of 4x4 matrices
    for i in range(numinstances):
      #####################################
      # rotation increment
      #####################################
      incraxis = vec3(random.uniform(-1,1),
                      random.uniform(-1,1),
                      random.uniform(-1,1)).normal()
      incrmagn = random.uniform(-0.05,0.05)
      rot = quat(incraxis,incrmagn)
      as_mtx4 = mtx4()
      trans = vec3(random.uniform(-1,1),
                   random.uniform(-1,1),
                   random.uniform(-1,1))*0.01
      as_mtx4.compose(trans,rot,1.0)
      self.deltas[i]=as_mtx4 # copy into numpy block
      #####################################
      # initial matrix
      #####################################
      Z = random.uniform(-2.5,-50)
      pos = vec3(random.uniform(-2.5,2.5)*Z,
                 random.uniform(-2.5,2.5)*Z,
                 Z)
      sca = random.uniform(0.1,0.65)
      as_mtx4.compose(pos,quat(),sca)
      self.instancematrices[i]=as_mtx4

  ########################################################
  def update(self,deltatime):
    matrix_update(self.instancematrices,self.deltas)
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
    models += [Model("src://environ/objects/misc/ref/uvsph.glb")]
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
