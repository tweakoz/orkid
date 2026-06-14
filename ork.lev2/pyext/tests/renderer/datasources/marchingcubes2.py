#!/usr/bin/env ork.python

################################################################################
# lev2 sample which renders a vector field mesh
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import sys, math, threading, time, signal
#import pyvista as pv
import numpy as np
from obt import path as obt_path
from noise import pnoise3

from orkengine.core import vec3, vec4, VarMap, CrcStringProxy, lev2_pyexdir
from orkengine import lev2
from orkengine.lev2 import meshutil
from ork.app.application import ComponentizedApplication

lev2_pyexdir.addToSysPath()
from lev2utils.cameras import setupUiCameraX

tokens = CrcStringProxy()

################################################################################

half_dim = 2  # Half-width of the sampling region
resolution = 16  # Number of points in each dimension
octaves = 4
ok_to_quit = False

################################################################################

class MCUBES2(ComponentizedApplication):

  def __init__(self):
    super().__init__()
    self.materials = set()

    # create cube verts and faces
    self.v = np.array([[0,0,0],[1,0,0],[1,1,0],[0,1,0],[0,0,1],[1,0,1],[1,1,1],[0,1,1]])
    self.f = [[0,1,2,3],[4,5,6,7],[0,1,5,4],[2,3,7,6],[0,3,7,4],[1,2,6,5]]
    self.volume = np.zeros((resolution, resolution, resolution))
    print(self.volume.shape)

    spacing = (half_dim * 2 / resolution, half_dim * 2 / resolution, half_dim * 2 / resolution)
    self.grid = pv.ImageData(
        dimensions=(resolution,resolution,resolution),
        spacing=spacing,
        origin=(-half_dim, -half_dim, -half_dim),
    )

    self.updcounter = 0
    self.gencounter = 0
    self.gpucounter = 0

    self.createEzApp(ssaa=0, width=1280, height=640)

    self.genthread = threading.Thread(target=self.genThreadImpl)
    self.genthread.start()

  ##############################################

  def _onUiInit(self):
    lg = self.ezapp.topLayoutGroup
    self.sgviewport_item = lg.makeChild(
      uiclass=lev2.ui.SceneGraphViewport,
      args=["PrimarySG"],
      fill=True
    )

  ##############################################

  def _onGpuInit(self, ctx):

    # Create scene
    sg_params = VarMap()
    sg_params.preset = "ForwardPBR"
    sg_params.SkyboxIntensity = 1.0
    sg_params.DiffuseIntensity = 1.0
    sg_params.SpecularIntensity = 1.0
    sg_params.AmbientLight = vec3(0.0)
    sg_params.DepthFogDistance = float(1e6)
    sg_params.SkyboxTexPathStr = "nebula"

    self.scene = lev2.scenegraph.Scene(sg_params)
    self.layer1 = self.scene.createLayer("std_forward")

    # Camera
    self.cameralut = lev2.CameraDataLut()
    self.camera, self.uicam = setupUiCameraX(
      cameralut=self.cameralut,
      camname="Camera0"
    )
    self.uicam.lookAt(vec3(5, 5, 5), vec3(0, 0, 0), vec3(0, 1, 0))

    # Attach to viewport
    sgviewport = self.sgviewport_item.widget
    sgviewport.cameraName = "Camera0"
    sgviewport.scenegraph = self.scene
    sgviewport.forkDB()
    sgviewport.evhandler = lambda e: self._onViewportEvent(e)
    sgviewport.ignoreEvents = False

    # Create bary wire pipeline
    material = lev2.FreestyleMaterial()
    material.gpuInit(ctx, "orkshader://basic")
    material.rasterstate.setBlendingMacro(tokens.OFF)
    material.rasterstate.culltest = tokens.PASS_FRONT
    material.rasterstate.depthtest = tokens.LEQUALS
    permu = lev2.FxPipelinePermutation(rendermodel="ForwardPBR")
    permu.technique = material.shader.technique("tek_fnormal_wire")
    pipeline = material.fxcache.findPipeline(permu)
    pipeline.bindParam(material.param("mvp"), tokens.RCFD_Camera_MVP_Mono)
    pipeline.bindParam(material.param("m"), tokens.RCFD_M)
    pipeline.sharedMaterial = material
    self.materials.add(material)

    # Create initial mesh node
    result_submesh = lev2.meshutil.SubMesh.createFromDict({
        "vertices": [{"p": vec3(item[0], item[1], item[2])*0.5} for item in self.v],
        "faces": self.f
    })
    barysubmesh = result_submesh.withBarycentricUVs()
    union_prim = lev2.RigidPrimitive(barysubmesh, ctx)
    union_sgnode = union_prim.createNode("union", self.layer1, pipeline)
    union_sgnode.enabled = True
    self.node = (barysubmesh, union_prim, union_sgnode)

    # Init lighting
    self.scene.lightingmanager.gpuInit(ctx)


  ##############################################

  def _onViewportEvent(self, uievent):
    handled = self.uicam.uiEventHandler(uievent)
    if handled:
      self.uicam.updateMatrices()
      self.camera.copyFrom(self.uicam.cameradata)
    return lev2.ui.HandlerResult()

  ##############################################

  def genMesh(self, time=0.0):

    ############################
    # scroll volume up by one
    ############################
    self.volume[:-1] = self.volume[1:]

    ############################
    # generate new slice at bottom
    ############################

    for z in range(resolution):
        for x in range(resolution):
          value = pnoise3(x / octaves, time*8, z / octaves, octaves, 0.5, 0.5)
          self.volume[resolution-1][z][x] = value

    ############################
    # generate mesh from volume
    ############################

    values = np.array(self.volume.flat)
    mesh = self.grid.contour( [0.01], values, method="marching_cubes")
    return mesh.points, mesh.faces

  ##############################################

  def genThreadImpl(self):
    global ok_to_quit
    abstime = 0.0
    while not ok_to_quit:
      verts,faces = self.genMesh(abstime)
      self.gencounter += 1
      abstime += 0.01
      result_submesh = meshutil.SubMesh.createFromDict2({
          "vertices": verts,
          "faces": faces
      })
      self.barysubmesh = result_submesh.withBarycentricUVs()
      self.updcounter += 1
      time.sleep(0.01)

  ##############################################

  def _onUpdate(self, updinfo):
    self.camera.copyFrom(self.uicam.cameradata)
    self.scene.updateScene(self.cameralut)
    self.sgviewport_item.widget.setDirty()
    time.sleep(0.0005)

  ##############################################

  def _onGpuUpdate(self, ctx):
    if hasattr(self,"barysubmesh") and self.gpucounter<self.updcounter:
      self.node[1].fromSubMesh(self.barysubmesh,ctx)
      self.gpucounter = self.updcounter

###############################################################################

app = MCUBES2()
app.ezapp.mainThreadLoop()
ok_to_quit = True
app.ezapp.shutdown()
