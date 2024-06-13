#!/usr/bin/env python3

################################################################################
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import math, random, argparse, sys
from orkengine.core import *
from orkengine.lev2 import *

coreappinit() # setup filesystem

################################################################################

l2exdir = (lev2exdir()/"python").normalized.as_string
sys.path.append(l2exdir) # add parent dir to path
from lev2utils.cameras import *
from lev2utils.shaders import *
from lev2utils.misc import *
from lev2utils.primitives import createGridData
from lev2utils.scenegraph import createSceneGraph

################################################################################

mesh = meshutil.Mesh()
mesh.readFromWavefrontObj("data://tests/simple_obj/cone.obj")
#mesh.readFromWavefrontObj("src://actors/rijid/ref/rijid.obj")
submesh = mesh.submesh_list[0]
print(submesh)
triangulated = submesh.triangulated()
stripped = triangulated.copy(preserve_normals=False,
                             preserve_colors=False,
                             preserve_texcoords=False)
print(stripped)
for i,v in enumerate(stripped.vertices):
  print("vtx %02d: %s" % (i,v.position))

joined = stripped.coplanarJoined()
joined.writeWavefrontObj("joined.obj")
print(joined)
