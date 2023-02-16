from orkengine.core import *
from orkengine.lev2 import *


def createSceneGraph( app=None, rendermodel = "ForwardPBR" ):
    sceneparams = VarMap()
    sceneparams.preset = rendermodel
    app.scene = app.ezapp.createScene(sceneparams)
    app.layer1 = app.scene.createLayer("layer1")
