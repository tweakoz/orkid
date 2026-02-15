#!/usr/bin/env ork.python

################################################################################
# ECS Scene Player
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
################################################################################

import os, sys, argparse
from orkengine.core import vec2, vec3, vec4, VarMap, CrcStringProxy, lev2_pyexdir
from orkengine import lev2
from orkengine import ecs
from ork.app.application import ComponentizedApplication
from ork.ecs import EcsRuntime

tokens = CrcStringProxy()

lev2_pyexdir.addToSysPath()
from lev2utils.primitives import createGridData

################################################################################

parser = argparse.ArgumentParser(description="ECS Scene Player")
parser.add_argument("--scene", "-s", type=str, help="Scene file to play (.json)")
args = parser.parse_args()

################################################################################

class EcsPlayer(ComponentizedApplication):

  def __init__(self):
    super().__init__()
    self.runtime = EcsRuntime()
    self._playing = False
    self.createEzApp(
      name="OrkidEcsPlayer",
      fullscreen=True,
      pre_init_fns=[ecs.ecsInitCallback])

  ##############################################################################
  # UI Setup
  ##############################################################################

  def _onUiInit(self):
    lg = self.ezapp.topLayoutGroup
    lg.margin = 0
    lg.clearColorStd = vec4(0.08, 0.08, 0.1, 1)

    # Fullscreen viewport
    vp_item = lg.makeChild(
      fill=True, margin=0,
      uiclass=lev2.ui.SceneGraphViewport,
      args=["Viewport", vec4(0.08, 0.08, 0.1, 1)])
    self.sgv = vp_item.widget

  ##############################################################################
  # GPU Init
  ##############################################################################

  def _onGpuInit(self, ctx):
    self.runtime.setup_camera()
    self.runtime.uicam.distance = 1
    self.grid_data = createGridData()

    # Viewport bindings
    self.sgv.cameraName = "spawncam"
    self.sgv.camera_evhandler = lambda ev: self._onCameraEvent(ev)
    self.sgv.forkDB()

    # Load scene if provided
    if args.scene and os.path.exists(args.scene):
      self.runtime.load_scene(args.scene)

    # Stage scene (same as editor idle — staged simulation with grid)
    self._stage_scene()
    print("ECS Player Ready — press SPACEBAR to start simulation")

  ##############################################################################
  # Staged / Playing states
  ##############################################################################

  def _create_scenegraph_with_grid(self):
    """Create fresh scenegraph + add grid overlay."""
    self.runtime.create_scenegraph()
    grid_node = self.runtime.layer.createDrawableNodeFromData("grid", self.grid_data)
    grid_node.sortkey = 1
    grid_node.pickable = False

  def _stage_scene(self):
    """Stage the scene — entities created but not ticking (edit-like idle)."""
    self._create_scenegraph_with_grid()
    self.runtime.stage_simulation()
    self.runtime.bind_to_viewport(self.sgv)
    self._playing = False

  def _start_simulation(self):
    """Start (or restart) the simulation."""
    self._create_scenegraph_with_grid()
    self.runtime.start_simulation()
    self.runtime.bind_to_viewport(self.sgv)
    self._playing = True
    print("Simulation started (SPACEBAR to restart)")

  ##############################################################################
  # Camera event handling
  ##############################################################################

  def _onCameraEvent(self, uievent):
    if uievent.code == tokens.KEY_DOWN.hashed and uievent.super:
      kc = uievent.keycode
      if kc == 262:  # Command+Right Arrow → Start/Restart
        self._start_simulation()
        return lev2.ui.HandlerResult()
      elif kc == 264:  # Command+Down Arrow → Stop (back to staged)
        if self._playing:
          self._stage_scene()
        return lev2.ui.HandlerResult()
    return self.runtime.handle_camera_event(uievent)

  ##############################################################################
  # Update loop
  ##############################################################################

  def _onUpdate(self, updinfo):
    # Always update — staged mode needs camera sync, playing mode needs full tick
    if self.runtime.controller:
      self.runtime.update()
    self.sgv.setDirty()

################################################################################

app = EcsPlayer()
app.ezapp.mainThreadLoop()
app.ezapp.shutdown()
