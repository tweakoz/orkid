import random
import numpy as np
from orkengine.core import *
from orkengine.lev2 import *
import pyopencl as cl
mf = cl.mem_flags
################################################################################
class InstanceSet(object):
  ########################################################
  def __init__(self,model,num_instances,layer):
    super().__init__()
    self.model = model
    self.sgnode = model.createInstancedNode(num_instances,"node1",layer)
    idata = self.sgnode.instanceData
    self.instancematrices = np.array(idata.matrices, copy = False)
    self.instancecolors = np.array(idata.colors, copy = False)
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
    self.instanceset=None
    self.instance_set_class = instance_set_class
  ##############################################
  def onGpuInit(self,ctx):
    layer = self.scene.createLayer("layer1")
    model = Model("src://environ/objects/misc/ref/uvsph.glb")
    self.instanceset = self.instance_set_class(model,layer)
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
    self.instanceset.update(updinfo.deltatime)
    ###################################
    self.scene.updateScene(self.cameralut) # update and enqueue all scenenodes
################################################
class ClKernel(object):
  def __init__(self):
    super().__init__()
    ################################################################################
    #if "PYOPENCL_CTX" not in os.environ:
    #os.environ["PYOPENCL_CTX"]='0' # select open cl device
    ################################################################################
    # Create OpenCL context and compile CL kernel
    ################################################################################
    self.ctx = cl.create_some_context()
    self.queue = cl.CommandQueue(self.ctx)
    self.prg = cl.Program(self.ctx,
    """
    __kernel void cl_concatenate_mtx4(
        __global const float* instancematrices,
        __global const float* deltas,
        __global float* result) {

      int instanceindex = get_global_id(0);
      uint mtxbase = instanceindex<<4;

      __global const float* fb = instancematrices + mtxbase;
      __global const float* fa = deltas + mtxbase;
      __global float* fc = result + mtxbase;

      fc[0] = fa[0] * fb[0] + fa[1] * fb[4] + fa[2] * fb[8] + fa[3] * fb[12];
      fc[1] = fa[0] * fb[1] + fa[1] * fb[5] + fa[2] * fb[9] + fa[3] * fb[13];
      fc[2] = fa[0] * fb[2] + fa[1] * fb[6] + fa[2] * fb[10] + fa[3] * fb[14];
      fc[3] = fa[0] * fb[3] + fa[1] * fb[7] + fa[2] * fb[11] + fa[3] * fb[15];

      fc[4] = fa[4] * fb[0] + fa[5] * fb[4] + fa[6] * fb[8] + fa[7] * fb[12];
      fc[5] = fa[4] * fb[1] + fa[5] * fb[5] + fa[6] * fb[9] + fa[7] * fb[13];
      fc[6] = fa[4] * fb[2] + fa[5] * fb[6] + fa[6] * fb[10] + fa[7] * fb[14];
      fc[7] = fa[4] * fb[3] + fa[5] * fb[7] + fa[6] * fb[11] + fa[7] * fb[15];

      fc[8]  = fa[8] * fb[0] + fa[9] * fb[4] + fa[10] * fb[8] + fa[11] * fb[12];
      fc[9]  = fa[8] * fb[1] + fa[9] * fb[5] + fa[10] * fb[9] + fa[11] * fb[13];
      fc[10] = fa[8] * fb[2] + fa[9] * fb[6] + fa[10] * fb[10] + fa[11] * fb[14];
      fc[11] = fa[8] * fb[3] + fa[9] * fb[7] + fa[10] * fb[11] + fa[11] * fb[15];

      fc[12] = fa[12] * fb[0] + fa[13] * fb[4] + fa[14] * fb[8] + fa[15] * fb[12];
      fc[13] = fa[12] * fb[1] + fa[13] * fb[5] + fa[14] * fb[9] + fa[15] * fb[13];
      fc[14] = fa[12] * fb[2] + fa[13] * fb[6] + fa[14] * fb[10] + fa[15] * fb[14];
      fc[15] = fa[12] * fb[3] + fa[13] * fb[7] + fa[14] * fb[11] + fa[15] * fb[15];

    }
    """
    ).build()
