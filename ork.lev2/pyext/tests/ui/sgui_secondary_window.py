#!/usr/bin/env ork.python
################################################################################
# Multi-window scenegraph test: Two windows, each with its own scenegraph
# Tests independent 3D rendering and camera control on both windows
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
################################################################################

import signal
import sys
import math
from orkengine.core import vec3, vec4, dvec4, quat, mtx4, VarMap, CrcStringProxy
from orkengine import lev2

tokens = CrcStringProxy()

################################################################################

# Auto-close after 5 seconds for automated testing
AUTO_CLOSE = "--auto-close" in sys.argv

################################################################################

l2exdir = (lev2.lev2exdir() / "python").normalized.as_string
if l2exdir not in sys.path:
  sys.path.append(l2exdir)
from lev2utils.cameras import setupUiCameraX
from lev2utils.primitives import createGridData, createCubePrim
from lev2utils.shaders import createPipeline

################################################################################

class DualSceneGraphWindow:

  def __init__(self):
    super().__init__()

    self.win_width = 640
    self.win_height = 600

    self.ezapp = lev2.OrkEzApp.create(
      self,
      name="DualSceneGraph::Primary",
      width=self.win_width,
      height=self.win_height,
      left=100,
      top=100
    )
    self.ezapp.setRefreshPolicy(lev2.RefreshFastest, 0)
    self.ezapp.topWidget.enableUiDraw()

    # Primary window: SceneGraphViewport in layout
    lg = self.ezapp.topLayoutGroup
    lg.margin = 4
    self.primary_sgv = lg.makeChild(
      uiclass=lev2.ui.SceneGraphViewport,
      args=["PrimarySG"],
      fill=True
    )

    self.secondary_win = None
    self.secondary_sgv = None
    self.frame_count = 0

    # Cameras and scenes (set up in onGpuInit)
    self.scene1 = None
    self.scene2 = None
    self.cameralut1 = lev2.CameraDataLut()
    self.cameralut2 = lev2.CameraDataLut()
    self.camera1 = lev2.CameraData()
    self.camera2 = lev2.CameraData()
    self.uicam1 = None
    self.uicam2 = None

    # Rotation for visual interest
    self.rotation_angle = 0.0

    def onCtrlC(signum, frame):
      print("signalling EXIT")
      self.ezapp.signalExit()

    signal.signal(signal.SIGINT, onCtrlC)

  ##############################################

  def _create_scene_params(self):
    params = VarMap()
    params.preset = "ForwardPBR"
    params.SkyboxIntensity = 0.5
    params.DiffuseIntensity = 1.0
    params.SpecularIntensity = 1.0
    params.AmbientLevel = vec3(0.2)
    params.DepthFogDistance = 1000.0
    params.clearcolor = vec3(0.1, 0.1, 0.15)
    return params

  ##############################################

  def _setup_primary_scene(self, ctx):
    # Create scene for primary window
    params = self._create_scene_params()
    params.clearcolor = vec3(0.1, 0.12, 0.15)  # Slightly blue tint

    self.scene1 = lev2.scenegraph.Scene(params)
    self.layer1 = self.scene1.createLayer("std_forward")

    # Add a grid
    self.grid1_data = createGridData()
    self.grid1_data.modcolor = vec3(0.4, 0.6, 1.0)
    self.grid1_node = self.layer1.createDrawableNodeFromData("grid1", self.grid1_data)
    self.grid1_node.sortkey = 1

    # Add a spinning cube using primitive with pipeline
    self.cube1_prim = createCubePrim(ctx=ctx, size=1.0)
    self.cube1_prim.topColor = dvec4(0.2, 0.5, 1.0, 1.0)
    self.cube1_prim.bottomColor = dvec4(0.1, 0.3, 0.7, 1.0)
    pipeline_cube1 = createPipeline(app=self, ctx=ctx, rendermodel="FORWARD_PBR", techname="std_mono_fwd")
    self.cube1_node = self.cube1_prim.createNode("cube1", self.layer1, pipeline_cube1)
    self.cube1_node.sortkey = 2

    # Attach scene to viewport
    sgw = self.primary_sgv.widget
    sgw.scenegraph = self.scene1
    sgw.forkDB()
    sgw.evhandler = lambda e: self._onPrimaryUiEvent(e)
    sgw.ignoreEvents = False

    # Initialize lighting
    self.scene1.lightingmanager.gpuInit(ctx)

    # Setup camera for primary window
    self.camera1, self.uicam1 = setupUiCameraX(
      cameralut=self.cameralut1,
      camname="default",
      eye=vec3(3, 2, 3),
      tgt=vec3(0, 0, 0),
      up=vec3(0, 1, 0)
    )

  ##############################################

  def _setup_secondary_scene(self, ctx):
    # Create secondary window positioned beside primary
    sec_x = 100 + self.win_width + 20
    self.secondary_win = self.ezapp.createSecondaryWindow(
      width=self.win_width,
      height=self.win_height,
      x=sec_x,
      y=100,
      title="DualSceneGraph::Secondary",
      decorated=True,
      resizable=True
    )

    # Set up layout for secondary window
    uic = self.secondary_win.ui_context
    win_w = self.secondary_win.width
    win_h = self.secondary_win.height

    root = lev2.ui.LayoutGroup.create("sec_lg")
    root.setRect(0, 0, win_w, win_h)
    uic.top = root
    root.margin = 4

    # Add SceneGraphViewport to secondary window
    self.secondary_sgv = root.makeChild(
      uiclass=lev2.ui.SceneGraphViewport,
      args=["SecondarySG"],
      fill=True
    )

    # Create scene for secondary window
    params = self._create_scene_params()
    params.clearcolor = vec3(0.15, 0.1, 0.1)  # Slightly red tint

    self.scene2 = lev2.scenegraph.Scene(params)
    self.layer2 = self.scene2.createLayer("std_forward")

    # Add a grid with different color
    self.grid2_data = createGridData()
    self.grid2_data.modcolor = vec3(1.0, 0.5, 0.4)
    self.grid2_node = self.layer2.createDrawableNodeFromData("grid2", self.grid2_data)
    self.grid2_node.sortkey = 1

    # Add a spinning cube with different colors for secondary window
    self.cube2_prim = createCubePrim(ctx=ctx, size=1.0)
    self.cube2_prim.topColor = dvec4(1.0, 0.4, 0.2, 1.0)
    self.cube2_prim.bottomColor = dvec4(0.7, 0.2, 0.1, 1.0)
    pipeline_cube2 = createPipeline(app=self, ctx=ctx, rendermodel="FORWARD_PBR", techname="std_mono_fwd")
    self.cube2_node = self.cube2_prim.createNode("cube2", self.layer2, pipeline_cube2)
    self.cube2_node.sortkey = 2

    # Attach scene to viewport
    sgw = self.secondary_sgv.widget
    sgw.scenegraph = self.scene2
    sgw.forkDB()
    sgw.evhandler = lambda e: self._onSecondaryUiEvent(e)
    sgw.ignoreEvents = False

    # Initialize lighting
    self.scene2.lightingmanager.gpuInit(ctx)

    # Setup camera for secondary window
    self.camera2, self.uicam2 = setupUiCameraX(
      cameralut=self.cameralut2,
      camname="default",
      eye=vec3(-3, 2, -3),
      tgt=vec3(0, 0, 0),
      up=vec3(0, 1, 0)
    )

    print("Secondary window with scenegraph created")

  ##############################################

  def onGpuInit(self, ctx):
    print("Primary window GPU init")
    self._setup_primary_scene(ctx)
    self._setup_secondary_scene(ctx)

  ##############################################

  def _onPrimaryUiEvent(self, uievent):
    if self.uicam1 is None:
      return lev2.ui.HandlerResult()
    handled = self.uicam1.uiEventHandler(uievent)
    if handled:
      self.uicam1.updateMatrices()
      self.camera1.copyFrom(self.uicam1.cameradata)
    return lev2.ui.HandlerResult()

  ##############################################

  def _onSecondaryUiEvent(self, uievent):
    if self.uicam2 is None:
      return lev2.ui.HandlerResult()
    handled = self.uicam2.uiEventHandler(uievent)
    if handled:
      self.uicam2.updateMatrices()
      self.camera2.copyFrom(self.uicam2.cameradata)
    return lev2.ui.HandlerResult()

  ##############################################

  def onUpdate(self, updinfo):
    self.frame_count += 1
    self.rotation_angle += updinfo.deltatime * 0.5

    # Update cube transform in primary scene
    if hasattr(self, 'cube1_node'):
      rot = quat(vec3(0, 1, 0), self.rotation_angle)
      pos = vec3(0, 0.5, 0)
      self.cube1_node.updateTransformTRS(pos, rot, vec3(1))

    # Update cube transform in secondary scene (different rotation axis)
    if hasattr(self, 'cube2_node'):
      rot = quat(vec3(1, 0, 0), self.rotation_angle * 1.3)
      pos = vec3(0, 0.75, 0)
      self.cube2_node.updateTransformTRS(pos, rot, vec3(1))

    # Update both scenes
    if self.scene1:
      self.scene1.updateScene(self.cameralut1)
    if self.scene2:
      self.scene2.updateScene(self.cameralut2)

    # Mark viewports dirty
    if self.primary_sgv:
      self.primary_sgv.widget.setDirty()
    if self.secondary_sgv:
      self.secondary_sgv.widget.setDirty()

    # Auto-close after 5 seconds for automated testing
    if AUTO_CLOSE and self.frame_count > 300:
      print("Test complete - closing")
      self.ezapp.signalExit()

  ##############################################

  def onUiEvent(self, uievent):
    return lev2.ui.HandlerResult()

################################################################################

DualSceneGraphWindow().ezapp.mainThreadLoop()
print("Dual scenegraph window test passed!")
