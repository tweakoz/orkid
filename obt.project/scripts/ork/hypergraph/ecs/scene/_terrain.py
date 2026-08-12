###############################################################################
# Scene terrain authoring (terrain + colliders)
#
# Split out of scene/__init__.py for readability. Mixed into Scene via multiple
# inheritance (class Scene(TerrainMixin, WalkerMixin, ProjectilesMixin) in __init__.py), so the public API is
# unchanged: self.terrain(...) still works.  Methods reference self.* (entity /
# declare_component / SG / asset / ...) provided by the core Scene + sibling mixins.
###############################################################################

from orkengine.core import vec3
from ork.hypergraph.ecs.scene._helpers import Transform

# bake-key scheme salt — bump to orphan every stored-atlas cache dir (mirrors the
# hm.section / vkfxshader cook salts).
TEXBAKE_KEY_SCHEME = "tbake.v1"


def texbake_material_source(mat_cls, fallback_src=None):
  """The material's DECLARED SOURCE text, for content-keying its bake.

  A terrain DSL's MATERIAL_CLASS is usually declared INSIDE the DSL file, which load_dsl_class
  execs under the synthetic module name `terrain_dsl_module` with no __file__ — inspect then
  raises "is a built-in class" for it. Keying off the class NAME in that case would silently make
  the cache blind to every material edit (stale atlas, no rebake), so an unreadable class demands
  the caller's `fallback_src` (the DSL file text — where the class actually lives) or it raises.

  Importable classes hash their WHOLE MODULE text, not inspect.getsource(class): the classifier
  leans on module-level helpers (strata_phase / _tan_deg / the strata constants) whose edits
  change baked pixels just as surely as a class-body edit."""
  import inspect, sys
  mod  = sys.modules.get(getattr(mat_cls, "__module__", ""), None)
  path = getattr(mod, "__file__", None)
  if path:
    try:
      with open(path, "r") as f:
        return f.read()
    except Exception:
      pass
  try:
    return inspect.getsource(mat_cls)
  except Exception:
    pass
  if fallback_src is not None:
    return fallback_src
  raise RuntimeError("texbake cache key: no source for material class <%s.%s> and no fallback — "
                     "the bake key would go blind to material edits"
                     % (getattr(mat_cls, "__module__", "?"), mat_cls.__name__))


def texbake_material_digest(mat_cls, mat_params, cap_targets, fallback_src=None):
  """MATERIAL half of the stored-atlas cache dir: hashed from the material's DECLARED SOURCE,
  never from the compiled .fxv2's filename digest.

  That compiled digest hashes the WHOLE generated file, so a RENDER-ONLY technique variant (the
  mesh-shader block ORKID_TERRAIN_MESHSHADER appends) renames the cache dir even though the bake
  executes one byte-identical technique (FWD_SSBO_CUSTOM_CAPTURE, forced by the drawable) and the
  baked pixels cannot differ. Key off what the pixels DO depend on — material source + params +
  capture targets + codegen version — mirroring the terrain half (hashed from DSL SOURCE) and the
  hm.section precedent that deliberately keeps ORKID_SECTION_MIPS out of its cook key.

  The source half walks the material's WHOLE MRO (up to the Ptex3d base): a fork like
  XXX3GrassMat now INHERITS its classifier from XXX3Mat (shared method, not a body copy),
  so keying the leaf class alone would go blind to shared-classifier edits in the base's
  file — a stale atlas would warm-bind forever. Base classes with importable source hash
  their own file text; the exec'd DSL leaf falls back to the DSL file text as before."""
  import hashlib
  from ork.hypergraph.ptex3d import CODEGEN_VERSION, Ptex3d
  srcs = []
  for c in mat_cls.__mro__:
    if c in (Ptex3d, object) or issubclass(Ptex3d, c):
      break                       # engine base — covered by CODEGEN_VERSION discipline
    srcs.append(texbake_material_source(c, fallback_src))
  key = "\x00".join([TEXBAKE_KEY_SCHEME, CODEGEN_VERSION,
                     "\x00".join(srcs),
                     repr(sorted((mat_params or {}).items())), repr(list(cap_targets))])
  return "material_" + hashlib.sha1(key.encode("utf-8")).hexdigest()[:16]


class TerrainMixin:
  """Scene terrain authoring (terrain + colliders)"""

  def terrain(self, name, *, render_dimension=512, bake_dimension=None, chunk=128, dsl_file=None,
              dsl_class=None, dsl_kwargs=None, walkable=False, visible_y_bias=0.0,
              capture=False, mode="proc", bake_res=2048,
              spawn=None):
    """Self-describing chunked-terrain entity. Loads the terrain DSL class and reads its
    OWN physical scale + look — EXTENT_M / MATERIAL_CLASS / MATERIAL_PARAMS (heights are
    TRUE METERS on the graph — no vertical scale constant), and
    the material's SAMPLER_CHANNELS — exactly the attrs ork.terrain.viewer2.py reads. Wires
    the full ECS contract from them: the HeightField bake (with channel→sampler bindings
    derived from SAMPLER_CHANNELS), the Ptex3d material (the terrain's OWN MATERIAL_CLASS,
    driven through the chunked SSBO-pull TerrainChunkVertexSource), and the
    TerrainChunkDrawableData entity. Nothing about scale / material / samplers is restated —
    change the terrain DSL and the scene follows. Terrains with no MATERIAL_CLASS fall back to
    the generic Solid (so plain heightfields work too).

    `render_dimension` is the RENDER-mesh + physics grid resolution; `bake_dimension` (>= it,
    default = render_dimension) is the resolution the terrain is COMPUTED + the material BAKED at —
    the render mesh downsamples from it, so fine detail lives in the baked normal atlas on a cheap
    mesh; `bake_res` is that atlas's texel count. The DSL is resolution-independent via EXTENT_M (so
    none are class attrs); `chunk` is the render chunk size. `dsl_class` disambiguates a
    multi-HeightField file; `dsl_kwargs` parameterizes the terrain DSL ctor (e.g. iters=). Returns
    the entity handle.

    `walkable` puts the scene in TERRAIN-PHYSICS mode: it wires a terrain_collider on this
    baked heightfield plus a walker() character (first-person, spawned at the terrain top so
    it drops onto the surface; camera far derived from the terrain EXTENT_M so you can see
    across it). Defaults are tuned for MOUNTAINS — a strong horizontal move_force so you can
    climb steep slopes (the climb limit is physics: a horizontal force F drives a body of mass
    m up to a slope of atan(F / (m*g)); the walk force has no vertical component, the slope
    normal redirects it). Pass `walkable=True`, or a dict of walker() overrides to tune the
    feel (e.g. `walkable=dict(move_force=16000, max_speed=30, gravity=vec3(0,-9.8,0))` —
    more force / lower gravity both climb steeper).

    `visible_y_bias` shifts the RENDERED terrain in world-Y relative to the (unchanged) physics
    collider — the render and Bullet heightfield don't sample identically, so they need a small
    constant nudge to line up under the walker's feet. Default -0.5 (visible sits 0.5 m BELOW
    physics). Baked into the chunk vertex shader (a constant, so normals are unaffected); dial it
    per-scene if your terrain needs a different offset."""
    from ork.hypergraph.dflow.terrain.resolve import resolve_dsl_file, load_dsl_class
    from ork.hypergraph.dflow.terrain.gpu_chunk import TerrainChunkVertexSource
    from ork.hypergraph.assets.materials.terrain.solid import Solid
    from orkengine.lev2 import TerrainChunkDrawableData
    if not dsl_file:
      raise ValueError("self.terrain() requires dsl_file (terrain DSL .py name or path)")

    cls        = load_dsl_class(resolve_dsl_file(dsl_file), dsl_class)
    extent_m   = float(getattr(cls, "EXTENT_M", 4096.0))
    # bake_dimension (>= render_dimension): the terrain is COMPUTED + the material BAKED at this hi-res;
    # the render mesh (and physics) use `render_dimension`, downsampled from it. Defaults to render_dimension.
    bake_dim   = int(bake_dimension) if bake_dimension else int(render_dimension)
    if bake_dim < int(render_dimension):
      raise ValueError("terrain(%r): bake_dimension(%d) must be >= render_dimension(%d)"
                       % (name, bake_dim, int(render_dimension)))
    # RELAX — equal-area UV relaxation (project_terrain_relaxuv). A terrain DSL opts in by calling
    # self.relax_uv(h), which captures the "relaxed_uv" + "binormal" channels. Derive the flag from the
    # DSL's OWN captures (single source of truth) so the vertex-source shader (stride-8 interleave), the
    # HeightField bake, and the C++ consume all key off the same declaration — no separate flag to keep
    # in sync (would mismatch -> garbage). The probe is a trace-only instantiation (no GPU); its
    # generatedflow() closes the trace so it can't leak into the real bake.
    relax = False
    try:
      _probe = cls(**(dsl_kwargs or {}))
      relax  = "relaxed_uv" in _probe.channels
      _probe.generatedflow()
    except Exception as _e:
      import sys as _sys
      print("terrain(%r): relax probe failed (%s) — assuming non-relaxed UVs" % (name, _e), file=_sys.stderr)
    mat_cls    = getattr(cls, "MATERIAL_CLASS", None) or Solid
    mat_params = dict(getattr(cls, "MATERIAL_PARAMS", {}) or {})
    # SAMPLER_CHANNELS is {sampler_uniform: baked_channel}; the HeightField wants the inverse
    # {channel: sampler}. The C++ materializeAll post-pass binds each baked EXR onto the
    # resolved material's sampler — no hand-synced path strings on the material side.
    sampler_channels = dict(getattr(mat_cls, "SAMPLER_CHANNELS", {}) or {})
    channel_samplers = {ch: samp for samp, ch in sampler_channels.items()}

    mat_name = name + "_mat"

    # ONE material: its class declares surface() (proc) + self.capture(...) + surface_stored() (the
    # explicit-capture reconstruction, report §5). mode='stored' compiles surface_stored as the forward
    # + the named/packed-capture technique; the drawable bakes that capture each run and binds the N
    # target textures back onto THIS material, so surface_stored samples them (same-session bake-then-
    # bind, the impostor pattern). mode='proc' renders surface() live. The HeightField binds the terrain
    # channel_samplers to this material (the capture exprs read them).
    p = self.asset.Ptex3d(
        mat_name,
        dsl_class     = mat_cls,
        vertex_source = TerrainChunkVertexSource(
            dim=render_dimension, extent_m=extent_m, chunk=chunk, y_bias=visible_y_bias,
            bake_dim=bake_dim, relax=relax),
        mode          = mode,
        capture       = capture,     # mode='proc' + capture=True => impostor-style debug env-dump only
        **mat_params)
    hf = self.asset.HeightField(
        name, dsl_file=dsl_file, dsl_class=(dsl_class or None), dimension=bake_dim,
        extent_m=extent_m, material=mat_name,
        channel_samplers=channel_samplers, **(dsl_kwargs or {}))
    cap_targets = list(getattr(p, "capture_targets", []) or [])
    if mode == "stored" and not cap_targets:
      raise ValueError(
          f"terrain({name!r}, mode='stored') but {mat_cls.__name__} declares no self.capture(...) + "
          f"surface_stored() — author them (report §5.1) or use mode='proc'.")

    cap_mode = "proc"
    cap_dir  = ""
    if mode == "stored":
      # DISK CACHE — deterministic dir derivable from the baked objects: a MATERIAL-SOURCE digest
      # (texbake_material_digest — declared source + params + capture targets + CODEGEN_VERSION, NOT the
      # compiled .fxv2 name) AND a terrain-content hash. The baked atlas samples ctx.P/N (the HEIGHTS) and the
      # terrain channels (FlowMetrics/FlowDischarge/...) — all deterministic outputs of the terrain DSL + its
      # scale — so editing the heightmap MUST re-bake (resample at the new heights). The dflow graph won't
      # deep-serialize (class-touch gap) and exposes no Python module-walk, so the terrain identity is hashed
      # from its DSL SOURCE + ctor kwargs + the bake geometry (dim/extent/height/y_bias/chunk). WARM (all PNGs
      # present) -> bind via sampler_textures, NO bake. COLD -> the drawable bakes, writes the cache, binds
      # same-session (correct first run). Edit the Material -> new material digest; edit the terrain (ErodeFlow
      # / iters=) -> new terrain hash; either -> new dir -> one-run cold re-bake.
      import os as _os, hashlib as _hashlib
      from orkengine.core import Path as _Path
      try:
        with open(resolve_dsl_file(dsl_file), "r") as _f:
          _terr_src = _f.read()
      except Exception:
        _terr_src = str(dsl_file)
      # A DSL-embedded MATERIAL_CLASS has no importable source (load_dsl_class execs the file as
      # `terrain_dsl_module`), so the DSL text — which IS where that class is written — is the fallback.
      _digest = texbake_material_digest(mat_cls, mat_params, cap_targets, fallback_src=_terr_src)
      # RELAX-CONSUME VERSION: the relaxed UV/atlas is produced by the C++ relax MODULE (not the DSL
      # source), so a module-algorithm change (folded native-res -> coarse-grid) changes the baked atlas
      # WITHOUT changing _terr_src -> the cap_dir would collide and the player would bind the STALE atlas.
      # Bump this token whenever the relax module's output changes (mirror of the C++ cook salt). Only
      # added when relaxed, so non-relaxed terrains keep their existing cache keys.
      _relax_tok = "relaxuv.v5-cap512" if relax else "norelax"
      _terr_key  = "\x00".join([_terr_src, repr(sorted((dsl_kwargs or {}).items())),
                                str(bake_dim), str(extent_m), "meters",
                                str(visible_y_bias), str(chunk), _relax_tok])
      _terr_hash = _hashlib.sha1(_terr_key.encode("utf-8")).hexdigest()[:12]
      cap_dir = _Path.expandPathString(
          f"<assetcache>/ptex3d_capture/{_digest}__{name}__t{_terr_hash}__r{bake_res}")
      _atlas  = {t: f"{cap_dir}/{t}.png" for t in cap_targets}
      if all(_os.path.isfile(pp) for pp in _atlas.values()):
        _st = dict(getattr(p.gendata, "sampler_textures", {}) or {})
        _st.update(_atlas)
        p.gendata.sampler_textures = _st     # WARM: bound at materialize (no runtime bake)
        cap_mode = "proc"
        print("TERRAIN-TEXBAKE: WARM bind %s" % cap_dir, flush=True)
      else:
        cap_mode = "stored"                  # COLD: bake -> write cache -> bind (C++ one-shot)
        print("TERRAIN-TEXBAKE: COLD %s" % cap_dir, flush=True)

    ent = self.entity(
        name + "0",
        components=[self.SG.component(nodes={
            name: {"drawable": TerrainChunkDrawableData(
                hf_asset         = name,
                material_asset   = mat_name,
                chunk            = chunk,
                render_dimension = render_dimension,  # render mesh + physics res (manifest dim = bake_dimension)
                capture_mode     = cap_mode,
                capture_res      = bake_res,
                capture_targets  = cap_targets,
                capture_dir      = cap_dir)},
        })])

    if walkable:
      # TERRAIN-PHYSICS mode: static heightfield collider + a first-person walker. Spawn well
      # above the terrain (surface height at the origin is unknown at compose time — dropping
      # in is the only generic-safe placement) with a generous near/far so distant relief
      # renders without Z-fighting.
      # No projectile_pool, so the walk script's `/`-shoot self-disables (no ball_spawner).
      self.terrain_collider(hf, 
                            friction=1.0, 
                            restitution=0.0, 
                            render_dimension=render_dimension)
      # MOUNTAIN-CLIMBING defaults: move_force 8000 N over an 80 kg body at g=19.8 climbs
      # slopes up to atan(8000/(80*19.8)) ≈ 78°. rest_friction holds you still on the slope
      # at rest; the static collider carries friction 1.0 so the character's own values read.
      # spawn: callers SHOULD pass one measured against their baked terrain (the
      # noise basis owns the relief; heights are true meters with no scale constant
      # to derive a drop height from). Default drops from a quarter extent — generous
      # for any sanely-proportioned terrain, and the walker settles on contact.
      walker_kw = dict(
          spawn        = spawn if spawn is not None else vec3(0.0, extent_m*0.25, 0.0),
          cam_near     = 0.5,        # owner-set 2026-07-22 for the TRUE 1.7m eye (2.0 clipped walls/ground
                                     # within reach; walker propagates this into VrNear too). NOT 0.1 —
                                     # far depth precision is dominated by near; 0.1 on a big cam_far
                                     # Z-fights the distant terrain into oblivion. WATCHPOINT: 0.5 is 4x
                                     # the old near ratio — if far shimmer appears (vale rims), this is why
          cam_far      = 100000.0,   # generous flat far (clears any terrain) — sizing it to extent_m was
                                     # too tight: the far edge sat at the far plane and clipped
          move_force   = 8000.0,
          # halved from 10.0 (owner, aug07) — a village walk, not a jog; sprint still
          # scales from this base via SetSprint.
          max_speed    = 5.0,
          jump_impulse = 660.0,
          eye_height   = 1.7,    # TRUE human eye height above the ground (the old 4.85-above-
                                 # capsule-center vista default made every terrain read as a
                                 # miniature; scenes wanting an elevated view pass eye_height=)
          cam_distance = 0.0,    # first person
          # -9.8 * 1.25 (owner, aug07): heavier planet — snappier jump arcs, faster
          # kiva/ledge falls. jump_impulse unscaled, so jump HEIGHT drops ~20%.
          gravity      = vec3(0.0, -12.25, 0.0)
          )
      if isinstance(walkable, dict):
        walker_kw.update(walkable)
      self.walker(**walker_kw)

    return ent

  def terrain_collider(self,
                       hf_asset,
                       *,
                       name="terrain_collider",
                       friction=0.9,
                       restitution=0.05,
                       gravity=None,
                       render_dimension=0,
                       surface_weights="",
                       friction_rows=None):
    """Static heightfield collider for a baked HeightField asset. `hf_asset` is the
    asset wrapper (or its name string); physics scale comes from the asset's manifest
    at load. Heights are TRUE METERS: the C++ shape wraps Bullet's centered heightfield
    in a compound whose child offset restores absolute meters, so the entity sits at
    y = 0 and world y == the baked height. Ensures BulletSystem (default gravity -12.25
    if absent). `render_dimension` (0 = full EXR res): when the HeightField bakes at a
    higher bake_dimension than the rendered mesh, pass the render grid here so the
    collider high-quality-downsamples to it (Image::resampledOf) and physics matches
    the visible mesh.

    W·M SURFACE RESPONSE (physics leg): `surface_weights` names the RGBA class-weight
    capture the terrain MATERIAL also consumes (baked to
    <assetcache>/terrain/<asset>/<capture>.exr — the SAME W). "" (default) = feature off
    (byte-identical to the pre-W collider). `friction_rows` = M[:,friction], the per-class
    friction DELTAS (list of up to 4 floats, RGBA-class order) added at each contact via
    base + W·M[:,friction]; the natural residual (1-Σw) carries delta 0. Talus (a low or
    negative row) slides; a plaza/roadbed class (a positive row) grips."""
    from orkengine import ecs as _ecs
    explicit = gravity is not None
    if gravity is None:
      gravity = vec3(0.0, -12.25, 0.0)  # matches the walker default (-9.8 * 1.25, owner aug07)
    self._ensure_system("BulletSystem", linGravity=gravity)
    if explicit:  # an EXPLICIT gravity is authoritative regardless of helper order
      self._systems["BulletSystem"].kwargs["linGravity"] = gravity
    gd     = getattr(hf_asset, "gendata", None)
    aname  = getattr(gd, "asset_name", None) or str(hf_asset)
    shape  = _ecs.BulletShapeTerrainData()
    shape.hf_asset = aname
    shape.render_dimension = int(render_dimension)
    if surface_weights:
      shape.surface_weights = str(surface_weights)
      shape.friction_rows   = [float(r) for r in (friction_rows or [])]
    cdecl  = self.declare_component(
        "BulletObjectComponent",
        shape=shape, mass=0.0, friction=float(friction), restitution=float(restitution))
    return self.entity(
        name,
        transform=Transform(translation=vec3(0.0, 0.0, 0.0)),
        components=[cdecl])

  def scatter_collider(self, hf_asset, *, sink, name=None,
                       friction=0.8, restitution=0.1):
    """ONE static compound collider for a placed ScatterSet: each item contributes a
    primitive proxy whose KIND+DIMS ride the set itself (declared at the scatter() sink
    via colliders={type: ("sphere", r) | ("capsule", r, h) | ("box", x, y, z)} — nothing
    hardcoded here). The entity sits at the ORIGIN (item transforms are absolute world).
    Broadphase cost = one body; bullet's compound AABB tree handles thousands of items."""
    from orkengine import ecs as _ecs
    self._ensure_system("BulletSystem", linGravity=vec3(0.0, -9.8, 0.0))
    gd    = getattr(hf_asset, "gendata", None)
    aname = getattr(gd, "asset_name", None) or str(hf_asset)
    shape = _ecs.BulletShapeScatterData()
    shape.scatter_asset = aname
    shape.sink          = str(sink)
    cdecl = self.declare_component(
        "BulletObjectComponent",
        shape=shape, mass=0.0, friction=float(friction), restitution=float(restitution))
    return self.entity(name or ("%s_%s_collider" % (aname, sink)),
                       transform=Transform(translation=vec3(0.0, 0.0, 0.0)),
                       components=[cdecl])

  def spine_collider(self, spine_asset, *, shoulder_m=3.0, lift_m=0.0, ground_asset="",
                     name=None, friction=0.9, restitution=0.05):
    """The road WALKABLE RIBBON collider (physics-proxy law, owner 2026-07-22): a
    simplified proxy swept from the street_spine BAKED ARTIFACT (route_spine must set
    export_name=<spine_asset>) — per segment a flat deck band + two shoulder crossfall
    bands; NEVER the render mesh (embellishments are render-only). lift_m MUST equal
    the render deck lift (single-source it from one scene constant). The entity sits
    at the ORIGIN (spine coords are absolute world). Fail-loud if the artifact is
    missing (declaration order = dependency order, the scatter convention)."""
    from orkengine import ecs as _ecs
    self._ensure_system("BulletSystem", linGravity=vec3(0.0, -9.8, 0.0))
    shape = _ecs.BulletShapeSpineData()
    shape.spine_asset  = str(spine_asset)
    shape.shoulder_m   = float(shoulder_m)
    shape.lift_m       = float(lift_m)
    shape.ground_asset = str(ground_asset)  # pin outer chords to this HeightField's baked EXR
    cdecl = self.declare_component(
        "BulletObjectComponent",
        shape=shape, mass=0.0, friction=float(friction), restitution=float(restitution))
    return self.entity(name or ("%s_spine_collider" % spine_asset),
                       transform=Transform(translation=vec3(0.0, 0.0, 0.0)),
                       components=[cdecl])
