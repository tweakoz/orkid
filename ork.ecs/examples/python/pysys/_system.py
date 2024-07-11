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
    self.upd_counter = 0
    self.cdict = dict()
    self.pdict = dict()

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

def onComponentActivate(component):
  ent = component.entity
  eid = ent.id
  sgc = ent.findComponentByName("SceneGraphComponent")
  the_sys.cdict[eid] = sgc
  #print("onComponentActivate eid %s"%e)

def onComponentDeactivate(component):
  eid = component.entity.id
  del the_sys.cdict[eid]
  if eid in the_sys.pdict:
    del the_sys.pdict[eid]
  #print("onComponentDeactivate eid %s"%e)

###############################################################################

def onSystemNotify(simulation, evID, table):
  the_sys.notif_count += 1
  if evID.hashed == tokens.SET_TARGET.hashed:
    ent = table[tokens.ent]
    pos = table[tokens.pos]
    if ent.id in the_sys.cdict:
      c = the_sys.cdict[ent.id]
      
      #print("ent<%s> c<%s> NEWPOS<%s>"%(ent.id,c,pos))
      # change color
      r = 0.5 + (0.5*math.sin(the_sys.notif_count))
      g = 0.5 + (0.5*math.sin(the_sys.notif_count))
      b = 0.5 + (0.5*math.sin(the_sys.notif_count))
      rgb = vec4(r,g,b,1)
      
      c.notify( tokens.ChangeModColor, rgb )
      
      the_sys.pdict[ent.id] = pos
      
      #c.pos = pos
    #print("notifcount<%d>"%(the_sys.notif_count))
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

  ###############
  # update camera
  ###############

  the_sys.timeaccum += dt
  
  if True: #the_sys.timeaccum>(1.0/120.0):

    the_sys.timeaccum = 0.0

    phase = gt * 0.1
    tgt = vec3(0,0,0)
    eye = vec3(math.sin(phase),0,-math.cos(phase))*30.0

    #as_np = np.array(tgt.as_buffer)
    #print(as_np.shape)
    #print(as_np)
    #print(tgt)
    
    #print(float(the_sys.upd_counter)/gt)
    
    the_sys.sys_sg.notify( tokens.UpdateCamera,{
       tokens.eye: eye,
       tokens.tgt: tgt,
       tokens.up: vec3(0,1,0),
       tokens.near: 0.1,
       tokens.far: 1000.0,
       tokens.fovy: 90.0*(3.14159/180.0),
    })
    
    for item in the_sys.pdict.items():
      eid = item[0]
      pos = item[1]
      sgc = the_sys.cdict[eid] 
      ent = sgc.entity
      ent.translation = ent.translation*0.99 + pos*0.01
      
    #time.sleep(0.1)
