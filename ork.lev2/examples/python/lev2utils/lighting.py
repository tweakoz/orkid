import math 
from orkengine.core import *
from orkengine.lev2 import *

class MyCookie: 
  def __init__(self,path):
    self.path = path
    self.tex = Texture.load(path)
    self.irr = PbrCommon.requestIrradianceMaps(path)
    
class MySpotLight:
  def __init__( self,
                app=None,
                index=0,
                model=None,
                frq=1.0,
                color=vec3(1),
                cookie=None,
                fovbase=20.0,
                fovamp=20.0,
                voffset=1,
                vscale=1,
                bias=1e-3,
                dim=2048,
                range=100.0,
                radius=12,
                layers = None):
    
    if layers == None:
      if hasattr(app,"fwd_layers"):
        layers = app.fwd_layers
      
    self.radius = radius
    self.voffset = voffset
    self.vscale = vscale
    self.cookie = cookie
    self.frequency = frq
    self.fovamp = fovamp
    self.fovbase = fovbase
    self.drawable_model = model.createDrawable()
    self.modelnode = app.scene.createDrawableNodeOnLayers(layers,"model-node",self.drawable_model)
    self.modelnode.worldTransform.scale = 0.25
    self.modelnode.worldTransform.translation = vec3(0)
    self.spot_light = DynamicSpotLight()
    self.spot_light.data.color = color
    self.spot_light.data.fovy = math.radians(45)
    self.spot_light.lookAt(
      vec3(0,2,1)*4, # eye
      vec3(0,0,0), # tgt 
      vec3(0,1,0)) # up
    self.spot_light.data.range = range
    self.spot_light.data.shadowBias = bias
    self.spot_light.data.shadowMapSize = dim
    self.spot_light.cookieTexture = cookie.tex
    self.spot_light.irradianceCookie = cookie.irr
    self.spot_light.shadowCaster = True
    print(self.spot_light.shadowMatrix)
    self.lnode = app.layer_fwd.createLightNode("spotlight%d"%index,self.spot_light)
    pass
  def update(self,abstime):
    phase = abstime*self.frequency
    ########################################
    x = math.sin(phase)
    y = math.sin(phase*self.frequency*2.0)*self.vscale
    ty = math.sin(phase*2.0)
    z = math.cos(phase)
    fovy = self.fovbase+(1.0+math.sin(phase*3.5))*self.fovamp*0.5
    self.spot_light.data.fovy = math.radians(fovy)
    LPOS =       vec3(x*self.radius,self.voffset+y,z*self.radius)

    self.spot_light.lookAt(
      LPOS, # eye
      vec3(0,ty+1,0), # tgt 
      vec3(0,1,0)) # up
    
    self.modelnode.worldTransform.translation = LPOS
    self.modelnode.worldTransform.orientation = quat(vec3(1,1,1).normalized,phase*self.frequency*16)
