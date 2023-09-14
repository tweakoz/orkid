#!/usr/bin/env python3

################################################################################
# lev2 sample which renders a scenegraph to a window
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import math, sys, os, random, numpy, argparse
from obt import path
from orkengine.core import *
from orkengine.lev2 import *
l2exdir = (lev2exdir()/"python").normalized.as_string
sys.path.append(l2exdir) # add parent dir to path
from common.cameras import *
from common.primitives import createParticleData
from common.scenegraph import createSceneGraph

################################################################################
parser = argparse.ArgumentParser(description='scenegraph particles example')
parser.add_argument("-i", "--inputorjfile",type=str, default="", help='load particle orj file')
args = vars(parser.parse_args())
inputorjfile = args["inputorjfile"]
orjfile = path.Path(inputorjfile)
print(orjfile,inputorjfile)
use_orjfile = len(inputorjfile) and orjfile.exists()
################################################################################

class ParticlesApp(object):

  def __init__(self):
    super().__init__()

    self.materials = set()

    self.ezapp = OrkEzApp.create(self, left=100, top=100, width=900, height=900)
    self.ezapp.setRefreshPolicy(RefreshFastest, 0)
    self.ezapp.topWidget.enableUiDraw()
    lg_group = self.ezapp.topLayoutGroup
    self.griditems = lg_group.makeGrid( width = 2,
                                        height = 1,
                                        margin = 1,
                                        uiclass = ui.SceneGraphViewport,
                                        args = ["SGVP",vec4(1,0,1,1)] )

    ################################################
    # set vertical proportional layout guide 
    ################################################

    vguides = lg_group.vertical_guides
    vguides[1].proportion = 0.35
    vguides[2].proportion = 0.35

    ################################################
    # replace left viewport with particle editor
    ################################################

    self.objmodel = ui.ObjModel()
    self.ged_item = lg_group.makeChild( uiclass = ui.GedSurface,
                                        args = ["GEDSURF",self.objmodel] )
    lg_group.replaceChild(self.griditems[0].layout, self.ged_item)
    self.ged_surf = self.ged_item.widget

    ################################################
    # camera / event handler
    ################################################

    setupUiCamera( app=self, eye = vec3(0,0,30), constrainZ=True, up=vec3(0,1,0))
    self.griditems[1].widget.evhandler = lambda x: self.onSceneGraphUiEvent(x)

  ################################################
  # scenegraph viewport UI event handler
  ################################################

  def onSceneGraphUiEvent(self, uievent):
    handled = self.uicam.uiEventHandler(uievent)
    if handled:
      self.uicam.updateMatrices()
      self.camera.copyFrom( self.uicam.cameradata )
      #print(self.uicam.cameradata.eye )
    return ui.HandlerResult()

  ##############################################

  def onGpuInit(self,ctx):

    ###################################
    # create scenegraph
    ###################################

    sg_params = VarMap()
    sg_params.SkyboxIntensity = 3.0
    sg_params.DiffuseIntensity = 1.0
    sg_params.SpecularIntensity = 1.0
    sg_params.AmbientLevel = vec3(.125)
    sg_params.preset = "ForwardPBR"

    self.scenegraph = scenegraph.Scene(sg_params)
    self.layer = self.scenegraph.createLayer("layer")
    self.griditems[1].widget.scenegraph = self.scenegraph
    self.griditems[1].widget.forkDB()

    ###################################
    # create particle drawable 
    ###################################

    if use_orjfile:
      class ImplObject(object):
        def __init__(self):
          super().__init__()
          with open(str(orjfile), "r") as f:
            as_json = f.read()
            print(as_json)
            self.graphdata = Object.deserializeJson(as_json)
            print(self.graphdata)
            self.drawable_data = ParticlesDrawableData()
            self.drawable_data.graphdata = self.graphdata
      self.ptc_data = ImplObject()
    else:
      self.ptc_data = createParticleData()

    ptc_drawable = self.ptc_data.drawable_data.createDrawable()

    ##################
    # create particle sg node
    ##################

    self.particlenode = self.layer.createDrawableNode("particle-node",ptc_drawable)
    self.particlenode.sortkey = 2;

    ##################
    # attach particle graph to editor
    ##################

    self.objmodel.attach(self.ptc_data.graphdata, True)

  ################################################

  def onUpdate(self,updinfo):
    self.scenegraph.updateScene(self.cameralut) # update and enqueue all scenenodes
    self.griditems[1].widget.setDirty()

  ##############################################

  def onUiEvent(self,uievent):
    return ui.HandlerResult()

###############################################################################

ParticlesApp().ezapp.mainThreadLoop()
