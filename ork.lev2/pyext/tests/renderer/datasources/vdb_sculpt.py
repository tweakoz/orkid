#!/usr/bin/env ork.python

import math, sys, random, threading, time, signal
import numpy as np
from obt import path as obt_path 
from ork import path as ork_path
from orkengine.core import vec2,vec3,vec4,CrcStringProxy, lev2_pyexdir
from orkengine.lev2 import vdb as ork_vdb
from orkengine.lev2 import OrkEzApp, RefreshFastest, ui, PBRMaterial, FxPipelinePermutation
from orkengine.lev2 import primitives, RigidPrimitive, meshutil, MicroMesh, Image
sys.path.append(str(ork_path.py_lev2utils)) # add parent dir to path
lev2_pyexdir.addToSysPath()
from cameras import *
import shaders
from primitives import createPointsPrimV12C4, createGridData
from scenegraph import createSceneGraph
#from _boilerplate import BasicUiCamSgApp

tokens = CrcStringProxy()

#############################
# create levelset sphere
#############################

CENTER = vec3(0,0,0)
RADIUS1 = 5.0 
VOXEL_SIZE = RADIUS1/40.0
HALF_WIDTH = 3.0/VOXEL_SIZE
ISO_PARM = 0.95 #float(0.5+math.sin(self.phase*0.81)*0.45)
TIME_RATE = 2.5
STROKE_DIST = 5.7/VOXEL_SIZE
STROKE_RADIUS = 0.5/VOXEL_SIZE
SMOOTHING_PASSES = 4

sphere = ork_vdb.FloatGrid.createLevelSetSphere( "a",         # element name
                                                 RADIUS1,     # world units
                                                 CENTER,      # world units 
                                                 VOXEL_SIZE,  # world units
                                                 HALF_WIDTH)  # voxel units



voxel_brush_dim = 7
voxel_brush = ork_vdb.VoxelMapF(voxel_brush_dim,voxel_brush_dim,voxel_brush_dim)
for ix in range(voxel_brush_dim):
  for iy in range(voxel_brush_dim):
    for iz in range(voxel_brush_dim):
      x = -0.5+float(ix)/float(voxel_brush_dim)
      y = -0.5+float(iy)/float(voxel_brush_dim)
      z = -0.5+float(iz)/float(voxel_brush_dim)
      val = math.sqrt(x*x + y*y + z*z)/math.sqrt(3.0)
      val = math.sqrt(3.0)-val
      print(x,y,z,val)
      voxel_brush.pset(ix,iy,iz,1.02*val)
      
      
#assert(False)
#sphere.background = 0.0

xform = sphere.xform # ork_vdb.Transform.create(1.0)
#print(xform)
colorgrid = ork_vdb.Vec3FGrid.create( "rgb", xform, vec3(1,1,1))
outside = sphere.background

################################################################################

class PointsPrimApp(object):

  def __init__(self):
    super().__init__()
    self.ezapp = OrkEzApp.create(self,msaa=1)
    self.ezapp.setRefreshPolicy(RefreshFastest, 0)
    self.materials = set()
    setupUiCamera( app=self, eye = vec3(6,6,6), constrainZ=True, up=vec3(0,1,0))
    self.phase = 0.0
    self.sphere = sphere 
    self.next_sphere = None 
    self.this_sphere = None
    self.ok_to_exit = False
    self.result_submesh = None
    self.next_submesh = None
    self.this_submesh = None
    self.smoothed = [
      None,
      None,
      None,
      None,
    ]
    
    def latlon_to_xyz(latitude_degrees, longitude_degrees, radius):
        # Convert latitude and longitude from degrees to radians
        latitude = math.radians(latitude_degrees)
        longitude = math.radians(longitude_degrees)
        
        # Calculate Cartesian coordinates
        x = radius * math.cos(latitude) * math.cos(longitude)
        y = radius * math.cos(latitude) * math.sin(longitude)
        z = radius * math.sin(latitude)
        return vec3(x,y,z)
        
    def upd_sphere_fn():
      #counter = 0
      while not self.ok_to_exit:
                
        long = self.phase*TIME_RATE
        lat = self.phase*2.7*TIME_RATE
        center = latlon_to_xyz(lat,long,STROKE_DIST)

        radius = STROKE_RADIUS
        #self.sphere.fill(center,radius,1.0)
        #self.sphere.fill(center*0.95,radius,1.0)
        self.sphere.blitWithBrush(center,voxel_brush)
        paint_color = vec3(1.0,0,0)
        color_radius = 0.001
        center2 = latlon_to_xyz(lat,long,5.3)
        colorgrid.fill(center2,color_radius,paint_color)
        #print(center)
        
        
        mesh_dict = self.sphere.toQuads(ISO_PARM)
        #print(mesh_dict)
        num_verts = len(mesh_dict["vertices"])
        num_faces = len(mesh_dict["faces"])
        #print(f"num_verts:{num_verts} num_faces:{num_faces}")
        if (num_verts>0) and (num_faces>0):
          self.result_submesh = mesh_dict
          self.next_colorgrid = colorgrid.clone 
        else:
          self.result_submesh = None
        self.next_submesh = self.result_submesh
        self.next_sphere = self.sphere
        time.sleep(0.01)

    self.thr = threading.Thread(target=upd_sphere_fn)
    self.thr.start()

    def onCtrlC(signum, frame):
      print("signaling EXIT to ezapp")
      self.ezapp.signalExit()
      self.ok_to_exit = True

    signal.signal(signal.SIGINT, onCtrlC)

    
  ################################################
  # gpu data init:
  #  called on main thread when graphics context is
  #   made available
  ##############################################

  def onGpuInit(self,ctx):

    ###################################
    # create scenegraph
    ###################################

    sg_params = {
      "SkyboxIntensity": 2.0, 
      "DiffuseIntensity": 1.0, 
      "SpecularIntensity": 1.0, 
    }
    
    createSceneGraph( app=self,
                      rendermodel="ForwardPBR",
                      params_dict=sg_params,
                      use_float_buffer=True )

    ###################################
    # create grid
    ###################################

    if False:
      self.grid_data = createGridData()
      self.grid_node = self.layer1.createGridNode("grid",self.grid_data)
      self.grid_node.sortkey = 1

    ###################################
    # create points primitive 
    ###################################
    
    self.points_prim = primitives.PointsPrimitiveV12C4.create(40<<20)
    self.points_prim.updateWithVdbFloatGrid(self.sphere,ctx)

    ###################################
    # create mesh primitive 
    ###################################

    mtl = shaders.createPbrMaterialWithColor( ctx=ctx, 
                                              color = vec4(1,.5,.5,1)*1.5,
                                              metallic = 0.0,
                                              roughness = 0.9 )
    self.mesh_prim = RigidPrimitive()
    self.mesh_node = self.mesh_prim.createNode("mesh-node",self.layer1, mtl)
    
    ##################
    # create shading pipeline
    ##################

    pipeline = shaders.createPipeline( app = self,
                                       ctx = ctx,
                                       shadertext = shaders.POINTCLOUD_SHADERTEXT,
                                       blending=tokens.OFF,
                                       depthtest=tokens.LESS,
                                       techname = "tek_points_fwd",
                                       rendermodel = "ForwardPBR" )

    pointsize_param = pipeline.sharedMaterial.param("pointsize")
    pipeline.bindParam( pointsize_param, 1.0 ) # set pointsize

    ##################
    # create points sg node
    ##################

    #self.points_node = self.points_prim.createNode("node1",self.layer1,pipeline)
    #self.points_node.sortkey = 2;


  ################################################

  def onUpdate(self,updinfo):
    self.abstime = updinfo.absolutetime
    self.scene.updateScene(self.cameralut) # update and enqueue all scenenodes
    self.phase = self.abstime*TIME_RATE
    
  ################################################

  def onDraw(self,drawevent):
    context = drawevent.context
    self.ezapp.processMainSerialQueue()
    
    if self.this_submesh != self.next_submesh:
      self.points_prim.updateWithVdbFloatGrid(self.next_sphere,context)
      if self.next_submesh is not None:
        v = self.next_submesh["vertices"]
        f = self.next_submesh["faces"]
        #self.mesh_prim.fromVertsAndFacesDict(v,f,context)
        as_micromesh = MicroMesh.fromVertAndFaceLists(v,f)
        conn = as_micromesh.vertexConnectivity
        #as_micromesh.asyncSmoothed(conn,SMOOTHING_PASSES,self.mesh_prim,context)
        as_micromesh.asyncSmoothedWithColorGrid(conn,self.next_colorgrid,SMOOTHING_PASSES,self.mesh_prim,context)
      self.this_submesh = self.next_submesh
      self.this_sphere = self.next_sphere

    self.scene.renderOnContext(context);

  ##############################################

  def onUiEvent(self,uievent):
    handled = self.uicam.uiEventHandler(uievent)
    if handled:
      self.camera.copyFrom( self.uicam.cameradata )
    return ui.HandlerResult()
    
  ##############################################

  def onGpuExit(self,ctx):
    print("onGpuExit")
    self.ok_to_exit = True
    self.thr.join()

  ##############################################

  def onUpdateExit(self):
    print("onUpdateExit")
    self.ok_to_exit = True
    self.thr.join()

###############################################################################

def onRunLoopIteration():
  pass

###############################################################################

PointsPrimApp().ezapp.mainThreadLoop(on_iter=onRunLoopIteration)
