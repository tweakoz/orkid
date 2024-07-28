from orkengine.core import *
from orkengine.lev2 import *


def createParams( rendermodel = "ForwardPBR" ):

  sceneparams = VarMap()
  sceneparams.preset = rendermodel
  sceneparams.SkyboxIntensity = float(1)
  sceneparams.SpecularIntensity = float(1)
  sceneparams.DiffuseIntensity = float(1)
  sceneparams.AmbientLight = vec3(0.0)
  sceneparams.DepthFogDistance = float(1e6)
  sceneparams.SkyboxTexPathStr = "src://envmaps/tozenv_nebula"

  if rendermodel == "DeferredPBR":
    sceneparams.layers = ["std_deferred","depth_prepass"]
  elif rendermodel == "ForwardPBR":
    sceneparams.layers = ["std_forward","depth_prepass"]    

  return sceneparams

def createSceneGraph( app=None, 
                      rendermodel = "ForwardPBR",
                      params_dict = None,
                      layer_name = None):
    sceneparams = VarMap()
    sceneparams.preset = rendermodel

    if params_dict != None:
      for k in params_dict.keys():
        sceneparams.__setattr__(k,params_dict[k])
    app.scene = app.ezapp.createScene(sceneparams)
    
    if layer_name == None:
      if rendermodel in ["ForwardPBR","FWDPBRVR"]:
        layer_name = "std_forward"
      elif rendermodel in ["DeferredPBR","PBRVR"]:
        layer_name = "std_deferred"
    
    app.layer1 = app.scene.createLayer(layer_name)
    app.rendernode = app.scene.compositorrendernode

    return app.scene