#!/usr/bin/env python3

################################################################################
# Copyright 1996-2020, Michael T. Mayers.
# Distributed under the Boost Software License - Version 1.0 - August 17, 2003
# see http://www.boost.org/LICENSE_1_0.txt
################################################################################

import math, random, argparse, sys
from orkengine.core import *
from orkengine.lev2 import *

coreappinit() # setup filesystem

################################################################################

sys.path.append((thisdir()/".."/".."/"examples"/"python").normalized.as_string) # add parent dir to path
from common.cameras import *
from common.shaders import *
from common.misc import *
from common.primitives import createGridData
from common.scenegraph import createSceneGraph

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
for i,v in enumerate(stripped.vertexpool.orderedVertices):
  print("vtx %02d: %s" % (i,v.position))

joined = stripped.coplanarJoined()
joined.writeWavefrontObj("joined.obj")
print(joined)