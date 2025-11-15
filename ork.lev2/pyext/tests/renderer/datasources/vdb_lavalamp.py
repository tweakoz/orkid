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

    ############################################
    self.SGC = self.addComponent("std_scenegraph", StandardSceneGraphComponent)
    self.LUI = self.addComponent("loggerui", LoggerUIComponent, filter_regex=[".*"]) 
    self.LLA = self.addComponent("lavalamp", LavalampComponent)
    ############################################
    self.createEzApp()
  
  ###############################################################################

llapp = LavaLampApp()
llapp.ezapp.mainThreadLoop(on_iter=lambda: None)
