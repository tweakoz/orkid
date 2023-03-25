#!/usr/bin/env python3
################################################################################
# Copyright 1996-2020, Michael T. Mayers.
# Distributed under the Boost Software License - Version 1.0 - August 17, 2003
# see http://www.boost.org/LICENSE_1_0.txt
################################################################################
import ork.path
from orkengine.core import *
from orkengine.lev2 import *
import trimesh, xatlas


#!/usr/bin/env python3

################################################################################
# Copyright 1996-2020, Michael T. Mayers.
# Distributed under the Boost Software License - Version 1.0 - August 17, 2003
# see http://www.boost.org/LICENSE_1_0.txt
################################################################################

import math, random, argparse, sys
from orkengine.core import *
from orkengine.lev2 import *

################################################################################

sys.path.append((thisdir()/".."/".."/"examples"/"python").normalized.as_string) # add parent dir to path
from common.cameras import *
from common.shaders import *
from common.misc import *
from common.primitives import createGridData, createFrustumPrim
from common.scenegraph import createSceneGraph

################################################################################

class SceneGraphApp(object):

  def __init__(self):
    super().__init__()
    self.ezapp = OrkEzApp.create(self)
    self.ezapp.setRefreshPolicy(RefreshFastest, 0)
    self.materials = set()
    setupUiCamera(app=self,eye=vec3(5,5,5),tgt=vec3(0,0,0))

  ##############################################

  def onGpuInit(self,ctx):

    createSceneGraph(app=self,rendermodel="ForwardPBR")

    ##################################
    # solid wire pipeline
    ##################################
    solid_wire_pipeline = createPipeline( app = self,
                                       ctx = ctx,
                                       rendermodel = "ForwardPBR",
                                       shaderfile=Path("orkshader://basic"),
                                       techname="tek_vuv_wire" )

    material = solid_wire_pipeline.sharedMaterial
    solid_wire_pipeline.bindParam( material.param("m"), tokens.RCFD_M)

    #################################################################
    # load obj source meshes
    #################################################################

    cyl_orkmesh = meshutil.Mesh()
    cyl_orkmesh.readFromWavefrontObj("data://tests/simple_obj/cylinder.obj")
    tor_orkmesh = meshutil.Mesh()
    tor_orkmesh.readFromWavefrontObj("data://tests/simple_obj/torus.obj")

    cyl_vertices = cyl_orkmesh.submesh_list[0].vertices
    cyl_faces = cyl_orkmesh.submesh_list[0].polys

    tor_vertices = tor_orkmesh.submesh_list[0].vertices
    tor_faces = tor_orkmesh.submesh_list[0].polys

    #################################################################
    # apply transform to trimesh
    #################################################################

    def applyXF(tmesh,pos,rot,scale): 
      xf = mtx4.composed(pos,rot,scale)
      tmesh.apply_transform(xf.transposed)

    #################################################################
    # create trimesh from ork vertices and faces
    #################################################################

    def createTM(ork_verts,ork_faces,pos,rot,scale): 
      tmesh = trimesh.base.Trimesh( vertices=[vtx.position.as_list for vtx in ork_verts], 
                                    faces=[face.indices for face in ork_faces])
      applyXF(tmesh,pos,rot,scale)
      return tmesh

    #################################################################
    # create transformed source meshes
    #################################################################

    tm_cyl_outer = createTM( cyl_vertices, cyl_faces, vec3(0,0,0), quat(), 1 )
    tm_cyl_inner = createTM( cyl_vertices, cyl_faces, vec3(0,0,0), quat(), vec3(0.9,2.0,0.9) )

    tm_ring_1 = createTM( tor_vertices, tor_faces, vec3(-1,0,0), quat(), 1 )
    tm_ring_2 = createTM( tor_vertices, tor_faces, vec3(+1,0,0), quat(), 1 )
    tm_ring_3 = createTM( tor_vertices, tor_faces, vec3(0,0,-1), quat(), 1 )
    tm_ring_4 = createTM( tor_vertices, tor_faces, vec3(0,0,+1), quat(), 1 )

    #################################################################
    # do boolean operations
    #################################################################

    boolean_out = tm_cyl_outer.union(tm_ring_1)
    boolean_out = boolean_out.union(tm_ring_2)
    boolean_out = boolean_out.union(tm_ring_3)
    boolean_out = boolean_out.union(tm_ring_4)
    boolean_out = boolean_out.difference(tm_cyl_inner)

    boolean_out = boolean_out.unwrap()

    atlas = xatlas.Atlas()
    atlas.add_mesh(boolean_out.vertices, boolean_out.faces)
    atlas.generate()
    
    print(atlas.mesh_count)  # Number of meshes
    print(len(atlas))        # Convenience binding for `atlas.mesh_count`
    print(atlas.get_mesh(0)) # Data for the i-th mesh
    print(atlas[0])          # Convenience binding for `atlas.get_mesh`

    print(atlas.width)       # Width of the atlas 
    print(atlas.height)      # Height of the atlas

    print(atlas.utilization )       # Utilization of the first atlas
    print(atlas.get_utilization(0)) # Utilization of i-th atlas
    
    #vmapping, indices, uvs = xatlas.parametrize(boolean_out.vertices, boolean_out.faces)
    assert(False)

    #################################################################
    # create ork meshes/primitives from boolean results
    #################################################################

    boolean_out_vertices = [{  "p": vec3(item[0], item[1], item[2])} for item in boolean_out.vertices]
    for i in range(len(boolean_out_vertices)):
      uv = boolean_out.visual.uv[i]
      boolean_out_vertices[i]["uv0"] = vec2(uv[0],uv[1])


    result_submesh = meshutil.SubMesh.createFromDict({
        "vertices": boolean_out_vertices,
        "faces": boolean_out.faces
    })

    print(result_submesh)
    result_submesh.writeWavefrontObj("boolean_out.obj")

    #cleaned_result = result_submesh.withFaceNormals()
    cleaned_result = result_submesh.withSmoothedNormals(90*constants.DTOR)
    cleaned_result = cleaned_result.withTextureBasis()
    self.barysubmesh = cleaned_result.withBarycentricUVs()
    self.union_prim = meshutil.RigidPrimitive(self.barysubmesh,ctx)
    self.union_sgnode = self.union_prim.createNode("union",self.layer1,solid_wire_pipeline)
    self.union_sgnode.enabled = True
    #################################################################
    self.context = ctx

  ##############################################

  def onGpuIter(self):
    pass

  ##############################################

  def onUiEvent(self,uievent):
    handled = self.uicam.uiEventHandler(uievent)
    if handled:
      self.camera.copyFrom( self.uicam.cameradata )

  ################################################

  def onUpdate(self,updinfo):
    self.abstime = updinfo.absolutetime
    #########################################
    self.scene.updateScene(self.cameralut) 

###############################################################################

sgapp = SceneGraphApp()

sgapp.ezapp.mainThreadLoop(on_iter=lambda: sgapp.onGpuIter() )
