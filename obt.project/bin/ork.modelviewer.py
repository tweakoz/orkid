#!/usr/bin/env python3

################################################################################
# lev2 sample which renders a scenegraph, optionally in VR mode
# Copyright 1996-2020, Michael T. Mayers.
# Distributed under the Boost Software License - Version 1.0 - August 17, 2003
# see http://www.boost.org/LICENSE_1_0.txt
################################################################################

import math, random, argparse, sys, os
from orkengine.core import *
from orkengine.lev2 import *

################################################################################

sys.path.append((thisdir()/".."/".."/"ork.lev2"/"examples"/"python").normalized.as_string) # add parent dir to path
from common.cameras import *
from common.shaders import *
from common.primitives import createGridData
from common.scenegraph import createSceneGraph

################################################################################

parser = argparse.ArgumentParser(description='scenegraph example')
parser.add_argument('--vrmode', action="store_true", help='run in vr' )
parser.add_argument('--showgrid', action="store_true", help='show grid' )
parser.add_argument('--showskeleton', action="store_true", help='show skeleton' )
parser.add_argument("-f", '--forceregen', action="store_true", help='force asset regeneration' )
parser.add_argument('--forwardpbr', action="store_true", help='use forward pbr renderer' )
parser.add_argument("-m", "--model", type=str, required=False, default="data://tests/pbr1/pbr1", help='asset to load')
parser.add_argument("-i", "--lightintensity", type=float, default=1.0, help='light intensity')
parser.add_argument("-d", "--camdist", type=float, default=0.0, help='camera distance')
parser.add_argument("-e", "--envmap", type=str, default="", help='environment map')

################################################################################

args = vars(parser.parse_args())
vrmode = (args["vrmode"]==True)
showgrid = args["showgrid"]
modelpath = args["model"]
lightintens = args["lightintensity"]
camdist = args["camdist"]
fwdpbr = args["forwardpbr"]
envmap = args["envmap"]

if args["forceregen"]:
  os.environ["ORKID_LEV2_FORCE_MODEL_REGEN"] = "1"

if args["showskeleton"]:
  os.environ["ORKID_LEV2_SHOW_SKELETON"] = "1"


################################################################################

class SceneGraphApp(object):

  def __init__(self):
    super().__init__()
    self.ezapp = OrkEzApp.create(self)
    self.ezapp.setRefreshPolicy(RefreshFastest, 0)
    self.materials = set()
    setupUiCamera(app=self,eye=vec3(0,0.5,3))
    self.modelinsts=[]

  ##############################################

  def onGpuInit(self,ctx):

    params_dict = {
      "SkyboxIntensity": float(lightintens),
      "DiffuseIntensity": lightintens*float(1),
      "SpecularIntensity": lightintens*float(1),
      "depthFogDistance": float(10000),
    }

    if envmap != "":
      params_dict["SkyboxTexPathStr"] = envmap

    rendermodel = "DeferredPBR"
    if fwdpbr:
      rendermodel = "ForwardPBR"
    if vrmode:
      rendermodel = "PBRVR"

    createSceneGraph( app=self,
                      params_dict=params_dict,
                      rendermodel=rendermodel )

    self.model = Model(modelpath)
    self.sgnode = self.model.createNode("node",self.layer1)

    center = self.model.boundingCenter
    radius = self.model.boundingRadius*2.5

    if camdist!=0.0:
      radius = camdist

    self.uicam.lookAt( center-vec3(0,0,radius), 
                       center, 
                       vec3(0,1,0) )

    #self.uicam.base_zmoveamt = radius*0.01 

    self.camera.copyFrom( self.uicam.cameradata )

    ###################################

    if showgrid:
      self.grid_data = createGridData()
      self.grid_node = self.layer1.createGridNode("grid",self.grid_data)
      self.grid_node.sortkey = 1

  ##############################################

  def onUiEvent(self,uievent):
    handled = self.uicam.uiEventHandler(uievent)
    if handled:
      self.camera.copyFrom( self.uicam.cameradata )

  ################################################

  def onUpdate(self,updinfo):

    self.scene.updateScene(self.cameralut) 

###############################################################################

SceneGraphApp().ezapp.mainThreadLoop()
