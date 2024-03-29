#!/usr/bin/env python3
################################################################################
# lev2 sample which renders to an offscreen buffer
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################
import time
from orkengine.core import *
from orkengine.lev2 import *
from PIL import Image
import numpy as np
from ork.deco import Deco
deco = Deco()
################################################################################
frustum = Frustum(mtx4.lookAt(vec3(0,0,-1),vec3(0,0,0),vec3(0,1,0)),
                  mtx4.perspective(45,1,0.1,3))
################################################################################
qsubmesh = meshutil.SubMesh()
qsubmesh.addQuad(frustum.nearCorner(3), # near
                 frustum.nearCorner(2),
                 frustum.nearCorner(1),
                 frustum.nearCorner(0))
qsubmesh.addQuad(frustum.farCorner(0), # far
                 frustum.farCorner(1),
                 frustum.farCorner(2),
                 frustum.farCorner(3))
qsubmesh.addQuad(frustum.nearCorner(1), # top
                 frustum.farCorner(1),
                 frustum.farCorner(0),
                 frustum.nearCorner(0))
qsubmesh.addQuad(frustum.nearCorner(3), # bottom
                 frustum.farCorner(3),
                 frustum.farCorner(2),
                 frustum.nearCorner(2))
qsubmesh.addQuad(frustum.nearCorner(0), # left
                 frustum.farCorner(0),
                 frustum.farCorner(3),
                 frustum.nearCorner(3))
qsubmesh.addQuad(frustum.nearCorner(2), # right
                 frustum.farCorner(2),
                 frustum.farCorner(1),
                 frustum.nearCorner(1))
################################################################################
quad_iglmesh = qsubmesh.toIglMesh(4)
print(deco.white("QuadMesh.Vertices:")+"\n%s"%quad_iglmesh.vertices)
print(deco.white("QuadMesh.Faces:")+"\n%s"%quad_iglmesh.faces)
qsubmesh.writeWavefrontObj("submesh_quads.obj")
################################################################################
print(deco.white("QuadMesh Stats:"))
verts_x = quad_iglmesh.vertices[:, 0]
verts_y = quad_iglmesh.vertices[:, 1]
verts_z = quad_iglmesh.vertices[:, 2]
bb_min = vec3( np.min(verts_x),
               np.min(verts_y),
               np.min(verts_z))
bb_max = vec3( np.max(verts_x),
               np.max(verts_y),
               np.max(verts_z))
bb_mean = vec3( np.mean(verts_x),
                np.mean(verts_y),
                np.mean(verts_z) )
bb_center = (bb_max+bb_min)*0.5
print( deco.yellow("bounding min: ")+str(bb_min))
print( deco.yellow("bounding max: ")+str(bb_max))
print( deco.yellow("bounding center: ")+str(bb_center))
print( deco.yellow("bounding mean: ")+str(bb_mean))

print(deco.white("QuadMesh.VtxNormals: ")+"\n%s"%quad_iglmesh.vertexNormals())
print(deco.white("QuadMesh.FacNormals: ")+"\n%s"%quad_iglmesh.faceNormals())
print(deco.white("QuadMesh.CorNormals: ")+"\n%s"%quad_iglmesh.cornerNormals(20))
################################################################################
tsubmesh = qsubmesh.triangulate()
tsubmesh.igl_test()
tri_iglmesh = tsubmesh.toIglMesh(3)
print(deco.white("TriMesh.Faces: ")+"\n%s"%tri_iglmesh.faces)
reoriented = tri_iglmesh.reOriented()
print(deco.white("reoriented.Faces: ")+"\n%s"%reoriented.faces)
print(deco.white("TriMesh.GaussianCurvature: ")+"\n%s"%tri_iglmesh.gaussianCurvature())
print(deco.white("TriMesh.PrincipleCurvature.K1: ")+"\n%s"%tri_iglmesh.principleCurvature().k1)
print(deco.white("TriMesh.PrincipleCurvature.K2: ")+"\n%s"%tri_iglmesh.principleCurvature().k2)

print(deco.white("TriMesh.edgeStats<avg length>: ")+str(tri_iglmesh.averageEdgeLength()))
areastats = tri_iglmesh.areaStatistics()
anglstats = tri_iglmesh.angleStatistics()
print(deco.white("TriMesh.areaStats<min,max,avg,sigma>: ")+str(areastats))
print(deco.white("TriMesh.angleStats<min,max,avg,sigma> (degrees): ")+str(anglstats))
print(deco.white("TriMesh.ambientOcclusion: ")+str(tri_iglmesh.ambientOcclusion(500)))

print(deco.white("reoriented.irregularVertexCount: ")+str(reoriented.irregularVertexCount()))
#print(deco.white("TriMesh.parameterizeHarmonic:")+"\n%s"%reoriented.parameterizeHarmonic())
#print(deco.white("TriMesh.parameterizeLCSM:")+"\n%s"%tri_iglmesh.parameterizeLCSM())
tsubmesh.writeWavefrontObj("submesh_tris.obj")
