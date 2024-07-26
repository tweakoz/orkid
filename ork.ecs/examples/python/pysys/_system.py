###############################################################################
# (simulation/update)-thread private python simulation system
################################################################################
import math, time, random
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
    self.ent_counter = 0

###############################################################################

def onSystemInit(simulation):
  simulation.vars.mysys = MySystem()

###############################################################################

def onSystemLink(simulation):
  #print("onSystemLink<%s>"%simulation)
  simulation.vars.mysys.sys_sg = simulation.findSystemByName("SceneGraphSystem")
  simulation.vars.mysys.sys_py = simulation.findSystemByName("PythonSystem")
  simulation.vars.mysys.python_components = simulation.vars.mysys.sys_py.vars.components

###############################################################################

def onSystemActivate(simulation):
  #print("onSystemActivate<%s>"%simulation)
  pass

###############################################################################

def onSystemStage(simulation):
  #print("onSystemStage<%s>"%simulation)
  pass

###############################################################################

def onComponentActivate(simulation,component):
  ent = component.entity
  eid = ent.id
  sgc = ent.findComponentByName("SceneGraphComponent")
  ent.vars.sgc = sgc
  ent.vars.incept = simulation.vars.mysys.gametime
  ent.vars.target_pos = vec3(0)
  i = random.randint(-100,100)
  j = random.randint(-100,100)
  k = random.randint(-100,100)
  i = float(i)/100.0
  j = float(j)/100.0
  k = float(k)/100.0
  axis = vec3(i,j,k).normalized
  angle = random.randint(-1,1)*3.14159/180.0
  ent.vars.dr = quat(axis,angle)
  ent.vars.r = quat()
  #print("onComponentActivate eid %s"%e)

def onComponentDeactivate(component):
  eid = component.entity.id
  #print("onComponentDeactivate eid %s"%e)

###############################################################################

def onSystemNotify(simulation, evID, table):
  simulation.vars.mysys.notif_count += 1
  if evID.hashed == tokens.SET_TARGET.hashed:
    entID = table[tokens.ent]
    #print( "GOT SET_TARGET<%s>"%entID.id)
    ent = simulation.entityByID(entID)
    #print( "GOT SET_TARGET<%s>"%ent)
    ent.vars.target_pos = table[tokens.pos]
  elif evID.hashed == tokens.PRINT_CAMERA.hashed:
    phase = simulation.vars.mysys.gametime * 0.01
    tgt = vec3(0,0,0)
    eye = vec3(math.sin(phase),0,-math.cos(phase))*30.0
    print("\n###################")
    print("eye<%s>"%eye)
    print("tgt<%s>"%tgt)
    print("###################\n")
  else:
    print("unknown onSystemNotify<%s:%s>"%(evID,table))
  #if (simulation.vars.mysys.notif_count%500)==0:
  #  print("onSystemNotify notifcount: %s"%simulation.vars.mysys.notif_count)

###############################################################################

def onSystemUpdate(simulation):

  simulation.vars.mysys.upd_counter += 1

  ###############
  # query time
  ###############

  dt = simulation.deltaTime
  gt = simulation.gameTime

  simulation.vars.mysys.gametime = gt
  ###############
  # update camera
  ###############

  simulation.vars.mysys.timeaccum += dt
  
  if simulation.vars.mysys.timeaccum>(1.0/120.0):

    simulation.vars.mysys.timeaccum = 0.0

    num_components = simulation.vars.mysys.python_components.size
    for index in range(0,num_components):
      comp = simulation.vars.mysys.python_components[index]
      ent = comp.entity
      sgc = ent.vars.sgc
      incept = ent.vars.incept
      target_pos = ent.vars.target_pos

      lerp_factor = 0.0002
      inv_lerp_factor = 1.0 - lerp_factor
      ent.translation = ent.translation*inv_lerp_factor + target_pos*lerp_factor
      ent.orientation = ent.orientation * ent.vars.dr
 
      # change color
      age = simulation.vars.mysys.gametime - incept
      r = 0.5 + (0.5*math.sin(age*3.0))
      g = 0.5 + (0.5*math.sin(age*5.0))
      b = 0.5 + (0.5*math.sin(age*7.0))
      rgb = vec4(r,g,b,1)
      sgc.notify( tokens.ChangeModColor, rgb )

    simulation.vars.mysys.ent_counter += num_components
    if (simulation.vars.mysys.upd_counter&0xff)==0:
      ents_per_sec = simulation.vars.mysys.ent_counter / simulation.vars.mysys.gametime
      #print("ents_per_sec<%s>"%ents_per_sec)
      
  if True:      
    phase = gt*0.04
    tgt = vec3(0,0,0)
    eye = vec3(math.sin(phase),0,-math.cos(phase))*30.0

    simulation.vars.mysys.sys_sg.notify( tokens.UpdateCamera,{
       tokens.eye: eye,
       tokens.tgt: tgt,
       tokens.up: vec3(0,1,0),
       tokens.near: 1.0,
       tokens.far: 1000.0,
       tokens.fovy: 90.0*(3.14159/180.0),
    })
  
  #time.sleep(0.1)
