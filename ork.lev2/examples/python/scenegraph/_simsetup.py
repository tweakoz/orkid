import random
import numpy as np
from orkengine.core import *
from orkengine.lev2 import *
################################################################################
class InstanceSet(object):
  ########################################################
  def __init__(self,model,num_instances,layer):
    super().__init__()
    self.model = model
    self.sgnode = model.createInstancedNode(num_instances,"node1",layer)
    self.instancematrices = np.array(self.sgnode.instanceData, copy = False)
    self.deltas = np.zeros((num_instances,4,4),dtype=np.float32) # array of 4x4 matrices
    for i in range(num_instances):
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
################################################################################
class SimApp(object):
  ################################################
  def __init__(self,vrmode,instance_set_class):
    super().__init__()
    self.sceneparams = VarMap()
    self.sceneparams.preset = "PBRVR" if vrmode else "PBR"
    self.qtapp = OrkEzQtApp.create(self)
    self.qtapp.setRefreshPolicy(RefreshFastest, 0)
    self.instancesets=[]
    self.instance_set_class = instance_set_class
  ##############################################
  def onGpuInit(self,ctx):
    layer = self.scene.createLayer("layer1")
    models = [Model("src://environ/objects/misc/ref/uvsph.glb")]
    ###################################
    for model in models:
      self.instancesets += [self.instance_set_class(model,layer)]
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
