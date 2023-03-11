#!/usr/bin/env python3
################################################################################
# Copyright 1996-2020, Michael T. Mayers.
# Distributed under the Boost Software License - Version 1.0 - August 17, 2003
# see http://www.boost.org/LICENSE_1_0.txt
################################################################################
import ork.path
from orkengine.core import *
from orkengine.lev2 import *

coreappinit() # setup filesystem
mesh = meshutil.Mesh()
mesh.readFromWavefrontObj("data://tests/simple_obj/monkey.obj")

submesh = mesh.submesh_list[0]

slicing_plane = plane(vec3(0,1,0),0)

sliced = submesh.sliceWithPlane(slicing_plane)
print(sliced)

sliced["front"].writeWavefrontObj(str(ork.path.temp()/"monkey_slice_front_out.obj"));
sliced["back"].writeWavefrontObj(str(ork.path.temp()/"monkey_slice_back_out.obj"));
sliced["intersects"].writeWavefrontObj(str(ork.path.temp()/"monkey_slice_isect_out.obj"));

clipped = submesh.clipWithPlane(slicing_plane)
print(clipped)

clipped["front"].triangulate().writeWavefrontObj(str(ork.path.temp()/"monkey_clipped_front_out.obj"));
clipped["back"].triangulate().writeWavefrontObj(str(ork.path.temp()/"monkey_clipped_back_out.obj"));
