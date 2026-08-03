#!/usr/bin/env ork.python
###############################################################################
# ork.spatialaudio.demo.py — LIVE spatial-audio showcase player (the owner-run
# command). Builds scn_spatial_audio_showcase's SpatialAudioShowcase scene
# in-process, starts the simulation immediately, opens the REAL audio device,
# and runs the host WindLayer (the runtime-synthesized positioned wind) — the
# one layer the pure-C++ player cannot host.
#
#   ork.spatialaudio.demo.py                # windowed, real audio, mouse cam
#   ork.spatialaudio.demo.py --fullscreen
#   ork.spatialaudio.demo.py --autowalk     # camera rides the scripted
#                                           # listener walk (the harness path)
#
# Camera (manual mode): standard uicam — drag to orbit, wheel zoom; the
# listener is the camera. Sim starts already running; Cmd+Down stops.
###############################################################################

import argparse
import importlib.util
import math
import os
import sys

_ROOT = os.path.abspath(__file__)
for _ in range(3):
  _ROOT = os.path.dirname(_ROOT)
sys.path.insert(0, os.path.join(_ROOT, "obt.project", "scripts"))

from orkengine.core import vec3, vec4, CrcStringProxy, lev2_pyexdir
from orkengine import lev2
from orkengine import ecs
from ork.app.application import ComponentizedApplication
from ork.hypergraph.ecs import EcsRuntime

tokens = CrcStringProxy()
lev2_pyexdir.addToSysPath()

SCENE_PY = os.path.join(_ROOT, "ork.data", "scenes", "scn_spatial_audio_showcase.py")

parser = argparse.ArgumentParser(description="spatial audio showcase (live)")
parser.add_argument("--fullscreen", "-f", action="store_true")
parser.add_argument("--autowalk", action="store_true",
                    help="drive the camera along the scripted listener walk")
args = parser.parse_args()

# import the scene module by path
_spec = importlib.util.spec_from_file_location("scn_spatial_audio_showcase", SCENE_PY)
scn_mod = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(scn_mod)


class ShowcaseDemo(ComponentizedApplication):

  def __init__(self):
    super().__init__()
    self.runtime = EcsRuntime()
    self.wind = scn_mod.WindLayer()
    self.wind_ok = False
    self.time = 0.0
    self.createEzApp(
        name="SpatialAudioShowcase",
        fullscreen=args.fullscreen,
        enable_audio=True,
        enable_audio_output=True,
        enable_audio_synth=True,
        pre_init_fns=[ecs.ecsInitCallback])

  ############################################################################

  def _onUiInit(self):
    lg = self.ezapp.topLayoutGroup
    lg.margin = 0
    lg.clearColorStd = vec4(0.08, 0.08, 0.1, 1)
    vp_item = lg.makeChild(
        fill=True,
        margin=0,
        uiclass=lev2.ui.SceneGraphViewport,
        args=["Viewport", vec4(0.08, 0.08, 0.1, 1)])
    self.sgv = vp_item.widget

  ############################################################################

  def _onGpuInit(self, ctx):
    eye0, tgt0 = scn_mod.listener_pose(0.0)
    self.runtime.setup_camera(eye=eye0, tgt=tgt0)
    self.sgv.cameraName = "spawncam"
    self.sgv.camera_evhandler = lambda ev: self.runtime.handle_camera_event(ev)
    self.sgv.forkDB()

    scene = scn_mod.SpatialAudioShowcase()
    sd = ecs.SceneData()
    scene.build(sd)
    self.runtime.scene_data = sd
    self.runtime.create_scenegraph()
    self.runtime.bind_to_viewport(self.sgv)
    # sim start is DEFERRED to the update loop (a few frames in) so the
    # ECS staging gpu-phase never runs the context loading phase outside a
    # frame (the offscreen-harness segfault; harmless caution windowed).
    self._sim_started = False
    self._gpu_frames = 0

  ############################################################################

  def _onGpuUpdate(self, ctx):
    self._gpu_frames = getattr(self, "_gpu_frames", 0) + 1
    self.runtime.gpuUpdate(ctx)

  ############################################################################

  def _onUpdate(self, updinfo):
    self.time += updinfo.deltatime
    if not getattr(self, "_sim_started", False):
      if getattr(self, "_gpu_frames", 0) >= 3:
        self.runtime.start_simulation()
        self.runtime.bind_to_viewport(self.sgv)
        self._sim_started = True
        self.time = 0.0
        print("spatial audio showcase: simulation RUNNING (listener = camera)")
      return
    if self.runtime.controller is None:
      return
    if args.autowalk and self.runtime._sys_ref:
      # scripted walk: same path the headless gate measures
      t = self.time % scn_mod.WALK_TOTAL
      eye, tgt = scn_mod.listener_pose(t)
      self.runtime.controller.systemNotify(
          self.runtime._sys_ref,
          tokens.UpdateCamera,
          {
            tokens.eye: eye,
            tokens.tgt: tgt,
            tokens.up: vec3(0, 1, 0),
            tokens.near: 0.1,
            tokens.far: 500.0,
            tokens.fovy: math.radians(75.0),
          })
      self.runtime.controller.updateSimulation()
    else:
      # mouse camera: EcsRuntime syncs uicam -> UpdateCamera + ticks the sim
      self.runtime.update(updinfo)

    synth = self.ezapp.audio_synth
    if synth is not None:
      if not self.wind_ok and self.time > 0.5:
        self.wind_ok = self.wind.setup(synth)
        print("wind layer: %s" % ("ON" if self.wind_ok else "FAILED"))
      if self.wind_ok:
        self.wind.update(synth)
    self.sgv.setDirty()


app = ShowcaseDemo()
app.ezapp.mainThreadLoop()
app.ezapp.shutdown()
