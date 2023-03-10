from orkengine.core import *
from orkengine.lev2 import *

constants = mathconstants()

def setupUiCameraX( near = 0.1,
                    far = 100.0,
                    fov_deg = 45,
                    constrainZ = True,
                    cameralut=None,
                    camname="spawncam",
                    eye = vec3(1,1,1),
                    tgt = vec3(0,0,0),
                    up = vec3(0,1,0) ):

  ###################################
  # rendering cam
  ###################################
  camera = CameraData()
  camera.perspective(near, far, fov_deg)
  cameralut.addCamera(camname,camera)
  ###################################
  # ui cam
  ###################################
  uicam = EzUiCam()
  uicam.fov = fov_deg*constants.DTOR
  uicam.constrainZ = constrainZ
  ###################################
  # initial view
  ###################################
  uicam.lookAt( eye, tgt, up )
  ###################################
  camera.copyFrom( uicam.cameradata )

  return camera, uicam

###########################################################3

def setupUiCamera( app = None,
                   near = 0.1,
                   far = 100.0,
                   fov_deg = 45,
                   constrainZ = True,
                   eye = vec3(1,1,1),
                   tgt = vec3(0,0,0),
                   up = vec3(0,1,0) ):

  cameralut = CameraDataLut()
  camera, uicam = setupUiCameraX(near=near,
                                 far=far,
                                 fov_deg=fov_deg,
                                 constrainZ=constrainZ,
                                 eye=eye,
                                 tgt=tgt,
                                 up=up,
                                 cameralut=cameralut)
  app.camera = camera
  app.cameralut = cameralut
  app.uicam = uicam


