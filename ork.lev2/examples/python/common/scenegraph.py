from orkengine.core import *
from orkengine.lev2 import *


def createSceneGraph( app=None, 
                      rendermodel = "ForwardPBR",
                      params_dict = None ):
    sceneparams = VarMap()
    sceneparams.preset = rendermodel

    if params_dict != None:
      for k in params_dict.keys():
        sceneparams.__setattr__(k,params_dict[k])
    app.scene = app.ezapp.createScene(sceneparams)
    app.layer1 = app.scene.createLayer("layer1")
