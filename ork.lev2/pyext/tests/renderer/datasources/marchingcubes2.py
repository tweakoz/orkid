#!/usr/bin/env python3

################################################################################
# lev2 sample which renders a vector field mesh
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import sys, math, threading, time, signal
import pyvista as pv
import numpy as np
from obt import path as obt_path
from noise import pnoise3

#from orkengine.core import vec3, vec4, quat, mtx4
from orkengine.core import lev2_pyexdir, Crc64Context, vec3
from orkengine.lev2 import meshutil
this_dir = obt_path.Path(__file__).parent
lev2_pyexdir.addToSysPath()
sys.path.append(str(this_dir/".."/"..")) # add parent dir to path
from _boilerplate import BasicUiCamSgApp

################################################################################

half_dim = 2  # Half-width of the sampling region
resolution = 16  # Number of points in each dimension
octaves = 4

################################################################################
# generate a vector field via PyVista
#################################################################################


################################################################################

class MCUBES2(BasicUiCamSgApp):

  def __init__(self):
    super().__init__(ssaa=0)
    self.abstime = 0.0
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
    
    self.genthread = threading.Thread(target=self.genTreadImpl)


    self.genthread.start()

    def onCtrlC(signum, frame):
      print("signalling EXIT to ezapp")
      self.ezapp.signalExit()

    signal.signal(signal.SIGINT, onCtrlC)
    
  ##############################################

  def onGpuInit(self,ctx):
    super().onGpuInit(ctx)
    #(barysubmesh,union_prim, union_sgnode)
    self.node = self.createBaryDrawableFromVertsAndFaces(ctx,self.v,self.f,0.5)

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

  def genTreadImpl(self):
    abstime = 0.0
    while True:
      verts,faces = self.genMesh(abstime)
      self.gencounter += 1
      abstime += 0.01     
      scale = 0.5
      result_submesh = meshutil.SubMesh.createFromDict2({
          "vertices": verts,
          "faces": faces
      })
      self.barysubmesh = result_submesh.withBarycentricUVs()
      self.updcounter += 1
      time.sleep(0.03)
    
  ##############################################

  def onGpuUpdate(self,ctx):
    super().onGpuUpdate(ctx)
    if hasattr(self,"barysubmesh") and self.gpucounter<self.updcounter:
      self.node[1].fromSubMesh(self.barysubmesh,ctx)
      self.gpucounter = self.updcounter
    
  ##############################################

def onRunLoopIteration():
  # we just need this in-python runloop iteration 
  #  in order to catch ctrl-c from python
  #  so the python signal handler can trigger it's designated callback
  pass

###############################################################################

MCUBES2().ezapp.mainThreadLoop(on_iter=onRunLoopIteration)

