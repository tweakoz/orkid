#!/usr/bin/env ork.python

################################################################################
# lev2 sample: Recursive Fullscreen 3D Mouse Mode Test
# Demonstrates nested SceneGraphViewport inside embedded UI surface
# Run with: ./test_fsmouse_recursive.py
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import math, sys
from orkengine.core import vec3, vec4, quat, CrcStringProxy, VarMap, lev2_pyexdir
from orkengine import lev2
from orkengine.lev2 import CameraDataLut
from ork.app.application import ComponentizedApplication
from ork.app.std_scenegraph import StandardSceneGraphComponent

lev2_pyexdir.addToSysPath()
from lev2utils.cameras import setupUiCameraX
from lev2utils.primitives import createGridData, createCubePrim
from lev2utils.shaders import createPipeline

################################################################################

tokens = CrcStringProxy()

################################################################################

class RecursiveFsmouseApp(ComponentizedApplication):

  def __init__(self):
    super().__init__()
    self.SGC = self.addComponent("std_scenegraph",
                                 StandardSceneGraphComponent,
                                 grid_variant="_V4",
                                 eye=vec3(0, 3, 8))
    # Run in fullscreen mode with fsmouse enabled (hide HW cursor, render virtual)
    self.createEzApp(name="RecursiveFsmouse", ssaa=1, fullscreen=True, fsmouse=True)

  ##############################################

  def _onUiInit(self):
    # Create LayoutSurface (512x512 pixels)
    self.layout_surface = lev2.ui.LayoutSurface("nested_sgvp_surface", w=512, h=512, margin=8)
    self.layout_surface.setVirtualSize(512, 512)

    # Add a 1x1 grid with a SceneGraphViewport
    lg = self.layout_surface.layoutGroup
    self.nested_items = lg.makeGrid(
      width=1,
      height=1,
      margin=8,
      uiclass=lev2.ui.SceneGraphViewport,
      args=["NestedSGVP", vec4(0.2, 0.3, 0.4, 1)]
    )
    self.nested_sgvp = self.nested_items[0].widget
    lg.clearColorGuide = vec4(0.8, 0.6, 0.2, 1)

  ##############################################

  def _onGpuInit(self, ctx):
    SGC = self.SGC
    SG = SGC.scenegraph

    ############################################
    # Create shared geometry for nested scenegraph
    ############################################

    self.nested_grid_data = createGridData()
    self.nested_cube_prim = createCubePrim(ctx=ctx, size=1.0)
    self.nested_pipeline = createPipeline(app=self, ctx=ctx, rendermodel="FORWARD_PBR", techname="std_mono_fwd")

    ############################################
    # Set up the nested scenegraph
    ############################################

    nested_sg_params = VarMap()
    nested_sg_params.SkyboxIntensity = 1.0
    nested_sg_params.DiffuseIntensity = 1.0
    nested_sg_params.SpecularIntensity = 1.0
    nested_sg_params.AmbientLevel = vec3(0.2)
    nested_sg_params.preset = "ForwardPBR"
    nested_sg_params.ssaa = 2
    nested_sg_params.enable_skybox = True
    nested_sg_params.clearcolor = vec3(0.1, 0.1, 0.5)
    nested_sg_params.SkyboxTexPathStr = "nebula"

    self.nested_sg = lev2.scenegraph.Scene(nested_sg_params)
    self.nested_layer = self.nested_sg.createLayer("std_forward")

    # Create grid in nested scene
    self.nested_grid_node = self.nested_layer.createDrawableNodeFromData("nested_grid", self.nested_grid_data)
    self.nested_grid_node.sortkey = 1

    # Create cube in nested scene
    self.nested_cube_node = self.nested_cube_prim.createNode("nested_cube", self.nested_layer, self.nested_pipeline)
    self.nested_cube_node.worldTransform.translation = vec3(0, 0.5, 0)

    # Initialize nested lighting
    self.nested_sg.lightingmanager.gpuInit(ctx)

    ############################################
    # Setup camera for nested viewport
    ############################################

    self.nested_camname = "nested_cam"
    self.nested_cameralut = CameraDataLut()
    self.nested_camera, self.nested_uicam = setupUiCameraX(
      cameralut=self.nested_cameralut,
      camname=self.nested_camname,
      eye=vec3(0, 2, 5),
      tgt=vec3(0, 0, 0),
      up=vec3(0, 1, 0),
      far=1000.0
    )

    ############################################
    # Connect nested SGVP to nested scenegraph
    ############################################

    self.nested_sgvp.cameraName = self.nested_camname
    self.nested_sgvp.scenegraph = self.nested_sg
    self.nested_sgvp.camera_evhandler = lambda ev: self._onNestedCameraEvent(ev)
    self.nested_sgvp.forkDB()

    ############################################
    # Create the outer UI surface node
    ############################################

    self.ui_prim_data = lev2.UISurfacePrimitiveData()
    self.ui_prim_data.size = 1.0
    self.ui_prim_data.blendMode = tokens.ADDITIVE
    self.ui_prim_data.doubleSided = True
    self.ui_prim_data.max_samples_per_axis = 8

    self.ui_drawable = self.ui_prim_data.createDrawable(surface=self.layout_surface)
    self.ui_node = SG.createDrawableNodeOnLayers(
      SGC.fwd_layers,
      "uisurface-node",
      self.ui_drawable
    )
    self.ui_node.sortkey = 100
    self.ui_node.worldTransform.translation = vec3(+4, 2, -8)
    self.ui_node.view_relative = True

  ##############################################

  def _onNestedCameraEvent(self, uievent):
    handled = self.nested_uicam.uiEventHandler(uievent)
    if handled:
      self.nested_uicam.updateMatrices()
      self.nested_camera.copyFrom(self.nested_uicam.cameradata)
    return lev2.ui.HandlerResult()

  ##############################################

  def _onUpdate(self, updevent):
    # Update nested scenegraph
    self.nested_sg.updateScene(self.nested_cameralut)
    self.nested_sgvp.setDirty()

    # Spin the cube
    abstime = updevent.absolutetime
    self.nested_cube_node.worldTransform.orientation = quat(vec3(0, 1, 0), abstime)

###############################################################################

RecursiveFsmouseApp().ezapp.mainThreadLoop()
