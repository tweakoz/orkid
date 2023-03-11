#!/usr/bin/env python3
################################################################################
# Copyright 1996-2020, Michael T. Mayers.
# Distributed under the Boost Software License - Version 1.0 - August 17, 2003
# see http://www.boost.org/LICENSE_1_0.txt
################################################################################
import ork.path
from orkengine.core import *
from orkengine.lev2 import *

tokens = CrcStringProxy()

coreappinit() # setup filesystem
a = meshutil.Mesh()
a.readFromWavefrontObj("data://tests/simple_obj/cylinder.obj")
b = meshutil.Mesh()
b.readFromWavefrontObj("data://tests/simple_obj/icosphere.obj")

a_igl = a.submesh_list[0].toIglMesh(3)
b_igl = b.submesh_list[0].toIglMesh(3)
c_igl = meshutil.IglMesh()

print( a_igl.piecewiseConstantWindingNumber )
print( b_igl.piecewiseConstantWindingNumber )

#print( a_igl.isVertexManifold )
print( b_igl.isVertexManifold )
print( a_igl.isEdgeManifold )
print( b_igl.isEdgeManifold )

c_igl.booleanOf(a_igl,tokens.RESOLVE,b_igl)

c_igl.toSubMesh().writeWavefrontObj("output.obj")
#print(a_igl)
#print(b_igl)
print(c_igl)
