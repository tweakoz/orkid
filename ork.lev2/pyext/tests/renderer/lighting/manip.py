#!/usr/bin/env ork.python

################################################################################
# lev2 sample which tests the manipulation gizmo system
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import math, random, argparse, sys
from orkengine.core import vec2, vec3, vec4, quat, mtx4
from orkengine.core import lev2_pyexdir, Transform
from orkengine.core import CrcStringProxy, thisdir, VarMap
from orkengine import lev2
from ork.app.application import ComponentizedApplication

################################################################################

lev2_pyexdir.addToSysPath()
from lev2utils.cameras import setupUiCamera
from lev2utils.primitives import createGridData

################################################################################
parser = argparse.ArgumentParser(description='manipulation gizmo test')
################################################################################
args = vars(parser.parse_args())
################################################################################
tokens = CrcStringProxy()

class MANIP_APP(ComponentizedApplication):

  def __init__(self):
    super().__init__(lui="yes")

    self.materials = set()
    self.createEzApp(ssaa=2, msaa=0, fullscreen=False)

  ##############################################

  def _onGpuInit(self, ctx):

    # Setup camera for mono mode
    setupUiCamera(app=self, eye=vec3(0, 5, 10))

    sceneparams = VarMap()
    sceneparams.SkyboxIntensity = float(1)
    sceneparams.SpecularIntensity = float(1)
    sceneparams.DiffuseIntensity = float(1)
    sceneparams.AmbientLight = vec3(0.1)
    sceneparams.DepthFogDistance = float(10000)
    sceneparams.preset = "ForwardPBR"

    self.scene = self.ezapp.createScene(sceneparams)
    self.layer_fwd = self.scene.createLayer("std_forward")

    ###################################
    # Create a simple model to manipulate
    ###################################

    model = lev2.XgmModel("data://tests/pbr_calib.glb")
    self.drawable_model = model.createDrawable()
    self.model_node = self.scene.createDrawableNodeOnLayers(
        [self.layer_fwd], "model-node", self.drawable_model)

    # Create a Transform for manipulation
    self.target_transform = Transform()
    self.target_transform.translation = vec3(0, 1, 0)
    self.target_transform.orientation = quat()
    self.target_transform.scale = 1.0

    # Link the node's transform to our target
    self.model_node.worldTransform = self.target_transform

    ###################################
    # Create ManipController and DecompTransformManipulator
    ###################################

    self.manip_controller = lev2.ManipController()
    self.manip_interface = lev2.DecompTransformManipulator(self.target_transform)
    self.manip_controller.target = self.manip_interface
    self.manip_controller.mode = lev2.ManipMode.TRANSLATE

    ###################################
    # Create ManipGizmo drawable (renders in scenegraph)
    ###################################

    self.gizmo_data = lev2.ManipGizmoDrawableData()
    self.gizmo_data.controller = self.manip_controller
    self.gizmo_drawable = self.gizmo_data.createDrawable()
    self.gizmo_node = self.scene.createDrawableNodeOnLayers(
        [self.layer_fwd], "manip-gizmo", self.gizmo_drawable)
    self.gizmo_node.sortkey = 999  # Render on top

    ###################################
    # Grid
    ###################################

    self.grid_data = createGridData()
    self.grid_data.shader_suffix = "_V4"
    self.grid_data.modcolor = vec3(1)
    self.grid_data.intensityA = 1
    self.grid_data.intensityB = .95
    self.grid_data.lineWidth = 0.05
    self.grid_drawable = self.grid_data.createDrawable()
    self.grid_node = self.scene.createDrawableNodeOnLayers(
        [self.layer_fwd], "grid-node", self.grid_drawable)
    self.grid_node.sortkey = 1

    ###################################

    print("Manipulation Test Ready")
    print("  T - Translate mode")
    print("  R - Rotate mode")
    print("  S - Scale mode")

    lmgr = self.scene.lightingmanager
    lmgr.gpuInit(ctx)

  ##############################################

  def _onUiEvent(self, uievent):
    # Check for mode switching keys
    if uievent.code == tokens.KEY_DOWN.hashed:
      if uievent.keycode == ord("T"):
        self.manip_controller.mode = lev2.ManipMode.TRANSLATE
        print("Mode: TRANSLATE")
        return lev2.ui.HandlerResult()
      elif uievent.keycode == ord("R"):
        self.manip_controller.mode = lev2.ManipMode.ROTATE
        print("Mode: ROTATE")
        return lev2.ui.HandlerResult()
      elif uievent.keycode == ord("S"):
        self.manip_controller.mode = lev2.ManipMode.SCALE
        print("Mode: SCALE")
        return lev2.ui.HandlerResult()

    # Camera handling
    handled = self.uicam.uiEventHandler(uievent)
    if handled:
      self.camera.copyFrom(self.uicam.cameradata)

    return lev2.ui.HandlerResult()

  ################################################

  def _onUpdate(self, updinfo):
    self.scene.updateScene(self.cameralut)

  ################################################

  def _onGpuUpdate(self, ctx):
    # Sync node transform from the manipulated DecompTransform
    pass

###############################################################################

MANIP_APP().ezapp.mainThreadLoop()
