#!/usr/bin/env python3
################################################################################
# lev2 sample which renders to an offscreen buffer
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import numpy, trimesh
from orkengine.core import *
from orkengine.lev2 import *

def FrustumQuads():
    fpmtx = mtx4.perspective(45,1,0.1,3)
    fvmtx = mtx4.lookAt(vec3(0,0,-1),vec3(0,0,0),vec3(0,1,0))
    frus = Frustum()
    frus.set(fvmtx,fpmtx)
    qsubmesh = meshutil.SubMesh()
    qsubmesh.addQuad(frus.nearCorner(3), # near
                     frus.nearCorner(2),
                     frus.nearCorner(1),
                     frus.nearCorner(0))
    qsubmesh.addQuad(frus.farCorner(0), # far
                     frus.farCorner(1),
                     frus.farCorner(2),
                     frus.farCorner(3))
    qsubmesh.addQuad(frus.nearCorner(1), # top
                     frus.farCorner(1),
                     frus.farCorner(0),
                     frus.nearCorner(0))
    qsubmesh.addQuad(frus.nearCorner(3), # bottom
                     frus.farCorner(3),
                     frus.farCorner(2),
                     frus.nearCorner(2))
    qsubmesh.addQuad(frus.nearCorner(0), # left
                     frus.farCorner(0),
                     frus.farCorner(3),
                     frus.nearCorner(3))
    qsubmesh.addQuad(frus.nearCorner(2), # right
                     frus.farCorner(2),
                     frus.farCorner(1),
                     frus.nearCorner(1))
    return qsubmesh

def HalfBoxQuadsInsideOut(ex, exh):
    submesh = meshutil.SubMesh()
    # box bottom
    submesh.addQuad(
                    dvec3(-ex,0,ex),
                    dvec3(ex,0,ex),
                    dvec3(ex,0,-ex),
                    dvec3(-ex,0,-ex), 
                    )

    #box left side
    submesh.addQuad(
                    dvec3(-ex,exh,-ex),
                    dvec3(-ex,exh,ex),
                    dvec3(-ex,0,ex),
                    dvec3(-ex,0,-ex),
                    dvec4(1,0,0,1) 
                    )

    #box right side
    submesh.addQuad(dvec3(ex,0,-ex),
                    dvec3(ex,0,ex),
                    dvec3(ex,exh,ex),
                    dvec3(ex,exh,-ex),
                    dvec4(1,0,0,1) 
                    )

    #box front side
    submesh.addQuad(
                    dvec3(-ex,exh,ex),
                    dvec3(ex,exh,ex),
                    dvec3(ex,0,ex),
                    dvec3(-ex,0,ex),
                    dvec4(0,0,1,1)                     
                    )
    
    #box back side
    submesh.addQuad(dvec3(-ex,0,-ex),
                    dvec3(ex,0,-ex),
                    dvec3(ex,exh,-ex),
                    dvec3(-ex,exh,-ex),
                    dvec4(0,0,1,1) 
                    )
    return submesh

def fullBoxQuads(exh, exv):
    submesh = meshutil.SubMesh()
    # box top
    submesh.addQuad(
                    dvec3(-exh,exv,exh),
                    dvec3(exh,exv,exh),
                    dvec3(exh,exv,-exh),
                    dvec3(-exh,exv,-exh), 
                    dvec4(0,1,0,1) 
                    )

    # box bottom
    submesh.addQuad(
                    dvec3(-exh,-exv,-exh), 
                    dvec3(exh,-exv,-exh),
                    dvec3(exh,-exv,exh),
                    dvec3(-exh,-exv,exh),
                    dvec4(0,1,0,1) 
                    )

    #box left side
    submesh.addQuad(
                    dvec3(-exh,-exv,-exh),
                    dvec3(-exh,-exv,exh),
                    dvec3(-exh,exv,exh),
                    dvec3(-exh,exv,-exh),
                    dvec4(1,0,0,1) 
                    )

    #box right side
    submesh.addQuad(
                    dvec3(exh,exv,-exh),
                    dvec3(exh,exv,exh),
                    dvec3(exh,-exv,exh),
                    dvec3(exh,-exv,-exh),
                    dvec4(1,0,0,1) 
                    )

    #box front side
    submesh.addQuad(
                    dvec3(-exh,-exv,exh),
                    dvec3(exh,-exv,exh),
                    dvec3(exh,exv,exh),
                    dvec3(-exh,exv,exh),
                    dvec4(0,0,1,1)                     
                    )
    
    #box back side
    submesh.addQuad(
                    dvec3(-exh,exv,-exh),
                    dvec3(exh,exv,-exh),
                    dvec3(exh,-exv,-exh),
                    dvec3(-exh,-exv,-exh),
                    dvec4(0,0,1,1) 
                    )
    return submesh


################################################################################

def applyXF(tmesh,pos,rot,scale): 
  xf = mtx4.composed(pos,rot,scale)
  tmesh.apply_transform(xf.transposed)

################################################################################
# create trimesh from ork vertices and faces
################################################################################

def submeshToTrimesh(subm,pos,rot,scale): 
  ork_verts = subm.vertices
  ork_faces = subm.polys
  tmesh = trimesh.base.Trimesh( vertices=[vtx.position.as_list for vtx in ork_verts], 
                                faces=[face.indices for face in ork_faces])
  applyXF(tmesh,pos,rot,scale)
  return tmesh

def trimeshToSubmesh(tmesh):
    out_vertices = [{  "p": vec3(item[0], item[1], item[2])} for item in tmesh.vertices]

    result_submesh = meshutil.SubMesh.createFromDict({
        "vertices": out_vertices,
        "faces": tmesh.faces
    })
    return result_submesh

#qsubmesh.addQuad(vec3(-3,0,-3), # cut
#                 vec3(+3,0,-3),
#                 vec3(+3,0,+3),
#                 vec3(-3,0,+3))
