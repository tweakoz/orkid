#!/usr/bin/env python3

################################################################################
# lev2 sample which renders a vector field mesh
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import math, random, argparse, sys, signal
from obt import path as obt_path
import pyvista as pv
import numpy as np

#from orkengine.core import vec3, vec4, quat, mtx4
from orkengine.core import lev2_pyexdir
from orkengine.core import CrcStringProxy
from orkengine import lev2

this_dir = obt_path.Path(__file__).parent
lev2_pyexdir.addToSysPath()
sys.path.append(str(this_dir/".."/"..")) # add parent dir to path
from _boilerplate import *

tokens = CrcStringProxy()

################################################################################
# generate a vector field via PyVista
#################################################################################

def genVectorField():
  # Make a grid
  x, y, z = np.meshgrid(np.linspace(-8, 8, 8),
                        np.linspace(-8, 8, 8),
                        np.linspace(-8, 8, 8),
                        indexing='ij')

  points = np.empty((x.size, 3))
  points[:, 0] = x.ravel('F')
  points[:, 1] = y.ravel('F')
  points[:, 2] = z.ravel('F')

  # Compute a direction for the vector field
  direction = np.sin(points)**3

  # Create a PyVista PolyData object to hold the arrows
  all_arrows = pv.PolyData()

  # Add arrows to the PolyData object
  for i in range(len(points)):
    arrow = pv.Arrow(start=points[i], direction=direction[i], scale=1.5)
    all_arrows = all_arrows.merge(arrow)

  # Convert the combined arrows mesh to NumPy format
  vertices = all_arrows.points
  faces_arr = all_arrows.faces

  # Correctly interpret the faces array
  faces_list = []
  i = 0
  while i < len(faces_arr):
    n = faces_arr[i]  # number of points in this face
    faces_list.append(faces_arr[i + 1: i + 1 + n].tolist())
    i += n + 1

  return vertices, faces_list

################################################################################

class MoleculeApp(BasicUiCamSgApp):

  def __init__(self):
    super().__init__(ssaa=1)

  ##############################################

  def onGpuInit(self,ctx):
    super().onGpuInit(ctx)

    ##################################
    # solid wire pipeline
    ##################################

    solid_wire_pipeline =  self.createBaryWirePipeline()
    material = solid_wire_pipeline.sharedMaterial
    solid_wire_pipeline.bindParam( material.param("m"), tokens.RCFD_M)

    ###################################
    # generate mesh
    ###################################

    v,f = genVectorField()
    result_submesh = lev2.meshutil.SubMesh.createFromDict({
        "vertices": [{  "p": vec3(item[0], item[1], item[2])*0.5} for item in v],
        "faces": f
    })
    self.barysubmesh = result_submesh.withBarycentricUVs()
    self.union_prim = lev2.RigidPrimitive(self.barysubmesh,ctx)
    self.union_sgnode = self.union_prim.createNode("union",self.layer1,solid_wire_pipeline)
    self.union_sgnode.enabled = True

    ###################################

###############################################################################

MoleculeApp().ezapp.mainThreadLoop()

