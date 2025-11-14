#!/usr/bin/env ork.python

import sys
from ork import path as ork_path
from orkengine.core import vec3, lev2_pyexdir
sys.path.append(str(ork_path.py_lev2utils)) # add parent dir to path
lev2_pyexdir.addToSysPath()
from cameras import *
from primitives import createGridData
from scenegraph import createSceneGraph
from _lavalamp import LavalampComponent
from ork.app.application import ComponentizedApplication
from ork.app.loggerui import LoggerUIComponent
################################################################################

class PointsPrimApp(ComponentizedApplication):

  def __init__(self):
    super().__init__()

    self.materials = set()

    # Add lavalamp component
    self.lavalamp = self.addComponent("lavalamp", LavalampComponent)
    self.addComponent("loggerui", LoggerUIComponent, filter_regex=[".*"]) 

    ############################################
    # Configure EzApp creation args
    ############################################

    self.ezapp_args = {
      'ssaa': 1
    }
    self.createEzApp()

  ################################################
  # GPU initialization - called after component onGpuInit
  ################################################

  def _onGpuInit(self, ctx):
    """Initialize scene graph"""

    ###################################
    # create scenegraph
    ###################################

    sg_params = {
      "SkyboxIntensity": 1.0,
      "DiffuseIntensity": 6.0,
    }

    createSceneGraph(app=self, rendermodel="ForwardPBR", params_dict=sg_params)
    setupUiCamera(app=self, eye=vec3(6,6,6), constrainZ=True, up=vec3(0,1,0))

    ###################################
    # create grid
    ###################################

    self.grid_data = createGridData()
    self.grid_node = self.layer1.createDrawableNodeFromData("grid", self.grid_data)
    self.grid_node.sortkey = 1

###############################################################################

PointsPrimApp().ezapp.mainThreadLoop(on_iter=lambda: None)
