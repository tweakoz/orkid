#!/usr/bin/env ork.python

import math, sys, random, threading, time, signal
from obt import path as obt_path 
from ork import path as ork_path

from orkengine.core import vec3
from orkengine.lev2 import OrkEzApp, RefreshFastest, ui, primitives

import scenegraph 
import cameras 


###############################################################################

class TestSystem(object):
  def __init__(self):
    super().__init__()
    self.update_priority = 0
    self.gpu_priority = 0
    self.ok_to_exit = False
  def onTerminate(self):
    self.ok_to_exit = True
    pass
  def onGpuInit(self,ctx):
    pass
  def onGpuUpdate(self,ctx):
    pass
  def onDraw(self,drawevent):
    pass
  def onGpuExit(self,ctx):
    pass
  def onUpdateInit(self,ctx):
    pass
  def onUpdate(self,updinfo):
    pass
  def onUpdateExit(self,ctx):
    pass

###############################################################################
  
PRIORITY_COUNT = 16

class TestApp(object):

  def __init__(self):
    super().__init__()
    self.ezapp = OrkEzApp.create(self)
    self.ezapp.setRefreshPolicy(RefreshFastest, 0)
    cameras.setupUiCamera( app=self, eye = vec3(6,6,6), constrainZ=True, up=vec3(0,1,0))

    self.systems_for_update = list()
    self.systems_for_gpu = list()
    for i in range(PRIORITY_COUNT):
      self.systems_for_update.append(list())
      self.systems_for_gpu.append(list())

    def onCtrlC(signum, frame):
      print("signaling EXIT to ezapp")
      self.onTerminate()



    signal.signal(signal.SIGINT, onCtrlC)

  def createSystem(self,clazz,kwargs=dict()):
    sys = clazz(self,**kwargs)
    self.systems_for_update[sys.update_priority].append(sys)
    self.systems_for_gpu[sys.gpu_priority].append(sys)
    return sys
       
  ################################################

  def onTerminate(self):
    for pri in range(PRIORITY_COUNT):
      for sys in self.systems_for_gpu[pri]:
        sys.onTerminate()
    self.ezapp.signalExit()
    self.ok_to_exit = True

  ################################################
  # gpu data init:
  #  called on main thread when graphics context is
  #   made available
  ##############################################

  def onGpuInit(self,ctx):

    ###################################
    # create scenegraph
    ###################################

    sg_params = {
      "SkyboxIntensity": 1.0, 
      "DiffuseIntensity": 6.0, 
    }
    
    scenegraph.createSceneGraph(app=self,rendermodel="ForwardPBR",params_dict=sg_params)

    ###################################
    # initialize systems
    ###################################

    for pri in range(PRIORITY_COUNT):
      for sys in self.systems_for_gpu[pri]:
        sys.onGpuInit(ctx)


  ##############################################

  def onGpuUpdate(self,ctx):
    for pri in range(PRIORITY_COUNT):
      for sys in self.systems_for_gpu[pri]:
        sys.onGpuUpdate(ctx)
  
  ################################################

  def onDraw(self,drawevent):
    context = drawevent.context
    self.ezapp.processMainSerialQueue()    
    for pri in range(PRIORITY_COUNT):
      for sys in self.systems_for_update[pri]:
        sys.onDraw(drawevent)
    self.scene.renderOnContext(context);

  ##############################################

  def onGpuExit(self,ctx):
    print("onGpuExit")
    for pri in range(PRIORITY_COUNT):
      for sys in self.systems_for_gpu[pri]:
        sys.onGpuExit(ctx)
    self.ok_to_exit = True
    self.thr.join()

  ################################################

  def onUpdateInit(self):
    for pri in range(PRIORITY_COUNT):
      for sys in self.systems_for_update[pri]:
        sys.onUpdateInit()

  ################################################

  def onUpdate(self,updinfo):
    self.abstime = updinfo.absolutetime
    for pri in range(PRIORITY_COUNT):
      for sys in self.systems_for_update[pri]:
        sys.onUpdate(updinfo)
    self.scene.updateScene(self.cameralut) # update and enqueue all scenenodes
    
  ##############################################

  def onUpdateExit(self):
    print("onUpdateExit")
    self.ok_to_exit = True
    for pri in range(PRIORITY_COUNT):
      for sys in self.systems_for_update[pri]:
        sys.onUpdateExit()
    #self.thr.join()

  ##############################################

  def onUiEvent(self,uievent):
    handled = self.uicam.uiEventHandler(uievent)
    if handled:
      self.camera.copyFrom( self.uicam.cameradata )
    return ui.HandlerResult()
    
###############################################################################
