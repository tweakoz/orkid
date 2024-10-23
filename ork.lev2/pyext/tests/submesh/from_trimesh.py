#!/usr/bin/env python3
################################################################################
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################
import ork.path
from orkengine.core import *
from orkengine.lev2 import *
import trimesh


#!/usr/bin/env python3

################################################################################
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import math, random, argparse, sys
from orkengine.core import *
from orkengine.lev2 import *

################################################################################

l2exdir = (lev2exdir()/"python").normalized.as_string
sys.path.append(l2exdir) # add parent dir to path
from lev2utils.cameras import *
from lev2utils.shaders import *
from lev2utils.misc import *
from lev2utils.primitives import createGridData, createFrustumPrim
from lev2utils.scenegraph import createSceneGraph

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

    ###################################
    # create grid
    ###################################

    self.grid_data = createGridData()
    self.grid_node = self.layer1.createGridNode("grid",self.grid_data)
    self.grid_node.sortkey = 1

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

    capsule = trimesh.primitives.Capsule(radius=1, height=2.0)

    print(capsule.is_watertight)
    print(capsule.euler_number)
    print(capsule.volume)

    trimesh.base.Trimesh

    capsule_vertices = []
    for item in capsule.vertices:
      capsule_vertices.append({
        "p": vec3(item[0], item[1], item[2])
    })  

    capsule_submesh = meshutil.SubMesh.createFromDict({
        "vertices": capsule_vertices,
        "faces": capsule.faces
    })

    print(capsule_submesh)
    #print(capsule_submesh.vertices)
    #print(capsule_submesh.polys)
    #print(capsule_submesh.edges)

    self.barysub_isect = capsule_submesh.withBarycentricUVs()
    self.capsule_prim = RigidPrimitive(capsule_submesh,ctx)
    self.capsule_sgnode = self.capsule_prim.createNode("capsule",self.layer1,solid_wire_pipeline)
    self.capsule_sgnode.enabled = True
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

