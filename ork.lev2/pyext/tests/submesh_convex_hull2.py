#!/usr/bin/env python3
################################################################################
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################
import math, random, argparse, sys, time
from threading import Lock
from orkengine.core import *
from orkengine.lev2 import *
from _boilerplate import *
import math, sys, os, random, numpy

################################################################################

class SceneGraphApp(BasicUiCamSgApp):

  ##############################################
  def __init__(self):
    super().__init__()
    self.mutex = Lock()
    self.uicam.lookAt( vec3(0,0,20), vec3(0,0,0), vec3(0,1,0) )
    self.camera.copyFrom( self.uicam.cameradata )
    self.NUMPOINTS = 64
    self.pnt = [vec3(0) for i in range(self.NUMPOINTS)]
    self.numsteps = 0
  ##############################################
  def updatePoints(self,abstime):
    paramA = 4+math.sin(abstime*0.2)*4
    paramB = 1+math.sin(abstime*0.23)*0.15
    paramC = 1+math.sin(abstime*0.27)*0.05
    paramD = 1+math.sin(abstime*0.29)*0.025

    for i in range(self.NUMPOINTS):
        t = i/(self.NUMPOINTS-1)
        t = t*2*math.pi
        r = 1.0
        # compute sphereical coordinates
        x = r*math.sin(t*paramA)*math.cos(t*paramB)
        y = r*math.sin(t*paramA)*math.sin(t*paramB)
        z = r*math.cos(t*paramA)
        self.pnt[i] = vec3(x,y,z)*4

  ##############################################
  def onGpuInit(self,ctx):
    super().onGpuInit(ctx,add_grid=False)
    ##############################
    self.pseudowire_pipe = self.createPseudoWirePipeline()
    solid_wire_pipeline = self.createBaryWirePipeline()
    material = solid_wire_pipeline.sharedMaterial
    solid_wire_pipeline.bindParam( material.param("m"), tokens.RCFD_M)
    ##############################
    submesh_isect = meshutil.SubMesh()
    self.barysub_isect = submesh_isect.withBarycentricUVs()
    self.prim3 = meshutil.RigidPrimitive(self.barysub_isect,ctx)
    self.sgnode3 = self.prim3.createNode("m3",self.layer1,solid_wire_pipeline)
    self.sgnode3.enabled = True
    ##############################
    self.pts_drawabledata = LabeledPointDrawableData()
    self.pts_drawabledata.pipeline_points = self.createPointsPipeline()
    #self.sgnode_pts = self.layer1.createDrawableNodeFromData("points",self.pts_drawabledata)
    self.time = 0.0
    self.incr_time = True
    #print("self.pts_drawabledata",self.pts_drawabledata)
    ################################################################################
  ##############################################
  def onUpdate(self,updevent):
    super().onUpdate(updevent)
    if self.incr_time:
      self.time += updevent.deltatime
    self.updatePoints(self.time)
    θ = self.abstime # * math.pi * 2.0 * 0.1
    ##############################
    submesh_isect = meshutil.SubMesh()
    for i in range(self.NUMPOINTS):
      submesh_isect.makeVertex(position=self.pnt[i])
    #print(submesh_isect)
    #for v in submesh_isect.vertices:
    #  print(v.position)
    self.hull = submesh_isect.convexHull(self.numsteps)
    self.barysub_isect = self.hull.withBarycentricUVs()
    #assert(False)
    ##############################

    #time.sleep(0.25)
  ##############################################
  def onGpuIter(self):
    super().onGpuIter()

    # intersection mesh
    #self.barysub_isect = self.submesh_isect.withBarycentricUVs()
    self.pts_drawabledata.pointsmesh = self.hull
    self.prim3.fromSubMesh(self.barysub_isect,self.context)

  def onUiEvent(self,uievent):
    super().onUiEvent(uievent)
    if uievent.code == tokens.KEY_DOWN.hashed:
        if uievent.keycode == 32: # spacebar
          self.numsteps = (self.numsteps + 1) % 4
        if uievent.keycode == ord('A'):
          self.incr_time = not self.incr_time
###############################################################################

sgapp = SceneGraphApp()

sgapp.ezapp.mainThreadLoop(on_iter=lambda: sgapp.onGpuIter() )

