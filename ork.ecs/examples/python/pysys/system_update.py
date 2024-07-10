###############################################################################
# (simulation/update)-thread private python simulation system
################################################################################
import math
import numpy as np
from orkengine.ecssim import *
tokens = CrcStringProxy()
###############################################################################
# not until numpy supports sub-interpreters...
#platform = CL.get_platforms()
#my_gpu_devices = platform[0].get_devices(device_type=CL.device_type.GPU)
#cl_ctx = CL.Context(devices=my_gpu_devices)
#cl_queue = CL.CommandQueue(cl_ctx)
#print(my_gpu_devices)
###############################################################################

class MySystem:
  def __init__(self):
    self.notif_count = 0
    self.timeaccum = 0.0

the_sys = MySystem()

###############################################################################
###############################################################################

def onSystemInit(simulation):
  print("onSystemInit<%s>"%simulation)

###############################################################################

def onSystemLink(simulation):
  print("onSystemLink<%s>"%simulation)
  the_sys.sys_sg = simulation.findSystemByName("SceneGraphSystem")

###############################################################################

def onSystemActivate(simulation):
  print("onSystemActivate<%s>"%simulation)

###############################################################################

def onSystemStage(simulation):
  print("onSystemStage<%s>"%simulation)

###############################################################################

def onSystemNotify(simulation, evID, table):
  the_sys.notif_count += 1
  if evID.hashed == tokens.Function1.hashed:
    hello = table[tokens.hello]
    #v3 = table[tokens.v3]
    #v3b = vec3(1,0,1)
    #print(v3)
    #print("notifcount<%d>"%(the_sys.notif_count))
  else:
    print("onSystemNotify<%s:%s>"%(evID,table))
    assert(False)
  if (the_sys.notif_count%500)==0:
    print("onSystemNotify notifcount: %s"%the_sys.notif_count)

###############################################################################

def onSystemUpdate(simulation):

  ###############
  # query time
  ###############

  dt = simulation.deltaTime
  gt = simulation.gameTime

  ###############
  # update camera
  ###############

  the_sys.timeaccum += dt
  
  if the_sys.timeaccum>(1.0/120.0):

    the_sys.timeaccum = 0.0

    phase = gt * 0.1
    eye = vec3(0,0,0)
    tgt = vec3(math.sin(phase),0,-math.cos(phase))*10.0

    as_np = np.array(tgt.as_buffer)
    #print(as_np.shape)
    print(as_np)
    
    the_sys.sys_sg.notify( tokens.UpdateCamera,{
       tokens.eye: eye,
       tokens.tgt: tgt,
       tokens.up: vec3(0,1,0),
       tokens.near: 0.1,
       tokens.far: 1000.0,
       tokens.fovy: 90.0*(3.14159/180.0),
    })
