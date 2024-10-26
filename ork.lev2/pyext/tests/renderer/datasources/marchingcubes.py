#!/usr/bin/env python3

################################################################################
# lev2 sample which renders a vector field mesh
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import sys
import pyvista as pv
import numpy as np
from obt import path as obt_path
from noise import pnoise3

#from orkengine.core import vec3, vec4, quat, mtx4
from orkengine.core import lev2_pyexdir, Crc64Context
this_dir = obt_path.Path(__file__).parent
lev2_pyexdir.addToSysPath()
sys.path.append(str(this_dir/".."/"..")) # add parent dir to path
from _boilerplate import BasicUiCamSgApp

################################################################################

half_dim = 5  # Half-width of the sampling region
resolution = 400  # Number of points in each dimension
falloff_radius = 2.0  # Radius of the radial falloff function
falloff_power = 1.5  # Power of the radial falloff function
octaves = 7

def hashParams():
  crcA = Crc64Context()
  crcA.accum("marchingcubes:1.0") # version string (change if you alter the boolean stage)
  crcA.accum(half_dim)
  crcA.accum(resolution)
  crcA.accum(falloff_radius)
  crcA.accum(falloff_power)
  crcA.accum(octaves)
  crcA.finish()
  return crcA.result
  
################################################################################
# generate a vector field via PyVista
#################################################################################

def genMesh():
  hash = hashParams()
  obj_path = obt_path.temp()/f"marchingcubes_{hash}.obj"
  #################################
  if obj_path.exists():
    print(f"Loading mesh from {obj_path}")
    mesh = pv.read(obj_path)
    vertices = mesh.points
    faces_arr = mesh.faces
    faces_list = []
    i = 0
    while i < len(faces_arr):
      n = faces_arr[i]
      faces_list.append(faces_arr[i + 1: i + 1 + n].tolist())
      i += n + 1
    print(f"Loaded mesh with {len(vertices)} vertices and {len(faces_list)} faces")
    return vertices, faces_list
  #################################
  else: # regenerate mesh
  #################################
    def cloud_shape(x, y, z, 
                    scale=1.0, 
                    octaves=octaves, 
                    persistence=0.5, 
                    lacunarity=2.0):
      value = pnoise3( x * scale, 
                       y * scale, 
                       z * scale, 
                       octaves=octaves, 
                       persistence=persistence, 
                       lacunarity=lacunarity)
      # Radial falloff function to ramp density to 0 at the boundaries
      distance = np.sqrt(x ** 2 + y ** 2 + z ** 2)
      falloff = max(0, 1 - (distance / falloff_radius) ** falloff_power)
      
      return value * falloff
    #################################
    # Define sampling parameters
    #################################
    spacing = (half_dim * 2 / resolution, half_dim * 2 / resolution, half_dim * 2 / resolution)
    grid = pv.ImageData(
        dimensions=(resolution,resolution,resolution),
        spacing=spacing,
        origin=(-half_dim, -half_dim, -half_dim),
    )
    x, y, z = grid.points.T
    #################################
    # sample and plot
    #################################
    values = np.array([cloud_shape(xi, yi, zi) for xi, yi, zi in zip(x, y, z)])
    mesh = grid.contour([0.01], values, method='marching_cubes')
    mesh = mesh.clean()
    # Extract vertices and faces
    vertices = mesh.points
    faces_arr = mesh.faces
      
    # Convert faces array to list of lists format
    
    faces_list = []
    i = 0
    reverse_winding = True
    while i < len(faces_arr):
      n = faces_arr[i]
      if reverse_winding:
        faces_list.append(faces_arr[i + 1: i + 1 + n].tolist())
      else:
        faces_list.append(faces_arr[i + 1: i + 1 + n].tolist()[::-1])
      i += n + 1
    # Write mesh to file
    mesh.save(obj_path)
    
    return vertices, faces_list



################################################################################

class MoleculeApp(BasicUiCamSgApp):

  def __init__(self):
    super().__init__(ssaa=0)

  ##############################################

  def onGpuInit(self,ctx):
    super().onGpuInit(ctx)
    v,f = genMesh()
    self.node = self.createBaryDrawableFromVertsAndFaces(ctx,v,f,0.5)
    #self.node = self.createPbrDrawableFromVertsAndFaces(ctx,v,f,0.5)


###############################################################################

MoleculeApp().ezapp.mainThreadLoop()

