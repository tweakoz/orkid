from orkengine.core import *
from orkengine.lev2 import *

print("AAA")
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

print("BBB")

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


class UiWanderingCameraPanel:

  def __init__(self,cameralut=None,camname=None):

    self.camera, self.uicam = setupUiCameraX( cameralut=cameralut,
                                              camname=camname)

    self.cur_eye = vec3(0,0,0)
    self.cur_tgt = vec3(0,0,1)
    self.dst_eye = vec3(0,0,0)
    self.dst_tgt = vec3(0,0,0)
    self.counter = 0

  def update(self):
    import random
    def genpos():
      r = vec3(0)
      r.x = random.uniform(-10,10)
      r.z = random.uniform(-10,10)
      r.y = random.uniform(  0,10)
      return r 
    
    if self.counter<=0:
      self.counter = int(random.uniform(1,1000))
      self.dst_eye = genpos()
      self.dst_tgt = vec3(0,random.uniform(  0,2),0)

    self.cur_eye = self.cur_eye*0.9995 + self.dst_eye*0.0005
    self.cur_tgt = self.cur_tgt*0.9995 + self.dst_tgt*0.0005
    self.uicam.distance = 1
    self.uicam.lookAt( self.cur_eye,
                       self.cur_tgt,
                       vec3(0,1,0))

    self.counter = self.counter-1

    self.uicam.updateMatrices()

    self.camera.copyFrom( self.uicam.cameradata )

print("CCC")
