#!/usr/bin/env ork.python

################################################################################
# ptc2.py — timer-driven ECS particle demo with random spawns + clean drains.
#
# Periodically spawns a ParticlesComponent entity at a random location on the
# grid. Each instance EMITS for a random [4, 8] seconds, then is sent STOP
# (transition to DRAINING) so its in-flight particles complete naturally
# rather than cutting off mid-flight. The entity is despawned only after
# the drain grace period elapses (longer than the longest particle lifespan
# in the graph) — guaranteeing no visible pop.
#
# Up to 3 instances coexist; over-cap requests are skipped until a slot
# frees up.
#
# Sibling: ptc1.py — keyboard-driven demo with 4 fixed corner entities.
################################################################################

import random, sys

from orkengine.core import vec3, vec4, CrcStringProxy, lev2_pyexdir
from orkengine import lev2, ecs
from ork.app.application import ComponentizedApplication
from ork.ecs import EcsRuntime
from ork.dflow.particles import resolve_dsl_file, load_dsl_class

tokens = CrcStringProxy()
lev2_pyexdir.addToSysPath()
from lev2utils.primitives import createGridData


LAYERNAME       = "std_forward"
MAX_CONCURRENT  = 3
LIFETIME_MIN    = 4.0
LIFETIME_MAX    = 8.0
SPAWN_INTERVAL  = 1.5            # min seconds between spawns when room available
GRID_EXTENT     = 12.0           # ± on x and z
DRAIN_GRACE     = 3.0            # seconds AFTER stop_time before despawn —
                                 # must be ≥ the graph's longest particle
                                 # lifespan. elliptical.py has LifeSpan=2.0,
                                 # so 3.0 gives a comfortable margin.


class Ptc2App(ComponentizedApplication):

  def __init__(self, dsl_class):
    super().__init__()
    self._dsl_class       = dsl_class
    self.runtime          = EcsRuntime()
    # Each entry: dict { ent, stop_at, despawn_at, stopped }
    self._active          = []
    self._last_spawn_time = None
    self.createEzApp(
      name="ECSParticles-Random",
      fullscreen=False,
      pre_init_fns=[ecs.ecsInitCallback])

  ##############################################################################

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
    self.sgv.camera_evhandler = lambda ev: self.runtime.handle_camera_event(ev)
    self.sgv.forkDB()

    self._build_scene()
    self._start()

    print(f"ptc2: random-spawn ECS particles, max {MAX_CONCURRENT} concurrent, "
          f"life [{LIFETIME_MIN}, {LIFETIME_MAX}]s + {DRAIN_GRACE}s drain")

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
      "SkyboxTexPathStr": "ork_envmaps|pillars4k",
    })
    sd.declareSystem("ParticlesGlobalSystem")

    arch_ptc = sd.declareArchetype("ArchParticles")
    c_ptc    = arch_ptc.declareComponent("ParticlesComponent")

    self._ptc_system = self._dsl_class()
    drawable_data = lev2.ParticlesDrawableData()
    drawable_data.graphdata = self._ptc_system.generatedflow()
    c_ptc.drawabledata = drawable_data
    c_ptc.layername    = LAYERNAME
    c_ptc.pool_size    = 12
    c_ptc.duration     = 0.0     # runs until we send STOP
    # drain_linger doesn't strictly drive despawn (Python owns timing), but
    # match DRAIN_GRACE so the slot internally recycles around when we kill it.
    c_ptc.drain_linger = DRAIN_GRACE

    sp = sd.declareSpawner("ptc_random")
    sp.archetype = arch_ptc
    sp.autospawn = False

  def _create_scenegraph_with_grid(self):
    self.runtime.create_scenegraph()
    grid_node = self.runtime.layer.createDrawableNodeFromData("grid", self.grid_data)
    grid_node.sortkey = 1
    grid_node.pickable = False

  def _start(self):
    self._create_scenegraph_with_grid()
    self.runtime.start_simulation()
    self.runtime.bind_to_viewport(self.sgv)

  ##############################################################################

  def _onGpuUpdate(self, ctx):
    self.runtime.gpuUpdate(ctx)

  def _onUpdate(self, updinfo):
    if self.runtime.controller:
      self.runtime.update(updinfo)
    self.sgv.setDirty()
    self._tick_spawner(updinfo)

  ##############################################################################

  def _tick_spawner(self, updinfo):
    """Three-phase lifecycle per spawn:
        spawn → wait stop_at → STOP (DRAINING) → wait despawn_at → despawn.
    The drain phase between stop_at and despawn_at lets in-flight particles
    finish naturally, so the entity vanishes only after the screen is empty
    of its particles."""
    now = self._sim_now(updinfo)
    ctrl = self.runtime.controller

    survivors = []
    for entry in self._active:
      # Phase 2 → 3: hit STOP at stop_at if we haven't already.
      if not entry["stopped"] and now >= entry["stop_at"]:
        cref = ctrl.findComponent(entry["ent"], "ParticlesComponent")
        ctrl.componentNotify(cref, tokens.STOP, True)
        entry["stopped"] = True
        print(f"  STOP @ t={now:.2f}s — DRAINING for {DRAIN_GRACE:.1f}s "
              f"(then despawn)")

      # Phase 3 → done: despawn once the drain grace has elapsed.
      if now >= entry["despawn_at"]:
        ctrl.despawnEntity(entry["ent"])
        print(f"  despawn @ t={now:.2f}s  (concurrent now {len(survivors)})")
      else:
        survivors.append(entry)
    self._active = survivors

    # Spawn gating.
    if len(self._active) >= MAX_CONCURRENT:
      return
    if self._last_spawn_time is not None and \
       (now - self._last_spawn_time) < SPAWN_INTERVAL:
      return

    # Spawn one at a random position with a random emission lifetime.
    x = random.uniform(-GRID_EXTENT, GRID_EXTENT)
    z = random.uniform(-GRID_EXTENT, GRID_EXTENT)
    lifetime = random.uniform(LIFETIME_MIN, LIFETIME_MAX)

    sad = ecs.SpawnAnonDynamic("ptc_random")
    sad.overridexf.translation = vec3(x, 1.0, z)
    sad.overridexf.scale = 1.0
    entref = ctrl.spawnEntity(sad)

    self._active.append({
      "ent":        entref,
      "stop_at":    now + lifetime,
      "despawn_at": now + lifetime + DRAIN_GRACE,
      "stopped":    False,
    })
    self._last_spawn_time = now
    print(f"  spawn @ t={now:.2f}s pos=({x:+.1f}, {z:+.1f}) emit={lifetime:.2f}s "
          f"(concurrent now {len(self._active)})")

  def _sim_now(self, updinfo):
    """Simulation game time — falls back to absolutetime if controller missing."""
    if self.runtime.controller:
      return updinfo.absolutetime
    return 0.0


def main():
  dsl_path  = resolve_dsl_file("elliptical")
  dsl_class = load_dsl_class(dsl_path)
  print(f"ptc2: hosting {dsl_class.__name__} from {dsl_path.name}")
  app = Ptc2App(dsl_class)
  app.ezapp.mainThreadLoop()
  app.ezapp.shutdown()


if __name__ == "__main__":
  main()
