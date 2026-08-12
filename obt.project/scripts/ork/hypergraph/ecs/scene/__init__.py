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

import types

from orkengine import lev2
from orkengine.core import vec3, vec4
from orkengine.core import Transform as _CoreTransform

from ork.hypergraph.ecs.scene.assets import _ASSET_REGISTRY, _materialize as _materialize_asset

# Trigger registration of every concrete-named asset wrapper that physically
# lives in ork.hypergraph.assets/<category>/<name>.py (post-refactor location
# for new wrappers; the asset_core/* shims still point back to assets.py for
# the older 12 classes). Each `@_register` decorator fires on module load
# and appends to _ASSET_REGISTRY — Scene._AUTHOR_GLOBALS picks them up at
# the bottom of this file.
import ork.hypergraph.assets  # noqa: F401  side-effect: asset registration


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
# Direct diffuse BRDF — which lobe the analytic lights (point, spot, sun) use.
# The names are the TOKEN spelling and mirror pbr::DiffuseBrdfModel; the crc of
# the name is the identity C++ resolves, so tokens.OREN_NAYAR and "OREN_NAYAR"
# are one declaration. Absent means the engine default (oren-nayar).
#
# What lands in the scene params is the NAME, not the token: an .ecs carries
# scene params through a JSON svar that knows string/number/bool only, and a
# name is also the thing a human reads in the file.
###############################################################################

_DIFFUSE_BRDF_MODELS = ("LAMBERT", "OREN_NAYAR", "BURLEY")

_DIFFUSE_BRDF_BY_CRC = None


def _diffuse_brdf_by_crc():
  """crc -> name, built through the CrcStringProxy — the SAME flyweight a scene's
  tokens.X comes from, so the two hashes are the same machinery and not two
  spellings of it. (CrcString itself takes no string from python; the proxy is
  the only python-side hasher.)"""
  global _DIFFUSE_BRDF_BY_CRC
  if _DIFFUSE_BRDF_BY_CRC is None:
    from orkengine.core import CrcStringProxy
    proxy = CrcStringProxy()
    _DIFFUSE_BRDF_BY_CRC = {
      getattr(proxy, name).hashed: name for name in _DIFFUSE_BRDF_MODELS}
  return _DIFFUSE_BRDF_BY_CRC


def _resolve_diffuse_brdf(value):
  """token (tokens.OREN_NAYAR) or name -> canonical name. Refuses anything else
  here, at compose time, naming the valid set."""
  valid = ", ".join(_DIFFUSE_BRDF_MODELS)
  name = value if isinstance(value, str) else None
  if name is None:
    hashed = getattr(value, "hashed", None)
    if hashed is None:
      raise TypeError(
        f"scenegraph(diffuse_brdf=...) wants a token (tokens.OREN_NAYAR) or a "
        f"name; got {type(value).__name__}. valid: {valid}")
    name = _diffuse_brdf_by_crc().get(hashed)
    if name is None:
      raise ValueError(
        f"scenegraph(diffuse_brdf=crc:{hashed}) names no diffuse lobe. valid: {valid}")
  if name not in _DIFFUSE_BRDF_MODELS:
    raise ValueError(
      f"scenegraph(diffuse_brdf={name}) names no diffuse lobe. valid: {valid}")
  return name


###############################################################################
# Public helpers — re-exported by package __init__.
###############################################################################

# Color helpers (hsv, colors, _Hsv) live in ork.hypergraph.colors (universal
# helper module — no ECS coupling). Re-imported here so existing
# `from ork.hypergraph.ecs.scene import hsv, colors` lines stay valid during
# the migration. New code should import from ork.hypergraph.colors directly.
from ork.hypergraph.colors import (
  hsv, wavelength, colortemp, mix, colors,
  _LazyColor, _Hsv, _PBR_VEC3_FIELDS, _PBR_VEC4_FIELDS,
)


from ork.hypergraph.ecs.scene._helpers import Transform, axis_angle, _coerce_transform


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
    # DEPTH PREPASS — an ENGINE INVARIANT, ON for every ECS scene. Written here
    # and not at scenegraph() because this is the last word: declareParams is dict
    # assignment and sub_calls run in order, so any mixin that amended the params
    # (sky, cloud deck, a scene appending its own declareParams) has already run,
    # and the invariant is not something a mixin gets to lose.
    if self.typename == "SceneGraphSystem":
      sysdata.declareParams({"DepthPrepass": True})
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
               "lifetime", "_lowered")

  def __init__(self, name, arch, autospawn, transform, publish_xf="",
               lifetime=None):
    self.name = name
    self.arch = arch            # _ArchetypeDecl
    self.autospawn = autospawn
    self.transform = transform  # lev2.Transform or None
    self.publish_xf = publish_xf  # str — opt-in entity transform publication
    self.lifetime = lifetime    # None | secs | (min_secs, max_secs)

  def lower(self, sd):
    sp = sd.declareSpawner(self.name)
    sp.archetype = self.arch._lowered
    sp.autospawn = self.autospawn
    if self.lifetime is not None:
      lt = self.lifetime
      lt_min, lt_max = (float(lt[0]), float(lt[1])) if isinstance(lt, (tuple, list)) \
                       else (float(lt), float(lt))
      sp.lifetimeMin = lt_min
      sp.lifetimeMax = lt_max
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
               "_primary_layer", "_aux_channels")

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
    self._aux_channels = []
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
      # E2B item D — GENERIC AUX CHANNELS + POSTFX chain.
      #   aux_channels=["heat"]: the forward node renders layer "aux_<name>"
      #   into a dedicated RT per channel (materials opt in with an aux
      #   technique pair; ParticlesComponents auto-attach to every declared
      #   aux layer — non-participants are skipped at draw, so it's free).
      #   postfx=[(name, node), ...]: reflected post-fx chain (PBR2 P3.D
      #   registry — round-trips into the zero-Python player).
      aux_channels = params_dict.pop("aux_channels", None)
      postfx       = params_dict.pop("postfx",       None)
      # diffuse_brdf=tokens.OREN_NAYAR — resolved (and refused) at compose time;
      # see _resolve_diffuse_brdf. Omitted keeps the engine default.
      if "diffuse_brdf" in params_dict:
        params_dict["diffuse_brdf"] = _resolve_diffuse_brdf(params_dict["diffuse_brdf"])
      if aux_channels:
        self._aux_channels = [str(c) for c in aux_channels]
        params_dict["AuxChannels"] = ",".join(self._aux_channels)
      if postfx:
        for fx_name, fx_node in postfx:
          self._decl.sub_calls.append(("addPostFxNode", (fx_name, fx_node), {}))
          self._decl.sub_calls.append(("appendPostFxOrder", (fx_name,), {}))
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

  def instanced_node(self, name, *, model, capacity, layers=None):
    """Declare a SYSTEM-LEVEL instanced node: one model drawn up to
    `capacity` times in a single instanced draw. Entities join it by
    naming it in their components' instance_node_name (the serializable
    pairing contract — see Scene.projectile_pool for the bundled form).

    Idle/freed instance slots hold a zero-basis matrix (invisible by
    contract); a paired BulletObjectComponent's motion state writes the
    live slots. layers defaults to [primary, depth_prepass] — instanced
    models participate in early-z via the instanced prepass technique."""
    if layers is None:
      layers = [self._primary_layer, "depth_prepass"]
    drw = lev2.InstancedModelDrawableData(model)
    drw.resize(int(capacity))
    self._decl.sub_calls.append(("declareNodeOnLayer", (), {
        "name":     name,
        "drawable": drw,
        "layers":   list(layers)}))
    return name

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
        # Opt out of the automatic depth_prepass-layer add (NodeDef._skipAutoDepthPrepass).
        # The depth_prepass layer is ALSO the shadow-caster set (sun cascades + spot maps
        # enqueue it), so this removes the node from the prepass AND from shadow casting
        # in one move. Color-pass depth stays correct without prepass membership (the
        # forward pass tests LEQUALS, never EQUAL). Stars/_cloud_deck previously had to
        # use manual declareNodeOnLayer calls to reach this flag.
        if "skip_auto_dpp" in spec:
          call_kwargs["skip_auto_dpp"] = bool(spec["skip_auto_dpp"])
        cdecl.sub_calls.append(("declareNodeOnLayer", (), call_kwargs))
    return cdecl


###############################################################################
# The Scene base class.
###############################################################################

from ork.hypergraph.ecs.scene._terrain import TerrainMixin
from ork.hypergraph.ecs.scene._walker import WalkerMixin
from ork.hypergraph.ecs.scene._projectiles import ProjectilesMixin
from ork.hypergraph.ecs.scene._sun import SunMixin
from ork.hypergraph.ecs.scene._moon import MoonMixin
from ork.hypergraph.ecs.scene._stars import StarsMixin
from ork.hypergraph.ecs.scene._celestial_sky import CelestialSkyMixin
from ork.hypergraph.ecs.scene._sky_dome import SkyDomeMixin
from ork.hypergraph.ecs.scene._cloud_deck import CloudDeckMixin
from ork.hypergraph.ecs.scene._sky import SkyMixin


class Scene(TerrainMixin, WalkerMixin, ProjectilesMixin, SunMixin, MoonMixin,
            StarsMixin, CelestialSkyMixin, SkyDomeMixin, CloudDeckMixin,
            SkyMixin):
  """Tier 3 declarative ECS scene composite. Subclass and declare your scene
  in __init__; call build(sd) to lower into a reflected ecs.SceneData.

  See ~/HYPERECS.md §1.3 and §4 for the full design.

  Minimal example (M0 acid test)::

      class HelloScene(Scene):
        def __init__(self):
          super().__init__()
          SG = self.scenegraph(preset="ForwardPBR", layers=["std_forward"])
          self.entity("cube",
            transform={"translation": vec3(0, 0, 0)},
            components=[SG.component(nodes={
              "c": {"layer": "std_forward",
                    "drawable": SG.drawables.model("data://tests/pbr_calib.glb")},
            })])
  """

  # Populated at module-bottom (after all asset wrappers are imported and
  # _ASSET_REGISTRY is fully built). Contains the bare-name vocabulary that
  # subclass methods see at LOAD_GLOBAL time without polluting their module.
  _AUTHOR_GLOBALS = None

  def __init_subclass__(cls, **kwargs):
    """Inject the DSL vocabulary (colors helpers, vec3/4, Transform, axis_angle,
    every registered asset wrapper) into each method's __globals__ at class
    creation time. This makes bare-name references work inside Scene-subclass
    methods *without* polluting the author's module __dict__.

    Mechanism: rebind each function via types.FunctionType with a merged
    globals dict (helpers first, original module globals on top so explicit
    author imports always win). The function's __code__ and __closure__ are
    preserved — super() and existing imports continue to work unchanged."""
    super().__init_subclass__(**kwargs)
    helpers = Scene._AUTHOR_GLOBALS
    if helpers is None:
      return
    for attr, val in list(vars(cls).items()):
      if not isinstance(val, types.FunctionType):
        continue
      merged = {**helpers, **val.__globals__}
      wrapped = types.FunctionType(
        val.__code__, merged, val.__name__,
        val.__defaults__, val.__closure__)
      wrapped.__kwdefaults__ = val.__kwdefaults__
      setattr(cls, attr, wrapped)

  # Default scenegraph used by `self.SG` when a scene doesn't declare its own
  # (default_sg=True). Override per-scene via super().__init__(skybox_path=...,
  # **sg_params) or by passing default_sg=False and calling self.scenegraph(...).
  DEFAULT_SKYBOX = "<ork_envmaps2>/blender_forest.xir"

  def __init__(self, *, default_sg=True, skybox_path=None, **sg_params):
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
    # The sky's implied cloud decks: staged in Pass 1, declared at the head of
    # Pass 2 unless the scene declared decks of its own (see CloudDeckMixin).
    self._staged_cloud_decks   = None
    self._cloud_decks_declared = False
    # Asset DSL namespace: self.asset.<GenName>("name", **kwargs).
    # See ork/ecs/scene/assets.py for the registry of available gens.
    self.asset = _AssetNamespace(self)
    # self.SG boilerplate: lazily created on first access (so it never clashes
    # with a scene that declares its own scenegraph()). See the SG property.
    self._sg_handle    = None
    self._default_sg   = default_sg
    self._default_sg_kw = dict(sg_params)
    if skybox_path is not None:
      self._default_sg_kw["skybox_path"] = skybox_path

  @property
  def SG(self):
    """The scene's SceneGraphHandle. If the scene didn't declare one and
    default_sg is on, a standard ForwardPBR scenegraph (DEFAULT_SKYBOX, unit
    intensities, low ambient) is created on first access — so material scenes can
    skip the boilerplate and just use `self.SG.component(...)`."""
    if self._sg_handle is None:
      if not self._default_sg:
        raise RuntimeError(
          "self.SG accessed but the scene declared no scenegraph and "
          "default_sg=False — call self.scenegraph(...) or pass default_sg=True")
      kw = dict(preset="ForwardPBR",
                skybox_path=self.DEFAULT_SKYBOX,
                SkyboxIntensity=1.0, DiffuseIntensity=1.0, SpecularIntensity=1.0,
                AmbientLight=vec3(0.06))
      kw.update(self._default_sg_kw)
      self._sg_handle = self.scenegraph(**kw)
    return self._sg_handle

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
    and the caller omits `layername`, default it to "std_transparent"
    when the SG declared it (blended particles draw AFTER std_forward, so
    they composite over opaque geometry — fire occludes the projectile),
    else the primary forward layer. The C++ fallback when layername is
    empty is the SceneGraphSystem's "sg_default" layer, which the PBR
    compositor doesn't render — defaulting here matches the
    SG.component(nodes={...}) ergonomics."""
    if typename == "ParticlesComponent" and "layername" not in kwargs:
      sg_decl = self._systems.get("SceneGraphSystem")
      declared = []
      if sg_decl is not None:
        for method, args, _ in sg_decl.sub_calls:
          if method == "declareLayer" and args:
            declared.append(args[0])
      if "std_transparent" in declared:
        kwargs["layername"] = "std_transparent"
      elif declared:
        kwargs["layername"] = declared[0]
      # auto-attach to every declared aux channel layer (item D) — the
      # material opts in per channel; non-participants skip at draw time.
      if "layername" in kwargs and getattr(self.SG, "_aux_channels", None):
        kwargs["layername"] += "".join(",aux_" + c for c in self.SG._aux_channels)
    return _ComponentDecl(typename=typename, kwargs=kwargs)

  def spinner(self):
    """Auto-rotation behavior: a PythonComponent running the shared _spin.py
    (constant rate about world +Y — the rate lives in _spin.py, one value for
    every example scene). Ensures the host PythonSystem. Returns a fresh
    _ComponentDecl — drop one into each entity that should spin::

        self.entity("ball", transform=...,
                    components=[self.SG.component(nodes={...}), self.spinner()])

    No rate kwarg by design: PythonComponentData carries no per-component data
    slot, so a rate here couldn't be honored. Edit _spin.py to change it (or add
    a reflected float to PythonComponentData first)."""
    import os as _os
    self._ensure_system("PythonSystem")
    script = _os.path.join(_os.path.dirname(_os.path.abspath(__file__)), "_spin.py")
    return self.declare_component("PythonComponent", scriptFile=script)

  ##############################################################################
  # E.2-walk — the REUSABLE walk-on-terrain library (declarative; plays in the
  # pure-C++ player). terrain_collider() declares a static bullet heightfield
  # colliding with a baked HeightField asset (the SAME normalized artifact the
  # chunk renderer draws); walker() declares the upright capsule + the
  # CharacterControllerComponent (input arrives as controller messages from
  # whatever host — WASD move/turn, SPACE jump; the camera publishes from the
  # character). Any scene gets walking with two calls.
  ##############################################################################

  def _ensure_system(self, typename, **kwargs):
    if typename not in self._systems:
      self.system_data(typename, **kwargs)

  def append_system_script(self, script_path):
    """Append an ADDITIONAL composable system-scoped script to the PythonSystem (creating the
    system if absent). Every such script runs alongside the primary `systemUpdateScript` — each
    gets its own namespace and all the onSystem* hooks (InputKey, update, render-tick, ...). Use
    this to LAYER behavior (e.g. a VR head-pose script) onto whatever the scene already declared,
    without clobbering its primary input/logic script."""
    ps = self._systems.get("PythonSystem")
    if ps is None:
      self.system_data("PythonSystem", systemScripts=[script_path])
      return
    scripts = list(ps.kwargs.get("systemScripts", []))
    if script_path not in scripts:
      scripts.append(script_path)
    ps.kwargs["systemScripts"] = scripts

  def _set_primary_system_script(self, script_path):
    """Set the PythonSystem's PRIMARY script (systemUpdateScript), creating the system if absent;
    keep an already-declared primary. Distinct from the composable EXTRA scripts
    (append_system_script / systemScripts) and ORDER-INDEPENDENT with respect to them — so a
    walker() primary and a VR-appended head-pose script compose regardless of declaration order."""
    ps = self._systems.get("PythonSystem")
    if ps is None:
      self.system_data("PythonSystem", systemUpdateScript=script_path)
    else:
      ps.kwargs.setdefault("systemUpdateScript", script_path)


  def spawner(self, name, arch, *, autospawn=True, transform=None,
              publish_xf="", lifetime=None):
    """Declare a spawner. Multiple spawners can share one archetype (the
    pattern from physics/FPS.py).

    publish_xf — when non-empty, the spawned entity's live transform is
    registered in the Simulation's published-xf registry under
    "<publish_xf><N>" (counter is monotonic per base name across
    spawn/despawn cycles). Consumers that want to track this entity
    (e.g. VdbCollider.follow_entity="saddle0") look it up by the full
    key. Empty (default) → no publication.

    lifetime — seconds (or (min, max) tuple) after which each spawned
    entity auto-despawns; applies to BOTH scheduled and script-dynamic
    spawns (reflected SpawnData LifetimeMin/Max). None (default) = lives
    forever. The recycle mechanism for projectile pools."""
    if name in self._spawners:
      raise ValueError(f"spawner {name!r} already declared in this scene")
    if not isinstance(arch, _ArchetypeDecl):
      raise TypeError(f"self.spawner expected an _ArchetypeDecl; got "
                      f"{type(arch).__name__}")
    transform = _coerce_transform(transform)
    decl = _SpawnerDecl(name=name, arch=arch,
                        autospawn=autospawn, transform=transform,
                        publish_xf=publish_xf, lifetime=lifetime)
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
    self._sg_handle = handle   # so self.SG reflects an explicitly-declared sg too
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

    # The sky's implied deck set, if the scene never declared its own. Runs
    # BEFORE the asset/system walk below so its assets, entities and launch
    # state lower like any other Pass-1 declaration.
    self.declare_staged_cloud_decks()

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
  "axis_angle",
  "SceneGraphHandle",
  "hsv",
  "wavelength",
  "colortemp",
  "colors",
]


# DSL vocabulary surfaced into every Scene subclass's method globals via
# Scene.__init_subclass__. Populated here (post-class-def) so _ASSET_REGISTRY
# reflects all wrappers that have been @_register'd by the assets.py import
# (every PbrMaterial / SphereSdf / VdbGridToDrawable / etc. shows up
# bare-name to author code with zero imports).
Scene._AUTHOR_GLOBALS = {
  # Color constructors + palette
  "hsv":        hsv,
  "wavelength": wavelength,
  "colortemp":  colortemp,
  "mix":        mix,
  "colors":     colors,
  # Math / geometry primitives
  "vec3":       vec3,
  "vec4":       vec4,
  "Transform":  Transform,
  "axis_angle": axis_angle,
  # All registered asset wrappers (PbrMaterial, SphereSdf, etc.)
  **_ASSET_REGISTRY,
}
