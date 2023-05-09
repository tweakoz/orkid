#!/usr/bin/env python3
################################################################################
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################
import ork.path
from orkengine.core import *
from orkengine.lev2 import *

coreappinit() # setup filesystem
mesh = meshutil.Mesh()
mesh.readFromWavefrontObj("data://tests/simple_obj/monkey.obj")

submesh = mesh.submesh_list[0]

as_tris = submesh.triangulated()
as_quads = as_tris.quadulated(area_tolerance=100.0, #
                              exclude_non_coplanar=False, #
                              exclude_non_rectangular=False, #
                              )

print(submesh)
print(as_tris)
print(as_quads)

as_quads.writeWavefrontObj(str(ork.path.temp()/"monkey_quadulated_out.obj"));