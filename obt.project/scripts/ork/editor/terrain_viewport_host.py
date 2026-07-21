################################################################################
# terrain_viewport_host — the terrain family's ECS-hosting viewport component for
# the standalone dflow editor shell (ork.dflow.edit.py). JUL13_DFLOW E3.
#
# The shell's viewport "really displays an ECS": this host owns a live ecs.Simulation
# (via TerrainRuntime — the same in-code ForwardPBR terrain scene the C++ player bakes
# + renders), the binding of that scene into the shell's SceneGraphViewport widget, and
# a family-neutral TRANSPORT (start / pause / stop) over the ECS update tick. Animated
# dataflow (a future family carrying moving parts) advances only while PLAYING; a static
# terrain simply has nothing to animate, but the transport is still meaningful (the ECS
# systems halt).
#
# The ECS scene consumes dflow OUTPUT ARTIFACTS by name (house law): TerrainRuntime
# embeds the elaborated document as a HeightFieldGenData whose deferred C++ bake writes
# <assetcache>/terrain/terra/*.exr, and the scene's TerrainChunkDrawableData references
# the "terra" asset. This host NEVER reaches into the editor's runtime objects from the
# ECS side — it drives the bakes through terrain_runtime.py's own sliced-rebuild
# machinery, and on bake-final swaps the viewport to the fresh scenegraph.
#
# It ALSO serves as the node-model host: canvas edits + display/bypass flags route
# through TerrainNodeGraphModel -> _recordEdit / _requestRebake here, driving live
# rebakes exactly as the terrain editor does. (Converging ork.terrain.edit.py itself onto
# this host is a follow-up for owner adjudication — out of this slice; terrainedit stays
# untouched.)
################################################################################

import time

from orkengine.lev2 import PostFxNodeACES, PostFxNodeHSVG

from ork.editor.terrain_runtime import TerrainRuntime
from ork.editor.terrain_runtime import display_trace as _dtrace

# rebake settle / hold-last-frame timings — identical to ork.terrain.edit.py's proven
# values (a rebake fires this long after the last edit tick; the OLD scenegraph keeps
# presenting until the fresh terrain is staged, or this cap elapses).
_REBAKE_IDLE_S = 0.25
_SWAP_HOLD_MAX_S = 3.0

# transport states (family-neutral; surfaced by the shell as toolbar buttons)
STOPPED = "stopped"
PLAYING = "playing"
PAUSED = "paused"


class TerrainEcsViewportHost:
  """ECS-hosting viewport for the terrain family. Owns a TerrainRuntime (the live ECS
  terrain scene), binds it into the shell's SceneGraphViewport, runs the rebake pipeline,
  and exposes a start/pause/stop transport over the ECS update tick.

  Lifecycle (driven by the shell's frame hooks)::

    host = TerrainEcsViewportHost(source, ...)     # loads the document (pure python)
    host.gpuInit(ctx)                              # camera + post-fx + live ECS scene (GPU)
    host.bindViewport(sgv)                         # populate the viewport widget
    # per frame:
    host.gpuUpdate(ctx)                            # GPU thread: rebake pipeline + swap
    host.update()                                  # update thread: transport-gated sim tick

  Node-model host interface (canvas edits): _recordEdit / _requestRebake.
  """

  def __init__(self, source, *, dsl_class=None, extent_m=None,
               preview_dim=1024, full_dim=4096, chunk=128, dsl_kwargs=None,
               on_status=None, skybox=None):
    self.runtime = TerrainRuntime(preview_dim=preview_dim, full_dim=full_dim, chunk=chunk)
    self.runtime.load(source, dsl_class=dsl_class, extent_m=extent_m,
                      **(dsl_kwargs or {}))
    # envmap override (-e/--envmap): a full <ork_envmaps2>/<name>.xir path (resolved by the shell)
    # drives BOTH the skybox and the IBL radiance for this terrain scene. None -> the runtime default.
    if skybox:
      self.runtime.skybox_path = skybox
    self._on_status = on_status
    self._sgv = None

    # COMPOSE (multi-document shell): set on a CONTRIBUTOR host in composeInto — its edits
    # then route to the PRIMARY's rebake pipeline (the primary owns the ONE simulation). A
    # standalone / primary host leaves this None and drives its own rebakes as before.
    self._primary = None

    # post-fx nodes (ACES tone map + HSVG) built ONCE in gpuInit and re-spliced into
    # every fresh scenegraph by the runtime — the render matches ork.terrain.edit.py.
    self._aces = None
    self._hsvg = None

    # rebake pipeline state (mirrors terrainedit's _onGpuUpdate machinery)
    self._rebake_pending = False
    self._last_edit_time = 0.0
    self._sliced_precook = False
    self._precook_started = 0.0
    self._precook_status_next = 0.0
    self._sgv_swap_pending = False
    self._sgv_swap_started = 0.0
    self._rebuilding = False
    self._rebuild_count = 0            # swap counter (bypass/edit rebake oracle)
    self._plane_swap_count = 0         # #88 v2: in-place display-plane rebinds (no full swap)

    # transport: gate the ECS update tick. PLAYING advances the simulation systems;
    # PAUSED / STOPPED hold them (the scenegraph still renders the frozen frame, and the
    # camera still orbits — camera events update the cameralut directly). tick_count is
    # the observable: it advances only on a ticked (PLAYING) frame.
    self._state = PLAYING
    self._tick_count = 0

  ##############################################################################
  # node-model host interface (TerrainNodeGraphModel._glue.edit -> here)
  ##############################################################################

  def _recordEdit(self, label, coalesce_key=None):
    # v1 shell: no undo stack yet (converging terrainedit's UndoStack onto this host is
    # a follow-up). An edit is a real rebake trigger; the label is a no-op breadcrumb.
    pass

  def _requestRebake(self):
    # a CONTRIBUTOR (composed) host owns no rebake pipeline — delegate to the primary so
    # any binding's edit rebuilds the ONE composed scene.
    if self._primary is not None:
      _dtrace("requestRebake -> delegated to primary host")
      self._primary._requestRebake()
      return
    _dtrace(f"requestRebake (pending; precook_active={self._sliced_precook})")
    self._rebake_pending = True
    self._last_edit_time = 0.0         # rebake ASAP (explicit flag / edit)

  ##############################################################################
  # compose seam (family-neutral viewport-host protocol) — the FIRST payload-bearing
  # binding is the PRIMARY (owns scene+sim+camera); each SUBSEQUENT one is a CONTRIBUTOR
  # that folds its payload into the primary's scene. A future family (hypermesh) plugs in
  # by implementing this same protocol on its own viewport host.
  ##############################################################################

  @property
  def has_viewport_payload(self):
    """This host contributes a live payload to the composed viewport (vs a doc-only family
    whose binding carries no viewport_host)."""
    return True

  def createScene(self, ctx):
    """PRIMARY role: build the shared scene + simulation + camera. (gpuInit is the terrain
    family's implementation of this seam.)"""
    self.gpuInit(ctx)

  def composeInto(self, primary, ctx):
    """CONTRIBUTOR role: fold THIS host's terrain payload into `primary`'s shared scene and
    rebuild it. This host owns NO simulation; its edits route to the primary's rebake
    pipeline (see _requestRebake) and the shell's ONE transport drives the primary."""
    self._primary = primary
    primary.addContributor(self)
    primary.rebuildComposed(ctx)

  def addContributor(self, host):
    """PRIMARY: register a contributor host's terrain runtime as a composed payload."""
    self.runtime.add_contributor(host.runtime)

  def add_external_decorator(self, fn):
    """PRIMARY: register a family-neutral scene decorator (a NON-terrain contributor's mesh
    drawable) so it folds into this runtime's forward layer and survives per-edit rebuilds."""
    self.runtime.add_external_decorator(fn)

  @property
  def layer(self):
    return self.runtime.layer

  def rebuildComposed(self, ctx):
    """PRIMARY: rebuild the composed scene from all registered contributors and rebind the
    viewport to the fresh scenegraph. Called at compose time (per contributor added)."""
    self.runtime.create_live_scene(ctx, dim=self._current_dim())
    if self._sgv is not None:
      self._sgv.scenegraph = self.runtime.scenegraph

  ##############################################################################
  # GPU init — camera + post-fx + the live ECS terrain scene
  ##############################################################################

  def gpuInit(self, ctx):
    self.runtime.set_context(ctx)
    self.runtime.setup_camera()

    # tone-map / color-grade nodes at neutral defaults (exposure 1 / gamma 1 / sat 1) —
    # held here so the runtime re-splices the SAME objects into each rebuilt scenegraph.
    self._aces = PostFxNodeACES()
    self._aces.exposure = 1.0
    self._aces.gpuInit(ctx, 8, 8)
    self._hsvg = PostFxNodeHSVG()
    self._hsvg.hue = 0.0
    self._hsvg.saturation = 1.0
    self._hsvg.value = 1.0
    self._hsvg.gamma = 1.0
    self._hsvg.gpuInit(ctx, 8, 8)
    self.runtime.postfx_nodes = [self._aces, self._hsvg]

    # in-code ECS scene from the DOCUMENT — the C++ terrain path bakes + renders it.
    self.runtime.create_live_scene(ctx, dim=self._current_dim())

  def bindViewport(self, sgv):
    """Populate the shell's SceneGraphViewport widget with the live ECS scene + camera."""
    self._sgv = sgv
    sgv.cameraName = "spawncam"
    sgv.scenegraph = self.runtime.scenegraph
    sgv.camera_evhandler = self.onViewportEvent
    sgv.forkDB()
    self.runtime.bind_viewport(sgv)

  def _current_dim(self):
    # the shell hosts at the preview dim (no full-res toggle in v1); the sliced-rebuild
    # currency check keys off this same dim/extent.
    return self.runtime.preview_dim

  ##############################################################################
  # per-frame hooks
  ##############################################################################

  def gpuUpdate(self, ctx):
    """GPU thread: pump the sim's GPU phase, then run the rebake pipeline (MT3 sliced
    pre-cook when the products already exist at this dim/extent; burst otherwise) and the
    hold-last-frame swap. IDENTICAL sequencing to ork.terrain.edit.py — the fresh
    scenegraph rebind is render-sequential with the viewport's repaint."""
    self.runtime.gpuUpdate(ctx)
    if self._rebake_pending and (time.time() - self._last_edit_time) >= _REBAKE_IDLE_S:
      self._rebake_pending = False
      # #88 v2 FAST PATH: an interior->interior display REVISIT whose target product is already
      # current on disk morphs the HELD drawable's height plane in place (no scene swap: the
      # scenegraph, simulation and camera stay live). Declines LOUDLY to the full-swap routes
      # below whenever coherence / currency is not satisfied (never a wrong-plane bind).
      if self.runtime.try_inplace_display_rebind(ctx):
        self._plane_swap_count += 1
      elif self.runtime.begin_sliced_rebuild(ctx, dim=self._current_dim()):
        _dtrace("route: SLICED pre-cook armed")
        self._sliced_precook = True
        # a cold-cache pre-cook (session opened capture-CURRENT, cook blobs gone) can
        # grind through loop recomputes for MINUTES while hold-last-frame presents the
        # old surface — without feedback that is indistinguishable from the #88
        # "permanently stale" report. Slow must LOOK slow, never broken.
        self._precook_started = time.time()
        self._precook_status_next = 0.0
        self._status(f"rebaking display -> {self.runtime.display_key or 'default'} ...")
      else:
        _dtrace("route: BURST swap")
        self._doRebuildSwap(ctx)                                # burst (pre-MT3 path)
    if self._sliced_precook:
      now = time.time()
      if now >= self._precook_status_next:
        elapsed = now - self._precook_started
        if elapsed > 2.0:
          self._status(f"rebaking display -> {self.runtime.display_key or 'default'} "
                       f"... {elapsed:.0f}s (cold cook cache)")
        self._precook_status_next = now + 2.0
    if self._sliced_precook and self.runtime.sliced_rebuild_ready():
      _dtrace("sliced pre-cook ready -> swap")
      self._sliced_precook = False
      elapsed = time.time() - self._precook_started
      if elapsed > 2.0:
        self._status(f"display rebake done ({elapsed:.0f}s)")
      self._doRebuildSwap(ctx)
    if self._sgv_swap_pending:
      timed_out = (time.time() - self._sgv_swap_started) > _SWAP_HOLD_MAX_S
      if self.runtime.display_ready() or timed_out:
        if timed_out:
          _dtrace("REBIND on TIMEOUT (display_ready never fired)")
          self._status("hold-last-frame: fresh terrain not staged — swapping anyway")
        else:
          _dtrace("REBIND (display_ready)")
        if self._sgv is not None:
          self._sgv.scenegraph = self.runtime.scenegraph       # rebind (render-sequential)
        self._sgv_swap_pending = False

  def _doRebuildSwap(self, ctx):
    """Two-phase swap (Phase A materialize on the GPU thread, Phase B fresh scenegraph +
    sim). Bumps the rebuild counter (the flag / edit rebake observable)."""
    self.runtime.schedule_rebuild()
    self._rebuilding = True
    try:
      if self.runtime.prepare_rebuild(ctx, dim=self._current_dim()):     # GPU: materialize
        self.runtime.apply_pending_rebuild()                             # GPU: fresh sg + sim
        self._rebuild_count += 1
        _dtrace(f"swap complete rebuild_count={self._rebuild_count} (hold-last-frame armed)")
        # HOLD-LAST-FRAME: keep presenting the OLD scenegraph until the fresh terrain has
        # staged (display_ready), so an edit rebake never shows a black frame.
        self._sgv_swap_pending = True
        self._sgv_swap_started = time.time()
      else:
        _dtrace("swap SKIPPED (prepare_rebuild returned False — no _needs_rebuild)")
    except Exception as e:      # ops-self-defend: a bad edit must not kill the viewport
      _dtrace(f"swap EXCEPTION: {e}")
      self._status(f"rebuild failed: {e}")
    finally:
      self._rebuilding = False

  def update(self):
    """Update thread: transport-gated ECS tick. Only advances the simulation systems
    while PLAYING (and never during a rebuild swap). Camera orbit works regardless (its
    events update the cameralut directly)."""
    if self._state == PLAYING and not self._rebuilding:
      self.runtime.update()
      self._tick_count += 1

  ##############################################################################
  # transport (family-neutral: start / pause / stop over the ECS update tick)
  ##############################################################################

  def start(self):
    """Resume / begin advancing the ECS systems."""
    self._state = PLAYING
    return self._state

  def pause(self):
    """Freeze the ECS systems (scene stays; a subsequent start() continues)."""
    self._state = PAUSED
    return self._state

  def stop(self):
    """Halt the ECS systems (a subsequent start() re-runs from the held scene)."""
    self._state = STOPPED
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
  def plane_swap_count(self):
    """#88 v2: count of in-place display-plane rebinds (interior->interior revisits that
    skipped the full scene swap). The revisit gate asserts this bumps while rebuild_count
    does NOT on a current-product revisit."""
    return self._plane_swap_count

  @property
  def controller(self):
    return self.runtime.controller

  @property
  def scenegraph(self):
    return self.runtime.scenegraph

  ##############################################################################
  # viewport input — camera only (keyboard family features are out of this slice;
  # the key registry reserves the post-fx / material chords)
  ##############################################################################

  def onViewportEvent(self, uievent):
    return self.runtime.handle_camera_event(uievent)

  ##############################################################################

  def _status(self, msg):
    print(f"[terrain-viewport] {msg}", flush=True)
    if self._on_status is not None:
      try:
        self._on_status(msg)
      except Exception:
        pass
