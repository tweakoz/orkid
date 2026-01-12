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
from ork.app.std_scenegraph import StandardSceneGraphComponent

################################################################################

lev2_pyexdir.addToSysPath()

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

    # Add standard scenegraph component
    self.SGC = self.addComponent("scenegraph", StandardSceneGraphComponent,
                                  eye=vec3(0, 5, 10),
                                  tgt=vec3(0, 0, 0),
                                  up=vec3(0, 1, 0))

    self.createEzApp(ssaa=2, msaa=0, fullscreen=False)

  ##############################################

  def _onGpuInit(self, ctx):
    SGC = self.SGC

    ###################################
    # Create a simple model to manipulate
    ###################################

    model = lev2.XgmModel("data://tests/pbr_calib.glb")
    self.drawable_model = model.createDrawable()
    self.model_node = SGC.scenegraph.createDrawableNodeOnLayers(
        [SGC.layer_fwd], "model-node", self.drawable_model)

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
    self.gizmo_node = SGC.scenegraph.createDrawableNodeOnLayers(
        [SGC.layer_fwd], "manip-gizmo", self.gizmo_drawable)
    self.gizmo_node.sortkey = 999  # Render on top

    ###################################

    print("Manipulation Test Ready")
    print("  T - Translate mode")
    print("  R - Rotate mode")
    print("  S - Scale mode")

  ##############################################

  def _onGpuLink(self, ctx):
    SGC = self.SGC
    # Bind ManipController to viewport for event handling (done in C++)
    SGC.SGVPW.bindManipController(self.manip_controller)

  ##############################################

  def _onUiEvent(self, uievent):
    # Check for mode switching keys (global, not routed through viewport)
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

    # Note: Camera and manip events are handled through the SceneGraphViewport
    return lev2.ui.HandlerResult()

###############################################################################

MANIP_APP().ezapp.mainThreadLoop()
