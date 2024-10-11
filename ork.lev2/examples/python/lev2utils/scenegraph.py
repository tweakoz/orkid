from orkengine.core import *
from orkengine.lev2 import *


def createSceneGraph( app=None, 
                      rendermodel = "ForwardPBR", # DEPRECATED
                      params_dict = None,
                      layer_name = "All"):

    sceneparams = VarMap()
    sceneparams.preset = rendermodel

    if params_dict != None:
      for k in params_dict.keys():
        sceneparams.__setattr__(k,params_dict[k])
        if k == "preset":
          if params_dict[k] == "DeferredPBR":
            rendermodel = "DeferredPBR"
          elif params_dict[k] == "ForwardPBR":
            rendermodel = "ForwardPBR"
          else:
            assert(False) # unknown preset

    app.scene = app.ezapp.createScene(sceneparams)
    
    if rendermodel in ["ForwardPBR","FWDPBRVR"]:
      layer_name = "std_forward"
    
    app.layer1 = app.scene.createLayer(layer_name)
    app.rendernode = app.scene.compositorrendernode

    return app.scene