################################################################################
# hypermesh_viewport_host — the HYPERMESH family's viewport component for the
# standalone dflow editor shell (ork.dflow.edit.py). Roads/terrain-simultaneously
# STAGE 1: a hypermesh graph contributes its MATERIALIZED MESH to the shell's shared
# viewport, so box/extrude/L-system meshes open (and compose beside terrain).
#
# It implements the SAME family-neutral compose seam terrain_viewport_host.py documents:
#   * PRIMARY (createScene / viewport_setup): build a minimal ForwardPBR scenegraph that
#     hosts hm.materialize_live()'s GpuMesh (via the established make_drawable path), a
#     camera framing it, and bind the shell's SceneGraphViewport widget to it.
#   * CONTRIBUTOR (composeInto / viewport_compose): fold the mesh drawable into the PRIMARY
#     host's scenegraph (the substrate law: artifacts by name) — surviving the primary's
#     scene rebuilds via its generic external-decorator seam.
#
# A hypermesh mesh is TRANSPORT-INERT for the shared world clock (a static GpuMesh); the
# family-neutral start/pause/stop transport still advances a tick counter (the shell's
# observable) and, for an ANIMATED asset (onUpdate / S.time), drives its per-frame plug
# pokes while PLAYING. Structural / param edits re-materialize on the next rebake
# (rebuild-on-explicit-rebake v1 — never a silent no-op; a failed bake fails LOUDLY).
################################################################################

from orkengine.core import vec3, VarMap, lev2_pyexdir
from orkengine import lev2
from orkengine.lev2 import CameraDataLut

# transport states (family-neutral; identical labels to terrain_viewport_host)
STOPPED = "stopped"
PLAYING = "playing"
PAUSED = "paused"

# IBL environment so the mesh is LIT (a black viewport is a gate failure). Matches the
# terrain host's default courtyard so a composed scene shares one look.
_SKYBOX = "<ork_envmaps2>/blender_courtyard.xir"

# COMPOSE PLACEMENT POLICY (v1, contributor-into-terrain-primary):
#   A hypermesh mesh is authored around the object origin at its own native scale (a Box's
#   half-extent is `size`, a few units). A terrain primary spans EXTENT_M (tens of thousands
#   of meters) and its surface sits at hundreds/thousands of meters at the world origin — so a
#   raw-origin-overlap contributor is BOTH sub-pixel (native size << primary extent, viewed
#   from an extent-scaled camera) AND buried (origin y=0 is far below the terrain surface).
#   Anchoring alone cannot make a 2-unit box visible on a 32 km terrain, so v1 placement:
#     (1) AUTO-FIT SCALE: uniform-scale the contributor so its horizontal footprint == a fixed
#         fraction of the primary's XZ extent -> an extent-independent apparent size (~5deg),
#         VISIBLE BY DEFAULT at ANY native scale (a size=1 box and a size=1000 box both read).
#     (2) SURFACE ANCHOR: sit the scaled bounds-bottom on the terrain surface (+ a small margin),
#         sampling the MAX height over the footprint so no corner buries. The mesh's XZ center
#         lands at the terrain center (world origin) where the framing camera looks.
#   Applied in the contributor decorator, so it is RE-APPLIED on every terrain scene rebuild
#   (survives rebakes by construction). A hypermesh PRIMARY (no terrain surface) keeps raw
#   origin overlap (byte-identical to the standalone/hypermesh-into-hypermesh path). Explicit
#   per-object placement / user scale is a v2 follow-up.
_FIT_FRAC = 0.06        # target footprint as a fraction of the primary XZ extent
_MARGIN_FRAC = 0.02     # bottom-clearance margin as a fraction of the scaled mesh height


def plan_surface_anchor(aabb_min, aabb_max, extent, surface_height_fn,
                        fit_frac=_FIT_FRAC, margin_frac=_MARGIN_FRAC):
  """Pure placement math (unit-testable, no engine): given a contributor mesh's object-space AABB
  (aabb_min/aabb_max = (x,y,z) sequences), the primary's XZ `extent`, and a surface_height_fn(x,z)
  -> world height, return (scale, tx, ty, tz):
    * scale normalizes the mesh's horizontal footprint to fit_frac*extent (visible-by-default).
    * (tx,tz) put the scaled XZ center at the world origin (the terrain center / camera focus).
    * ty sits the scaled bounds-bottom margin_frac*height above the MAX surface over the footprint.
  Deterministic; raises on a degenerate (zero-size / non-positive extent) input."""
  lo, hi = aabb_min, aabb_max
  cx = 0.5 * (float(lo[0]) + float(hi[0]))
  cz = 0.5 * (float(lo[2]) + float(hi[2]))
  sx = float(hi[0]) - float(lo[0])
  sy = float(hi[1]) - float(lo[1])
  sz = float(hi[2]) - float(lo[2])
  extent = float(extent)
  if extent <= 0.0:
    raise ValueError("plan_surface_anchor: primary extent must be > 0 (got %r)" % extent)
  native = max(sx, sz)
  if native <= 1e-9:
    native = max(sy, 1e-9)
  scale = (fit_frac * extent) / native
  half = 0.5 * scale * native
  pts = [(0.0, 0.0), (half, half), (half, -half), (-half, half), (-half, -half)]
  surf = max(surface_height_fn(px, pz) for (px, pz) in pts)
  margin = margin_frac * (scale * sy)
  ty = surf + margin - scale * float(lo[1])        # scaled bounds-bottom -> surf + margin
  return scale, -scale * cx, ty, -scale * cz


# [M] MATERIAL OVERRIDE modes — mirror ork.hypermesh.viewer.py's [M] cycle names (asset / white /
# mirror / mirror2 / x3 / groups / faces) as make_drawable kwargs. dflowedit surfaces the same set as
# a CLI override (--material MODE) since the sibling viewer has no CLI flag (it cycles on the [M] key).
_MATERIAL_MODES = ("asset", "white", "mirror", "mirror2", "x3", "groups", "faces")


def _resolve_material_mode(mode):
  """Material MODE name -> make_drawable(**kwargs), mirroring the hypermesh viewer's [M] modes.
  None / 'asset' / 'default' -> {} (make_drawable's own Solid default). Raises LOUDLY (naming the
  valid modes) on an unknown mode — ops-self-defend, never a silent fallback."""
  if not mode or mode in ("asset", "default"):
    return {}
  from orkengine.core import vec3
  white = vec3(1.0, 1.0, 1.0)
  table = {
      "white":   dict(albedo=white, roughness=0.5, metallic=0.0),
      "mirror":  dict(albedo=white, roughness=0.0, metallic=1.0),
      "mirror2": dict(albedo=white, roughness=0.2, metallic=1.0),
      "x3":      dict(albedo=vec3(0.3, 0.7, 0.3), roughness=1.0, metallic=0.0),
      "groups":  dict(_cls="GroupView", roughness=0.9),
      "faces":   dict(_cls="TopoView", roughness=0.8),
  }
  if mode not in table:
    raise ValueError("unknown material mode %r; choices: %s"
                     % (mode, ", ".join(("asset",) + tuple(sorted(table)))))
  spec = dict(table[mode])
  clsname = spec.pop("_cls", None)
  if clsname:
    from ork.hypergraph.assets.materials.hypermesh import GroupView, TopoView
    spec["material_cls"] = {"GroupView": GroupView, "TopoView": TopoView}[clsname]
  return spec


class HypermeshViewportHost:
  """Viewport host for the hypermesh family. Owns (as PRIMARY) a plain ForwardPBR scenegraph
  hosting a live hypermesh GpuMesh drawable + an orbit camera, binds it into the shell's
  SceneGraphViewport, and exposes the family-neutral transport. As a CONTRIBUTOR it folds its
  drawable into the primary host's scene instead."""

  def __init__(self, graphdata, *, asset=None, is_animated=False, title="hypermesh",
               on_status=None, skybox=None, material_mode=None):
    self._graph = graphdata          # the live hypermesh dflow.GraphData (== document.elaborate())
    self._asset = asset              # the Hypermesh instance (bare-name load) for onUpdate; else None
    self._animated = bool(is_animated)
    self._title = title
    self._on_status = on_status
    # OVERRIDES (CLI): envmap skybox (full <ork_envmaps2>/<name>.xir path, or None -> _SKYBOX) and
    # a make_drawable material mode mirroring ork.hypermesh.viewer.py's [M] cycle (None -> the Solid
    # default). Resolved to make_drawable kwargs once here; a bad mode raises LOUDLY at construction.
    self._skybox = skybox or _SKYBOX
    self._material_mode = material_mode
    self._material_kw = _resolve_material_mode(material_mode)

    self._ctx = None
    self._sgv = None
    self._scenegraph = None
    self._layer = None
    self._cameralut = None
    self._camera = None
    self._uicam = None
    self._camname = "spawncam"

    self._live = None                # LiveHypermesh (owns the pooled GpuMesh SSBOs — keep alive)
    self._cdd = None                 # the ComputeDrawableData for the mesh
    self._node = None                # the scene node hosting the drawable
    self._node_ctr = 0

    # CONTRIBUTOR state: set in composeInto (this host owns no scene of its own then).
    self._primary = None
    self._compose_ctx = None
    self._mesh_aabb = None           # (min,max) object-space AABB of the composed mesh (cached)
    self._composed_node = None       # the CURRENT scenegraph's contributor node (suppression handle)

    # rebake pipeline (rebuild-on-explicit-rebake v1)
    self._rebake_pending = False

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

  def createScene(self, ctx):
    """PRIMARY role seam alias (terrain uses gpuInit)."""
    self.gpuInit(ctx)

  def composeInto(self, primary, ctx):
    """CONTRIBUTOR role: materialize THIS graph's mesh and fold its drawable into `primary`'s
    scene. Uses the primary's generic external-decorator seam so the drawable survives the
    primary's scene rebuilds; falls back to a one-shot add if the primary predates that seam
    (a v1 hypermesh mesh is static, so the fallback is correct until the primary next rebuilds
    — reported LOUDLY so it is never a silent gap)."""
    self._primary = primary
    self._compose_ctx = ctx
    self._buildLive(ctx)
    if self._cdd is None:
      self._status("compose: no renderable mesh — hypermesh payload contributes nothing")
      return
    # object-space bounds (once) — drives the surface-anchor + auto-fit placement below.
    self._mesh_aabb = self._computeMeshAabb(ctx)

    def _decorate(scenegraph, layer):
      self._node_ctr += 1
      node = layer.createDrawableNodeFromData(
          "hypermesh_%s_%d" % (self._title, self._node_ctr), self._cdd)
      self._composed_node = node                    # suppression handle (visibility oracle)
      self._applyAnchorPlacement(node)              # surface-anchor + auto-fit (no-op sans terrain surface)

    if hasattr(primary, "add_external_decorator"):
      primary.add_external_decorator(_decorate)     # re-applied on every primary scene rebuild
    else:
      sg = getattr(primary, "scenegraph", None)
      layer = self._primaryForwardLayer(primary)
      if sg is not None and layer is not None:
        _decorate(sg, layer)
        self._status("compose: primary has no external-decorator seam — added once (will drop on "
                     "a primary rebake; open the hypermesh source standalone for a persistent view)")
      else:
        self._status("compose: could not reach the primary's scenegraph/layer — payload not folded")

  def addContributor(self, host):
    """PRIMARY role: a NON-hypermesh contributor (terrain) folded into a hypermesh primary is
    NOT supported in v1 (this host bakes no terrain payload). Refuse LOUDLY rather than silently
    drop it — the reverse (terrain primary + hypermesh contributor) IS the supported pairing."""
    self._status("addContributor: terrain-into-hypermesh compose is unsupported in v1 — open the "
                 "terrain source first so it is the primary (its payload will host the hypermesh tab)")

  def rebuildComposed(self, ctx):
    """PRIMARY role: nothing composed into a hypermesh primary (see addContributor) — just keep
    this host's own scene current."""
    pass

  def _primaryForwardLayer(self, primary):
    layer = getattr(primary, "layer", None)
    if layer is not None:
      return layer
    rt = getattr(primary, "runtime", None)
    return getattr(rt, "layer", None) if rt is not None else None

  def _computeMeshAabb(self, ctx):
    """Read the live GpuMesh position channel back to the CPU (READ-ONLY map) and return its
    object-space AABB (min, max) as numpy vec3s, or None if unavailable. Guarded: a GPU-resident
    -count mesh (extrude / boolean) stores the LIVE vertex count in the header (uint[0]) while
    num_verts holds pooled CAPACITY — trust the header count when it is a plausible subset so the
    AABB spans exactly the live primitives, never the degenerate capacity tail."""
    import numpy
    from orkengine.core import CrcStringProxy
    mesh = getattr(self._live, "mesh", None)
    if mesh is None:
      return None
    nv = int(getattr(mesh, "num_verts", 0))
    if nv <= 0:
      return None
    tok = CrcStringProxy()
    try:
      fxi = ctx.FXI
      hdr = mesh.header_ssbo()
      if hdr is not None:
        hm = fxi.mapStorageBuffer(hdr, 0, 12, tok.READ_ONLY)
        live_nv = int(numpy.frombuffer(hm.data, dtype=numpy.uint32, count=3)[0])
        fxi.unmapStorageBuffer(hm)
        if 0 < live_nv <= nv:
          nv = live_nv
      pssbo = mesh.channel_ssbo(0)                    # 0 == POSITION
      if pssbo is None:
        return None
      m = fxi.mapStorageBuffer(pssbo, 0, nv * 16, tok.READ_ONLY)
      arr = numpy.frombuffer(m.data, dtype=numpy.float32).reshape(nv, 4)[:, :3].copy()
      fxi.unmapStorageBuffer(m)
    except Exception as ex:
      self._status(f"compose: mesh-bounds readback failed ({ex}) — contributor stays at origin")
      return None
    return arr.min(axis=0), arr.max(axis=0)

  def _applyAnchorPlacement(self, node):
    """Position the freshly-created contributor `node` on the PRIMARY terrain surface with an
    auto-fit scale (see the module PLACEMENT POLICY). A no-op (raw origin overlap) when the
    primary exposes no terrain surface sampler (a hypermesh primary) or the mesh bounds are
    unavailable — keeping the standalone / hypermesh-into-hypermesh paths byte-identical."""
    aabb = self._mesh_aabb
    if aabb is None:
      return
    rt = getattr(self._primary, "runtime", None)
    if rt is None or not hasattr(rt, "terrain_height"):
      return                                          # hypermesh primary: no surface -> origin overlap
    extent = float(getattr(rt, "extent_m", 0.0) or 0.0)
    if extent <= 0.0:
      return
    lo, hi = aabb
    # the surface is baked lazily by the primary's display bake; ensure the CPU height array is
    # current before sampling (idempotent, guarded — a missing bake leaves the last-loaded heights).
    try:
      rt._load_display_heights()
    except Exception:
      pass
    try:
      scale, tx, ty, tz = plan_surface_anchor(lo, hi, extent, rt.terrain_height)
    except ValueError as ex:
      self._status(f"compose: placement skipped ({ex}) — contributor stays at origin")
      return
    from orkengine.core import vec3
    node.worldTransform.scale = float(scale)
    node.worldTransform.translation = vec3(tx, ty, tz)   # scaled XZ center -> world origin
    self._status("anchored on terrain surface: scale=%.4g footprint=%.1fm originY=%.1f"
                 % (scale, scale * max(float(hi[0]) - float(lo[0]), float(hi[2]) - float(lo[2])), ty))

  def setComposedVisible(self, visible):
    """Toggle the CURRENT composed contributor node's render enable — the visibility oracle's
    suppression handle (render honors Node::_enabled). Returns True if a node was toggled."""
    n = self._composed_node
    if n is None:
      return False
    n.enabled = bool(visible)
    return True

  ##############################################################################
  # PRIMARY: GPU init — scenegraph + camera + the live mesh drawable
  ##############################################################################

  def gpuInit(self, ctx):
    self._ctx = ctx
    self._buildScene(ctx)
    self._buildLive(ctx)
    self._installDrawable()

  def _buildScene(self, ctx):
    vm = VarMap()
    vm.preset = "ForwardPBR"
    vm.SkyboxTexPathStr = self._skybox
    vm.SkyboxIntensity = 1.0
    vm.DiffuseIntensity = 1.0
    vm.SpecularIntensity = 1.0
    vm.AmbientLevel = vec3(0.10)
    vm.ssaa = 0
    vm.msaa = 0
    self._scenegraph = lev2.scenegraph.Scene(vm)
    self._layer = self._scenegraph.createLayer("std_forward")
    self._scenegraph.createLayer("depth_prepass")
    pbr = self._scenegraph.pbr_common
    pbr.useDepthPrepass = True
    pbr.useFloatColorBuffer = True
    self._scenegraph.lightingmanager.gpuInit(ctx)

    lev2_pyexdir.addToSysPath()
    from lev2utils.cameras import setupUiCameraX
    self._cameralut = CameraDataLut()
    self._camera, self._uicam = setupUiCameraX(
        near=0.1, far=20000.0, fov_deg=45, cameralut=self._cameralut, camname=self._camname,
        eye=vec3(6, 5, 9), tgt=vec3(0, 0, 0), up=vec3(0, 1, 0))

  def _buildLive(self, ctx):
    """Materialize the graph to a live GpuMesh + build the mesh drawable. Ops self-defend: a
    graph that yields no mesh (e.g. a field/roads-only graph) leaves _cdd None + a loud status
    rather than a black-and-silent viewport."""
    from ork.hypergraph.dflow.hypermesh import make_drawable
    try:
      self._live = lev2.hypermesh.materialize_live(self._graph, ctx)
    except Exception as ex:
      self._live = None
      self._cdd = None
      self._status(f"materialize failed: {ex}")
      return
    mesh = getattr(self._live, "mesh", None)
    if mesh is None or int(getattr(mesh, "num_verts", 0)) <= 0:
      self._cdd = None
      self._status("materialize produced no mesh (0 verts) — no viewport payload")
      return
    # A road ribbon may share its graph with an unrelated building-seed InstanceSet; the mesh
    # renders UN-INSTANCED (instance_from omitted -> explicit-scoping default). A forest asset
    # that WANTS its graph set can be opened in ork.hypermesh.viewer (which opts in).
    # material override (--material MODE) folds in as make_drawable kwargs; empty -> Solid default.
    self._cdd, _ = make_drawable(self._live, ctx, animated=self._animated, **self._material_kw)

  def _installDrawable(self):
    if self._cdd is None or self._layer is None:
      return
    if self._node is not None:
      self._layer.removeDrawableNode(self._node)
      self._node = None
    self._node_ctr += 1
    self._node = self._layer.createDrawableNodeFromData(
        "hypermesh_%s_%d" % (self._title, self._node_ctr), self._cdd)

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
    """GPU thread: apply a pending rebake (re-materialize + swap the drawable). A hypermesh mesh
    is static per frame; the drawable's own in-frame hook re-evaluates an ANIMATED graph."""
    self._ctx = ctx
    if self._rebake_pending:
      self._rebake_pending = False
      self._doRebake(ctx)

  def _doRebake(self, ctx):
    try:
      self._buildLive(ctx)
      self._installDrawable()
      self._rebuild_count += 1
    except Exception as ex:                 # ops self-defend: a bad edit must not kill the viewport
      self._status(f"rebake failed: {ex}")

  def update(self):
    """Update thread: publish the scene with the current camera (so the widget repaints + camera
    orbit works regardless of transport), then the transport-gated tick. PLAYING advances the
    tick counter (the shell's transport observable) and drives an animated asset's onUpdate."""
    if self._scenegraph is not None and self._cameralut is not None:
      try:
        self._scenegraph.updateScene(self._cameralut)
      except RuntimeError:
        return                              # scenegraph torn down during shutdown
    if self._state == PLAYING:
      if self._animated and self._asset is not None and hasattr(self._asset, "onUpdate"):
        try:
          self._asset.onUpdate(_MiniUpdInfo(self._tick_count))
        except Exception as ex:
          self._status(f"onUpdate raised: {ex}")
      self._tick_count += 1

  ##############################################################################
  # node-model host interface (structural edits route here via on_changed)
  ##############################################################################

  def _recordEdit(self, label, coalesce_key=None):
    pass

  def _requestRebake(self):
    """A structural/param edit landed (or the shell's propsheet change) — re-materialize on the
    next GPU tick. A CONTRIBUTOR host owns no scene; its rebake is a documented v1 gap (the
    primary owns the composed scene) surfaced as a loud status, never a silent no-op."""
    if self._primary is not None:
      self._status("edit on a composed (contributor) hypermesh tab does not re-bake the shared "
                   "viewport in v1 — open this source standalone to see live mesh edits")
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
    print(f"[hypermesh-viewport] {msg}", flush=True)
    if self._on_status is not None:
      try:
        self._on_status(msg)
      except Exception:
        pass


class _MiniUpdInfo:
  """The minimal updinfo an animated hypermesh asset's onUpdate reads (absolutetime). The shared
  world clock owns wall time; a composed hypermesh advances off the transport tick (a fixed dt)
  so pausing the transport freezes its animation identically to the terrain family."""
  _DT = 1.0 / 60.0

  def __init__(self, tick):
    self.absolutetime = float(tick) * self._DT
    self.deltatime = self._DT
