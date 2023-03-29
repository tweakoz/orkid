#!/usr/bin/env python3
################################################################################
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################
import ork.path
import math, random, argparse, sys
from orkengine.core import *
from orkengine.lev2 import *

def submesh_from_lists(vertices,faces):
    return meshutil.SubMesh.createFromDict({
        "vertices": [{  "p": vec3(item[0], item[1], item[2])} for item in vertices],
        "faces": faces
    })

################################################################################
# INPUT MESH
################################################################################
vertices1 = [[-0.014844934456050396, 0.014004657045006752, 0.009999999776482582], [0.014844934456050396, 0.014004657045006752, 0.009999999776482582], [0.014844934456050396, -0.014004657045006752, 0.009999999776482582], [-0.014844934456050396, -0.014004657045006752, 0.009999999776482582], [-14.845727920532227, 14.00540542602539, 10.000534057617188], [-14.845727920532227, -14.00540542602539, 10.000534057617188], [14.845727920532227, -14.00540542602539, 10.000534057617188], [14.845727920532227, 14.00540542602539, 10.000534057617188]]
faces1 =  [[1, 0, 4, 7], [3, 2, 6, 5], [1, 7, 6, 2], [4, 5, 6, 7], [3, 5, 4, 0], [0, 1, 2, 3]]
submesh1 = submesh_from_lists(vertices1,faces1)
near1 = plane(0, 0, 1,-0.01)
far1 = plane(-0, -0, -1,10.0005)
left1  = plane(0.558692, 0, 0.829375,-0)
right1  = plane(-0.558692, 0, 0.829375,-0)
top1  = plane(0, 0.58111, 0.813825,-9.53674e-07)
bottom1  = plane(0, -0.58111, 0.813825,-9.31323e-10)
################################################################################
# PLANES THAT WE ARE CUTTING WITH
################################################################################
vertices2 = [[-0.014844934456050396, 0.014004657045006752, 0.009999999776482582], [0.014844934456050396, 0.014004657045006752, 0.009999999776482582], [0.014844934456050396, -0.014004657045006752, 0.009999999776482582], [-0.014844934456050396, -0.014004657045006752, 0.009999999776482582], [-14.845727920532227, 14.00540542602539, 10.000534057617188], [-14.845727920532227, -14.00540542602539, 10.000534057617188], [14.845727920532227, -14.00540542602539, 10.000534057617188], [14.845727920532227, 14.00540542602539, 10.000534057617188]]
faces2 = [[1, 0, 4, 7], [3, 2, 6, 5], [1, 7, 6, 2], [4, 5, 6, 7], [3, 5, 4, 0], [0, 1, 2, 3]]
submesh2 = submesh_from_lists(vertices2,faces2)
near2  = plane(0,0,1,-0.01)
far2  = plane(-0,-0,-1,10.0005)
left2  = plane(0.558692,0,0.829375,-0)
right2  = plane(-0.558692,0,0.829375,-0)
top2  = plane(0, 0.58111,0.813825,-9.53674e-07)
bottom2  = plane(0,-0.58111,0.813825,-9.31323e-10)
################################################################################
a = submesh1.clippedWithPlane(plane=near2)["front"]
b = a.clippedWithPlane(plane=far2)["front"]
c = b.clippedWithPlane(plane=left2)["front"]
d = c.clippedWithPlane(plane=right2)["front"]
e = d.clippedWithPlane(plane=top2)["front"]
f = e.clippedWithPlane(plane=bottom2)["front"]


#clip poly num verts<4>

#  iva<0> of inuminverts<4>
#  is_vertex_a_front<0> is_vertex_b_front<0>
#  add a to back cnt<1>

#  iva<1> of inuminverts<4>
#  is_vertex_a_front<0> is_vertex_b_front<1>
#  add a to back cnt<2>
#
#  plane crossed iva<1> ivb<2>
#  isect1<0>
#  isect2<0>
# NO INTERSECT vA<0.0148449 -0.0140047 0.01> vB<14.8457 -14.0054 10.0005>
# NO INTERSECT pdA<-9.49949e-07>
# NO INTERSECT pdB<1.90735e-06>
# NO INTERSECT plane_n<0 0.58111 0.813825> d<-9.53674e-07>

#  iva<2> of inuminverts<4>
#  is_vertex_a_front<1> is_vertex_b_front<1>
#  add a to front cnt<1>

#  iva<3> of inuminverts<4>
#  is_vertex_a_front<1> is_vertex_b_front<0>
#  add a to front cnt<2>
#
#  plane crossed iva<3> ivb<0>
#  isect1<0>
#  isect2<0>
# NO INTERSECT vA<-14.8457 -14.0054 10.0005> vB<-0.0148449 -0.0140047 0.01>
# NO INTERSECT plane_n<0 0.58111 0.813825> d<-9.53674e-07>

#numfront<2> numback<2>
