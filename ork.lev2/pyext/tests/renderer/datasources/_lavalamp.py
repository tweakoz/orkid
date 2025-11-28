#!/usr/bin/env ork.python

import math, threading, time, sys
import numpy as np
from collections import deque
from ork import path as ork_path
from orkengine.core import vec2, vec3, CrcStringProxy, Logger, lev2_pyexdir
from orkengine.lev2 import vdb as ork_vdb, primitives, RigidPrimitive, MicroMesh
from ork.app.application import ApplicationComponent
from concurrent.futures import ThreadPoolExecutor

sys.path.append(str(ork_path.py_lev2utils)) # add parent dir to path
lev2_pyexdir.addToSysPath()

from shaders import createPipeline


tokens = CrcStringProxy()
geoexec_st1 = ThreadPoolExecutor(max_workers=1)
geoexec_st2 = ThreadPoolExecutor(max_workers=1)
geoexec_st3 = ThreadPoolExecutor(max_workers=1)

#############################
# VDB Lavalamp Component
#############################

class LavalampComponent(ApplicationComponent):
  """Component that manages VDB-based lavalamp effect with mesh generation"""

  def __init__(self):
    super().__init__()

    # VDB sphere parameters
    self.RADIUS1 = 5.0
    self.VOXEL_SIZE = self.RADIUS1/14.0
    self.WIDTH = 4.0/self.VOXEL_SIZE
    self.RADIUS2 = 10.0
    self.ISO_PARM = 0.87
    self.TIME_RATE = 1.0

    # Create levelset sphere
    self.sphere = ork_vdb.FloatGrid.createLevelSetSphere(
      "a", self.RADIUS1, vec3(0,0,0), self.VOXEL_SIZE, self.WIDTH)
    self.outside = self.sphere.background

    # Logger channels
    self.LOGGER = Logger.instance()
    self.logchan = self.LOGGER.configureChannel("LAVA.GEOM", vec3(1,1,0), True)
    self.perfchan = self.LOGGER.configureChannel("LAVA.PERF", vec3(1,.5,0), True)

    # AX voxel shader
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

    self.cdata = ork_vdb.ax.CustomData()
    self.ve = ork_vdb.ax.VolumeExecutable.compile(voxel_shader, self.cdata)

    # Mesh shader
    self.SHADERTEXT = """
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

    # Thread control
    self.next_sphere = None
    self.this_sphere = None
    self.ok_to_exit = False
    self.next_trimesh = None
    self.thr = None

    # Moving average tracking for mesh stats (last ~180 samples at 60fps = ~3 seconds)
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

    # Will be initialized in onGpuInit
    self.points_prim = None
    self.mesh_prim = None
    self.mesh_pipe = None
    self.next_umesh = None
    self._umesh = None
    self.phi = 0.0

  ################################################
  # GPU initialization
  ################################################

  def _onGpuInit(self, ctx):
    """Initialize GPU resources"""

    # Create points primitive
    self.points_prim = primitives.PointsPrimitiveV12C4.create(40<<20)
    self.points_prim.updateWithVdbFloatGrid(self.sphere, ctx)

    # Create mesh primitive and pipeline
    self.mesh_pipe = createPipeline(
      app = self.app,
      ctx = ctx,
      rendermodel = "ForwardPBR",
      shadertext = self.SHADERTEXT,
      techname = "tek_x",
    )

    self.mesh_prim = RigidPrimitive()
    self._umesh = MicroMesh.fromVertAndFaceLists([], [])

    # Create points pipeline
    from orkengine.core import CrcStringProxy
    tokens = CrcStringProxy()
    from shaders import POINTCLOUD_SHADERTEXT

    self.points_pipeline = createPipeline(
      app = self.app,
      ctx = ctx,
      shadertext = POINTCLOUD_SHADERTEXT,
      blending = tokens.OFF,
      depthtest = tokens.LESS,
      techname = "tek_points_fwd",
      rendermodel = "ForwardPBR"
    )

    pointsize_param = self.points_pipeline.sharedMaterial.param("pointsize")
    self.points_pipeline.bindParam(pointsize_param, 1.0)

    # Start VDB update thread
    def upd_sphere_fn():
      while not self.ok_to_exit:
        self.cdata.set("freq", float(2.0 + math.sin(self.phi * 0.25) * 1.0))
        self.cdata.set("time", self.phi * 0.5)
        self.ve.executeOnGrid(self.sphere)

        mesh_dict = self.sphere.toTriMeshNumpy(self.ISO_PARM)
        num_verts = len(mesh_dict["vertices"])
        num_faces = len(mesh_dict["faces"])

        if (num_verts > 0) and (num_faces > 0):
          self.next_trimesh = mesh_dict
        else:
          self.next_trimesh = None
        self.next_sphere = self.sphere

        # Increment VDB count
        self.vdb_count += 1

        if self.next_trimesh != None:
          def _st1(v,f):
            def _st2(st1_mesh):
              def _st3(st2_mesh):
                st2_mesh.computeNormals()
                self.next_umesh = st2_mesh
              ###############################
              # stage 3
              ###############################
              conn_st2 = st1_mesh.vertexConnectivity
              geoexec_st3.submit(_st3, st1_mesh.smooth(conn_st2) )
            #################################
            # stage 2
            #################################
            umesh_st1 = MicroMesh.fromVertAndFaceLists(v,f)
            conn_st1 = umesh_st1.vertexConnectivity
            geoexec_st2.submit(_st2, umesh_st1.smooth(conn_st1) )
          ###################################
          # stage 1
          ###################################
          v = self.next_trimesh["vertices"]
          f = self.next_trimesh["faces"]
          geoexec_st1.submit(_st1, v, f)
          self.next_trimesh = None

        time.sleep(0.01666)
        

    self.thr = threading.Thread(target=upd_sphere_fn)
    self.thr.start()

  ################################################
  # GPU link - create scene graph nodes
  ################################################

  def _onGpuLink(self, ctx):
    """Create scene graph nodes after app scene graph is ready"""
    SGC = self.app.findComponentByName("std_scenegraph")
    # Create mesh scene graph node
    self.mesh_node = self.mesh_prim.createNode("mesh-node", SGC.layer1, self.mesh_pipe)
    self.mesh_node.sortkey = 2

    # Create points scene graph node
    self.points_node = self.points_prim.createNode("points-node", SGC.layer1, self.points_pipeline)
    self.points_node.sortkey = 2

  ################################################
  # Update (runs on update thread)
  ################################################

  def _onUpdate(self, updinfo):
    """Process mesh updates on update thread"""

    # Increment update count
    self.update_count += 1

    self.phi = updinfo.absolutetime * self.TIME_RATE

  ################################################
  # GPU update (runs on GPU/draw thread)
  ################################################

  def _onGpuUpdate(self, ctx):
    """Update GPU primitives on draw thread"""

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
      self.logchan.log(f"avg_verts<{avg_verts:.1f}>  avg_tris<{avg_tris:.1f}>  avg_quads<{avg_quads:.1f}>  avg_faces<{avg_faces:.1f}>")
      self.last_print_time = current_time

    # Print performance stats every 3 seconds
    if current_time - self.last_perf_print_time >= 0.25:
      elapsed = current_time - self.last_perf_print_time
      fps = self.frame_count / elapsed
      ups = self.update_count / elapsed
      vps = self.vdb_count / elapsed
      #self.perfchan.log(f"FPS<{fps:.1f}>  UPS<{ups:.1f}>  VPS<{vps:.1f}>")
      self.perfchan.perfItem("FPS", fps)
      self.perfchan.perfItem("UPS", ups)
      self.perfchan.perfItem("VPS", vps)
      # Reset counters
      self.frame_count = 0
      self.update_count = 0
      self.vdb_count = 0
      self.last_perf_print_time = current_time

    # Update points primitive if new VDB sphere is available
    if self.next_sphere != None:
      self.points_prim.updateWithVdbFloatGrid(self.next_sphere, ctx)
      self.next_sphere = None

    # Update mesh primitive if new smoothed micromesh is available
    if self.next_umesh != None:
      um = self.next_umesh
      self.next_umesh = None
      self.mesh_prim.updateWithMicroMesh(um, ctx)

  ################################################
  # Cleanup
  ################################################

  def _onGpuExit(self, ctx):
    """Cleanup GPU resources"""
    self.ok_to_exit = True
    if self.thr:
      self.thr.join()

  def _onUpdateExit(self):
    """Cleanup update thread resources"""
    self.ok_to_exit = True
    if self.thr:
      self.thr.join()
