#!/usr/bin/env ork.python

import sys
from ork import path as ork_path
from orkengine.core import vec3, vec4, lev2_pyexdir, VarMap
from orkengine import lev2
#sys.path.append(str(ork_path.py_lev2utils)) # add parent dir to path
#lev2_pyexdir.addToSysPath()
#from scenegraph import createSceneGraph
from _lavalamp import LavalampComponent
from ork.app.application import ComponentizedApplication
from ork.app.std_scenegraph import StandardSceneGraphComponent
from ork.app.loggerui import LoggerUIComponent
################################################################################

class LavaLampApp(ComponentizedApplication):

  def __init__(self):
    super().__init__()

    self.addComponent("loggerui", LoggerUIComponent, filter_regex=[".*"]) 
    self.sg_component = self.addComponent("std_scenegraph", StandardSceneGraphComponent)

    # Add lavalamp component
    #self.lavalamp = self.addComponent("lavalamp", LavalampComponent)

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
    ###########################
    lg_group = self.ezapp.topLayoutGroup
    self.griditems = lg_group.makeGrid(
      width=1,
      height=1,
      margin = 4,
      uiclass = lev2.ui.SceneGraphViewport,
      args = ["label",vec4(0.1,0.1,0.3,1)],
    )
    ###########################
    SGC = self.sg_component
    SG = SGC.scenegraph
    SGVP = self.griditems[0]
    SGVPW = SGVP.widget
    SGVPW.cameraName = SGC.camname
    SGVPW.scenegraph = SG
    #SGVPW.forkDB()
    ###########################
    self.SGVP = SGVP 

  ################################################

  def _onUpdate(self, updinfo):
    self.SGVP.widget.setDirty()

  
  ###############################################################################

llapp = LavaLampApp()
llapp.ezapp.mainThreadLoop(on_iter=lambda: None)
