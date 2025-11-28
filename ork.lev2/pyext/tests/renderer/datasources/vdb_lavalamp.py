#!/usr/bin/env ork.python

import sys
from _lavalamp import LavalampComponent
from orkengine.core import vec3
from ork.app.application import ComponentizedApplication
from ork.app.std_scenegraph import StandardSceneGraphComponent
from ork.app.loggerui import LoggerUIComponent
################################################################################

class LavaLampApp(ComponentizedApplication):

  def __init__(self):
    super().__init__()
    ############################################
    self.SGC = self.addComponent("std_scenegraph", 
                                 StandardSceneGraphComponent,
                                 eye=vec3(0,20,20) )
    self.LUI = self.addComponent("loggerui", LoggerUIComponent, filter_regex=[".*"]) 
    self.LLA = self.addComponent("lavalamp", LavalampComponent)
    ############################################
    self.createEzApp()
  
  ###############################################################################

llapp = LavaLampApp()
llapp.ezapp.mainThreadLoop(on_iter=lambda: None)
