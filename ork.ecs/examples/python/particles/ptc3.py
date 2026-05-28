#!/usr/bin/env ork.python

################################################################################
# ptc3.py — ECS particle demo for runtime-mutable exposed parameters.
#
# Hosts elliptical_exposed.py (the elliptical DSL with `expose("Turb")` and
# `expose("Rate")`) as 4 ECS entities at the grid corners. Number keys send
# SET_PARAM notifies that mutate those parameters live, so you can watch the
# turbulence amplitude and emission rate respond without a graph rebuild.
#
# Keys (all broadcast via systemNotify):
#   1 = trickle   Turb=0.0  Rate=0.10
#   2 = default   Turb=1.0  Rate=1.00     (matches elliptical.py constants)
#   3 = chaos     Turb=3.0  Rate=2.50
#   4 = wild      Turb=10.0 Rate=0.30
#
# Siblings: ptc1.py (keyboard-driven START/STOP/PAUSE/RESUME),
#           ptc2.py (timer-driven random spawns).
################################################################################

from orkengine.core import vec3, vec4, CrcStringProxy, lev2_pyexdir
from orkengine import lev2, ecs
from ork.app.application import ComponentizedApplication
from ork.ecs import EcsRuntime
from ork.dflow.particles import resolve_dsl_file, load_dsl_class

tokens = CrcStringProxy()
lev2_pyexdir.addToSysPath()
from lev2utils.primitives import createGridData


LAYERNAME = "std_forward"

# Preset table: keycode → (label, {paramname: value, ...})
PRESETS = {
  49: ("trickle", {"Turb":  0.0, "Rate": 0.10}),  # '1'
  50: ("default", {"Turb":  1.0, "Rate": 1.00}),  # '2'
  51: ("chaos",   {"Turb":  3.0, "Rate": 2.50}),  # '3'
  52: ("wild",    {"Turb": 10.0, "Rate": 0.30}),  # '4'
}


class Ptc3App(ComponentizedApplication):

  def __init__(self, dsl_class):
    super().__init__()
    self._dsl_class = dsl_class
    self.runtime = EcsRuntime()
    self.createEzApp(
      name="ECSParticles-Exposed",
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
    print("ptc3: exposed-param demo")
    for kc in sorted(PRESETS):
      label, params = PRESETS[kc]
      pstr = "  ".join(f"{k}={v}" for k, v in params.items())
      print(f"  {chr(kc)} = {label:<8s}  ({pstr})")

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
      if kc in PRESETS:
        label, params = PRESETS[kc]
        # One SET_PARAM notify per (name, value) pair — handlers on both
        # systemNotify (broadcast → all components, all slots) and
        # componentNotify (per-entity → that component's slots only) paths
        # decode {tokens.name, tokens.value} from the DataTable.
        for pname, pvalue in params.items():
          self.runtime.controller.systemNotify(
            self._sys_ptc, tokens.SET_PARAM,
            {tokens.name: pname, tokens.value: float(pvalue)})
        pstr = "  ".join(f"{k}={v}" for k, v in params.items())
        print(f"SET_PARAM ← {label}  ({pstr})")
        return lev2.ui.HandlerResult()
    return self.runtime.handle_camera_event(uievent)


def main():
  dsl_path  = resolve_dsl_file("elliptical_exposed")
  dsl_class = load_dsl_class(dsl_path)
  print(f"ptc3: hosting {dsl_class.__name__} from {dsl_path.name}")
  app = Ptc3App(dsl_class)
  app.ezapp.mainThreadLoop()
  app.ezapp.shutdown()


if __name__ == "__main__":
  main()
