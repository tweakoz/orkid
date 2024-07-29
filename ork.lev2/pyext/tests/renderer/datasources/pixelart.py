#!/usr/bin/env python3

################################################################################
# lev2 sample which renders a vector field mesh
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import sys
from obt import path as obt_path

from orkengine.core import vec3
from orkengine.core import lev2_pyexdir
this_dir = obt_path.Path(__file__).parent
lev2_pyexdir.addToSysPath()
sys.path.append(str(this_dir/".."/"..")) # add parent dir to path
from _boilerplate import BasicUiCamSgApp

################################################################################
# generate a mesh from pixels in a string
#################################################################################

galaxian = \
"""
 ****         ****
 *   **     **   *
    ***********
  ***  ****   ***
*****  ****   *****
    ***********    
    *   **    *    
   **   **    **   
   *           *
  ****       ****
"""

def genPolys():
  ###################################################
  # generate polygons and vertices
  # each pixel should be a cube centered at z==0
  # and the cubes radius should be extent
  ###################################################
  vertices = []
  faces = []
  extent = 1
  center = vec3(0, 0, 0)
  count = 0
  i = -1
  for line in galaxian.splitlines():
    j = -1
    i = i + 1
    y = i * -2.0
    print(line)
    for char in line:
      pixel = (char == '*')
      j = j + 1
      if pixel:
        x = j * 2.0
        z = 0.0
        center += vec3(x, y, z)
        count += 1
        base = len(vertices)
        vertices.append([x - extent, y - extent, z - extent])
        vertices.append([x + extent, y - extent, z - extent])
        vertices.append([x + extent, y + extent, z - extent])
        vertices.append([x - extent, y + extent, z - extent])
        vertices.append([x - extent, y - extent, z + extent])
        vertices.append([x + extent, y - extent, z + extent])
        vertices.append([x + extent, y + extent, z + extent])
        vertices.append([x - extent, y + extent, z + extent])
        # ensure quads are in CW order
        def add_face(a, b, c, d):
          faces.append([base + a, base + b, base + c, base + d])
        add_face(3, 2, 1, 0)
        add_face(4, 5, 6, 7)
        add_face(0, 1, 5, 4)
        add_face(1, 2, 6, 5)
        add_face(2, 3, 7, 6)
        add_face(3, 0, 4, 7)

  center = center * float(1.0 / count)
  
  for item in vertices:
      item[0] -= center.x
      item[1] -= center.y
      item[2] -= center.z
  return vertices, faces

################################################################################

class PixelArtApp(BasicUiCamSgApp):

  def __init__(self):
    super().__init__(ssaa=1)

  ##############################################

  def onGpuInit(self,ctx):
    super().onGpuInit(ctx)
    v,f = genPolys()
    self.createBaryDrawableFromVertsAndFaces(ctx,v,f,0.25)

###############################################################################

PixelArtApp().ezapp.mainThreadLoop()

