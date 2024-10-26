import random, os, sys
import numpy as np
from orkengine.core import *
from orkengine.lev2 import *
import pyopencl as cl
mf = cl.mem_flags
################################################################################
sys.path.append((thisdir()/"..").normalized.as_string)
from lev2utils.cameras import *
from lev2utils.shaders import *
from lev2utils.primitives import createCubePrim, createGridData
from lev2utils.scenegraph import createSceneGraph
################################################################################
class InstanceSet(object):
  ########################################################
  def __init__(self,model,num_instances,layer):
    super().__init__()
    self.numinstances = num_instances
    self.model = model
    self.sgnode = model.createInstancedNode(num_instances,"node1",layer)
    idata = self.sgnode.instanceData
    self.instancematrices = np.array(idata.matrices, copy = False)
    self.instancecolors = np.array(idata.colors, copy = False)
    self.delta_rots = np.zeros((num_instances,4,4),dtype=np.float32) # array of 4x4 matrices
    self.delta_tras = np.zeros((num_instances,4,4),dtype=np.float32) # array of 4x4 matrices
    for i in range(num_instances):
      #####################################
      # rotation increment
      #####################################
      incraxis = vec3(random.uniform(-1,1),
                      random.uniform(-1,1),
                      random.uniform(-1,1)).normalized
      incrmagn = random.uniform(-0.05,0.05)
      rot = quat(incraxis,incrmagn)
      as_mtx4 = mtx4()
      trans = vec3(random.uniform(-1,1),
                   random.uniform(-1,1),
                   random.uniform(-1,1))*0.027
      as_mtx4.compose(trans,rot,1.0)
      tramtx = mtx4.transMatrix(trans)
      rotmtx = mtx4.rotMatrix(rot)
      self.delta_rots[i]=rotmtx # copy into numpy block
      self.delta_tras[i]=tramtx # copy into numpy block
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
  ################################################
  def clupdate(self,dt):
    current = cl.Buffer(self.clkernel.ctx, mf.READ_ONLY | mf.COPY_HOST_PTR, hostbuf=self.instancematrices)
    deltarot = cl.Buffer(self.clkernel.ctx, mf.READ_ONLY | mf.COPY_HOST_PTR, hostbuf=self.delta_rots)
    deltatra = cl.Buffer(self.clkernel.ctx, mf.READ_ONLY | mf.COPY_HOST_PTR, hostbuf=self.delta_tras)
    globalsize = (self.numinstances,1,1)
    localsize = None
    self.clkernel.prg.cl_concatenate_mtx4(self.clkernel.queue, globalsize, localsize, current, deltarot, self.res_r)
    self.clkernel.prg.cl_concatenate_mtx4(self.clkernel.queue, globalsize, localsize, deltatra, self.res_r, self.res_t)
    cl.enqueue_copy(self.clkernel.queue, self.instancematrices, self.res_t)
################################################################################
class SimApp(object):
  ################################################
  def __init__(self,vrmode,instance_set_class):
    super().__init__()
    self.ezapp = OrkEzApp.create(self,fullscreen=False)
    self.ezapp.setRefreshPolicy(RefreshFastest, 0)
    self.instanceset=None
    self.instance_set_class = instance_set_class
    setupUiCamera(app=self,eye=vec3(30))
  ##############################################
  def onGpuInit(self,ctx):
    params_dict = {
      "SkyboxIntensity": float(2),
      "SpecularIntensity": float(1),
      "DepthFogDistance": float(10000)
    }
    createSceneGraph( app=self,
                      rendermodel="DeferredPBR",
                      params_dict=params_dict,
                      layer_name="std_deferred")
    model = XgmModel("src://environ/objects/misc/ref/uvsph.glb")
    self.instanceset = self.instance_set_class(model,self.layer1)
  ################################################
  def onUpdate(self,updinfo):
    self.instanceset.update(updinfo.deltatime)
    self.scene.updateScene(self.cameralut) # update and enqueue all scenenodes
  ################################################
  def onUiEvent(self,uievent):
    handled = self.uicam.uiEventHandler(uievent)
    if handled:
      self.camera.copyFrom( self.uicam.cameradata )
    return ui.HandlerResult()
  ################################################

################################################

class ClKernel(object):
  def __init__(self):
    super().__init__()
    ################################################################################
    if "PYOPENCL_CTX" not in os.environ:
      os.environ["PYOPENCL_CTX"]='0' # select open cl device
    ################################################################################
    # Create OpenCL context and compile CL kernel
    ################################################################################
    platform = cl.get_platforms()
    my_gpu_devices = platform[0].get_devices(device_type=cl.device_type.GPU)
    self.ctx = cl.Context(devices=my_gpu_devices)
    #self.ctx = cl.create_some_context()
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
