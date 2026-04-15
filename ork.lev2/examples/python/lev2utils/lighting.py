import math 
from orkengine.core import *
from orkengine.lev2 import *

class MyCookie: 
  def __init__(self,path):
    self.path = path
    self.tex = Texture.load(path)
    self.irr = PbrCommon.requestRadianceMaps(path)    
class MySpotLight:
  def __init__( self,
                app=None,
                index=0,
                model=None,
                frq=1.0,
                color=vec3(1),
                cookie=None,
                depth_cookie=None,
                fovbase=20.0,
                fovamp=20.0,
                voffset=1,
                vscale=1,
                bias=1e-5,
                dim=2048,
                range=100.0,
                radius=12,
                layers = None,
                stationary=False,
                eye=None,
                tgt=None):

    if layers == None:
      if hasattr(app,"layer_fwd"):
        layers = [app.layer_fwd]

    self.radius = radius
    self.voffset = voffset
    self.vscale = vscale
    self.frequency = frq
    self.fovamp = fovamp
    self.fovbase = fovbase
    self.stationary = stationary
    # model=None → no visible marker geometry for this light. The light
    # still exists, just no scene-graph node is created for its body.
    if model is not None:
      self.drawable_model = model.createDrawable()
      self.modelnode = app.scene.createDrawableNodeOnLayers(layers,"model-node",self.drawable_model)
      self.modelnode.worldTransform.scale = 0.25
      self.modelnode.worldTransform.translation = vec3(0)
    else:
      self.drawable_model = None
      self.modelnode      = None
    self.spot_light = DynamicSpotLight()
    self.spot_light.data.color = color
    self.spot_light.data.fovy = fovbase if stationary else 45
    # range/bias/shadowMapSize must be set BEFORE the initial lookAt,
    # since lookAt samples getRange()/getFovy() to build the projection
    # matrix. In the non-stationary path update() calls lookAt every
    # frame, which masks the ordering; stationary mode only calls it
    # once here, so stale defaults would stick.
    self.spot_light.data.range = range
    self.spot_light.data.shadowBias = bias
    self.spot_light.data.shadowMapSize = dim
    init_eye = eye if eye is not None else vec3(0,2,1)*4
    init_tgt = tgt if tgt is not None else vec3(0,0,0)
    self.spot_light.lookAt(
      init_eye, # eye
      init_tgt, # tgt
      vec3(0,1,0)) # up
    if stationary and self.modelnode is not None:
      self.modelnode.worldTransform.translation = init_eye
    self.spot_light.colorCookie = cookie
    self.spot_light.depthCookie = depth_cookie
    #self.spot_light.RadianceCookie = cookie.irr
    self.spot_light.shadowCaster = True
    #print(self.spot_light.shadowMatrix)
    self.lnode = app.layer_fwd.createLightNode("spotlight%d"%index,self.spot_light)
    pass
  def update(self,abstime):
    if self.stationary:
      return
    phase = abstime*self.frequency
    ########################################
    x = math.sin(phase)
    y = math.sin(phase*self.frequency*2.0)*self.vscale
    ty = math.sin(phase*2.0)
    z = math.cos(phase)
    fovy = self.fovbase+(1.0+math.sin(phase*3.5))*self.fovamp*0.5
    self.spot_light.data.fovy = fovy
    LPOS =       vec3(x*self.radius,self.voffset+y,z*self.radius)

    self.spot_light.lookAt(
      LPOS, # eye
      vec3(0,ty+1,0), # tgt 
      vec3(0,1,0)) # up
    
    if self.modelnode is not None:
      self.modelnode.worldTransform.translation = LPOS
      self.modelnode.worldTransform.orientation = quat(vec3(1,1,1).normalized,phase*self.frequency*16)
