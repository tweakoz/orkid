#!/usr/bin/env ork.python

import math, sys, random, threading, time, signal
import numpy as np
from collections import deque
from obt import path as obt_path
from ork import path as ork_path
from orkengine.core import vec2,vec3,CrcStringProxy, lev2_pyexdir, Logger
from orkengine.lev2 import vdb as ork_vdb, ui, primitives, RigidPrimitive, meshutil, MicroMesh
sys.path.append(str(ork_path.py_lev2utils)) # add parent dir to path
lev2_pyexdir.addToSysPath()
from cameras import *
from shaders import POINTCLOUD_SHADERTEXT, createPipeline, pseudowire_pipeline
from primitives import createPointsPrimV12C4, createGridData
from scenegraph import createSceneGraph
from _lavalamp import LavalampComponent
from ork.app.application import ComponentizedApplication

tokens = CrcStringProxy()
################################################################################

class PointsPrimApp(ComponentizedApplication):

  def __init__(self):
    super().__init__()

    self.materials = set()

    # Add lavalamp component
    self.lavalamp = self.addComponent("lavalamp", LavalampComponent)

    ############################################
    # Configure EzApp creation args
    ############################################

    self.ezapp_args = {
      'msaa': 1
    }

    ############################################
    # Create EzApp and initialize
    ############################################

    self.createEzApp()

  ################################################
  # App-level initialization after component init
  ################################################

  def _onAppLink(self):
    """Setup UI camera after app is initialized"""
    setupUiCamera(app=self, eye=vec3(6,6,6), constrainZ=True, up=vec3(0,1,0))

  ################################################
  # GPU initialization - called after component onGpuInit
  ################################################

  def _onGpuInit(self, ctx):
    """Initialize scene graph and connect component primitives"""

    ###################################
    # create scenegraph
    ###################################

    sg_params = {
      "SkyboxIntensity": 1.0,
      "DiffuseIntensity": 6.0,
    }

    createSceneGraph(app=self, rendermodel="ForwardPBR", params_dict=sg_params)

    ###################################
    # create grid
    ###################################

    self.grid_data = createGridData()
    self.grid_node = self.layer1.createDrawableNodeFromData("grid", self.grid_data)
    self.grid_node.sortkey = 1

    ###################################
    # create mesh node from component's primitive
    ###################################

    self.mesh_node = self.lavalamp.mesh_prim.createNode("mesh-node", self.layer1, self.lavalamp.mesh_pipe)

    ##################
    # create shading pipeline for points
    ##################

    pipeline = createPipeline(app=self,
                              ctx=ctx,
                              shadertext=POINTCLOUD_SHADERTEXT,
                              blending=tokens.OFF,
                              depthtest=tokens.LESS,
                              techname="tek_points_fwd",
                              rendermodel="ForwardPBR")

    pointsize_param = pipeline.sharedMaterial.param("pointsize")
    pipeline.bindParam(pointsize_param, 1.0)  # set pointsize

    ##################
    # create points sg node from component's primitive
    ##################

    self.primnode = self.lavalamp.points_prim.createNode("node1", self.layer1, pipeline)
    self.primnode.sortkey = 2

  ################################################
  # Update loop
  ################################################

  def _onUpdate(self, updinfo):
    """Update scene graph"""
    self.abstime = updinfo.absolutetime
    self.scene.updateScene(self.cameralut)  # update and enqueue all scenenodes

  ##############################################
  # UI event handling
  ##############################################

  def onUiEvent(self, uievent):
    """Handle UI events"""
    handled = self.uicam.uiEventHandler(uievent)
    if handled:
      self.camera.copyFrom(self.uicam.cameradata)
    return ui.HandlerResult()

###############################################################################

def onRunLoopIteration():
  pass

###############################################################################

PointsPrimApp().ezapp.mainThreadLoop(on_iter=onRunLoopIteration)
