#!/usr/bin/env python3
################################################################################
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################
import ork.path
import math, random, argparse, sys
import trimesh, pickle
from orkengine.core import *
from orkengine.lev2 import *
from _boilerplate import *
################################################################################
# apply transform to trimesh
################################################################################

def applyXF(tmesh,pos,rot,scale): 
  xf = mtx4.composed(pos,rot,scale)
  tmesh.apply_transform(xf.transposed)

################################################################################
# create trimesh from ork vertices and faces
################################################################################

def createTM(ork_verts,ork_faces,pos,rot,scale): 
  tmesh = trimesh.base.Trimesh( vertices=[vtx.position.as_list for vtx in ork_verts], 
                                faces=[face.indices for face in ork_faces])
  applyXF(tmesh,pos,rot,scale)
  return tmesh

################################################################################

class SceneGraphApp(BasicUiCamSgApp):
  ##############################################
  def __init__(self):
    super().__init__()
  ##############################################
  def onGpuInit(self,ctx):
    super().onGpuInit(ctx)
    ##################################
    # solid wire pipeline
    ##################################
    solid_wire_pipeline = self.createBaryWirePipeline()
    material = solid_wire_pipeline.sharedMaterial
    solid_wire_pipeline.bindParam( material.param("m"), tokens.RCFD_M)

    #################################################################
    # source mesh paths
    #################################################################

    cyl_path = "data://tests/simple_obj/cylinder.obj"
    tor_path = "data://tests/simple_obj/torus.obj"

    #################################################################
    # datablock cache setup
    #################################################################

    crcA = Crc64Context()
    crcA.accum(cyl_path)          # path to source mesh
    crcA.accum(tor_path)          # path to source mesh
    crcA.accum("boolean_ops:1.6") # version string (change if you alter the boolean stage)
    crcA.finish()
    
    cached_dblock = DataBlockCache.findDataBlock(crcA.result)

    ##########################################################
    # if in cache, load from cache
    ##########################################################
    if cached_dblock is not None:
      dstream = DataBlockInputStream(cached_dblock)
      verts = pickle.loads(dstream.readBytes())
      faces = pickle.loads(dstream.readBytes())
      boolean_out = trimesh.base.Trimesh( vertices=verts, faces=faces)

    ##########################################################
    # not in cache, so do the boolean ops and fill in cache
    ##########################################################
    else: 

      # load source meshes

      cyl_orkmesh = meshutil.Mesh()
      cyl_orkmesh.readFromWavefrontObj(cyl_path)
      tor_orkmesh = meshutil.Mesh()
      tor_orkmesh.readFromWavefrontObj(tor_path)

      cyl_vertices = cyl_orkmesh.submesh_list[0].vertices
      cyl_faces = cyl_orkmesh.submesh_list[0].polys

      tor_vertices = tor_orkmesh.submesh_list[0].vertices
      tor_faces = tor_orkmesh.submesh_list[0].polys

      # create transformed source meshes

      tm_cyl_outer = createTM( cyl_vertices, cyl_faces, vec3(0,0,0), quat(), 1 )
      tm_cyl_inner = createTM( cyl_vertices, cyl_faces, vec3(0,0,0), quat(), vec3(0.9,2.0,0.9) )

      tm_ring_1 = createTM( tor_vertices, tor_faces, vec3(-1,0,0), quat(), 1 )
      tm_ring_2 = createTM( tor_vertices, tor_faces, vec3(+1,0,0), quat(), 1 )
      tm_ring_3 = createTM( tor_vertices, tor_faces, vec3(0,0,-1), quat(), 1 )
      tm_ring_4 = createTM( tor_vertices, tor_faces, vec3(0,0,+1), quat(), 1 )

      # do boolean operations

      boolean_out = tm_cyl_outer.union(tm_ring_1)
      boolean_out = boolean_out.union(tm_ring_2)
      boolean_out = boolean_out.union(tm_ring_3)
      boolean_out = boolean_out.union(tm_ring_4)
      boolean_out = boolean_out.difference(tm_cyl_inner)

      # write to cache

      dblock = DataBlock()
      dblock.writeBytes(pickle.dumps(boolean_out.vertices))
      dblock.writeBytes(pickle.dumps(boolean_out.faces))
      DataBlockCache.setDataBlock(crcA.result,dblock)

    #################################################################
    # create ork meshes/primitives from boolean results
    #################################################################

    boolean_out_vertices = [{  "p": vec3(item[0], item[1], item[2])} for item in boolean_out.vertices]

    result_submesh = meshutil.SubMesh.createFromDict({
        "vertices": boolean_out_vertices,
        "faces": boolean_out.faces
    })

    print(result_submesh)
    result_submesh.writeWavefrontObj("boolean_out.obj")

    self.barysubmesh = result_submesh.withBarycentricUVs()


    self.union_prim = meshutil.RigidPrimitive(self.barysubmesh,ctx)
    self.union_sgnode = self.union_prim.createNode("union",self.layer1,solid_wire_pipeline)
    self.union_sgnode.enabled = True

###############################################################################
sgapp = SceneGraphApp()
sgapp.ezapp.mainThreadLoop(on_iter=lambda: sgapp.onGpuIter() )
