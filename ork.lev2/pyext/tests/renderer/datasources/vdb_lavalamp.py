#!/usr/bin/env ork.python

import math, sys, random, threading, time, signal
import numpy as np
from collections import deque
from obt import path as obt_path 
from ork import path as ork_path
from orkengine.core import vec2,vec3,CrcStringProxy, lev2_pyexdir, Logger
from orkengine.lev2 import vdb as ork_vdb, OrkEzApp, RefreshFastest, ui, primitives, RigidPrimitive, meshutil, MicroMesh
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

RADIUS1 = 5.0 
VOXEL_SIZE = RADIUS1/14.0
WIDTH = 4.0/VOXEL_SIZE
RADIUS2 = 10.0 
ISO_PARM = 0.87 #float(0.5+math.sin(self.phi*0.81)*0.45)
TIME_RATE = 1.0
sphere = ork_vdb.FloatGrid.createLevelSetSphere( "a", RADIUS1, vec3(0,0,0), VOXEL_SIZE, WIDTH)
SMOOTHING_PASSES = 2
outside = sphere.background

LOGGER = Logger.instance()
logchan = LOGGER.configureChannel("LAVA.GEOM", vec3(1,1,0), True)
perfchan = LOGGER.configureChannel("LAVA.PERF", vec3(1,.5,0), True)
#############################
# execute AX "voxel shader"
#############################

voxel_shader = """

vec3f@pos = getvoxelpws();
vec3f@timeshift = { 0,f$time*-1.0,0 };

vec3f@pos_a = vec3f@pos * 0.1 * f$freq + vec3f@timeshift * 1.0;
vec3f@pos_b = vec3f@pos * 0.17 * f$freq + vec3f@timeshift * 0.7;
vec3f@pos_c = vec3f@pos * 0.37 * f$freq + vec3f@timeshift * 0.46;
vec3f@pos_d = vec3f@pos * 0.57 * f$freq + vec3f@timeshift * 0.27;

f@a  = simplexnoise(vec3f@pos_a)*1.0;
f@a += simplexnoise(vec3f@pos_b)*0.5;
f@a += simplexnoise(vec3f@pos_c)*0.25;
f@a += simplexnoise(vec3f@pos_d)*0.125;

"""

cdata = ork_vdb.ax.CustomData()
ve = ork_vdb.ax.VolumeExecutable.compile(voxel_shader,cdata)

SHADERTEXT = """
////////////////////////////////////////
fxconfig fxcfg_default { glsl_version = "330"; }
////////////////////////////////////////
uniform_set ublock_vtx {
  mat4 mvp;
  float pointsize;
}
////////////////////////////////////////
uniform_set ublock_frg {
  vec4 modcolor;
}
////////////////////////////////////////
vertex_interface vif_x : ublock_vtx {
  inputs {
    vec4 pos : POSITION;
    vec4 nrm : NORMAL;
  }
  outputs {
    vec3 frg_col;
    vec3 frg_nrm;
  }
}
////////////////////////////////////////
fragment_interface fif_x : vif_x : ublock_frg {
  outputs { layout(location = 0) vec4 out_clr; }
}
////////////////////////////////////////
vertex_shader vs_x : vif_x {
  
  frg_col = normalize(pos.xyz);
  frg_nrm = normalize(nrm.xyz);
  gl_Position = mvp * vec4(pos.x,pos.y,pos.z,1);
  gl_PointSize = pointsize;
}
////////////////////////////////////////
fragment_shader fs_x : fif_x {
  vec3 normal=normalize(frg_nrm)*-1.0;
  normal = normal * 0.5 + 0.5; 
  out_clr = vec4(normal.xyz, 1);
}
////////////////////////////////////////
technique tek_x {
  fxconfig = fxcfg_default;
  pass p0 {
    vertex_shader   = vs_x;
    fragment_shader = fs_x;
    state_block     = default;
  }
}
"""
################################################################################

class PointsPrimApp(object):

  def __init__(self):
    super().__init__()
    self.ezapp = OrkEzApp.create(self,msaa=1)
    self.ezapp.setRefreshPolicy(RefreshFastest, 0)
    self.materials = set()
    setupUiCamera( app=self, eye = vec3(6,6,6), constrainZ=True, up=vec3(0,1,0))
    self.phi = 0.0
    self.sphere = sphere 
    self.next_sphere = None 
    self.this_sphere = None
    self.ok_to_exit = False
    self.next_trimesh = None
    self.smoothed = [
      None,
      None,
      None,
      None,
    ]

    # Moving average tracking for onDraw (keep last ~180 samples at 60fps = ~3 seconds)
    self.verts_history = deque(maxlen=180)
    self.faces_history = deque(maxlen=180)
    self.tris_history = deque(maxlen=180)
    self.quads_history = deque(maxlen=180)
    self.last_print_time = time.time()

    # Performance tracking - count calls
    self.frame_count = 0
    self.update_count = 0
    self.vdb_count = 0
    self.last_perf_print_time = time.time()

    def upd_sphere_fn():
      while not self.ok_to_exit:
        #self.sphere = self.sphere.scatterVoxels()
        cdata.set("freq",float(2.0+math.sin(self.phi*0.25)*1.0))
        cdata.set("time",self.phi*0.5)
        #cdata.set("freq",float(self.phi))
        ve.executeOnGrid(self.sphere)

        mesh_dict = self.sphere.toTriMeshNumpy(ISO_PARM)
        num_verts = len(mesh_dict["vertices"])
        num_faces = len(mesh_dict["faces"])

        if (num_verts>0) and (num_faces>0):
          self.next_trimesh = mesh_dict
        else:
          self.next_trimesh = None
        self.next_sphere = self.sphere

        # Increment VDB count
        self.vdb_count += 1

        time.sleep(0.01666)

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
    self.grid_node = self.layer1.createDrawableNodeFromData("grid",self.grid_data)
    self.grid_node.sortkey = 1

    ###################################
    # create points primitive 
    ###################################
    
    self.points_prim = primitives.PointsPrimitiveV12C4.create(40<<20)
    self.points_prim.updateWithVdbFloatGrid(self.sphere,ctx)

    ###################################
    # create mesh primitive 
    ###################################

    self.mesh_pipe = createPipeline( app = self, 
                                     ctx=ctx, 
                                     rendermodel = "ForwardPBR", 
                                     shadertext=SHADERTEXT,
                                     techname = "tek_x",
                                    )
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

    pointsize_param = pipeline.sharedMaterial.param("pointsize")
    pipeline.bindParam( pointsize_param, 1.0 ) # set pointsize

    ##################
    # create points sg node
    ##################

    self.primnode = self.points_prim.createNode("node1",self.layer1,pipeline)
    self.primnode.sortkey = 2;
    self.next_umesh = None
    
    self._umesh = MicroMesh.fromVertAndFaceLists( [], [] )

  ################################################

  def onUpdate(self,updinfo):
    # Increment update count
    self.update_count += 1

    self.abstime = updinfo.absolutetime
    self.scene.updateScene(self.cameralut) # update and enqueue all scenenodes
    self.phi = self.abstime*TIME_RATE
    if self.next_trimesh != None:
      v = self.next_trimesh["vertices"]
      f = self.next_trimesh["faces"]
      #print(f"updating trimesh: v:{len(v)} f:{len(f)}")
      self._umesh.updateFromLists(v,f)
      conn = self._umesh.vertexConnectivity
      self._umesh.asyncSmoothed(conn,
                                SMOOTHING_PASSES,
                                None,None)
      self._umesh.computeNormals()
      self.next_umesh = self._umesh
      self.next_trimesh = None
    
  ################################################

  def onDraw(self,drawevent):
    context = drawevent.context
    self.ezapp.processMainSerialQueue()

    # Increment frame count
    self.frame_count += 1

    # Track moving average of mesh stats
    num_verts = self._umesh.num_verts
    num_faces = self._umesh.num_faces
    num_tris = self._umesh.num_tris
    num_quads = self._umesh.num_quads
    self.verts_history.append(num_verts)
    self.faces_history.append(num_faces)
    self.tris_history.append(num_tris)
    self.quads_history.append(num_quads)

    # Print stats every 3 seconds
    current_time = time.time()
    if current_time - self.last_print_time >= 3.0:
      avg_verts = sum(self.verts_history) / len(self.verts_history) if self.verts_history else 0
      avg_faces = sum(self.faces_history) / len(self.faces_history) if self.faces_history else 0
      avg_tris = sum(self.tris_history) / len(self.tris_history) if self.tris_history else 0
      avg_quads = sum(self.quads_history) / len(self.quads_history) if self.quads_history else 0
      logchan.log(f"avg_verts<{avg_verts:.1f}>  avg_tris<{avg_tris:.1f}>  avg_quads<{avg_quads:.1f}>  avg_faces<{avg_faces:.1f}>")
      self.last_print_time = current_time

    # Print performance stats every 3 seconds
    if current_time - self.last_perf_print_time >= 3.0:
      elapsed = current_time - self.last_perf_print_time
      fps = self.frame_count / elapsed
      ups = self.update_count / elapsed
      vps = self.vdb_count / elapsed
      perfchan.log(f"FPS<{fps:.1f}>  UPS<{ups:.1f}>  VPS<{vps:.1f}>")
      # Reset counters
      self.frame_count = 0
      self.update_count = 0
      self.vdb_count = 0
      self.last_perf_print_time = current_time

    ##############################################
    # if there is a new vdb sphere,
    #   (which was computed on the vdb thread)
    #   update the points primitive
    ##############################################
    if self.next_sphere != None:
      self.points_prim.updateWithVdbFloatGrid(self.next_sphere,context)
      self.next_sphere = None
    ##############################################
    # if there is a new smoothed micromesh,
    #   (which was computed on the update thread)
    #   update the mesh primitive
    ##############################################
    if self.next_umesh != None:
      self.mesh_prim.updateWithMicroMesh(self.next_umesh,context)
      self.next_umesh = None
    ##############################################    

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
