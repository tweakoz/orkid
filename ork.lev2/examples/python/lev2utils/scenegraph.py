from orkengine.core import *
from orkengine.lev2 import *

###############################################################################

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

###############################################################################

def canonicalizeSG(parent,SG, rendermodel):

  if rendermodel in ["ForwardPBR","FWDPBRVR"]:
    layer_name = "std_forward"
  elif rendermodel in ["DeferredPBR","PBRVR"]:
    layer_name = "std_deferred"
  else:
    print("invalid rendermodel<%s>" % rendermodel)
    assert(False)

  parent.layer1 = SG.createLayer(layer_name)
  parent.layer_std = parent.layer1
  parent.layer_dpp = SG.createLayer("depth_prepass")
  parent.std_layers = [parent.layer_std,parent.layer_dpp]
  parent.rendernode = SG.compositorrendernode

###############################################################################

def createSceneGraph( app=None, 
                      rendermodel = None,
                      params_dict = None,
                      layer_name = None):


  if rendermodel == None:
    rendermodel = "ForwardPBR"      
  elif rendermodel == "deferred":
    rendermodel = "DeferredPBR"
  elif rendermodel == "forward":
    rendermodel="ForwardPBR"

  sceneparams = VarMap()

  if params_dict == None:
    params_dict = VarMap()
    params_dict.SkyboxIntensity = 3.0
    params_dict.DiffuseIntensity = 1.0
    params_dict.SpecularIntensity = 1.0
    params_dict.AmbientLevel = vec3(0)
    params_dict.DepthFogDistance = 10000.0
  else:
    for k in params_dict.keys():
      if k == "preset":
        rendermodel = params_dict[k]
      sceneparams.__setattr__(k,params_dict[k])
      print("sceneparams<%s> = %s" % (k,params_dict[k]))

  
  sceneparams.preset = rendermodel

  app.scene = app.ezapp.createScene(sceneparams)
  
  if layer_name == None:
    if rendermodel in ["ForwardPBR","FWDPBRVR"]:
      layer_name = "std_forward"
    elif rendermodel in ["DeferredPBR","PBRVR"]:
      layer_name = "std_deferred"
    else:
      print("required layer name for rendermodel<%s>" % rendermodel)
      assert(False)
  
  app.layer1 = app.scene.createLayer(layer_name)
  app.layer_std = app.layer1
  app.layer_dpp = app.scene.createLayer("depth_prepass")
  app.std_layers = [app.layer_std,app.layer_dpp]
  app.rendernode = app.scene.compositorrendernode

  return app.scene