#!/usr/bin/env python3

################################################################################
# lev2 sample which renders a mesh built using a solid CAD representation
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import sys, math
from solid import *
from solid.utils import *
import numpy as np
import pyvista as pv
from orkengine.core import vec3, quat, mtx4, Crc64Context
from obt import path as obt_path, command
import os
import subprocess

#from orkengine.core import vec3, vec4, quat, mtx4
from orkengine.core import lev2_pyexdir
this_dir = obt_path.Path(__file__).parent
lev2_pyexdir.addToSysPath()
sys.path.append(str(this_dir/".."/"..")) # add parent dir to path
from _boilerplate import BasicUiCamSgApp

################################################################################
# parametric model parameters
################################################################################

CYL_LENGTH = 11
NSEGS = 60
base_radius = 5
hole_radius = 0.5
countersink_radius = 2.0
countersink_length = 2.0
# derived
half_length = CYL_LENGTH / 2.01
hole_length = CYL_LENGTH*2
hlpcsld2 = half_length + countersink_length / 2

def hashParams():
  crcA = Crc64Context()
  crcA.accum("solid_python_ex1:1.0") # version string (change if you alter the boolean stage)
  crcA.accum(CYL_LENGTH)
  crcA.accum(NSEGS)
  crcA.accum(base_radius)
  crcA.accum(hole_radius)
  crcA.accum(countersink_radius)
  crcA.accum(countersink_length)
  crcA.accum(half_length)
  crcA.finish()
  return crcA.result
  
################################################################################
# generate a model using solidpython
#################################################################################

def boolean_example():
    
  ##################################################
  # create base by subtracting a sphere from a cube
  ##################################################

  cube_obj = translate([-5, -5, -5])(cube([10, 10, 10]))
  sphere_obj = translate([0, 0, 0])(sphere(r=6.5, segments=NSEGS))
  sphere2_obj = translate([0, 0, 0])(sphere(r=7.5, segments=NSEGS))
  result = difference()(
      cube_obj,
      sphere_obj
  )
  result = intersection()(
      sphere2_obj,
      result,
  )
   
  ##################################################
  # create 3 endcapped cylinders on the x, y, and z axes
  ##################################################
 
  my_cyl = cylinder(r=2, h=CYL_LENGTH, segments=NSEGS)
  hemi_sphere = sphere(r=2, segments=NSEGS)

  cylinder_z = translate([0, 0, -half_length])(my_cyl) + translate([0, 0, -half_length])(hemi_sphere) + translate([0, 0, half_length])(hemi_sphere)
  cylinder_y = rotate([90, 0, 0])(translate([0, 0, -half_length])(my_cyl) + translate([0, 0, -half_length])(hemi_sphere) + translate([0, 0, half_length])(hemi_sphere))
  cylinder_x = rotate([0, 90, 0])(translate([0, 0, -half_length])(my_cyl) + translate([0, 0, -half_length])(hemi_sphere) + translate([0, 0, half_length])(hemi_sphere))

  # Combine all the shapes
  result += cylinder_z
  result += cylinder_y
  result += cylinder_x
  
  ##################################################
  # add a sphere at the origin to join the cylinders
  ##################################################

  result += translate([0, 0, 0])(sphere(r=4, segments=NSEGS))

  ##################################################
  # drill holes into each cylinder along its major axis
  ##################################################


  holex = translate([0, 0, -hole_length/2])(cylinder(r=hole_radius, h=hole_length, segments=NSEGS))
  holey = rotate([90, 0, 0])(translate([0, 0, -hole_length/2])(cylinder(r=hole_radius, h=hole_length, segments=NSEGS)))
  holez = rotate([0, 90, 0])(translate([0, 0, -hole_length/2])(cylinder(r=hole_radius, h=hole_length, segments=NSEGS)))

  holes = union()(holex, holey, holez)
  result = difference()(result, holes) 

  ##################################################
  # countersink the holes (make them conical)
  ##################################################

  # Create countersink
  countersink = cylinder(r2=countersink_radius, r1=hole_radius, h=countersink_length, segments=NSEGS)
  countersink = translate([0, 0, -countersink_length/2])(countersink)

   # Locate countersinks at the ends of the holes
  result -= translate([hlpcsld2, 0, 0])(rotate([0, 90, 0])(countersink))
  result -= translate([-hlpcsld2, 0, 0])(rotate([0, -90, 0])(countersink))

  result -= translate([0, hlpcsld2, 0])(rotate([-90, 0, 0])(countersink))
  result -= translate([0, -hlpcsld2, 0])(rotate([90, 0, 0])(countersink))

  result -= translate([0, 0, hlpcsld2])(countersink)
  result -= translate([0, 0, -hlpcsld2])(rotate([0, 180, 0])(countersink))

  ##################################################

  return result

################################################################################
# convert to mesh format
#################################################################################

def genPolys():

  hash = hashParams()
  basename = f"boolean_example_{hash}"

  scad_path = obt_path.temp()/(basename+".scad")
  stl_path = obt_path.temp()/(basename+".stl")

  ################################
  # if not in cache, generate the mesh
  ################################

  if not stl_path.exists():
    print(f"generating {stl_path}")

    ################################
    # first generate and write the scad file
    ################################

    scad_code = scad_render(boolean_example())
    with open(str(scad_path), 'w') as f:
      f.write(scad_code)

    ################################
    # Ensure OpenSCAD is installed and in your PATH, otherwise provide the full path to OpenSCAD
    # run the openscad to convert the scad file to stl
    ################################

    command.run(["openscad", "-o", stl_path, scad_path])

  ################################
  # read the stl file into a mesh
  ################################

  assert(stl_path.exists())
  mesh = pv.read(stl_path)
  vertices = mesh.points
  faces = mesh.faces.reshape((-1, 4))[:, 1:]

  ################################

  return vertices, faces

################################################################################

class MoleculeApp(BasicUiCamSgApp):

  def __init__(self):
    super().__init__(ssaa=2)

  ##############################################

  def onGpuInit(self,ctx):
    super().onGpuInit(ctx)
    v,f = genPolys()
    self.node = self.createBaryDrawableFromVertsAndFaces(ctx,v,f,0.25)

###############################################################################

MoleculeApp().ezapp.mainThreadLoop()

