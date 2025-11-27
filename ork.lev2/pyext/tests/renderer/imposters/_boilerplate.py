
import math, random, argparse, sys, signal
from orkengine.core import vec3, vec4, quat, mtx4, dfrustum, dvec4, fmtx4_to_dmtx4, CrcStringProxy
from orkengine.core import lev2_pyexdir, Transform, thisdir
from orkengine import lev2
from ork.app.application import ComponentizedApplication
from ork.app.std_scenegraph import StandardSceneGraphComponent, StdSpotLight
from ork.app.loggerui import LoggerUIComponent

################################################################################

lev2_pyexdir.addToSysPath()
this_dir = thisdir()
this_dir.addToSysPath()

from lev2utils.cameras import setupUiCamera
from lev2utils.primitives import createGridData, createImposter

tokens = CrcStringProxy()

################################################################################

class ImposterBaseApp(ComponentizedApplication):
  def __init__(self,
               ssaa=1,
               grid_data=None, 
               eye=vec3(0,12,15), 
               sg_params=None):
    super().__init__()

    self.addComponent( "SGC", 
                       StandardSceneGraphComponent, 
                       eye=eye, 
                       sg_params=sg_params, 
                       grid_data=grid_data )

    self.time = 0.0
    clazz_name = self.__class__.__name__
    self.createEzApp(name=clazz_name, ssaa=ssaa)
    
  ################################################

  def _onUpdate(self,updinfo):
    abstime = updinfo.absolutetime
    self.lighttime = abstime
    self.time = abstime

################################################################################

def run(clazz):
  parser = argparse.ArgumentParser(description='scenegraph example')
  parser.add_argument("-e", "--envmap", type=str, default="cold", help='environment map')
  ################################################################################
  args = vars(parser.parse_args())
  envmap = args["envmap"]
  the_app = clazz(envmap=envmap)
  print(the_app)
  the_app.ezapp.mainThreadLoop()
