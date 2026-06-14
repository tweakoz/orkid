#!/usr/bin/env ork.python

################################################################################
# elliptical_ecs.py — keyboard-driven ECS particle demo.
#
# Hosts the HyperSyn elliptical particle system as 4 ECS entities at the grid
# corners. Each entity has one slot (pool_size=1, no duration), all 4 are
# auto-fired on spawn, and the keyboard controls all 4 in unison via
# systemNotify broadcast.
#
# Sibling: elliptical_ecs_timer.py — timer-driven demo with pool + duration.
#
# Usage:
#   elliptical_ecs.py
#   elliptical_ecs.py --dsl elliptical_per_particle
#
# Keys (all broadcast to every entity via systemNotify):
#   1 = START   — re-arm + run (resets graphinst)
#   2 = STOP    — DRAINING (in-flight particles continue, no new emissions)
#   3 = PAUSE   — freeze in place
#   4 = RESUME  — un-freeze (no reset)
################################################################################

import argparse, sys
from pathlib import Path

from orkengine.core import vec3, vec4, CrcStringProxy, lev2_pyexdir
from orkengine import lev2, ecs
from ork.app.application import ComponentizedApplication
from ork.ecs import EcsRuntime
from ork.dflow.particles import resolve_dsl_file, load_dsl_class

tokens = CrcStringProxy()
lev2_pyexdir.addToSysPath()
from lev2utils.primitives import createGridData


LAYERNAME = "std_forward"


class EllipticalEcsApp(ComponentizedApplication):

  def __init__(self, dsl_class):
    super().__init__()
    self._dsl_class = dsl_class
    self.runtime = EcsRuntime()
    self.createEzApp(
      name="EllipticalEcs",
      fullscreen=False,
      pre_init_fns=[ecs.ecsInitCallback])

  def _onUiInit(self):
    lg = self.ezapp.topLayoutGroup
    lg.margin = 0
    lg.clearColorStd = vec4(0.08, 0.08, 0.1, 1)
    vp_item = lg.makeChild(
      fill=True, margin=0,
      uiclass=lev2.ui.SceneGraphViewport,
      args=["Viewport", vec4(0.08, 0.08, 0.1, 1)])
    self.sgv = vp_item.widget

  def _onGpuInit(self, ctx):
    self.runtime.setup_camera(eye=vec3(0, 4, 25))
    self.grid_data = createGridData()
    self.sgv.cameraName = "spawncam"
    self.sgv.camera_evhandler = lambda ev: self._onCameraEvent(ev)
    self.sgv.forkDB()

    self._build_scene()
    self._start()

    self._sys_ptc = self.runtime.controller.findSystem("ParticlesGlobalSystem")
    print("ECS Particles Demo — keys: 1=START  2=STOP  3=PAUSE  4=RESUME")

  def _build_scene(self):
    sd = self.runtime.scene_data

    sysdata_sg = sd.declareSystem("SceneGraphSystem")
    sysdata_sg.declareLayer(LAYERNAME)
    sysdata_sg.declareParams({
      "preset": "ForwardPBR",
      "SkyboxIntensity":  float(1.0),
      "DiffuseIntensity": float(1.0),
      "SpecularIntensity":float(1.0),
      "AmbientLight":     vec3(0.1),
      "SkyboxTexPathStr": "pillars",
    })
    sd.declareSystem("ParticlesGlobalSystem")

    arch_ptc = sd.declareArchetype("ArchParticles")
    c_ptc    = arch_ptc.declareComponent("ParticlesComponent")

    self._ptc_system = self._dsl_class()
    drawable_data = lev2.ParticlesDrawableData()
    drawable_data.graphdata = self._ptc_system.generatedflow()
    c_ptc.drawabledata = drawable_data
    c_ptc.layername    = LAYERNAME
    # Single-slot, no auto-completion — slots run until STOPped manually.
    c_ptc.pool_size = 1
    c_ptc.duration  = 0.0

    CORNER = 12.0
    corners = [
      ("ptc_NE", vec3( CORNER, 1.0,  CORNER)),
      ("ptc_NW", vec3(-CORNER, 1.0,  CORNER)),
      ("ptc_SE", vec3( CORNER, 1.0, -CORNER)),
      ("ptc_SW", vec3(-CORNER, 1.0, -CORNER)),
    ]
    for name, pos in corners:
      sp = sd.declareSpawner(name)
      sp.archetype = arch_ptc
      sp.autospawn = True
      sp.transform.translation = pos

  def _create_scenegraph_with_grid(self):
    self.runtime.create_scenegraph()
    grid_node = self.runtime.layer.createDrawableNodeFromData("grid", self.grid_data)
    grid_node.sortkey = 1
    grid_node.pickable = False

  def _start(self):
    self._create_scenegraph_with_grid()
    self.runtime.start_simulation()
    self.runtime.bind_to_viewport(self.sgv)

  def _onGpuUpdate(self, ctx):
    self.runtime.gpuUpdate(ctx)

  def _onUpdate(self, updinfo):
    if self.runtime.controller:
      self.runtime.update(updinfo)
    self.sgv.setDirty()

  def _onCameraEvent(self, uievent):
    if uievent.code == tokens.KEY_DOWN.hashed and self._sys_ptc:
      kc = uievent.keycode
      handlers = {
        49: ("START",  tokens.START),   # '1'
        50: ("STOP",   tokens.STOP),    # '2'
        51: ("PAUSE",  tokens.PAUSE),   # '3'
        52: ("RESUME", tokens.RESUME),  # '4'
      }
      if kc in handlers:
        name, tok = handlers[kc]
        self.runtime.controller.systemNotify(self._sys_ptc, tok, True)
        print(f"systemNotify ← {name}")
        return lev2.ui.HandlerResult()
    return self.runtime.handle_camera_event(uievent)


def main():
  parser = argparse.ArgumentParser(description="Keyboard-driven ECS particles demo")
  parser.add_argument("--dsl", default="elliptical",
                      help="DSL name (resolved via ORK_PARTICLES_SEARCH_PATH; default 'elliptical')")
  args = parser.parse_args()

  dsl_path  = resolve_dsl_file(args.dsl)
  dsl_class = load_dsl_class(dsl_path)
  print(f"elliptical_ecs: hosting {dsl_class.__name__} from {dsl_path.name}")
  app = EllipticalEcsApp(dsl_class)
  app.ezapp.mainThreadLoop()
  app.ezapp.shutdown()


if __name__ == "__main__":
  main()
