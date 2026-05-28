###############################################################################
# ork.ecs.scene — Tier 3 Scene composite (HYPERECS M0)
#
# Authoring surface for declarative ECS scenes that lower to a reflected
# ecs.SceneData artifact. See ~/HYPERECS.md §4 for the full design + ~/HYPERECS_PLAN.md
# §2 for the M0 deliverables.
#
# M0 scope:
#   - Scene base class + two-pass lowering (Pass 1 collects in __init__,
#     Pass 2 emits in build()).
#   - Primitive methods: system_data, archetype, component, spawner.
#   - Entity sugar: self.entity(name, components=[...]) + .spawner() extension.
#   - SceneGraph handle: self.scenegraph(...) returning SG with .component(...)
#     and .drawables.<factory> factory namespace.
#   - Transform(**kwargs) Python-side helper (no C++ pyext change).
#
# Out of scope for M0 (deferred to later milestones):
#   - Asset DSL (M2)
#   - JSON round-trip (M3)
#   - Fragments (M4)
#   - Cross-entity wiring + scene-level params (M5)
#   - Live mutation / stage-then-swap (M6)
#   - external=True for scenegraph (M0 stub — accepts the flag but the host
#     is expected to supply a default-configured SG anyway)
###############################################################################

from orkengine import lev2
from orkengine.core import vec3, vec4
from orkengine.core import Transform as _CoreTransform

from ork.ecs.scene.assets import _ASSET_REGISTRY, _materialize as _materialize_asset


###############################################################################
# Preset → implied layers.
#
# Some renderer presets imply a fixed set of layers (e.g. ForwardPBR always
# needs std_forward + depth_prepass to render correctly; see
# lev2utils/scenegraph.py:18-20 for the non-ECS precedent). The Scene
# wrapper unions these into any user-supplied layers so authors don't have
# to repeat them on every call. Authors may still add scene-specific
# layers via the layers= kwarg — those are appended after the implied set.
###############################################################################

_PRESET_IMPLIED_LAYERS = {
  # PBR2 Phase 2 — std_transparent added to forward presets for transmissive
  # / refractive materials (P2.7+). Layer is empty until materials opt in;
  # zero cost when unused.
  "ForwardPBR":  ["std_forward",  "std_transparent", "depth_prepass"],
  "DeferredPBR": ["std_deferred", "depth_prepass"],
  "FWDPBRVR":    ["std_forward",  "std_transparent", "depth_prepass"],
  "FWDPBRVRDM":  ["std_forward",  "std_transparent", "depth_prepass"],
  "PBRVR":       ["std_deferred", "depth_prepass"],
}


def _effective_layers(preset, user_layers):
  """Union: preset's implied layers first (preserved order), then any user
  layers not already in the implied set."""
  implied = _PRESET_IMPLIED_LAYERS.get(preset, [])
  out = list(implied)
  for l in user_layers or ():
    if l not in out:
      out.append(l)
  return out


###############################################################################
# Public helpers — re-exported by package __init__.
###############################################################################

def _hsv_to_rgb(h, s, v):
  """HSV → RGB tuple. Hue in degrees [0, 360], s/v in [0, 1]."""
  h = (float(h) % 360.0) / 60.0
  c = float(v) * float(s)
  x = c * (1.0 - abs((h % 2.0) - 1.0))
  m = float(v) - c
  if   h < 1.0: r, g, b = c, x, 0.0
  elif h < 2.0: r, g, b = x, c, 0.0
  elif h < 3.0: r, g, b = 0.0, c, x
  elif h < 4.0: r, g, b = 0.0, x, c
  elif h < 5.0: r, g, b = x, 0.0, c
  else:         r, g, b = c, 0.0, x
  return (r + m, g + m, b + m)


class _Hsv:
  """Lazy HSV color — auto-converts to vec3 or vec4 at the consumption slot.

  Returned by hsv(). Consumers that know whether they expect vec3 or vec4
  call to_vec3() / to_vec4() to materialize. The PbrMaterial dispatcher
  does this automatically based on the field name. For other code paths
  the conversion is manual: ssss_node.subsurface_tint = hsv(120, 1, 1).to_vec3()."""

  __slots__ = ("h", "s", "v", "a", "_rgba")

  def __init__(self, h, s, v, a):
    self.h = float(h); self.s = float(s); self.v = float(v); self.a = float(a)
    r, g, b = _hsv_to_rgb(self.h, self.s, self.v)
    self._rgba = (r, g, b, self.a)

  def to_vec3(self, field_name=""):
    if self.a != 1.0:
      raise ValueError(
        f"hsv({self.h}, {self.s}, {self.v}, a={self.a}) → vec3 slot"
        + (f" {field_name!r}" if field_name else "")
        + f": 3-component color slots have no alpha channel. "
          "Drop the a= argument or default it to 1.0.")
    return vec3(*self._rgba[:3])

  def to_vec4(self):
    return vec4(*self._rgba)

  def __repr__(self):
    return f"hsv({self.h}, {self.s}, {self.v}, a={self.a})"


def hsv(h, s, v, a=1.0):
  """HSV(A) → color. Auto-converts to vec3 or vec4 at the consumption slot.

  Hue in degrees [0, 360], saturation/value/alpha in [0, 1].

  Examples:
      base_color       = hsv(0,   1.0, 1.0)        # → vec4 red opaque
      base_color       = hsv(0,   1.0, 1.0, 0.5)   # → vec4 red half-transparent
      subsurface_color = hsv(20,  0.6, 1.0)        # → vec3 warm pink
      subsurface_color = hsv(20,  0.6, 1.0, 0.7)   # → ValueError (alpha in vec3 slot)"""
  return _Hsv(h, s, v, a)


# Color slot kinds, used by PbrMaterial._coerce_hsv. Update when new
# reflected color fields land on the material — fields not listed here
# default to vec3 (the common case across all glTF lobes).
_PBR_VEC4_FIELDS = frozenset({"base_color"})
_PBR_VEC3_FIELDS = frozenset({
  "subsurface_color", "subsurface_radius",
  "sheen_color", "specular_color",
  "attenuation_color", "diffuse_transmission_color",
})


###############################################################################
# Named color palette — 64 commonly useful colors as lazy _Hsv values.
# Each auto-coerces to vec3 or vec4 at the consumption slot, same as hsv().
# Used as `colors.red`, `colors.oak1`, `colors.skyblue1`, etc.
#
# Conventions:
#   - Numbered variants (oak1/oak2, skin1..6, leaf1/leaf2, skyblue1/skyblue2)
#     go light→dark or near→far. skyblue1 = horizon-paler; skyblue2 = zenith.
#   - All values are HSV with H in degrees [0, 360], S/V in [0, 1], alpha=1.
#   - Override via colors.red.to_vec4() / .to_vec3() if explicit type needed.
###############################################################################

class _Colors:
  """64 named colors. Each auto-coerces to vec3 or vec4 at use site."""
  __slots__ = ()

  # ---- Primaries / secondaries (12) ----
  red       = _Hsv(0,   1.00, 1.00, 1.0)
  green     = _Hsv(120, 1.00, 1.00, 1.0)
  blue      = _Hsv(240, 1.00, 1.00, 1.0)
  yellow    = _Hsv(60,  1.00, 1.00, 1.0)
  cyan      = _Hsv(180, 1.00, 1.00, 1.0)
  magenta   = _Hsv(300, 1.00, 1.00, 1.0)
  orange    = _Hsv(30,  1.00, 1.00, 1.0)
  purple    = _Hsv(270, 0.80, 0.70, 1.0)
  pink      = _Hsv(330, 0.40, 1.00, 1.0)
  lime      = _Hsv(80,  0.95, 1.00, 1.0)
  teal      = _Hsv(180, 0.70, 0.60, 1.0)
  indigo    = _Hsv(255, 0.70, 0.50, 1.0)

  # ---- Neutrals (8) ----
  white     = _Hsv(0,   0.00, 1.00, 1.0)
  black     = _Hsv(0,   0.00, 0.00, 1.0)
  gray      = _Hsv(0,   0.00, 0.50, 1.0)
  lightgray = _Hsv(0,   0.00, 0.75, 1.0)
  darkgray  = _Hsv(0,   0.00, 0.25, 1.0)
  charcoal  = _Hsv(0,   0.00, 0.15, 1.0)
  cream     = _Hsv(40,  0.10, 0.97, 1.0)
  tan       = _Hsv(33,  0.33, 0.82, 1.0)

  # ---- Skin tones, light → dark (6) ----
  skin1     = _Hsv(20,  0.20, 0.95, 1.0)   # pale pink
  skin2     = _Hsv(22,  0.35, 0.90, 1.0)   # warm peach
  skin3     = _Hsv(30,  0.40, 0.78, 1.0)   # olive
  skin4     = _Hsv(25,  0.50, 0.60, 1.0)   # tan brown
  skin5     = _Hsv(20,  0.55, 0.40, 1.0)   # deep brown
  skin6     = _Hsv(15,  0.60, 0.25, 1.0)   # very dark

  # ---- Wood (4) ----
  oak1      = _Hsv(35,  0.40, 0.75, 1.0)   # pale oak
  oak2      = _Hsv(25,  0.55, 0.45, 1.0)   # aged oak
  mahogany  = _Hsv(10,  0.70, 0.40, 1.0)
  birch     = _Hsv(35,  0.15, 0.92, 1.0)

  # ---- Stone / minerals (6) ----
  granite   = _Hsv(0,   0.02, 0.42, 1.0)
  marble    = _Hsv(50,  0.05, 0.92, 1.0)
  slate     = _Hsv(220, 0.15, 0.40, 1.0)
  jade      = _Hsv(140, 0.55, 0.55, 1.0)
  ruby      = _Hsv(355, 0.85, 0.55, 1.0)
  sapphire  = _Hsv(220, 0.80, 0.50, 1.0)

  # ---- Plants (6) ----
  leaf1     = _Hsv(95,  0.70, 0.85, 1.0)   # spring green
  leaf2     = _Hsv(110, 0.65, 0.45, 1.0)   # forest
  grass     = _Hsv(90,  0.75, 0.60, 1.0)
  moss      = _Hsv(75,  0.45, 0.40, 1.0)
  sage      = _Hsv(85,  0.25, 0.65, 1.0)
  autumn    = _Hsv(20,  0.85, 0.70, 1.0)   # foliage orange

  # ---- Sky / water (8) ----
  skyblue1  = _Hsv(210, 0.35, 0.95, 1.0)   # horizon, paler
  skyblue2  = _Hsv(220, 0.70, 0.85, 1.0)   # zenith, deeper
  sunset    = _Hsv(15,  0.75, 0.95, 1.0)
  dawn      = _Hsv(20,  0.40, 0.95, 1.0)
  fog       = _Hsv(210, 0.05, 0.85, 1.0)
  ocean     = _Hsv(210, 0.60, 0.40, 1.0)
  lagoon    = _Hsv(180, 0.55, 0.70, 1.0)
  lake      = _Hsv(205, 0.50, 0.45, 1.0)

  # ---- Metals (6) ----
  gold      = _Hsv(45,  0.85, 0.95, 1.0)
  silver    = _Hsv(0,   0.02, 0.85, 1.0)
  copper    = _Hsv(20,  0.60, 0.75, 1.0)
  brass     = _Hsv(45,  0.55, 0.75, 1.0)
  iron      = _Hsv(220, 0.05, 0.35, 1.0)
  rust      = _Hsv(15,  0.75, 0.50, 1.0)

  # ---- Fire / warm (4) ----
  ember     = _Hsv(15,  1.00, 0.40, 1.0)   # deep red-orange
  flame     = _Hsv(30,  1.00, 1.00, 1.0)   # bright
  terracotta = _Hsv(15, 0.55, 0.70, 1.0)   # warm clay
  coral     = _Hsv(7,   0.55, 1.00, 1.0)

  # ---- Food / misc (4) ----
  chocolate = _Hsv(20,  0.60, 0.35, 1.0)
  wine      = _Hsv(350, 0.70, 0.30, 1.0)   # burgundy
  lavender  = _Hsv(265, 0.30, 0.85, 1.0)
  salmon    = _Hsv(10,  0.50, 1.00, 1.0)


colors = _Colors()


def Transform(**kwargs):
  """Kwargs-style constructor for lev2.Transform.

  Author writes:
      transform=Transform(translation=vec3(0, 5, 0))

  Implementation: constructs the underlying reflected Transform via the
  no-arg ctor and applies each kwarg via setattr. Equivalent to the
  Transform()+mutate pattern in physics/minimal.py etc.; centralized here
  so the §4 sugar surface reads as "typed constructor with kwargs."

  Future M-something: a C++ pyext kwargs constructor on Transform itself
  would let this helper disappear. Until then, this wrapper carries the
  ergonomic shape.
  """
  xf = _CoreTransform()
  for k, v in kwargs.items():
    setattr(xf, k, v)
  return xf


###############################################################################
# Pass-1 declaration records.
#
# Each method on Scene that the user calls stages one of these. Pass 2
# (Scene.build) walks the collections and emits the real ecs.SceneData
# declarations.
###############################################################################

class _SystemDecl:
  """A staged system declaration. typename is the registered ECS class name
  (e.g. 'SceneGraphSystem'); kwargs become reflected-property assigns on the
  resulting SystemData; sub_calls are deferred method invocations (e.g.
  declareLayer, declareParams). Stable across the M0 surface; will grow
  metadata fields as later milestones need them (phase, dependencies)."""
  __slots__ = ("typename", "kwargs", "sub_calls", "_lowered")

  def __init__(self, typename, kwargs):
    self.typename = typename
    self.kwargs = dict(kwargs)
    self.sub_calls = []         # list of (method_name, args, kwargs)
    self._lowered = None        # populated in Pass 2

  def lower(self, sd):
    sysdata = sd.declareSystem(self.typename)
    for k, v in self.kwargs.items():
      setattr(sysdata, k, v)
    for method, args, kwargs in self.sub_calls:
      getattr(sysdata, method)(*args, **kwargs)
    self._lowered = sysdata
    return sysdata


class _ComponentDecl:
  """A staged component declaration. typename is the registered ECS class
  name (e.g. 'SceneGraphComponent'); kwargs are reflected-property assigns
  on the ComponentData; sub_calls are deferred method invocations (e.g.
  declareNodeOnLayer for SceneGraphComponent). Not bound to an archetype
  until added to one via Scene.entity(components=[...]) or
  Scene.component(arch, ...)."""
  __slots__ = ("typename", "kwargs", "sub_calls", "_lowered")

  def __init__(self, typename, kwargs):
    self.typename = typename
    self.kwargs = dict(kwargs)
    self.sub_calls = []
    self._lowered = None

  def lower(self, arch):
    comp = arch.declareComponent(self.typename)
    # If a kwarg's value is an asset-DSL wrapper (e.g.
    # self.asset.ParticleSystem("ptc", ...)), capture its asset name
    # so the post-deserialize wire step can swap in the materialized
    # artifact. The kwarg itself receives the live built artifact for
    # the in-process (pre-serialize) staging case. Currently only
    # ParticlesComponentData has a reflected _particles_asset_name
    # slot; extend the mapping if other component types grow one.
    _ASSET_NAME_SLOT = {
      "drawabledata": "particles_asset_name",
    }
    for k, v in self.kwargs.items():
      gd = getattr(v, "gendata", None)
      asset_name = getattr(gd, "asset_name", "") if gd is not None else ""
      if asset_name and k in _ASSET_NAME_SLOT:
        live = _materialize_asset(v)
        setattr(comp, k, live)
        setattr(comp, _ASSET_NAME_SLOT[k], asset_name)
      else:
        setattr(comp, k, v)
    for method, args, kwargs in self.sub_calls:
      getattr(comp, method)(*args, **kwargs)
    self._lowered = comp
    return comp


class _ArchetypeDecl:
  """A staged archetype declaration. Holds the list of components that will
  be lowered when the archetype is. _lowered is set in Pass 2 so spawners
  can reference the real Archetype object."""
  __slots__ = ("name", "_components", "_lowered")

  def __init__(self, name):
    self.name = name
    self._components = []   # list of _ComponentDecl, in declaration order
    self._lowered = None

  def lower(self, sd):
    arch = sd.declareArchetype(self.name)
    for cdecl in self._components:
      cdecl.lower(arch)
    self._lowered = arch
    return arch


class _SpawnerDecl:
  """A staged spawner declaration. Always references its parent _ArchetypeDecl
  (NOT the lowered Archetype). Pass 2 resolves the reference via
  arch._lowered."""
  __slots__ = ("name", "arch", "autospawn", "transform", "publish_xf",
               "_lowered")

  def __init__(self, name, arch, autospawn, transform, publish_xf=""):
    self.name = name
    self.arch = arch            # _ArchetypeDecl
    self.autospawn = autospawn
    self.transform = transform  # lev2.Transform or None
    self.publish_xf = publish_xf  # str — opt-in entity transform publication

  def lower(self, sd):
    sp = sd.declareSpawner(self.name)
    sp.archetype = self.arch._lowered
    sp.autospawn = self.autospawn
    if self.publish_xf:
      sp.publishxf_name = self.publish_xf
    if self.transform is not None:
      # spawner.transform is a mutable sub-object in C++; copy known fields
      # from the user's Transform onto it. Field-by-field rather than
      # whole-assign because the SpawnData::transform property may not
      # accept whole-assignment in all bindings.
      src = self.transform
      tgt = sp.transform
      for field in ("translation", "orientation", "nonUniformScale", "scale"):
        if hasattr(src, field):
          try:
            v = getattr(src, field)
          except Exception:
            continue
          setattr(tgt, field, v)
    self._lowered = sp
    return sp


###############################################################################
# Handles returned to the author.
#
# These are thin Python objects that store deferred-declaration records and
# expose convenience methods. They are NOT reflected ECS objects — they're
# pure Python that lives only during Scene construction.
###############################################################################

class _EntityHandle:
  """Returned by self.entity(name, ...). Lets the author add additional
  spawners off the same archetype via .spawner(...). Future milestones
  may add .transform.x = Expr(...) wiring proxies; M0 ships the bare
  spawner-extension method."""
  __slots__ = ("_scene", "_arch", "default_spawner")

  def __init__(self, scene, arch, default_spawner):
    self._scene = scene
    self._arch = arch
    self.default_spawner = default_spawner

  @property
  def name(self):
    """The default spawner's name (= the entity name passed to
    self.entity(...) / self.probe(...)). None when spawner=False."""
    return self.default_spawner.name if self.default_spawner else None

  def spawner(self, name, *, transform=None, autospawn=True, publish_xf=""):
    """Add another spawner off this entity's archetype. Useful for
    multi-instance entities (multiple positions sharing one archetype)
    and for the spawner=False entity case where the user wants only
    dynamic spawns. publish_xf — see Scene.spawner."""
    return self._scene.spawner(name, self._arch,
                               autospawn=autospawn,
                               transform=transform,
                               publish_xf=publish_xf)


class _DrawablesFactory:
  """Factory namespace for drawables that the SceneGraph system consumes.
  M0: just model() (loads a glTF/asset path into a ModelDrawableData).
  Later milestones add instanced_model, rigid_primitive, vdb_mesh, etc."""
  __slots__ = ()

  def model(self, path):
    return lev2.ModelDrawableData(path)


class _AssetNamespace:
  """Scene-side asset DSL surface (HYPERECS M2a).

  Attribute access (self.asset.HollowFunnelMesh) returns a per-class factory
  bound to this Scene. Calling that factory:
    1. Validates the asset name is unique within the Scene.
    2. Instantiates the underlying assets.py class with the given kwargs.
    3. Eagerly calls .build() to produce the artifact.
    4. Registers the artifact under the given name in self._scene._assets.
    5. Returns the BUILT artifact (verts/tris tuple, FloatGrid, Drawable, ...).

  Downstream consumers in the same Scene see real artifacts, not gen
  instances; cross-asset references (e.g. MeshToSdf(input_mesh=funnel))
  receive the already-built funnel mesh directly. M2b will swap this layer
  for reflected gen-data registration on AssetSystem; the author surface
  here stays the same."""

  __slots__ = ("_scene",)

  def __init__(self, scene):
    self._scene = scene

  def __getattr__(self, gen_name):
    if gen_name.startswith("_"):
      raise AttributeError(gen_name)
    cls = _ASSET_REGISTRY.get(gen_name)
    if cls is None:
      raise AttributeError(
        f"unknown asset gen {gen_name!r}; registered: "
        f"{sorted(_ASSET_REGISTRY)}")
    scene = self._scene
    def factory(asset_name, **kwargs):
      if asset_name in scene._assets:
        raise ValueError(
          f"asset {asset_name!r} already declared in this scene")
      wrap = cls(**kwargs)
      # Tag + register reflected gendata so the asset graph survives
      # JSON round-trip (M2b.4). Pure-Python gens (e.g. HollowFunnelMesh,
      # no gendata) skip the registration — they don't round-trip yet.
      gendata = getattr(wrap, "gendata", None)
      if gendata is not None:
        gendata.asset_name = asset_name
        scene._asset_gens.append((asset_name, gendata))
      built = wrap.build()
      # Cache the built artifact on the wrapper. Downstream consumers
      # see the wrapper (with .gendata.asset_name discoverable for
      # cross-asset name capture); _materialize() unwraps to .built
      # for code that needs the live artifact. The Scene also keeps a
      # name→built lookup for resolvers that don't have the wrapper.
      wrap.built = built
      scene._assets[asset_name] = built
      return wrap
    factory.__name__ = f"asset_{gen_name}"
    return factory


class SceneGraphHandle:
  """Handle returned by self.scenegraph(...). Carries the SystemData decl
  and exposes:
    - component(nodes={...})  → SceneGraphComponent declaration
    - drawables.<factory>(...)  → DrawableData factories
  """
  __slots__ = ("_scene", "_decl", "external", "drawables",
               "_primary_layer")

  def __init__(self, scene, *, preset="ForwardPBR", layers=None,
               external=False, params=None):
    self._scene = scene
    self.external = external
    self.drawables = _DrawablesFactory()
    self._decl = _SystemDecl(typename="SceneGraphSystem", kwargs={})
    # The "primary" forward-rendering layer for this preset — used as
    # the implicit default when a node spec or particle component omits
    # `layer`/`layername`. depth_prepass auto-add happens C++-side via
    # NodeDef._skipAutoDepthPrepass=false, so authors only need to
    # name the forward layer (or, with this default, nothing at all).
    implied = _PRESET_IMPLIED_LAYERS.get(preset, [])
    self._primary_layer = implied[0] if implied else "std_forward"
    if external:
      # §4.6: external=True means host owns the SG; we reject config
      # kwargs. preset/layers/params would conflict with what the host
      # configured. M0 keeps this strict — pass nothing or pass nothing.
      if preset != "ForwardPBR" or layers is not None or params:
        raise ValueError(
          "scenegraph(external=True): host owns scenegraph configuration; "
          "remove preset/layers/params or set external=False")
      # Still declare the SystemData so the C++ hook exists; the host's
      # createSimulation(scenegraph=...) injection replaces the auto-
      # created scene at stage time (see HYPERECS.md §4.6).
    else:
      params_dict = dict(params or {})
      # PBR2 Phase 0 — skybox_probe= wrapper kwarg. Captured onto the
      # SystemData's _skybox_path field as an "asset://<name>" URI so
      # wire_scene_data can resolve to a baked .xir at load time. Literal
      # paths (any non-asset:// string) flow through as-is. Bare-string
      # _userParams["SkyboxTexPathStr"] continues to work unchanged for
      # scenes that don't want a wrapper.
      skybox_probe = params_dict.pop("skybox_probe", None)
      skybox_path  = params_dict.pop("skybox_path",  None)
      if skybox_probe is not None and skybox_path is not None:
        raise ValueError(
          "scenegraph(...) takes skybox_probe= OR skybox_path=, not both")
      if skybox_probe is not None:
        gd = getattr(skybox_probe, "gendata", None)
        asset_name = getattr(gd, "asset_name", "") if gd is not None else ""
        if not asset_name:
          raise ValueError(
            "scenegraph(skybox_probe=...) expects an asset wrapper with "
            "a gendata.asset_name (e.g. self.asset.HdriToXir('studio', ...))")
        self._decl.kwargs["skybox_path"] = f"asset://{asset_name}"
      elif skybox_path is not None:
        # Literal path (e.g. "<ork_envmaps2>/pillars8k.xir"). Flows through
        # wire_scene_data's non-asset:// branch → _userParams["SkyboxTexPathStr"].
        self._decl.kwargs["skybox_path"] = skybox_path
      params_dict.setdefault("preset", preset)
      self._decl.sub_calls.append(("declareParams", (params_dict,), {}))
      # Union preset's implied layers (e.g. ForwardPBR ⇒ std_forward +
      # depth_prepass) with any user-supplied extras. Authors who only
      # want the preset defaults can omit layers= entirely.
      for layer in _effective_layers(preset, layers):
        self._decl.sub_calls.append(("declareLayer", (layer,), {}))

  def component(self, *, nodes=None):
    """Construct a SceneGraphComponent declaration. nodes is a dict keyed
    by user-chosen node name; each value is a dict with at minimum
    'drawable' (the only required key), plus optional 'layer',
    'transform', and 'modcolor'. Each entry lowers to one
    declareNodeOnLayer call.

    When 'layer' is omitted, the node lands on this handle's primary
    layer (preset-derived: ForwardPBR → "std_forward",
    DeferredPBR → "std_deferred", ...). depth_prepass participation
    is automatic (C++ NodeDef._skipAutoDepthPrepass defaults false).

    The 'drawable' value may be either a built DrawableData OR an
    asset-DSL wrapper (e.g. self.asset.VdbGridToDrawable(...)); the
    wrapper is unwrapped via _materialize to the live artifact for the
    declareNodeOnLayer call. The pyext binding expects a real
    drawabledata_ptr_t."""
    cdecl = _ComponentDecl(typename="SceneGraphComponent", kwargs={})
    if nodes:
      for node_name, spec in nodes.items():
        drawable_in = spec["drawable"]
        # If the drawable is an asset-DSL wrapper, capture its asset name
        # onto the NodeDef so it survives the JSON round-trip. On reload,
        # the post-deserialize wiring step looks up the materialized
        # drawable by this name and replaces the placeholder that came
        # out of deserialization. See ork.ecs.scene.assets.wire_scene_data.
        asset_name = ""
        gd = getattr(drawable_in, "gendata", None)
        if gd is not None:
          asset_name = getattr(gd, "asset_name", "") or ""
        call_kwargs = {
          "name": node_name,
          "drawable": _materialize_asset(drawable_in),
          "layer": spec.get("layer", self._primary_layer),
        }
        if asset_name:
          call_kwargs["drawable_asset_name"] = asset_name
        # PBR2 Phase 0 — per-node HDRI override. The spec "envmap" key
        # accepts either an HdriToXir wrapper (captured as
        # "asset://<name>" URI; wire_scene_data resolves at load time)
        # or a literal path string (passed straight through to
        # loadEnvMapOverride). Empty / absent = use scene-global skybox.
        envmap_in = spec.get("envmap", None)
        if envmap_in is not None:
          if isinstance(envmap_in, str):
            call_kwargs["envmap_path"] = envmap_in
          else:
            env_gd = getattr(envmap_in, "gendata", None)
            env_name = getattr(env_gd, "asset_name", "") if env_gd is not None else ""
            if not env_name:
              raise ValueError(
                f"node {node_name!r}: 'envmap' must be a string path or an "
                f"asset wrapper with gendata.asset_name (e.g. "
                f"self.asset.HdriToXir('studio', ...)); got {type(envmap_in).__name__}")
            call_kwargs["envmap_path"] = f"asset://{env_name}"
        if "transform" in spec:
          call_kwargs["transform"] = spec["transform"]
        if "modcolor" in spec:
          call_kwargs["modcolor"] = spec["modcolor"]
        cdecl.sub_calls.append(("declareNodeOnLayer", (), call_kwargs))
    return cdecl


###############################################################################
# The Scene base class.
###############################################################################

class Scene:
  """Tier 3 declarative ECS scene composite. Subclass and declare your scene
  in __init__; call build(sd) to lower into a reflected ecs.SceneData.

  See ~/HYPERECS.md §1.3 and §4 for the full design.

  Minimal example (M0 acid test)::

      class HelloScene(Scene):
        def __init__(self):
          super().__init__()
          SG = self.scenegraph(preset="ForwardPBR", layers=["std_forward"])
          self.entity("cube",
            transform=Transform(translation=vec3(0, 0, 0)),
            components=[SG.component(nodes={
              "c": {"layer": "std_forward",
                    "drawable": SG.drawables.model("data://tests/pbr_calib.glb")},
            })])
  """

  def __init__(self):
    # Pass-1 collections. Python 3.7+ dicts preserve insertion order, which
    # we rely on for deterministic Pass 2 walk.
    self._systems    = {}   # typename → _SystemDecl
    self._archetypes = {}   # name → _ArchetypeDecl
    self._spawners   = {}   # name → _SpawnerDecl
    self._assets     = {}   # name → built artifact (M2a — eager-built)
    # (name, gendata) pairs for reflected asset gens. Collected during
    # Pass 1 by the asset factory; lowered to AssetSystemData at the
    # head of Pass 2 (Scene.build). M2b.4.
    self._asset_gens = []
    self._built      = False
    # Asset DSL namespace: self.asset.<GenName>("name", **kwargs).
    # See ork/ecs/scene/assets.py for the registry of available gens.
    self.asset = _AssetNamespace(self)

  # ---------------------------------------------------------------------------
  # Primitive layer — explicit ECS declarations
  # ---------------------------------------------------------------------------

  def system_data(self, typename, **kwargs):
    """Declare an arbitrary ECS system by class name. Returns the
    _SystemDecl handle (kwargs are reflected-property assigns)."""
    if typename in self._systems:
      raise ValueError(f"system {typename!r} already declared in this scene")
    decl = _SystemDecl(typename=typename, kwargs=kwargs)
    self._systems[typename] = decl
    return decl

  def archetype(self, name):
    """Declare an archetype. Returns the _ArchetypeDecl handle; pass it to
    self.component(arch, ...) and self.spawner(name, arch, ...) to attach
    components and spawners."""
    if name in self._archetypes:
      raise ValueError(f"archetype {name!r} already declared in this scene")
    decl = _ArchetypeDecl(name)
    self._archetypes[name] = decl
    return decl

  def component(self, arch, typename, **kwargs):
    """Declare a component on an archetype. Returns the _ComponentDecl
    handle so post-declaration mutations (like declareNodeOnLayer) can be
    queued for Pass 2."""
    if not isinstance(arch, _ArchetypeDecl):
      raise TypeError(f"self.component expected an _ArchetypeDecl from "
                      f"self.archetype(); got {type(arch).__name__}")
    cdecl = _ComponentDecl(typename=typename, kwargs=kwargs)
    arch._components.append(cdecl)
    return cdecl

  def declare_component(self, typename, **kwargs):
    """Build a FREE _ComponentDecl (not yet attached to any archetype).
    Pass it to Scene.entity(components=[...]) — parallel to what
    SG.component(nodes={...}) returns, but for arbitrary component types
    that don't have a dedicated handle yet (e.g. ParticlesComponent
    pre-M5).

    Particle-component special-case: when typename == "ParticlesComponent"
    and the caller omits `layername`, default it to the SG handle's
    primary layer (std_forward under ForwardPBR). The C++ fallback when
    layername is empty is the SceneGraphSystem's "sg_default" layer,
    which the PBR compositor doesn't render — defaulting here matches
    the SG.component(nodes={...}) ergonomics."""
    if typename == "ParticlesComponent" and "layername" not in kwargs:
      sg_decl = self._systems.get("SceneGraphSystem")
      # Walk the staged scenegraph's declared layers to find the
      # primary. We stored them via declareLayer sub-calls; the first
      # entry is the preset's primary forward layer.
      primary = None
      if sg_decl is not None:
        for method, args, _ in sg_decl.sub_calls:
          if method == "declareLayer" and args:
            primary = args[0]
            break
      if primary:
        kwargs["layername"] = primary
    return _ComponentDecl(typename=typename, kwargs=kwargs)

  def spawner(self, name, arch, *, autospawn=True, transform=None,
              publish_xf=""):
    """Declare a spawner. Multiple spawners can share one archetype (the
    pattern from physics/FPS.py).

    publish_xf — when non-empty, the spawned entity's live transform is
    registered in the Simulation's published-xf registry under
    "<publish_xf><N>" (counter is monotonic per base name across
    spawn/despawn cycles). Consumers that want to track this entity
    (e.g. VdbCollider.follow_entity="saddle0") look it up by the full
    key. Empty (default) → no publication."""
    if name in self._spawners:
      raise ValueError(f"spawner {name!r} already declared in this scene")
    if not isinstance(arch, _ArchetypeDecl):
      raise TypeError(f"self.spawner expected an _ArchetypeDecl; got "
                      f"{type(arch).__name__}")
    decl = _SpawnerDecl(name=name, arch=arch,
                        autospawn=autospawn, transform=transform,
                        publish_xf=publish_xf)
    self._spawners[name] = decl
    return decl

  # ---------------------------------------------------------------------------
  # Sugar layer — system handles and entity sugar
  # ---------------------------------------------------------------------------

  def scenegraph(self, *, preset="ForwardPBR", layers=None,
                 external=False, **params):
    """Declare the SceneGraphSystem and return its handle.

    The handle exposes .component(nodes={...}) for constructing
    SceneGraphComponent declarations, and .drawables.<factory>(...) for
    building drawables.

    layers=  — optional. When omitted, the preset's implied layers are
               declared automatically (ForwardPBR ⇒ std_forward +
               depth_prepass; see _PRESET_IMPLIED_LAYERS). When supplied,
               the values are appended after the implied set so scenes
               can declare scene-specific extras without losing the
               preset's required layers.

    external=True (HYPERECS §4.6): the host owns the scenegraph; this
    Scene only declares the system hook + drawables/components against
    it. Config kwargs are rejected. Only valid on scenegraph; no other
    handle accepts the flag."""
    if "SceneGraphSystem" in self._systems:
      raise ValueError("scenegraph already declared in this scene")
    handle = SceneGraphHandle(scene=self,
                              preset=preset, layers=layers,
                              external=external, params=params)
    self._systems["SceneGraphSystem"] = handle._decl
    return handle

  def entity(self, name, *, transform=None, components=(), spawner=True,
             publish_xf=""):
    """Entity sugar: archetype + N components + optional implicit spawner.

    Returns an _EntityHandle. spawner=True (the default) creates one
    auto-spawned spawner with the same name as the entity. spawner=False
    declares the archetype + components without any spawner; useful for
    archetypes that exist only for dynamic spawn calls. Additional
    spawners can be added via entity_handle.spawner(name, ...).

    publish_xf — see Scene.spawner; forwarded to the default spawner
    when spawner=True. Authors who want publication on a non-default
    spawner should call entity_handle.spawner(..., publish_xf=...)
    explicitly."""
    arch = self.archetype(f"Arch_{name}")
    for comp_decl in components:
      if not isinstance(comp_decl, _ComponentDecl):
        raise TypeError(
          f"self.entity({name!r}) components must be _ComponentDecl "
          f"(returned by handle.component(...) or self.component(...)); "
          f"got {type(comp_decl).__name__}")
      arch._components.append(comp_decl)
    sp = None
    if spawner:
      sp = self.spawner(name, arch, autospawn=True, transform=transform,
                        publish_xf=publish_xf)
    elif publish_xf:
      raise ValueError(
        f"self.entity({name!r}, publish_xf={publish_xf!r}, spawner=False): "
        f"nothing to publish — no default spawner exists. Declare the "
        f"publication on a specific spawner via entity_handle.spawner(...).")
    return _EntityHandle(scene=self, arch=arch, default_spawner=sp)

  # ---------------------------------------------------------------------------
  # Probe sugar — implicit entity carrying a ProbeComponent.
  # ---------------------------------------------------------------------------

  def probe(self, name, *,
            transform        = None,
            output_folder    = "/tmp/ecs_probes",
            output_prefix    = None,
            image_dim        = 1024,
            render_layer     = "std_forward",
            dynamic          = False):
    import os as _os
    # Materialize-time mkdir. ork.ecsedit's Bake Lighting writes PNGs
    # straight into _outputFolder; the C++ side doesn't create
    # intermediate directories so we do it here (idempotent). Covers
    # both author-time (ork.scene.tojson, ork.scene.viewer) and any
    # downstream Python that constructs the Scene class.
    expanded = _os.path.expandvars(_os.path.expanduser(output_folder))
    _os.makedirs(expanded, exist_ok=True)
    """Declare a probe entity at `transform`. Auto-adds ProbeSystem the
    first time it's called.

    Produces archetype + ProbeComponent + autospawn'd spawner, all named
    after `name`. ork.ecsedit.py's "Bake Lighting" button picks up every
    declared probe and writes <output_folder>/<output_prefix>_<idx>.png
    (cubemap → equirectangular). Default output_prefix = name.

    Kwargs map (snake_case → pyext camelCase):
      output_folder → outputFolder   (where the .png lands)
      output_prefix → outputPrefix   (filename stem; defaults to entity name)
      image_dim     → imageDim       (cubemap face resolution, 64..4096)
      render_layer  → renderLayer    (which scene layer the probe renders)

    Returns the _EntityHandle so authors can pin extra spawners or
    components onto the same archetype if needed."""
    if output_prefix is None:
      output_prefix = name
    # ProbeSystem auto-declaration (idempotent — multiple probe()
    # calls share one system). Mirrors the ParticlesGlobalSystem
    # auto-attach pattern in M3.10.
    if "ProbeSystem" not in self._systems:
      self.system_data("ProbeSystem")
    probe_decl = self.declare_component("ProbeComponent",
                                        imageDim     = image_dim,
                                        outputFolder = output_folder,
                                        outputPrefix = output_prefix,
                                        renderLayer  = render_layer,
                                        dynamic      = dynamic)
    return self.entity(name,
                       transform  = transform,
                       components = [probe_decl])

  # ---------------------------------------------------------------------------
  # Pass 2 — lower everything into ecs.SceneData
  # ---------------------------------------------------------------------------

  def build(self, sd):
    """Lower the staged declarations into the given ecs.SceneData.
    Idempotent guard: raises if called twice on the same Scene."""
    if self._built:
      raise RuntimeError("Scene.build(sd) called twice on the same instance")
    self._built = True

    # M2b.4: lazy-declare AssetSystem if any reflected asset gens were
    # registered in Pass 1. Queues a declareAssetGen sub-call per gen
    # to attach the gendata to the lowered AssetSystemData. Declared
    # at the head of the systems list — load order doesn't matter,
    # but conceptually assets exist before anything that uses them.
    if self._asset_gens:
      if "AssetSystem" in self._systems:
        raise RuntimeError(
          "Scene.build: AssetSystem already declared via self.system_data; "
          "asset registration is handled implicitly by self.asset.*")
      decl = _SystemDecl(typename="AssetSystem", kwargs={})
      for asset_name, gendata in self._asset_gens:
        decl.sub_calls.append(("declareAssetGen", (gendata,), {}))
      # Prepend so AssetSystemData appears first in scene_data.systemDatas
      # (cosmetic — load order doesn't depend on this).
      self._systems = {"AssetSystem": decl, **self._systems}

    # PBR2 P3.D — auto-attach PostFxNodeSSSS when any PbrMaterialGenData
    # declares has_subsurface=True. The node + the "ssss" entry in
    # postfx_order survive JSON round-trip (both reflected on
    # SceneGraphSystemData). Zero cost when no material uses subsurface.
    # Scans for the FIRST subsurface material and uses its subsurface_color
    # as the post-fx tint — v1 limitation. Multi-material SSSS w/ different
    # tints would need a per-pixel tint channel (target2) — P3.E.
    dominant_sss_gen = None
    for _name, gen in self._asset_gens:
      if (type(gen).__name__ == "PbrMaterialGenData"
          and getattr(gen, "has_subsurface", False)):
        dominant_sss_gen = gen
        break
    if dominant_sss_gen is not None and "SceneGraphSystem" in self._systems:
      from orkengine.lev2 import PostFxNodeSSSS
      sg_decl = self._systems["SceneGraphSystem"]
      ssss_node = PostFxNodeSSSS()
      ssss_node.subsurface_tint = dominant_sss_gen.subsurface_color
      # P3.D — DSL-driven SSSS knobs. C++ defaults are physical baselines;
      # any scene can override via its own self.scenegraph(...) kwargs in
      # the future (currently these are scene-DSL-injected with sane values
      # for the mtl_ss_* acid scenes — 32px reach, energy-preserving gain).
      ssss_node.blurfactor  = 32.0   # screen-space kernel reach in pixels
      ssss_node.strength    = 1.0    # energy-conserving; raise for drama
      ssss_node.debug_mode  = 0      # 0=normal; 1=mask 2=raw 3=blur 4=delta 5=lit 6=depth-tex 7=green 8=uniforms 9=lindepth 10=tint
      sg_decl.sub_calls.append(("addPostFxNode", ("ssss", ssss_node), {}))
      sg_decl.sub_calls.append(("appendPostFxOrder", ("ssss",), {}))

    # Pass 2 order:
    #   1. Systems first  — components reference layers; layers are
    #      idempotently created on the SG side regardless of declaration
    #      order, but declaring systems first matches the natural reading.
    #   2. Archetypes (which include their attached components) — so the
    #      Archetype C++ object exists before spawners point at it.
    #   3. Spawners — reference archetypes via arch._lowered.

    for typename, decl in self._systems.items():
      decl.lower(sd)
    for name, decl in self._archetypes.items():
      decl.lower(sd)
    for name, decl in self._spawners.items():
      decl.lower(sd)


###############################################################################
# Public export list
###############################################################################

__all__ = [
  "Scene",
  "Transform",
  "SceneGraphHandle",
  "hsv",
  "colors",
]
