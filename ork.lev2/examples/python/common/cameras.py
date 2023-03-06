from orkengine.core import *
from orkengine.lev2 import *

constants = mathconstants()

def setupUiCamera( app = None,
                   near = 0.1,
                   far = 100.0,
                   fov_deg = 45,
                   constrainZ = True,
                   eye = vec3(1,1,1),
                   tgt = vec3(0,0,0),
                   up = vec3(0,1,0) ):

  ###################################
  # rendering cam
  ###################################
  app.camera = CameraData()
  app.camera.perspective(near, far, fov_deg)
  app.cameralut = CameraDataLut()
  app.cameralut.addCamera("spawncam",app.camera)
  ###################################
  # ui cam
  ###################################
  app.uicam = EzUiCam()
  app.uicam.fov = fov_deg*constants.DTOR
  app.uicam.constrainZ = constrainZ
  ###################################
  # initial view
  ###################################
  app.uicam.lookAt( eye, tgt, up )
  ###################################
  app.camera.copyFrom( app.uicam.cameradata )
