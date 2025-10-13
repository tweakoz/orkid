
import math, random, argparse, sys, signal
from orkengine.core import vec3, vec4, quat, mtx4, dfrustum, dvec4, fmtx4_to_dmtx4, CrcStringProxy
from orkengine.core import lev2_pyexdir, Transform, thisdir
from orkengine import lev2

################################################################################

lev2_pyexdir.addToSysPath()
this_dir = thisdir()
this_dir.addToSysPath()

from lev2utils.cameras import setupUiCamera
from lev2utils.primitives import createGridData, createImposter
from lev2utils.scenegraph import createSceneGraph
from lev2utils.lighting import MySpotLight, MyCookie

tokens = CrcStringProxy()

################################################################################

class ImposterBaseApp(object):
  def __init__(self,is_stereo=False,extapp=None,envmap="cold",statedebug=False):
    super().__init__()
    self.statedebug = statedebug
    self.envmap = envmap
    
    if extapp==None:
      self.ezapp = lev2.OrkEzApp.create(self,ssaa=3,fullscreen=False)
      self.ezapp.setRefreshPolicy(lev2.RefreshFastest, 0)
    else:
      self.ezapp = extapp.ezapp
    #self.materials = set()
    self.extapp = extapp
    setupUiCamera(app=self,eye=vec3(0,-12,15))
    self.is_stereo = is_stereo
    self.time = 0.0
    
    self.RENDERMODEL = "FWDPBRVRDM" if is_stereo else "ForwardPBR"

    def onCtrlC(signum, frame):
      print("signalling EXIT to ezapp")
      self.ezapp.signalExit()

    signal.signal(signal.SIGINT, onCtrlC)

  ################################################

  def onGpuInit(self,ctx):
    #super().onGpuInit(ctx)
    if self.is_stereo and (self.extapp==None):
      self.vrdev = lev2.orkidvr.novr_device()
      self.vrdev.camera = "vrcam"
      self.vrdev.width = 1280
      self.vrdev.height = 1280
      self.vrdev.FOVD = 90

  ################################################

  def onUpdate(self,updinfo):
    abstime = updinfo.absolutetime
    self.lighttime = abstime
    self.time = abstime
    #########################
    if (self.extapp==None):
      if self.is_stereo:
        x = math.sin(abstime*0.1)
        z = -math.cos(abstime*0.1)
        xf_hmd = mtx4.lookAt( vec3(x,0.5,z)*4.0,  # eye
                              vec3(0,0,0),        # tgt
                              vec3(0,1,0))        # up
        self.vrdev.setPoseMatrix("hmd",xf_hmd)
    self.scene.updateScene(self.cameralut) 

  ################################################

  def onUiEvent(self,uievent):
    res = lev2.ui.HandlerResult()
    handled = self.uicam.uiEventHandler(uievent)
    if handled:
      self.camera.copyFrom( self.uicam.cameradata )
    return res


################################################################################

def run(clazz):
  parser = argparse.ArgumentParser(description='scenegraph example')
  parser.add_argument("-s", "--stereo", action="store_true", help='enable stereo rendering')
  parser.add_argument('-d', '--stateDebugger', action="store_true", help='Graphics state debugger')
  parser.add_argument("-e", "--envmap", type=str, default="cold", help='environment map')
  ################################################################################
  args = vars(parser.parse_args())
  is_stereo = args["stereo"]
  envmap = args["envmap"]
  statedebug = args["stateDebugger"]

  clazz(is_stereo=is_stereo,envmap=envmap,statedebug=statedebug).ezapp.mainThreadLoop()
