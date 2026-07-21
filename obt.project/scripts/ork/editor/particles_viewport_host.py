################################################################################
# particles_viewport_host — the PARTICLES family's live viewport component for the
# standalone dflow editor shell (ork.dflow.edit.py). JUL13_DFLOW E7.
#
# The shell's viewport hosts a RUNNING particle system built FROM THE BINDING'S LIVE
# GraphData (the exact object the canvas + propsheet edit — never a re-load). It
# implements the SAME family-neutral compose seam terrain/hypermesh document:
#   * PRIMARY (gpuInit / viewport_setup): build a minimal ForwardPBR scenegraph that
#     hosts a ParticlesDrawableData built over the live GraphData, a camera framed on
#     the emitter volume, the family default envmap, and bind the shell's viewport.
#   * CONTRIBUTOR (composeInto / viewport_compose): fold the particle drawable into a
#     PRIMARY host's scene (fireball over terrain in multi-doc) via the primary's
#     generic external-decorator seam, anchored on the terrain surface.
#
# TRANSPORT IS THE POINT for this family. The particle sim advances by
# GraphInst::compute(updata); the drawable is built with external_compute=True so its
# own enqueue lambda NEVER advances the sim — THIS host owns the advance and gates it on
# the transport: PLAYING advances updata + calls compute() (particles move); PAUSED /
# STOPPED hold (the last computed frame stays frozen in the render triple-buffer, and the
# camera still orbits). tick_count is the observable — it advances only on a PLAYING tick.
#
# LIVE-EDIT coupling (honest v1): a graph edit (propsheet param OR add/delete/connect/bypass)
# mutates the live GraphData, but a RUNNING GraphInst copies each unconnected literal into
# its plug instance at instantiation (dataflow::inpluginst ctor) — it does NOT re-read the
# GraphData's plug values per compute. So the honest coupling is RE-INSTANTIATION: an edit
# requests a rebake (_requestRebake) that rebuilds the drawable + graphinst on the next tick.
# BYPASS is a first-class structural edit here: the rebake builds from the family's EFFECTIVE
# graph (bypassed chain OPS wired around + dropped, bypassed RENDERERS omitted) on a CLONE, so
# a bypassed op genuinely leaves the sim (its compute never runs on the shared pool) while the
# shared canvas graph stays pristine. No silent stale-graph display; a failed rebake fails LOUDLY.
################################################################################

import math

from orkengine.core import vec3, vec4, VarMap, lev2_pyexdir, UpdateData, Object, dataflow
from orkengine import lev2
from orkengine.lev2 import (CameraDataLut, ParticlesDrawableData,
                            particles_drawable_graphinst, PostFxNodeACES, PostFxNodeHSVG)

# transport states (family-neutral; identical labels to terrain/hypermesh hosts)
STOPPED = "stopped"
PLAYING = "playing"
PAUSED = "paused"

# IBL environment: a dark night dome so the HDR-additive fire READS (a black viewport is a
# gate failure, but a bright dome would wash the flame out). The established skybox param.
_SKYBOX = "<ork_envmaps2>/blender_night.xir"

# fixed sim step (seconds). The transport advances the world clock by this per PLAYING tick
# so pausing freezes the animation deterministically (identical to the hypermesh host's
# per-tick fixed dt) — no wall-clock jump on resume.
_DT = 1.0 / 60.0

# COMPOSE PLACEMENT (v1, contributor-into-terrain-primary): a particle system is WORLD-SPACE
# (the sprite/streak renderers draw at absolute particle positions with an IDENTITY model
# matrix — the sgnode transform does NOT move or scale the rendered particles, only the emit
# origin the emitter reads from its Offset plug). So placement is done on the GRAPH: the emit
# origin is lifted onto the terrain surface (else it emits at its authored ~origin height,
# buried under a km-scale terrain) and — because a human-scale fire viewed from an extent-scaled
# terrain camera is sub-pixel — the fire is scaled to a terrain-visible landmark size (emitter
# spread AND sprite size, the latter a scale transform appended to the Size curve so a chunky
# few-hundred-meter flame reads from km away). The scale is applied to a CLONE of the graph
# (serialize/deserialize) so the shared canvas/propsheet graph the user edits stays PRISTINE at
# its authored human scale; a PRIMARY (standalone) uses the live graph unscaled, byte-identical.
_COMPOSE_VEL_FRAC = 0.022     # emitter emission velocity as a fraction of the primary XZ extent
_COMPOSE_RAD_FRAC = 0.009     # emitter emission radius as a fraction of the primary XZ extent
_COMPOSE_SPRITE_FRAC = 0.032  # sprite Size multiplier (× extent) so flames read as landmarks


class ParticlesViewportHost:
  """Viewport host for the particles family. Owns (as PRIMARY) a ForwardPBR scenegraph
  hosting a RUNNING ParticlesDrawableData built over the live GraphData + an orbit camera,
  binds it into the shell's SceneGraphViewport, drives the transport-gated sim advance, and
  re-instantiates the drawable on a graph edit. As a CONTRIBUTOR it folds its drawable into
  the primary host's scene instead."""

  def __init__(self, graphdata, *, title="particles", on_status=None, skybox=None,
               capabilities=None, testbench=None, bench_enabled=False, graph_factory=None):
    self._graph = graphdata          # the live dflow.GraphData (the SAME object the canvas edits)
    self._title = title
    self._on_status = on_status
    self._skybox = skybox or _SKYBOX

    # TESTBENCH (editor-only stimulus): a passive testbench.Testbench + the current enabled
    # state + a graph_factory(enabled) -> graphdata that re-instantiates the DSL with/without
    # the bench's construction kwargs (the honest enabled-toggle rebake). Motion is LIVE: the
    # bench's motion program is evaluated per transport tick into _bench_time and fed to the
    # running graphinst's ENTITY RESOLVER (setEntityResolver) — so the emitter, bound to a
    # bench entity, reads a moving transform every tick with NO rebake. None -> no bench, and
    # the resolver is never wired (a graphinst without a bench behaves byte-identically to before).
    self._bench = testbench
    self._bench_enabled = bool(bench_enabled)
    self._graph_factory = graph_factory
    self._bench_time = 0.0            # bench motion clock (advances only on a PLAYING tick)
    # the particles family capability mask (role-based bypassability + effective-graph
    # construction). Defaulted here so a direct construction (a test / a sibling tool) still
    # gets real bypass semantics; the shell injects the SAME object the editor document uses.
    if capabilities is None:
      from ork.hypergraph.dflow.particles.capabilities import ParticlesEditorCapabilities
      capabilities = ParticlesEditorCapabilities()
    self._caps = capabilities

    self._ctx = None
    self._sgv = None
    self._scenegraph = None
    self._layer = None
    self._cameralut = None
    self._camera = None
    self._uicam = None
    self._camname = "spawncam"
    self._aces = None
    self._hsvg = None

    # PRIMARY drawable state (the running particle system)
    self._drawable = None            # the CallbackDrawable (particles)
    self._graphinst = None           # the live GraphInst compute() advances (transport-gated)
    self._node = None                # the scene node hosting the drawable
    self._node_ctr = 0
    self._updata = UpdateData()      # the world clock the transport advances
    self._updata.absolutetime = 0.0
    self._updata.deltatime = _DT

    # CONTRIBUTOR state: set in composeInto (this host owns no scene of its own then).
    self._primary = None
    self._compose_ctx = None
    self._composed_node = None       # the CURRENT scenegraph's contributor node (suppression handle)
    self._composed_drawable = None   # keep the contributor drawable alive across primary rebuilds
    self._composed_graphinst = None

    # rebake pipeline (rebuild-on-explicit-rebake v1)
    self._rebake_pending = False
    # invalid-topology hold: True while the last rebake soft-failed (createDrawable -> None,
    # S8.5) and the last-good drawable is being held frozen on-screen with the sim stopped.
    # Cleared the next rebake that yields a valid drawable (recover-on-restart).
    self._topology_invalid = False

    # transport
    self._state = PLAYING
    self._tick_count = 0
    self._rebuild_count = 0

  ##############################################################################
  # compose seam (family-neutral viewport-host protocol)
  ##############################################################################

  @property
  def has_viewport_payload(self):
    return True

  @property
  def animates(self):
    """This payload ANIMATES per-frame while PLAYING (particles move) — the shell's selftest
    uses this to run the stronger PIXEL-diff transport proof (playing frames DIFFER, paused
    frames are IDENTICAL) that a static family (terrain/a static mesh) cannot give."""
    return True

  @property
  def display_flag_gates_render(self):
    """A particle graph has NO per-node display marker: EVERY non-bypassed renderer draws (a
    multi-terminal graph composes sprites + streaks + aux in one pass), and terminal visibility
    is controlled by BYPASS, not a display flag (unlike terrain/hypermesh, where the display flag
    picks which node's product materializes). So the display affordance is ABSENT here (the
    capability declares supports_display_flags=False) and the flagtest's display A/B oracle is
    inapplicable; the bypass oracle (real effective-graph re-instantiation) is the meaningful one."""
    return False

  def createScene(self, ctx):
    """PRIMARY role seam alias (terrain uses gpuInit)."""
    self.gpuInit(ctx)

  def composeInto(self, primary, ctx):
    """CONTRIBUTOR role: build THIS graph's particle drawable and fold it into `primary`'s
    scene via the primary's generic external-decorator seam (so it survives the primary's
    scene rebuilds). The drawable runs AUTONOMOUSLY (external_compute=False) since the shell
    drives only the PRIMARY host's update()/gpuUpdate(); it advances whenever the composed
    scene is enqueued. Falls back to a one-shot add if the primary predates that seam."""
    self._primary = primary
    self._compose_ctx = ctx
    self._buildComposedDrawable(ctx)
    if self._composed_drawable is None:
      self._status("compose: no particle drawable — particles payload contributes nothing")
      return

    def _decorate(scenegraph, layer):
      self._node_ctr += 1
      node = layer.createDrawableNode(
          "particles_%s_%d" % (self._title, self._node_ctr), self._composed_drawable)
      self._composed_node = node                    # suppression handle (visibility oracle)

    if hasattr(primary, "add_external_decorator"):
      primary.add_external_decorator(_decorate)     # re-applied on every primary scene rebuild
    else:
      sg = getattr(primary, "scenegraph", None)
      layer = self._primaryForwardLayer(primary)
      if sg is not None and layer is not None:
        _decorate(sg, layer)
        self._status("compose: primary has no external-decorator seam — added once (will drop on "
                     "a primary rebake; open the particles source standalone for a persistent view)")
      else:
        self._status("compose: could not reach the primary's scenegraph/layer — payload not folded")

  def addContributor(self, host):
    """PRIMARY role: a NON-particles contributor folded into a particles primary is not
    supported in v1 (this host hosts only its own particle payload). Refuse LOUDLY."""
    self._status("addContributor: compose INTO a particles primary is unsupported in v1 — open the "
                 "terrain source first so it is the primary (its payload hosts the particles tab)")

  def rebuildComposed(self, ctx):
    """PRIMARY role: nothing composed into a particles primary (see addContributor)."""
    pass

  def _primaryForwardLayer(self, primary):
    layer = getattr(primary, "layer", None)
    if layer is not None:
      return layer
    rt = getattr(primary, "runtime", None)
    return getattr(rt, "layer", None) if rt is not None else None

  def _buildComposedDrawable(self, ctx):
    """CONTRIBUTOR: build an AUTONOMOUS particle drawable over a terrain-PLACED, terrain-SCALED
    CLONE of the graph. The clone (serialize/deserialize round-trip) is scaled + anchored so the
    shared canvas graph the user edits stays PRISTINE at its authored human scale. Ops self-defend:
    a clone/placement/build failure leaves _composed_drawable None + a loud status (never black-
    and-silent), falling back to the live graph unplaced so the fire at least RUNS."""
    graph = self._placedScaledClone()
    try:
      dd = ParticlesDrawableData()
      dd.graphdata = graph
      dd.external_compute = False               # autonomous: the composed scene's enqueue advances it
      self._composed_drawable = dd.createDrawable()
      self._composed_graphinst = particles_drawable_graphinst(self._composed_drawable)
    except Exception as ex:
      self._composed_drawable = None
      self._status(f"compose: particle drawable build failed: {ex}")

  def _placedScaledClone(self):
    """Return a CLONE of the live graph, anchored on the PRIMARY terrain surface and scaled to a
    terrain-visible landmark size. When the primary exposes no terrain surface sampler (a particles
    primary / standalone), OR the clone/scale fails, returns the LIVE graph unchanged so the fire
    still runs. Only the emitter's typed Offset (vec3) + EmissionVelocity/EmissionRadius (float)
    plugs and the sprite Size curve's floatxf transformer are touched — all type-matched, guarded."""
    rt = getattr(self._primary, "runtime", None)
    if rt is None or not hasattr(rt, "terrain_height"):
      return self._graph
    extent = float(getattr(rt, "extent_m", 0.0) or 0.0)
    if extent <= 0.0:
      return self._graph
    try:
      clone = Object.deserializeJson(self._graph.serializeJson())
    except Exception as ex:
      self._status(f"compose: graph clone failed ({ex}) — composing the live graph unscaled")
      return self._graph
    try:
      rt._load_display_heights()
    except Exception:
      pass
    try:
      surf = float(rt.terrain_height(0.0, 0.0))
    except Exception:
      surf = 0.0
    vel = _COMPOSE_VEL_FRAC * extent
    rad = _COMPOSE_RAD_FRAC * extent
    emitter = self._findEmitter(clone)
    if emitter is not None:
      try:
        # a CONTRIBUTOR is bench-free in v1: if the emit Offset carries a bench-entity binding
        # (emitter_entity="@bench"), disconnect it so the terrain ANCHOR literal is authoritative
        # (a connected plug would otherwise override the literal). The primary/standalone view is
        # where the bench animates; a composed contributor uses the static anchored emit origin.
        try:
          clone.disconnect(emitter.inputs.Offset)
        except Exception:
          pass
        emitter.inputs.Offset = vec3(0.0, surf + rad, 0.0)   # emit origin ON the surface
      except Exception as ex:
        self._status(f"compose: emitter Offset set skipped ({ex})")
      for pname, pval in (("EmissionVelocity", vel), ("EmissionRadius", rad)):
        try:
          setattr(emitter.inputs, pname, float(pval))
        except Exception:
          pass
    else:
      self._status("compose: no emitter module found — particles stay at authored world origin")
    self._scaleSpriteSize(clone, _COMPOSE_SPRITE_FRAC * extent)
    self._status("compose: anchored on terrain surface y=%.1f, emission_vel=%.0fm/s sprite×%.0f "
                 "(terrain-scaled clone; canvas graph unchanged)"
                 % (surf, vel, _COMPOSE_SPRITE_FRAC * extent))
    return clone

  def _scaleSpriteSize(self, graph, factor):
    """Multiply the sprite renderer's Size by `factor` by appending a floatxf.scale to its Size
    plug's transformer chain (the Size is authored as a life curve — scaling the curve keeps its
    SHAPE, only its magnitude grows so flames become landmark-scale). Guarded: a renderer without
    a Size transformer (a literal-size DSL) is skipped rather than aborting."""
    P = lev2.particles
    for cls in (P.SpriteRenderer, P.StreakRenderer):
      try:
        r = graph.findModuleByClass(cls)
      except Exception:
        r = None
      if r is None:
        continue
      try:
        sizeplug = r.inputs.Size
        xf = sizeplug.transformer if sizeplug is not None else None
        if xf is None:
          continue
        sc = dataflow.floatxf.scale()
        sc.scale = float(factor)
        xf.append(sc)
      except Exception as ex:
        self._status(f"compose: sprite Size scale skipped for {cls} ({ex})")

  def _findEmitter(self, graph):
    """The first emitter module in `graph`, or None. Emitters are the modules whose reflected
    class name ends in 'Emitter' (Ring/Nozzle/Elliptical/Line)."""
    P = lev2.particles
    for cls in (P.RingEmitter, P.EllipticalEmitter, P.NozzleEmitter, P.LineEmitter):
      try:
        m = graph.findModuleByClass(cls)
      except Exception:
        m = None
      if m is not None:
        return m
    return None

  def setComposedVisible(self, visible):
    """Toggle the CURRENT composed contributor node's render enable — the visibility oracle's
    suppression handle (render honors Node::_enabled). Returns True if a node was toggled."""
    n = self._composed_node
    if n is None:
      return False
    n.enabled = bool(visible)
    return True

  ##############################################################################
  # PRIMARY: GPU init — scenegraph + camera + the running particle drawable
  ##############################################################################

  def gpuInit(self, ctx):
    self._ctx = ctx
    self._buildScene(ctx)
    if self._buildDrawable():
      self._installDrawable()

  def _buildScene(self, ctx):
    vm = VarMap()
    vm.preset = "ForwardPBR"
    vm.SkyboxTexPathStr = self._skybox
    vm.SkyboxIntensity = 1.0
    vm.DiffuseIntensity = 1.0
    vm.SpecularIntensity = 1.0
    vm.AmbientLevel = vec3(0.07)
    vm.ssaa = 0
    vm.msaa = 0

    # ACES tone-map + HSVG color-grade (neutral) so the HDR-additive fire tone-maps into a
    # readable image (bloom-friendly float buffer) — mirrors ork.particle.viewer.py's chain.
    self._aces = PostFxNodeACES()
    self._aces.exposure = 1.0
    self._aces.gpuInit(ctx, 8, 8)
    self._aces.addToSceneVars(vm, "PostFxChain")
    self._hsvg = PostFxNodeHSVG()
    self._hsvg.hue = 0.0
    self._hsvg.saturation = 1.0
    self._hsvg.value = 1.0
    self._hsvg.gamma = 1.0
    self._hsvg.gpuInit(ctx, 8, 8)
    self._hsvg.addToSceneVars(vm, "PostFxChain")

    self._scenegraph = lev2.scenegraph.Scene(vm)
    self._layer = self._scenegraph.createLayer("std_forward")
    self._scenegraph.createLayer("depth_prepass")
    pbr = self._scenegraph.pbr_common
    pbr.useFloatColorBuffer = True
    self._scenegraph.lightingmanager.gpuInit(ctx)

    lev2_pyexdir.addToSysPath()
    from lev2utils.cameras import setupUiCameraX
    self._cameralut = CameraDataLut()
    eye, tgt = vec3(0, 3.5, 17), vec3(0, 3, 0)
    # bench camera HINT (optional) — applied ONLY when the bench is enabled, so a
    # disabled/absent bench keeps the default frame byte-identical to the pre-bench view.
    if self._bench is not None and self._bench_enabled and self._bench.camera is not None:
      eye, tgt = self._benchCameraEyeTarget(self._bench.camera)
    self._camera, self._uicam = setupUiCameraX(
        near=0.1, far=20000.0, fov_deg=45, cameralut=self._cameralut, camname=self._camname,
        eye=eye, tgt=tgt, up=vec3(0, 1, 0))

  def _benchCameraEyeTarget(self, hint):
    """(eye, target) from a testbench.CameraHint: orbit `distance` from `target` at the hint's
    elevation/azimuth. Guarded — a malformed hint falls back to the default frame."""
    try:
      el = math.radians(hint.elevation_deg)
      az = math.radians(hint.azimuth_deg)
      tgt = hint.target
      direction = vec3(math.cos(el) * math.sin(az), math.sin(el), math.cos(el) * math.cos(az))
      return tgt + direction * hint.distance, tgt
    except Exception as ex:
      self._status(f"bench camera hint ignored ({ex})")
      return vec3(0, 3.5, 17), vec3(0, 3, 0)

  def _buildDrawable(self):
    """Build the particle drawable over the EFFECTIVE graph — the family capability folds the
    live GraphData's current bypass state into a transient sim graph (bypassed chain OPS wired
    around + dropped; bypassed RENDERERS omitted from the render set; a just-added FULLY-FLOATING
    op excluded until wired) on a serialize/deserialize CLONE, so the shared canvas graph the user
    edits stays PRISTINE (byte-identical when nothing is bypassed). external_compute=True so THIS
    host owns the transport-gated sim advance.

    Returns True and COMMITS a new drawable + graphinst on success. On an INVALID-topology soft-fail
    (createDrawable -> None, S8.5 — e.g. the user just deleted a mid-chain module, breaking the pool
    chain) it returns False WITHOUT touching the last-good _drawable: the drawable is built into
    LOCALS first and only committed once valid, so the last good frame is held frozen on-screen
    (sim stopped, loud status) — never nulled, never raised. The next valid rebake resumes."""
    graph = self._caps.build_effective_graph(self._graph)
    ops, renderers = self._caps.bypass_targets(self._graph)
    if ops or renderers:
      note = []
      if ops:
        note.append("wired around ops [%s]" % ", ".join(ops))
      if renderers:
        note.append("omitted renderers [%s]" % ", ".join(renderers))
      self._status("effective graph: " + "; ".join(note))
      if renderers and len(renderers) >= len(self._caps.render_terminals(self._graph)):
        self._status("ALL render terminals bypassed — empty (honest) render this rebake")
    for name in self._caps.unwired_ops(self._graph):
      self._status(f"{name} unconnected — excluded until wired (preview keeps running)")
    try:
      dd = ParticlesDrawableData()
      dd.graphdata = graph
      dd.external_compute = True                # transport owns compute() — NOT the enqueue lambda
      drawable = dd.createDrawable()
    except Exception as ex:
      self._status(f"particle drawable build failed: {ex}")
      self._holdOnInvalidTopology()
      return False
    if drawable is None:                        # S8.5 soft-fail: invalid topology
      self._holdOnInvalidTopology()
      return False
    graphinst = particles_drawable_graphinst(drawable)
    if graphinst is None:
      self._holdOnInvalidTopology()
      return False
    # COMMIT the valid drawable + (re)wire the transport-gated sim; recover-on-restart.
    resumed = self._topology_invalid
    self._topology_invalid = False
    self._drawable = drawable
    self._graphinst = graphinst
    self._wireBenchResolver(self._graphinst)
    if resumed:
      self._status("topology valid again — sim resumed")
    return True

  def _holdOnInvalidTopology(self):
    """S8.5 soft-fail landed: the effective graph is invalid (a mid-chain module was deleted, or
    a required pool input is unconnected) so createDrawable returned None. KEEP the last-good
    drawable displayed — it stays in its scene node, frozen in the render triple-buffer — and STOP
    the sim by dropping the graphinst (the transport-gated advance halts). Never null the drawable,
    never raise. The next rebake that yields a valid drawable resumes the transport (_buildDrawable).
    Owner law: a topology edit never crashes and never blacks out — a bad graph freezes the last
    good frame until the user fixes it."""
    self._graphinst = None
    if not self._topology_invalid:
      self._topology_invalid = True
      self._status("invalid topology — sim stopped; fix the graph to resume")

  def _wireBenchResolver(self, graphinst):
    """Wire the bench's entity resolver onto `graphinst` — ONLY when a bench is active. The
    emitter (bound to a bench entity via its published-name seam) then reads the moving
    transform each compute() tick. No bench / disabled -> the resolver is NEVER touched, so
    _resolveEntityXf stays null and the sim behaves exactly as it did before benches existed."""
    if graphinst is None or self._bench is None or not self._bench_enabled:
      return
    try:
      graphinst.setEntityResolver(self._benchResolver)
    except Exception as ex:                     # ops self-defend: a bad wire fails loud, not black
      self._status(f"bench resolver wire failed: {ex}")

  def _benchResolver(self, name):
    """resolver(entity_name) -> vec3 world position | None. Evaluates the bench entity's
    deterministic motion program at the current transport-locked bench clock. None for an
    unbound name (the module falls back to its identity default). LIVE: a motion-param edit
    changes the program in place, so the very next tick moves the emitter — no rebake."""
    bench = self._bench
    if bench is None or not self._bench_enabled:
      return None
    prog = bench.entities.get(name)
    if prog is None:
      return None
    try:
      return prog.position_at(self._bench_time)
    except Exception as ex:
      self._status(f"bench motion eval failed for {name!r}: {ex}")
      return None

  def setBenchEnabled(self, enabled):
    """Toggle the bench on/off — the HONEST re-instantiation path (the enabled state selects
    the EDITING instantiation kwargs, so the graph genuinely differs). Swaps the host graph via
    the graph_factory and requests a rebake (rebuild counter bumps), exactly like any structural
    edit. Returns the applied state."""
    enabled = bool(enabled)
    if enabled == self._bench_enabled:
      return self._bench_enabled
    self._bench_enabled = enabled
    if self._bench is not None:
      self._bench.enabled = enabled
    if self._graph_factory is not None:
      try:
        self._graph = self._graph_factory(enabled)
      except Exception as ex:
        self._status(f"bench enable graph rebuild failed: {ex}")
    self._requestRebake()
    return self._bench_enabled

  @property
  def bench(self):
    return self._bench

  @property
  def bench_enabled(self):
    return self._bench_enabled

  def _installDrawable(self):
    if self._drawable is None or self._layer is None:
      return
    if self._node is not None:
      self._layer.removeDrawableNode(self._node)
      self._node = None
    self._node_ctr += 1
    self._node = self._layer.createDrawableNode(
        "particles_%s_%d" % (self._title, self._node_ctr), self._drawable)

  def bindViewport(self, sgv):
    """Populate the shell's SceneGraphViewport widget with this host's scene + camera."""
    self._sgv = sgv
    sgv.scenegraph = self._scenegraph
    sgv.cameraName = self._camname
    sgv.camera_evhandler = self.onViewportEvent
    sgv.forkDB()

  ##############################################################################
  # per-frame hooks
  ##############################################################################

  def gpuUpdate(self, ctx):
    """GPU thread: the particle drawable's own onGpuUpdate lambda (gradient-LUT upload) is
    driven by the SceneGraphViewport's render path, so there is nothing to pump here beyond
    keeping the context current."""
    self._ctx = ctx

  def update(self):
    """Update thread: apply a pending rebake, then the transport-gated sim advance, then
    publish the scene. PLAYING advances the world clock + calls GraphInst::compute() (the
    particles move); PAUSED / STOPPED skip compute so the last frame stays frozen in the
    render triple-buffer. The scene is ALWAYS published so the camera orbits + the frozen
    frame keeps presenting regardless of transport."""
    if self._rebake_pending:
      self._rebake_pending = False
      self._doRebake()
    if self._state == PLAYING and self._graphinst is not None:
      try:
        self._graphinst.compute(self._updata)     # resolver reads _bench_time (this tick's phase)
      except Exception as ex:
        self._status(f"compute raised: {ex}")
      self._updata.absolutetime = float(self._updata.absolutetime) + _DT
      self._bench_time += _DT                      # bench motion advances only while PLAYING
      self._tick_count += 1
    if self._scenegraph is not None and self._cameralut is not None:
      try:
        self._scenegraph.updateScene(self._cameralut)
      except RuntimeError:
        return                              # scenegraph torn down during shutdown

  def _doRebake(self):
    """Re-instantiate the drawable + graphinst from the (mutated) live GraphData and swap the
    scene node — the honest live-edit coupling (a running GraphInst copies literal plug values
    at instantiation and never re-reads them, so an edit only shows after a fresh instance).
    Preserves the world clock so the sim continues from the same time. Runs on the update thread
    (serialized with compute) so the graphinst swap never races. A bad edit fails LOUD, not black:
    an invalid-topology rebake HOLDS the last-good node (no re-install) and stops the sim."""
    try:
      if self._buildDrawable():             # only swap the node in on a valid (committed) drawable
        self._installDrawable()
      self._rebuild_count += 1
    except Exception as ex:                 # ops self-defend: a bad edit must not kill the viewport
      self._status(f"rebake failed: {ex}")

  ##############################################################################
  # node-model host interface (structural edits route here via on_changed)
  ##############################################################################

  def _recordEdit(self, label, coalesce_key=None):
    pass

  def _requestRebake(self):
    """A structural/param edit landed (or the shell's propsheet change) — re-instantiate the
    particle system on the next update tick. A CONTRIBUTOR host owns no primary scene; its
    rebake is a documented v1 gap (the primary owns the composed scene) surfaced as a loud
    status, never a silent no-op."""
    if self._primary is not None:
      self._status("edit on a composed (contributor) particles tab does not re-instantiate the "
                   "shared viewport in v1 — open this source standalone to see live particle edits")
      return
    self._rebake_pending = True

  ##############################################################################
  # transport (family-neutral: start / pause / stop)
  ##############################################################################

  def start(self):
    self._state = PLAYING
    return self._state

  def pause(self):
    self._state = PAUSED
    return self._state

  def stop(self):
    self._state = STOPPED
    self._bench_time = 0.0            # stop RESETS the bench motion (pause only freezes it)
    return self._state

  @property
  def state(self):
    return self._state

  @property
  def tick_count(self):
    return self._tick_count

  @property
  def rebuild_count(self):
    return self._rebuild_count

  @property
  def scenegraph(self):
    return self._scenegraph

  @property
  def layer(self):
    return self._layer

  ##############################################################################

  def onViewportEvent(self, uievent):
    if self._uicam is None:
      return lev2.ui.HandlerResult()
    handled = self._uicam.uiEventHandler(uievent)
    if handled:
      self._uicam.updateMatrices()
      self._camera.copyFrom(self._uicam.cameradata)
    return lev2.ui.HandlerResult()

  def _status(self, msg):
    print(f"[particles-viewport] {msg}", flush=True)
    if self._on_status is not None:
      try:
        self._on_status(msg)
      except Exception:
        pass
