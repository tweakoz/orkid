#!/usr/bin/env ork.python

################################################################################
# lev2 sample which renders a UI to a window, 
#   delegates some rendering code to PyOpenGL and PyImGui
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

#pip3 install imgui_bundle

import traceback, time, json
import threading, concurrent.futures
import subprocess, os
from PIL import Image
from obt import path as obt_path
from string import Template
################################################################################
from imgui_bundle import imgui, hello_imgui, imgui_md
from imgui_bundle import imgui_color_text_edit as ed
#from imgui_bundle import imgui_fig
#from matplotlib import pyplot as plt, use as plt_use
import numpy as np
from OpenGL.GL import *
################################################################################
from orkengine.core import CrcStringProxy, lev2_pyexdir, VarMap
from orkengine.core import vec2, vec3, vec4, quat, mtx3, mtx4
from orkengine import lev2
################################################################################
lev2_pyexdir.addToSysPath()
from lev2utils.imgui import ImGuiWrapper, installImguiOnApp
################################################################################

tokens = CrcStringProxy()

################################################################################
# Our hybrid application class
################################################################################

class UiTestApp(object):

  def __init__(self):
    super().__init__()

    self.newAppState() # define application state (for save/restore)

    # create ezapp
    self.ezapp = lev2.OrkEzApp.create(self,fullscreen=False)
    self.ezapp.setRefreshPolicy(lev2.RefreshFastest, 0)
    self.ezapp.topWidget.enableUiDraw()

    # create and bind overlay event interceptor
    #  so we can route events to imgui
    installImguiOnApp(self)

    lg_group = self.ezapp.topLayoutGroup
    lg_group.margin = 5
    griditems = lg_group.makeGrid( width = 3,
                                   height = 1,
                                   margin = 1,
                                   uiclass = lev2.ui.LambdaBox,
                                   args = ["box",vec4(1,0,1,1)] )

    print(griditems)

    # set up event handlers for the grid items
    griditems[0].widget.onPressed(lambda: print("GRIDITEM0 PUSHED"))
    griditems[1].widget.onPressed(lambda: print("GRIDITEM1 PUSHED"))
    
    self.griditems = griditems
    
    IMGW = self.griditems[0].widget
    IMGW.enableDraw = False # disable default drawing for widget 0, as it will be drawn by PyOpenGL
    self.imgui_widget = IMGW
    
    OLGW = self.griditems[1].widget
    OLGW.enableDraw = True # disable default drawing for widget 0, as it will be drawn by PyOpenGL
    self.opengl_widget = OLGW

    self.status_text = "OK"

    # animation properties
    self.time = 0.0
    self.fps_accum = 0.0
    self.fps_time_base = time.time()
    self.ups_accum = 0.0
    self.ups_time_base = time.time()
    self.FPS = 0.0
    self.UPS = 0.0
    
    self.current_preset = "none"
    self.item_current_idx = 0

    #plt_use('Agg')
    #fig, ax = plt.subplots()
    #ax.plot([1, 2, 3], [4, 5, 6])
    #self.ax = ax
    #self.fig = fig

  ##############################################

  def newAppState(self):
    self.app_vars = VarMap()
    self.app_vars.preset_name = "default"

  ##############################################
  # onUpdate - called from update / simulation thread
  ##############################################

  def onUpdate(self,updev):
    self.time = updev.absolutetime
    self.ups_accum += 1.0
    now = time.time()
    delta = now-self.ups_time_base
    if delta>1.0:
      self.UPS = self.ups_accum/delta
      self.ups_accum = 0.0
      self.ups_time_base = time.time()

  ##############################################
  # onGpuInit - called once at startup (in rendering thread)
  #  this is where pyopengl initialization should be done
  ##############################################

  def onGpuInit(self,ctx):

    ##################################
    # setup imgui
    ##################################

    self.imgui_handler = ImGuiWrapper( self, 
                                       "ork_pyext_test_pyopengl2", 
                                       docking=True, 
                                       lock_to_panel=self.imgui_widget )
    self.imgui_handler.onGpuInit(ctx,self.app_vars)   
        
  ##############################################

  def onExit(self):
    self.imgui_handler.onExit(self.app_vars)

  ##############################################
  # invoked by the UI overlay widget
  #  this is where imgui events are processed
  #  if the event is not handled by imgui, it is passed to the app
  ##############################################

  def onOverlayUiEvent(self,uievent):
    handled = False
    if uievent.code == tokens.KEY_DOWN.hashed:
      keycode = uievent.keycode
      is_ctrl = uievent.ctrl 
    if not handled:
      return self.imgui_handler.onUiEvent(uievent)
       
  ##############################################
  # onGpuPostFrame - called after ALL orkid rendering is done
  #  this is where pyopengl/imgui rendering should be done
  ##############################################

  def onGpuPostFrame(self,ctx):
    self._renderImGui(ctx)
    imgui.update_platform_windows();
    imgui.render_platform_windows_default();
    self.fps_accum += 1.0
    now = time.time()
    delta = now-self.fps_time_base
    if delta>1.0:
      self.FPS = self.fps_accum/delta
      self.fps_accum = 0.0
      self.fps_time_base = time.time()
    
  ##############################################
  # _renderImGui - render the imgui UI
  ##############################################

  def _renderImGui(self,ctx):

    #################################
    # begin imgui rendering
    #################################

    self.imgui_handler.beginFrame()
    io = self.imgui_handler.imgui_io

    imgui.begin("Orkid/PyImGui/PyOpenGL integration example", True)      

    #################################
    # presets name
    #################################

    imgui.same_line()

    changed, self.app_vars.preset_name = imgui.input_text(
      label="PresetName", 
      str=self.app_vars.preset_name,
    )

    #################################
    # FPS
    #################################

    is_shift = io.key_shift
    is_ctrl = io.key_ctrl
    is_alt = io.key_alt
    is_super = io.key_super

    imgui.text("FPS %g"%(self.FPS))
    imgui.text("UPS %g"%(self.UPS))
    imgui.text("is_shift %s is_ctrl %s is_alt %s is_super %s"%(is_shift,is_ctrl,is_alt,is_super))
    
    # draw status color and text
    status_color = imgui.ImVec4(1,0,0,1)
    if self.status_text=="OK":
      status_color = imgui.ImVec4(0,1,0,1)
    imgui.text_colored(status_color, self.status_text)
    
    imgui.end()

    #################################

    if imgui.begin("Graph Window"):
        #imgui_fig.fig("x", figure=self.fig)
        imgui.end()
            
    #################################
    # end imgui rendering
    #################################

    self.imgui_handler.endFrame()

###############################################################################

the_app = UiTestApp()
the_app.ezapp.mainThreadLoop()
the_app.onExit()
