#!/usr/bin/env python3
################################################################################
# Copyright 1996-2020, Michael T. Mayers.
# Distributed under the Boost Software License - Version 1.0 - August 17, 2003
# see http://www.boost.org/LICENSE_1_0.txt
################################################################################
import ork.path
from orkengine.core import *
from orkengine.lev2 import *
import trimesh


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
                                       techname="tek_fnormal_wire" )

    material = solid_wire_pipeline.sharedMaterial
    solid_wire_pipeline.bindParam( material.param("m"), tokens.RCFD_M)
    #################################################################

    a = meshutil.Mesh()
    a.readFromWavefrontObj("data://tests/simple_obj/cylinder.obj")
    b = meshutil.Mesh()
    b.readFromWavefrontObj("data://tests/simple_obj/torus.obj")

    a_vertices = a.submesh_list[0].vertices
    b_vertices = b.submesh_list[0].vertices
    a_faces = a.submesh_list[0].polys
    b_faces = b.submesh_list[0].polys

    a_tm = trimesh.base.Trimesh( vertices=[vtx.position.as_list for vtx in a_vertices], 
                                faces=[face.indices for face in a_faces])
    b_tm = trimesh.base.Trimesh( vertices=[vtx.position.as_list for vtx in b_vertices], 
                                faces=[face.indices for face in b_faces])
    union = a_tm.union(b_tm)

    union_vertices = []
    for item in union.vertices:
      union_vertices.append({
        "p": vec3(item[0], item[1], item[2])
    })  

    union_submesh = meshutil.SubMesh.createFromDict({
        "vertices": union_vertices,
        "faces": union.faces
    })

    print(union_submesh)
    #print(union_submesh.vertices)
    #print(union_submesh.polys)
    #print(union_submesh.edges)

    self.barysub_isect = union_submesh.barycentricUVs()
    self.union_prim = meshutil.RigidPrimitive(self.barysub_isect,ctx)
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
