#!/usr/bin/env ork.python

################################################################################
# ECS FPS Example (ComponentizedApplication + EcsRuntime scaffold)
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
################################################################################

import os, sys, argparse
from orkengine.core import vec3, vec4, CrcStringProxy, lev2_pyexdir
from orkengine import lev2
from orkengine import ecs
from ork.app.application import ComponentizedApplication
from ork.ecs import EcsRuntime

tokens = CrcStringProxy()

lev2_pyexdir.addToSysPath()
from lev2utils.primitives import createGridData
from _fps_game import FpsGame

################################################################################

parser = argparse.ArgumentParser(description="ECS FPS Example")
parser.add_argument("--fullscreen", "-f", action="store_true")
parser.add_argument("-e", "--envmap", type=str, default="arena")
args = parser.parse_args()

################################################################################

class FpsApp(ComponentizedApplication):

  def __init__(self):
    super().__init__()
    self.runtime = EcsRuntime()
    self.game = FpsGame(self.runtime, envmap=args.envmap)
    self.createEzApp(
      fullscreen=args.fullscreen,
      disable_mouse_cursor=True,
      pre_init_fns=[ecs.ecsInitCallback])

  ##############################################################################
  # UI Setup
  ##############################################################################

  def _onUiInit(self):
    lg = self.ezapp.topLayoutGroup
    lg.margin = 0
    lg.clearColorStd = vec4(0, 0, 0, 1)

    vp_item = lg.makeChild(
      fill=True, margin=0,
      uiclass=lev2.ui.SceneGraphViewport,
      args=["Viewport", vec4(0, 0, 0, 1)])
    self.sgv = vp_item.widget

  ##############################################################################
  # GPU Init
  ##############################################################################

  def _onGpuInit(self, ctx):
    self.runtime.setup_camera(
      eye=vec3(0, 0, 0), tgt=vec3(0, 0, 1), up=vec3(0, 1, 0))
    self.runtime.uicam.rotOnMove = True
    self.grid_data = createGridData()

    self.sgv.cameraName = "spawncam"
    self.sgv.clip_events = False  # unbounded cursor with GLFW_CURSOR_DISABLED
    self.sgv.camera_evhandler = lambda ev: self._onCameraEvent(ev)
    self.sgv.forkDB()

    self.game.populate_scene()
    self.game.gpu_init(ctx)

    self._create_scenegraph()
    self.runtime.start_simulation()
    self.game.post_start()
    self.runtime.bind_to_viewport(self.sgv)

  ##############################################################################

  def _create_scenegraph(self):
    self.runtime.create_scenegraph()
    grid_node = self.runtime.layer.createDrawableNodeFromData("grid", self.grid_data)
    grid_node.sortkey = 1
    grid_node.pickable = False

  ##############################################################################
  # Camera event handling
  # Events arrive via sgv.camera_evhandler (widget tree routing),
  # preserving LayoutGroup::doRouteUiEvent (Shift+~ profiler toggle).
  ##############################################################################

  def _onCameraEvent(self, uievent):
    if uievent.code == tokens.KEY_DOWN.hashed and uievent.super:
      kc = uievent.keycode
      if kc == 262:  # Command+Right Arrow → Start/Restart
        self.game.reset()
        self._create_scenegraph()
        self.runtime.start_simulation()
        self.game.post_start()
        self.runtime.bind_to_viewport(self.sgv)
        return lev2.ui.HandlerResult()
      elif kc == 264:  # Command+Down Arrow → Stop
        if self.runtime.controller:
          self.runtime.destroy_simulation()
        return lev2.ui.HandlerResult()
    return self.game.handle_camera_event(uievent)

  ##############################################################################
  # Update loop
  ##############################################################################

  def _onGpuUpdate(self, ctx):
    self.runtime.gpuUpdate(ctx)

  def _onUpdate(self, updinfo):
    if self.runtime.controller:
      self.game.update(self.ezapp)
    self.sgv.setDirty()

  ##############################################################################

  def _onGpuExit(self, ctx):
    if self.runtime.controller:
      self.runtime.destroy_simulation()

################################################################################

app = FpsApp()
app.ezapp.mainThreadLoop()
app.ezapp.shutdown()
