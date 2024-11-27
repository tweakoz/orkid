#!/usr/bin/env ork.python

import math, sys, random, threading, time, signal
import numpy as np
from obt import path as obt_path 
from ork import path as ork_path
from orkengine.core import vec2,vec3,CrcStringProxy, lev2_pyexdir
from orkengine.lev2 import vdb as ork_vdb, OrkEzApp, RefreshFastest, ui, primitives, RigidPrimitive, meshutil
sys.path.append(str(ork_path.py_lev2utils)) # add parent dir to path
lev2_pyexdir.addToSysPath()
from cameras import *
from shaders import POINTCLOUD_SHADERTEXT, createPipeline, pseudowire_pipeline
from primitives import createPointsPrimV12C4, createGridData
from scenegraph import createSceneGraph
#from _boilerplate import BasicUiCamSgApp

tokens = CrcStringProxy()

#############################
# create levelset sphere
#############################

radius = 10.0 
desired_num_points = 10000000
voxel_size = 0.5 #radius / math.cbrt(desired_num_points);
sphere = ork_vdb.FloatGrid.createLevelSetSphere( "a", radius, vec3(0,0,0), voxel_size, 1.05)
outside = sphere.background

print(f"voxel_size:{voxel_size}")

#############################
# execute AX "voxel shader"
#############################

voxel_shader = """

vec3f@pos = getvoxelpws();
float@dist = length(vec3f@pos);
float@phi = atan2(vec3f@pos.z,vec3f@pos.x);
float@theta = atan2(vec3f@pos.y,vec3f@pos.x);
float@omega = atan2(vec3f@pos.z,vec3f@pos.y);

f@a = 1.0;
f@a = f@a * cos(float@phi*f$freq)*0.5+0.5;
//f@a = f@a * cos(float@theta*f$freq)*0.5+0.5;
//f@a = f@a * cos(float@omega*f$freq)*0.5+0.5;

//if (f@a<0.5) {
//  deletepoint(); // only for point grids, not volume grids
//}

"""

cdata = ork_vdb.ax.CustomData()
ve = ork_vdb.ax.VolumeExecutable.compile(voxel_shader,cdata)

################################################################################

class PointsPrimApp(object):

  def __init__(self):
    super().__init__()
    self.ezapp = OrkEzApp.create(self)
    self.ezapp.setRefreshPolicy(RefreshFastest, 0)
    self.materials = set()
    setupUiCamera( app=self, eye = vec3(6,6,6), constrainZ=True, up=vec3(0,1,0))
    self.phi = 0.0
    self.sphere = sphere 
    self.next_sphere = None 
    self.this_sphere = None
    self.ok_to_exit = False
    self.result_submesh = None
    self.next_submesh = None
    self.this_submesh = None
    
    def upd_sphere_fn():
      #counter = 0
      while not self.ok_to_exit:
        #self.sphere = self.sphere.scatterVoxels()
        cdata.set("freq",float(4.5+math.sin(self.phi*0.1)*4.25))
        #cdata.set("freq",float(self.phi))
        ve.executeOnGrid(self.sphere)
        
        iso_parm = float(0.5+math.sin(self.phi*3.81)*0.45)
        print(f"iso_parm:{iso_parm}")
        mesh_dict = self.sphere.toQuads(iso_parm)
        #print(mesh_dict)
        num_verts = len(mesh_dict["vertices"])
        num_faces = len(mesh_dict["faces"])
        if (num_verts>0) and (num_faces>0):
          self.result_submesh = mesh_dict #meshutil.SubMesh.createFromDict2(mesh_dict)
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
      "SkyboxIntensity": 1.0, 
      "DiffuseIntensity": 6.0, 
    }
    
    createSceneGraph(app=self,rendermodel="ForwardPBR",params_dict=sg_params)

    ###################################
    # create grid
    ###################################

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

    self.mesh_pipe = createPipeline( app = self, ctx=ctx, rendermodel = "ForwardPBR", techname="std_mono_forward_lit" )
    #self.mesh_pipe = pseudowire_pipeline( app = self, ctx=ctx )
    self.mesh_prim = RigidPrimitive()
    self.mesh_node = self.mesh_prim.createNode("mesh-node",self.layer1, self.mesh_pipe)
    
    ##################
    # create shading pipeline
    ##################

    pipeline = createPipeline( app = self,
                               ctx = ctx,
                               shadertext = POINTCLOUD_SHADERTEXT,
                               blending=tokens.OFF,
                               depthtest=tokens.LESS,
                               techname = "tek_points_fwd",
                               rendermodel = "ForwardPBR" )

    def _pointsize():
      val = float(float(2.0+math.sin(self.phi*2.0)*2.0))
      return 1.5

    pointsize_param = pipeline.sharedMaterial.param("pointsize")
    pipeline.bindParam( pointsize_param, lambda : _pointsize() ) # set pointsize

    ##################
    # create points sg node
    ##################

    self.primnode = self.points_prim.createNode("node1",self.layer1,pipeline)
    self.primnode.sortkey = 2;


  ################################################

  def onUpdate(self,updinfo):
    self.abstime = updinfo.absolutetime
    self.scene.updateScene(self.cameralut) # update and enqueue all scenenodes
    self.phi = self.abstime
    
  ################################################

  def onDraw(self,drawevent):
    context = drawevent.context
    self.ezapp.processMainSerialQueue()
    
    if self.this_submesh != self.next_submesh:
      self.points_prim.updateWithVdbFloatGrid(self.next_sphere,context)
      if self.next_submesh is not None:
        v = self.next_submesh["vertices"]
        f = self.next_submesh["faces"]
        self.mesh_prim.fromVertsAndFacesDict(v,f,context)

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
