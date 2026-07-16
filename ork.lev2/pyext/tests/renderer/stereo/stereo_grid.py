#!/usr/bin/env ork.python

################################################################################
# lev2 sample which renders a scenegraph in VR (stereo) mode.
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################
# Subsystem-startup/shutdown lifecycle via ComponentizedApplication (the
# HFSM-driven path; the legacy ad-hoc inline init is deprecated). Lifecycle
# hooks are the underscore-prefixed template methods (_onGpuInit / _onUpdate /
# _onUiEvent / _onGpuExit); teardown runs automatically at mainThreadLoop() end.
################################################################################

import math, argparse, os
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

parser = argparse.ArgumentParser(description='scenegraph example')
parser.add_argument("--variant", type=int, default=0, help='grid shader variant (1-3)')
parser.add_argument("--vr", action="store_true",
                    help='use the ACTIVE VR device (orkidvr.device()); falls back to NoVR when no runtime')
args = vars(parser.parse_args())
variant = args["variant"]
use_vr = args["vr"]

################################################################################

class StereoApp1(ComponentizedApplication):

  def __init__(self):
    super().__init__()
    self.materials = set()
    self.cameralut = CameraDataLut()
    self.ezapp_args = { "fullscreen": False, "ssaa": 2 }
    self.createEzApp()                                   # subsystem-based by default
    setupUiCamera(app=self, eye=vec3(0, 12, 15))

  ##############################################

  def _onGpuInit(self, ctx):

    # Device selection. WITHOUT --vr this is byte-identical to before: the NoVR
    #  device with the 1280x1280 config. WITH --vr, use the ACTIVE device that
    #  GfxInit selected; only when that is a live OpenXR session do we defer the
    #  view sizes to the runtime (it owns them). No runtime -> graceful NoVR path.
    self.active_openxr = False
    if use_vr:
      dev = orkidvr.device()
      if dev is not None and dev.active and os.environ.get("ORKID_VR_DRIVER") == "openxr":
        self.active_openxr = True

    if self.active_openxr:
      self.vrdev = orkidvr.device()
      self.vrdev.camera = "vrcam"   # scene contract; runtime owns width/height/pose
    else:
      if use_vr:
        print("[stereo_grid] --vr: no active OpenXR runtime — falling back to NoVR path")
      self.vrdev = orkidvr.novr_device()
      self.vrdev.camera = "vrcam"
      self.vrdev.width = 1280
      self.vrdev.height = 1280
    self.IVP = mtx4()

    vars = VarMap()
    vars.SkyboxIntensity = float(1.5)
    vars.DiffuseIntensity = float(1)
    vars.SkyboxTexPathStr = "nebula"

    createSceneGraph(app=self, rendermodel="FWDPBRVRDM", vars=vars)
    onode = self.outputnode  # created by createSceneGraph
    def onCameraChange(cdd):
      eyeindex = cdd.rendererProperty(tokens.eyeindex)
      viewdata = cdd.viewdata
      self.IVP = viewdata.IVPM  # mono IVP
    onode.onCameraChange(lambda cdd: onCameraChange(cdd))
    onode.flipY = True

    ###################################

    self.grid_data = createGridData()
    if variant == 1:
      self.grid_data.shader_suffix = ""
    elif variant == 2:
      self.grid_data.shader_suffix = "_V2"
    elif variant == 3:
      self.grid_data.shader_suffix = "_V3"
    elif variant == 4:
      self.grid_data.shader_suffix = "_V4"
    self.grid_node = self.layer1.createDrawableNodeFromData("grid", self.grid_data)
    self.grid_node.sortkey = 1
    self.scene.lightingmanager.gpuInit(ctx)

  ##############################################

  def _onUiEvent(self, uievent):
    handled = self.uicam.uiEventHandler(uievent)
    if handled:
      self.camera.copyFrom(self.uicam.cameradata)
    return ui.HandlerResult()

  ################################################

  def _onUpdate(self, updinfo):

    if self._shutting_down:
      return

    abstime = updinfo.absolutetime

    ########################################
    # stereo viewing setup
    ########################################

    self.vrdev.FOVD = 90    # degrees
    self.vrdev.IPD = 0.065  # meters
    self.vrdev.near = 0.1   # meters
    self.vrdev.far = 1e5    # meters

    # Under a live OpenXR session the runtime OWNS the head pose + projections
    #  (gpuUpdate rewrites _posemap each frame); don't fight it with a synthetic
    #  orbiting pose. The NoVR path keeps the original demo motion.
    if not self.active_openxr:
      x = math.sin(abstime * 0.125)
      z = -math.cos(abstime * 0.125)

      xf_hmd = mtx4.lookAt(vec3(x, 0.1, z) * -5,   # eye
                           vec3(0, 0, 0),          # tgt
                           vec3(0, 1, 0))          # up

      self.vrdev.setPoseMatrix("hmd", xf_hmd)

    ########################################

    self.scene.updateScene(self.cameralut)

  ################################################
  # GPU exit — drop references in a defined order so scene resources
  # tear down before lev2 shuts down the context.
  ################################################

  def _onGpuExit(self, ctx):
    self.grid_node = None
    self.grid_data = None
    self.scene = None

###############################################################################

StereoApp1().ezapp.mainThreadLoop()
