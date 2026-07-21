#!/usr/bin/env ork.python

################################################################################
# lev2 sample: articulated hand tracking (generic OpenXR XR_EXT_hand_tracking).
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################
# Reads the per-hand joint state each frame from the ACTIVE VR device and drives
# small markers at the palm + five fingertips of each hand. When hand tracking is
# unavailable (no runtime, or a runtime/system without XR_EXT_hand_tracking) the
# markers are simply hidden and the scene renders normally — the example runs
# windowless under a live XR session (ownsHmdPresentation) OR headless-degraded.
#
#   run under XR (owner box):   ORKID_VR_DRIVER=openxr ./stereo_handtracking.py --vr
#   run degraded (no runtime):  ./stereo_handtracking.py
#
# Marker poses are in the device XR reference space, the SAME convention as the
# controller grip poses (orkidvr publishes both through the shared pose path).
################################################################################

import math, argparse, os
import numpy as np
from orkengine.core import *
from orkengine.lev2 import *

################################################################################

lev2_pyexdir.addToSysPath()
from lev2utils.cameras import *
from lev2utils.shaders import *
from lev2utils.misc import *
from lev2utils.primitives import createGridData
from lev2utils.scenegraph import createSceneGraph
from ork.app.application import ComponentizedApplication

################################################################################

parser = argparse.ArgumentParser(description='hand-tracking scenegraph example')
parser.add_argument("--vr", action="store_true",
                    help='use the ACTIVE VR device (orkidvr.device()); falls back to NoVR when no runtime')
args = vars(parser.parse_args())
use_vr = args["vr"]

# joint ordinals in the default 26-joint set we place markers on (palm + fingertips).
PALM = 0
FINGERTIPS = [5, 10, 15, 20, 25]  # thumb, index, middle, ring, little tips
MARKER_JOINTS = [PALM] + FINGERTIPS

# bone segments (parent -> child) over the standard XR_EXT_hand_tracking 26-joint set:
# wrist(1) -> each finger's metacarpal, then chained to the tip. Palm(0) is derived — no bone.
WRIST = 1
BONES = []
for _base in (2, 6, 11, 16, 21):            # thumb, index, middle, ring, little
  _chain = [WRIST] + list(range(_base, _base + (4 if _base == 2 else 5)))
  BONES += list(zip(_chain[:-1], _chain[1:]))

################################################################################

class HandTrackApp(ComponentizedApplication):

  def __init__(self):
    super().__init__()
    self.materials = set()
    self.cameralut = CameraDataLut()
    self.ezapp_args = { "fullscreen": False, "ssaa": 2 }
    self.createEzApp()
    setupUiCamera(app=self, eye=vec3(0, 12, 15))

  ##############################################

  def _onGpuInit(self, ctx):

    self.active_openxr = False
    if use_vr:
      dev = orkidvr.device()
      if dev is not None and dev.active and os.environ.get("ORKID_VR_DRIVER") == "openxr":
        self.active_openxr = True

    if self.active_openxr:
      self.vrdev = orkidvr.device()
      self.vrdev.camera = "vrcam"   # runtime owns width/height/pose
    else:
      if use_vr:
        print("[stereo_handtracking] --vr: no active OpenXR runtime — falling back to NoVR path")
      self.vrdev = orkidvr.novr_device()
      self.vrdev.camera = "vrcam"
      self.vrdev.width = 1280
      self.vrdev.height = 1280

    # one-line availability report (honest degrade; live joints are owner-verified).
    self.hands_ok = hasattr(self.vrdev, "hand_tracking_supported")
    if self.hands_ok:
      print("[stereo_handtracking] hand_tracking_supported =", self.vrdev.hand_tracking_supported)
    else:
      print("[stereo_handtracking] hand-tracking binding absent in this build — markers disabled")

    vars = VarMap()
    vars.SkyboxIntensity = float(1.5)
    vars.DiffuseIntensity = float(1)
    vars.SkyboxTexPathStr = "nebula"
    createSceneGraph(app=self, rendermodel="FWDPBRVRDM", vars=vars)
    self.outputnode.flipY = True

    ###################################
    # ground grid for spatial reference
    ###################################

    self.grid_data = createGridData()
    self.grid_node = self.layer1.createDrawableNodeFromData("grid", self.grid_data)
    self.grid_node.sortkey = 1

    ###################################
    # marker instances: palm + 5 fingertips per hand, as PROCEDURAL UNIT SPHERES
    #  (MicroMesh -> RigidPrimitive + stock PBRMaterial — rides the same PBR
    #  technique set as models, so the FWDPBRVRDM stereo path is unchanged).
    #  Unit radius means node scale == joint radius directly. Hidden until a
    #  valid, active joint places them each frame; per-hand colors for
    #  handedness readability in the HMD.
    ###################################

    def make_unit_sphere_arrays(n=12):
      # CCW-from-outside lat/lon sphere (procedural_pbr test convention).
      verts, norms = [], []
      for i in range(n + 1):
        lat = math.pi * i / n
        for j in range(n * 2):
          lon = math.tau * j / (n * 2)
          x = math.sin(lat) * math.cos(lon)
          y = math.cos(lat)
          z = math.sin(lat) * math.sin(lon)
          verts.append((x, y, z))
          norms.append((x, y, z))
      w = n * 2
      faces = []
      for i in range(n):
        for j in range(w):
          a = i * w + j
          b = a + 1 if j < w - 1 else i * w
          c = a + w
          d = c + 1 if j < w - 1 else (i + 1) * w
          faces.extend([3, a, b, c, 3, b, d, c])
      verts_np = np.array(verts, dtype=np.float32)
      norms_np = np.array(norms, dtype=np.float32)
      up = np.array([0, 1, 0], dtype=np.float32)
      binormals_np = np.cross(norms_np, up)
      degen = np.linalg.norm(binormals_np, axis=1) < 1e-6
      binormals_np[degen] = np.cross(norms_np[degen], [1, 0, 0])
      lens = np.linalg.norm(binormals_np, axis=1, keepdims=True)
      binormals_np = binormals_np / np.where(lens < 1e-10, 1.0, lens)
      return verts_np, norms_np, binormals_np, faces

    def make_marker_prim(color):
      sv, sn, sb, sf = make_unit_sphere_arrays()
      # BGRA pre-swap: MicroMesh::updateRigidPrim packs colors via fvec4::ARGBU32
      # which lands as BGRA in memory; the GPU reads RGBA.
      colors = np.tile(np.array([color[2], color[1], color[0], 1.0],
                                dtype=np.float32), (len(sv), 1))
      mesh = MicroMesh.fromVertAndFaceLists(sv, sf)
      mesh.updateNormals(sn)
      mesh.updateBinormals(sb)
      mesh.updateColors(colors)
      prim = RigidPrimitive()
      prim.updateWithMicroMesh(mesh, ctx, tokens.TRIANGLES)
      return prim

    white_img  = Image.createFromFile("src://effect_textures/white.dds")
    normal_img = Image.createFromFile("src://effect_textures/default_normal.dds")
    def make_marker_material():
      mtl = PBRMaterial()
      mtl.assignImages(ctx, color=white_img, normal=normal_img,
                       mtlruf=white_img, doConform=True)
      mtl.baseColor = vec4(1, 1, 1, 1)
      mtl.roughnessFactor = 0.4
      mtl.metallicFactor = 0.0
      mtl.gpuInit(ctx)
      return mtl

    MARKER_COLORS = { "left": (0.2, 0.6, 1.0), "right": (1.0, 0.5, 0.15) }
    self.marker_prims = {}
    self.marker_mtl = make_marker_material()
    self.markers = { "left": {}, "right": {} }
    for side in ("left", "right"):
      self.marker_prims[side] = make_marker_prim(MARKER_COLORS[side])
      for j in MARKER_JOINTS:
        node = self.marker_prims[side].createNode("%s_j%d" % (side, j), self.layer1, self.marker_mtl)
        node.worldTransform.scale = 0.0  # hidden until placed
        self.markers[side][j] = node

    ###################################
    # bone segments: one UNIT-Z thin box per bone through the SAME stock-PBR path
    #  (stereo-safe). Node transform does all the work per frame: translation =
    #  parent joint, orientation = +Z rotated onto the bone direction, uniform
    #  scale = bone length (thickness rides length — a natural taper). No
    #  per-frame GPU uploads.
    ###################################

    def make_unit_bone_arrays(half=0.06):
      # thin box: cross-section (+-half, +-half) in XY, length 1 along +Z, CCW-outside.
      v = [(-half,-half,0),( half,-half,0),( half, half,0),(-half, half,0),
           (-half,-half,1),( half,-half,1),( half, half,1),(-half, half,1)]
      f = []
      quads = [(0,3,2,1),(4,5,6,7),(0,1,5,4),(2,3,7,6),(1,2,6,5),(3,0,4,7)]
      for a,b,c,d in quads:
        f.extend([3,a,b,c, 3,a,c,d])
      return np.array(v, dtype=np.float32), f

    def make_bone_prim(color):
      bv, bf = make_unit_bone_arrays()
      colors = np.tile(np.array([color[2], color[1], color[0], 1.0],
                                dtype=np.float32), (len(bv), 1))
      mesh = MicroMesh.fromVertAndFaceLists(bv, bf)
      mesh.computeNormals()
      mesh.updateColors(colors)
      prim = RigidPrimitive()
      prim.updateWithMicroMesh(mesh, ctx, tokens.TRIANGLES)
      return prim

    self.bone_prims = {}
    self.bones = { "left": {}, "right": {} }
    for side in ("left", "right"):
      # bones slightly dimmer than the markers so the tips/palm read as the accents.
      c = MARKER_COLORS[side]
      self.bone_prims[side] = make_bone_prim((c[0]*0.55, c[1]*0.55, c[2]*0.55))
      for (a, b) in BONES:
        node = self.bone_prims[side].createNode("%s_b%d_%d" % (side, a, b), self.layer1, self.marker_mtl)
        node.worldTransform.scale = 0.0  # hidden until placed
        self.bones[side][(a, b)] = node

    self.scene.lightingmanager.gpuInit(ctx)

  ##############################################

  def _onUiEvent(self, uievent):
    handled = self.uicam.uiEventHandler(uievent)
    if handled:
      self.camera.copyFrom(self.uicam.cameradata)
    return ui.HandlerResult()

  ################################################

  @staticmethod
  def _quat_z_to(direction):
    # rotation taking +Z onto 'direction' (unit). Degenerate cases: parallel -> identity,
    # antiparallel -> 180deg about X (any perpendicular axis works).
    # NOTE the NEGATED angle: ork quat/matrix rotation sense is inverted vs the standard
    # right-handed active convention (empirically: quat(+Y, +90deg) takes +Z to -X — the
    # house CONJUGATE trap), so the conjugate is what lands +Z on 'direction'.
    zhat = vec3(0, 0, 1)
    c = zhat.dot(direction)
    if c > 0.9999:
      return quat(vec3(0, 1, 0), 0.0)
    if c < -0.9999:
      return quat(vec3(1, 0, 0), math.pi)
    axis = zhat.cross(direction).normalized
    return quat(axis, -math.acos(max(-1.0, min(1.0, c))))

  def _place_hand(self, side, handstate):
    nodes = self.markers[side]
    bones = self.bones[side]
    # inactive hand -> hide every marker + bone (never leave stale positions).
    if (handstate is None) or (not handstate.active):
      for node in nodes.values():
        node.worldTransform.scale = 0.0
      for node in bones.values():
        node.worldTransform.scale = 0.0
      return
    for j in MARKER_JOINTS:
      node = nodes[j]
      jp = handstate.joint(j)
      if jp.position_valid:
        pos = jp.matrix.translation
        node.worldTransform.translation = pos
        # scale to the reported joint radius (floored so it stays visible).
        node.worldTransform.scale = max(jp.radius, 0.008)
      else:
        node.worldTransform.scale = 0.0
    for (a, b), node in bones.items():
      ja = handstate.joint(a)
      jb = handstate.joint(b)
      if ja.position_valid and jb.position_valid:
        pa = ja.matrix.translation
        pb = jb.matrix.translation
        seg = pb - pa
        length = seg.length
        if length > 1.0e-4:
          node.worldTransform.translation = pa
          node.worldTransform.orientation = self._quat_z_to(seg * (1.0 / length))
          node.worldTransform.scale = length
          continue
      node.worldTransform.scale = 0.0

  ################################################

  def _onUpdate(self, updinfo):

    if self._shutting_down:
      return

    abstime = updinfo.absolutetime

    self.vrdev.FOVD = 90
    self.vrdev.IPD = 0.065
    self.vrdev.near = 0.1
    self.vrdev.far = 1e5

    if not self.active_openxr:
      # NoVR preview: a slow orbit so the (hidden) markers + grid are inspectable.
      x = math.sin(abstime * 0.125)
      z = -math.cos(abstime * 0.125)
      xf_hmd = mtx4.lookAt(vec3(x, 0.1, z) * -5, vec3(0, 0, 0), vec3(0, 1, 0))
      self.vrdev.setPoseMatrix("hmd", xf_hmd)

    # drive the markers from the live hand state (no-op / hidden when unavailable).
    if self.hands_ok:
      self._place_hand("left", self.vrdev.left_hand)
      self._place_hand("right", self.vrdev.right_hand)

    self.scene.updateScene(self.cameralut)

  ################################################

  def _onGpuExit(self, ctx):
    self.markers = None
    self.marker_prims = None
    self.bones = None
    self.bone_prims = None
    self.marker_mtl = None
    self.grid_node = None
    self.grid_data = None
    self.scene = None

###############################################################################

HandTrackApp().ezapp.mainThreadLoop()
