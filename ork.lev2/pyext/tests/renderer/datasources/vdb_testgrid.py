#!/usr/bin/env ork.python

import time, sys

from os import path as os_path
from obt import deco
from obt import path as obt_path
from ork import path as ork_path

sys.path.append(str(ork_path.py_lev2utils)) # add parent dir to path

from orkengine.core import vec2,vec3,vec4,quat
from orkengine.core import CrcStringProxy, lev2_pyexdir
from orkengine import lev2
from testapp import TestApp

import _sys_testgrid

###############################################################################
tokens = CrcStringProxy()
###############################################################################

class MyApp (TestApp):
  #############################
  def __init__(self):
    super().__init__()
    self.rsys = self.createSystem(_sys_testgrid.System)
  #############################
  def onGpuInit(self,ctx):
    super().onGpuInit(ctx)
  def onGpuUpdate(self,ctx):
    super().onGpuUpdate(ctx)
  def onDraw(self,ctx):
    super().onDraw(ctx)
  def onGpuExit(self,ctx):
    super().onGpuExit(ctx)
  #############################
  def onUpdateInit(self):
    super().onUpdateInit()
  def onUpdate(self,updinfo):
    super().onUpdate(updinfo)
  def onUpdateExit(self):
    super().onUpdateExit()
  #############################
  def incrementColorScale(self):
    self.rsys.color_scale += 0.025
    if self.rsys.color_scale > 10.0:
      self.rsys.color_scale = 10.0
    print("color_scale<%f>" % self.rsys.color_scale)
  def decrementColorScale(self):
    self.rsys.color_scale -= 0.025
    if self.rsys.color_scale < 0.0:
      self.rsys.color_scale = 0.0
    print("color_scale<%f>" % self.rsys.color_scale)
  #############################
  def onUiEvent(self,uievent):
    match uievent.code:
      ######################
      case tokens.KEY_DOWN.hashed:
        if uievent.keycode == ord("-"): 
          self.decrementColorScale()
        elif uievent.keycode == ord("="):
          self.incrementColorScale()
        elif uievent.keycode == 256: # escape key
          self.onTerminate()
      ######################
      case tokens.KEY_REPEAT.hashed:
        if uievent.keycode == ord("-"): 
          self.decrementColorScale()
        elif uievent.keycode == ord("="):
          self.incrementColorScale()
      ######################
    return super().onUiEvent(uievent)
  #############################

###############################################################################

MyApp().ezapp.mainThreadLoop()
