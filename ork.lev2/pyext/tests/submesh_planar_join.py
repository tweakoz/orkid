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
submesh = mesh.submesh_list[0]
print(submesh)
triangulated = submesh.triangulated()
stripped = triangulated.copy(preserve_normals=False,
                             preserve_colors=False,
                             preserve_texcoords=False)
print(stripped)
for i,v in enumerate(stripped.vertexpool.orderedVertices):
  print("vtx %02d: %s" % (i,v.position))

stripped_as_polyset = stripped.as_polyset
print(stripped_as_polyset)
print(stripped_as_polyset.numpolys)

plane_polysets =  stripped_as_polyset.splitByPlane()
for pset_key in plane_polysets.keys():
  polyset_for_plane = plane_polysets[pset_key]
  #print(polyset_for_plane.numpolys)
  if polyset_for_plane.numpolys==30:
    psub = meshutil.SubMesh()
    psub.mergePolySet(polyset_for_plane)
    psub.writeWavefrontObj("island30.obj")
    islands = psub.as_polyset.splitByIsland()
    #print(islands)
    for i, island in enumerate(islands):
      island_sub = meshutil.SubMesh()
      island_sub.mergePolySet(island)
      island_sub.writeWavefrontObj("island%d.obj"%i)
      edges = island.boundaryEdges()
      print(edges)
      loop = island.boundaryLoop()
      print(loop)
    #print(polyset_for_plane.polys)
assert(False)



#print(island.polys)
#print(len(edges))

joined = submesh.coplanarJoined()
print(joined)
