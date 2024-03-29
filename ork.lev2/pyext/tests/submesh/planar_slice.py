#!/usr/bin/env python3
################################################################################
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################
import obt.path
from orkengine.core import *
from orkengine.lev2 import *

coreappinit() # setup filesystem
mesh = meshutil.Mesh()
mesh.readFromWavefrontObj("data://tests/simple_obj/monkey.obj")

submesh = mesh.submesh_list[0]

slicing_plane = dplane(dvec3(0,1,0),0)

sliced = submesh.slicedWithPlane(plane=slicing_plane)
print(sliced)

sliced["front"].writeWavefrontObj(str(obt.path.temp()/"monkey_slice_front_out.obj"));
sliced["back"].writeWavefrontObj(str(obt.path.temp()/"monkey_slice_back_out.obj"));
sliced["intersects"].writeWavefrontObj(str(obt.path.temp()/"monkey_slice_isect_out.obj"));

clipped = submesh.clippedWithPlane(plane=slicing_plane)
print(clipped)

clipped["front"].triangulated().writeWavefrontObj(str(obt.path.temp()/"monkey_clipped_front_out.obj"));
clipped["back"].triangulated().writeWavefrontObj(str(obt.path.temp()/"monkey_clipped_back_out.obj"));
