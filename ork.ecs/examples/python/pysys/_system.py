###############################################################################
# (simulation/update)-thread private python simulation system
################################################################################
import math, time
from orkengine.ecssim import *
tokens = CrcStringProxy()
###############################################################################
# not until numpy supports sub-interpreters...
#import numpy as np
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
    self.gametime = 0.0
    self.upd_counter = 0

the_sys = MySystem()

###############################################################################
###############################################################################

def onSystemInit(simulation):
  print("onSystemInit<%s>"%simulation)

###############################################################################

def onSystemLink(simulation):
  print("onSystemLink<%s>"%simulation)
  the_sys.sys_sg = simulation.findSystemByName("SceneGraphSystem")
  the_sys.sys_py = simulation.findSystemByName("PythonSystem")
  the_sys.python_components = the_sys.sys_py.vars.components

###############################################################################

def onSystemActivate(simulation):
  print("onSystemActivate<%s>"%simulation)

###############################################################################

def onSystemStage(simulation):
  print("onSystemStage<%s>"%simulation)

###############################################################################

def onComponentActivate(component):
  ent = component.entity
  eid = ent.id
  sgc = ent.findComponentByName("SceneGraphComponent")
  ent.vars.sgc = sgc
  ent.vars.incept = the_sys.gametime
  ent.vars.target_pos = vec3(0)

  #print("onComponentActivate eid %s"%e)

def onComponentDeactivate(component):
  eid = component.entity.id
  #print("onComponentDeactivate eid %s"%e)

###############################################################################

def onSystemNotify(simulation, evID, table):
  the_sys.notif_count += 1
  if evID.hashed == tokens.SET_TARGET.hashed:
    entID = table[tokens.ent]
    ent = simulation.entityByID(entID)
    ent.vars.target_pos = table[tokens.pos]
  else:
    print("onSystemNotify<%s:%s>"%(evID,table))
    assert(False)
  if (the_sys.notif_count%500)==0:
    print("onSystemNotify notifcount: %s"%the_sys.notif_count)

###############################################################################

def onSystemUpdate(simulation):

  the_sys.upd_counter += 1

  ###############
  # query time
  ###############

  dt = simulation.deltaTime
  gt = simulation.gameTime

  the_sys.gametime = gt
  ###############
  # update camera
  ###############

  the_sys.timeaccum += dt
  
  if the_sys.timeaccum>(1.0/120.0):

    the_sys.timeaccum = 0.0

    num_components = the_sys.python_components.size
    for index in range(0,num_components):
      comp = the_sys.python_components[index]
      ent = comp.entity
      sgc = ent.vars.sgc
      incept = ent.vars.incept
      target_pos = ent.vars.target_pos
      ent.translation = ent.translation*0.9995 + target_pos*0.0005
      # change color
      age = the_sys.gametime - incept
      r = 0.5 + (0.5*math.sin(age*3.0))
      g = 0.5 + (0.5*math.sin(age*5.0))
      b = 0.5 + (0.5*math.sin(age*7.0))
      rgb = vec4(r,g,b,1)
      sgc.notify( tokens.ChangeModColor, rgb )

  if True:      
    phase = gt * 0.1
    tgt = vec3(0,0,0)
    eye = vec3(math.sin(phase),0,-math.cos(phase))*30.0

    the_sys.sys_sg.notify( tokens.UpdateCamera,{
       tokens.eye: eye,
       tokens.tgt: tgt,
       tokens.up: vec3(0,1,0),
       tokens.near: 0.1,
       tokens.far: 1000.0,
       tokens.fovy: 90.0*(3.14159/180.0),
    })
  
  #time.sleep(0.1)
