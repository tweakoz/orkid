#!/usr/bin/env ork.python

import sys, argparse
from _lavalamp import LavalampComponent
from orkengine.core import vec3
from ork.app.application import ComponentizedApplication
from ork.app.std_scenegraph import StandardSceneGraphComponent
from ork.app.loggerui import LoggerUIComponent
from ork.app.frame_profiler import FrameProfilerComponent
################################################################################

parser = argparse.ArgumentParser()
parser.add_argument("-p", "--profiler", action="store_true", help="Use frame profiler instead of logger UI")
args = parser.parse_args()

################################################################################

class LavaLampApp(ComponentizedApplication):

  def __init__(self):
    super().__init__()
    ############################################
    self.SGC = self.addComponent("std_scenegraph",
                                 StandardSceneGraphComponent,
                                 eye=vec3(0,20,20),
                                 grid_variant=None)
    if args.profiler:
      self.addComponent("profiler", FrameProfilerComponent, gpu_filter=["*", "-fwd:total"])
    else:
      self.LUI = self.addComponent("loggerui", LoggerUIComponent, filter_regex=[".*"])
    self.LLA = self.addComponent("lavalamp", LavalampComponent)
    ############################################
    self.createEzApp(use_subsystems=['opq', 'core', 'gpu', 'lev2'])
  
  ###############################################################################

llapp = LavaLampApp()
llapp.ezapp.mainThreadLoop()
llapp.ezapp.shutdown()
