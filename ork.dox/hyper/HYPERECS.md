# HYPERECS — orkid declarative authoring tiers (standalone graph → ECS scene composite)

> **STATUS (2026-07-04):** the three-tier authoring model described here is **implemented and in daily use**.
> This doc is now the DESIGN + IMPLEMENTATION-STATUS reference; the remaining *open* (unimplemented) work lives
> in the lean `HYPERECS_PLAN_JUN10.md` (same dir — its Appendix A holds the locked contracts, A6 indexes this
> doc's standing commitments). Forward-work implementation specs (2026-07-04, owner-sequenced by holistic
> outcome): `~/JUL04_SDFM3M4.md` (SDF NanoVDB + cross-family reach) and `~/JUL04_GRAMMARS.md` (LRuleSet
> grammars, buildings, creatures).
>
> **What is landed:**
> - **Tier 1/2** — standalone + parameterized single-graph HyperSyn authoring (the permanent path; never removed).
> - **Tier 3 — the ECS Scene composite** — the `ork.hypergraph.ecs.scene` Python surface (a `Scene` with
>   system-handle namespaces + entity/spawner sugar + the dual-use asset DSL; e.g. `self.terrain(...)`,
>   `self.walker(...)`, `self.projectile_pool(...)`, `self.scenegraph(...)`) → reflected `.ecs` → the pure-C++
>   `ork.ecs.player.exe` host (zero-Python playback; the D.5 host demo, owner-verified). This is the live
>   playback path (`ork.scene.viewer.py` = tojson → player).
> - **The C++ materializers (§3.4) are REAL** — `AssetSystemData::materializeAll` (`ork.ecs/.../AssetSystem.cpp`)
>   + the `PbrMaterial`/`HeightField`/`Hypermesh` gendata `materialize()` (`ork.lev2/.../asset_gen.cpp`).
>   No longer the Python-only no-op an earlier draft warned about.
>
> **Reality-vs-spec deltas to keep in mind while reading:** (1) stage-then-swap completes in **2–3 frames**,
> not the single frame §3.6 states (barrier-protected, no tearing); (2) a few described sub-features remain
> partial — see the plan's "Open work." The retired `HYPERECS_JUN10.md` scorecard is gone; status now lives
> here + the plan's Status section. (Design-decision record: the chaining-vs-`with` syntax experiment validated
> this doc's kwargs-first choice — chaining as primary sugar, `with` only for 4+ children without cross-refs,
> implicit thread-local context rejected as invisible coupling.)

Grounded against the canonical HyperSyn contract at `.claude/skills/hypersyn/SKILL.md` + `PLAN.md`. The contract is referenced throughout; nothing in this doc supersedes SKILL.md without explicit callout.

---

## 0. Where this document fits

| Doc | Scope | Status |
|---|---|---|
| `SKILL.md` (orkid `.claude/skills/hypersyn`) | The HyperSyn contract — families, Ops, Phases, registry, validator subprocess, materialize semantics. **Canonical.** | Active |
| `PLAN.md` (alongside SKILL.md) | HyperSyn milestone/status breakdown | Active |
| `HYPERECS_PLAN_JUN10.md` (this dir) | HYPERECS **open work** + the locked contracts (Appendix A) | Active |
| **`HYPERECS.md`** (this doc) | The three-tier authoring model — standalone HyperSyn graph → ECS scene composite; declarative-then-reflected workflow; editor round-trip | Design + current status (Tier 1–3 landed) |

This document covers what `SKILL.md` originally deferred as "ECS instantiation (future)" and "Hypergraph composition" — now **built**: users author entire ECS scenes (particles + physics + sound + scenegraph) in one declarative Python file (`ork.hypergraph.ecs.scene`). Crucially, this surface was **added** without removing the existing standalone HyperSyn authoring path — both coexist permanently.

---

## 1. The three-tier authoring model

**Scene composite is opt-in composition, not a replacement.** Three independent tiers; none requires the next.

| Tier | Surface | Host | Uses ECS? | Round-trips through JSON? |
|---|---|---|---|---|
| **1. Pure ParticleSystem** | `class Foo(ParticleSystem)` | `ork.particle.viewer.py` (unchanged) | No | Just the graphdata (already serializable per SKILL.md) |
| **2. Parameterized ParticleSystem** | `class Foo(ParticleSystem): def __init__(self, *, sdf=None, ...): ...` | `ork.particle.viewer.py` (with optional kwargs CLI) | No | Same as Tier 1 |
| **3. Scene composite** | `class FooScene(Scene)` | `ork.scene.viewer.py` (new) or any ECS app | Yes | Full ECS scenedata |

### 1.1 Tier 1 — standalone single-graph (unchanged)

```python
class CurlBolt(ParticleSystem):
  def __init__(self):
    super().__init__()
    self.pool   = P.PoolData(size=10000)
    self.emit   = P.RingEmitter(self.pool, ...)
    self.curl   = P.CurlNoise(self.emit, ...)
    self.render(P.StreakRenderer(self.curl, ...))
```

Loaded by `ork.particle.viewer.py`. No Scene, no ECS, no asset reflection plumbing. This path is **permanent** — small particle demos, unit tests, throwaway visual sketches all live here. It's also the canonical authoring shape for the HyperSyn registry per SKILL.md.

### 1.2 Tier 2 — parameterized single-graph (one ergonomic improvement)

```python
class ColVdbSystem(ParticleSystem):
  def __init__(self, *, funnel_sdf=None, base_radius=1.0):
    super().__init__()
    if funnel_sdf is None:
      # eager build of any internal assets
      mesh = assets.HollowFunnelMesh(top_outer=8.0, ...).build()
      funnel_sdf = assets.MeshToSdf(input_mesh=mesh, voxel_size=0.08).build()
    self.pool = P.PoolData(size=5000)
    self.emit = P.RingEmitter(self.pool, ...)
    self.coll = P.VdbCollider(self.emit, sdf_grid=funnel_sdf, ...)
    self.render(...)
```

Same host as Tier 1; particle viewer gains a small CLI surface (`--param key=value`) to pass kwargs. Same graphdata artifact; same lev2 attachment path. The single API change vs. Tier 1 is `ParticleSystem.__init__(**kwargs)` pass-through (~3 lines).

### 1.3 Tier 3 — Scene composite (opt-in, ECS-backed)

```python
class ColVdbScene(Scene):
  def __init__(self):
    super().__init__()
    SG       = self.scenegraph(preset="ForwardPBR", layers=["std_forward"])
    particles = self.particles_global()

    mesh = self.asset.HollowFunnelMesh("funnel", top_outer=8.0, ...)
    sdf  = self.asset.MeshToSdf("funnel_sdf", input_mesh=mesh, voxel_size=0.08)
    mat  = self.material("chrome", color=vec4(1), metallic=1.0, roughness=0.05)

    self.funnel_ent = self.entity("funnel_viz",
      components=[SG.component(nodes={
        "mesh": {"layer": "std_forward",
                 "drawable": SG.drawables.vdb_mesh(grid=sdf, material=mat)},
      })])

    self.ptc_ent = self.entity("ptc",
      transform=Transform(translation=vec3(0, 5, 0)),
      components=[particles.component(system=ColVdbSystem(funnel_sdf=sdf))])

    # cross-entity continuous wire — uniform Expr syntax
    self.ptc_ent.transform.y = 5.0 + Expr.sin(Expr.sim.time) * 2.0
```

Hosted by `ork.scene.viewer.py` (or any ECS app via `EcsRuntime`). Lowers to a full `ecs.SceneData`. Round-trips through the standard reflection serializer.

### 1.4 Same artifact contract across tiers

A `ParticleSystem` subclass produces a `dflow::graphdata_ptr_t`. That artifact is host-agnostic:

- **Tier 1/2 host** wraps it in `ParticlesDrawableData`, attaches to a lev2 layer directly — no ECS.
- **Tier 3 host** wraps the same drawable data in a `ParticlesComponent` inside an entity — ECS.

The `ParticleSystem` class itself doesn't know or care which host it's running in. **Authors at Tier 1/2 never see any ECS, Scene, or asset-reflection machinery.**

### 1.5 Goals of the Scene composite (Tier 3)

A single declarative Python format that describes an **entire ECS scene** — particles, physics, scenegraph nodes, sound emitters, lights, animation, behavior FSMs, procedural assets, cross-entity wiring — in a form that is:

- **Readable + writable by humans and programmatic tools** (the HyperSyn ergonomics extend to scene level)
- **Convertible to a pure reflection-based object graph** the moment `Scene.build(sd)` runs
- **Round-trippable through the standard ECS JSON serializer** so a designer can load any authored scene into the editor and tweak parameters interactively
- **Owned by C++ ECS once handed off** — the Python `Scene` class is a transient builder, not a runtime entity
- **Capable of expressing any ECS scenedata** — not just the convenience patterns, but the full surface (see §4)

The HyperSyn families enumerated in `SKILL.md §"families"` (lines 40–55) become **citizens of the composite**; none is privileged.

---

## 2. The contract (SKILL.md) — load-bearing references

From SKILL.md, these five obligations are non-negotiable and shape every Scene-composite design decision below. (Tiers 1 and 2 inherit these via HyperSyn directly.)

### 2.1 Trace-then-materialize is the universal authoring pattern (lines 17–34)

Every HyperSyn family follows: Python expression tree built in `__init__` → `generatedflow()` emits `dflow::graphdata_ptr_t` → validator subprocess → materializer produces a typed runtime artifact. Scene composite **inherits** this — `Scene.__init__` builds the Python IR; `Scene.build(sd)` is the moment-of-materialize.

### 2.2 Subprocess isolation for validation/materialization (lines 506–551)

zmq REQ/REP worker pool, pre-warmed. Editor never runs user code in-process. `dry_run()` exists for in-process callers that accept fail-fast semantics. Scene composite must respect this: scene-level lowering can run in-process (it's just declarative restructuring), but materializing the underlying HyperSyn graphs goes through the worker pool.

### 2.3 Reflection-serializable outputs (lines 273–286, 575, ConnectionsProperty)

Every `DgModuleData` must be a fully reflected `ork::Object`. Already enforced for `GraphData`. The Scene composite **extends this**: the Scene's own outputs (continuous-wiring tables, scene-level parameter system, asset generator tree) must all be reflected, because the Scene is supposed to round-trip through the standard ECS JSON serializer.

**Corollary:** the Python `Scene` class itself does NOT serialize. Its `build(sd)` output is the canonical artifact.

### 2.4 Op registry introspection (lines 228–261)

`registry.list_families()`, `list_ops(family)`, `get_op(family, name)`, `OpInfo`, `PlugSpec`, `to_json_schema()`. Source provenance `(file, lineno)` auto-captured. Scene composite must expose its own analog: `scene.registry.list_*` over its peer-type vocabulary (entities, systems, components, wires, asset generators).

### 2.5 Deterministic materialization (lines 603–607)

Given identical graph + seeds, materializers produce bit-identical output. No `time()`, no dict-iteration leakage, every RNG seeded. Critical for: cache validity, editor diff, and coordinated multi-asset coherence (shared seeds for scene variants).

### 2.6 Phase model (lines 591–607)

`INSTALL` / `STARTUP` / `SCENE_LOAD` / `PRELOAD` / `LIVE`. Default `LIVE`. The Scene composite respects this per-module: a Scene's asset generator might emit `SCENE_LOAD`-phase generators (funnel SDF baked once at scene load) alongside `LIVE`-phase particle graphs.

---

## 3. Scene composite — design summary (Tier 3)

### 3.1 Surface (system-handle namespaces + entity sugar)

The Scene composite exposes a Python wrapper layer over the reflected ECS primitives. Each system, once declared, returns a handle whose attributes are the things that system contributes (components, sub-object factories, configuration). Entity sugar composes components from handles. See §4 for the full surface specification.

```python
class FooScene(Scene):
  def __init__(self):
    super().__init__()

    # ---- system declarations — each returns a typed handle ----
    SG       = self.scenegraph(external=True)             # adopt host's scenegraph (see §4.6)
    particles = self.particles_global()                    # marker system for Tier 3 particle hosting

    # ---- procedural assets (reflected; round-trip through editor) ----
    mesh = self.asset.HollowFunnelMesh("funnel", top_outer=8.0, ...)
    sdf  = self.asset.MeshToSdf("funnel_sdf", input_mesh=mesh, voxel_size=0.08)
    mat  = self.material("chrome", color=vec4(1), metallic=1.0, roughness=0.05)

    # ---- entity sugar (implicit archetype + spawner; components from handles) ----
    self.funnel_ent = self.entity("funnel",
      components=[SG.component(nodes={
        "mesh": {"layer": "std_forward",
                 "drawable": SG.drawables.vdb_mesh(grid=sdf, material=mat)},
      })])

    self.ptc_ent = self.entity("ptc",
      transform=Transform(translation=vec3(0, 5, 0)),
      components=[particles.component(system=ColVdbSystem(funnel_sdf=sdf),
                                      pool_size=1, duration=0.0)])

    # ---- scene-level params (reflected; shared across child graphs) ----
    self.expose("Wind", default=1.0)

    # ---- continuous wiring — uniform Expr syntax ----
    self.ptc_ent.transform.y = 5.0 + Expr.sin(Expr.sim.time) * 2.0
    self.ptc_ent.particles.gravity.Center = self.funnel_ent.transform.translation

    # ---- event wiring — declarative, traced once at build ----
    self.on(self.ptc_ent.collider.hit, self.funnel_ent.sound.trigger,
            payload=lambda e: {"velocity": e.impact_speed})
```

### 3.2 Two-pass lowering (load-bearing from day one)

**Pass 1 — collect & stable-id.** Walk `self._entities`, `self._systems`, `self._shared`, `self._wires`, `self._assets` in source order. Assign deterministic names. Resolve cross-refs into `(kind, name)` tuples so Pass 2 never needs forward lookups.

**Pass 2 — lower.** Walk in topo order. Materialize asset generators into reflected gen-data. Declare systems, archetypes, components, spawners on `sd`. Instantiate hosted HyperSyn graph builders (e.g. `ColVdbSystem`) in a fresh trace context; `generatedflow()` returns reflected graphdata. Emit reflected wiring entries. Drain `on_lower` callbacks for raw escapes.

**Why two-pass:** cross-entity references (`B.x = A.y + Expr.sin(...)`) must resolve regardless of declaration order. This forces the cleanly-separated collect/resolve pattern, which is also what makes the JSON round-trip lossless.

### 3.3 What lowers to reflected vs what stays Python

| Authored as Python | Lowers to reflected |
|---|---|
| `Scene` subclass instance | **discarded after `build()`** |
| `ExprNode` tree (e.g. `Expr.sin(Expr.time) * 0.5 + 0.5`) | **discarded after lowering** → connections + chain stages + typed modules + ParametersModule entries (all reflected) |
| Asset-generator DSL nodes (`self.asset.HollowFunnelMesh(...)`) | reflected `AssetGenData` subclasses on `AssetSystemData` |
| `self.entity(...)` declaration | `Archetype` + `Spawner` + components |
| `SG.component(nodes={...})` (system-handle component) | `SceneGraphComponent` + N × `declareNodeOnLayer(...)` |
| `particles.component(system=<ParticleSystem>)` | `ParticlesComponent` + `ParticlesDrawableData` (graph already reflected per SKILL.md) |
| `self.material(...)` | `PBRMaterial` |
| `self.expose("Wind", ...)` | `SceneParametersSystemData` entry |
| `self.ent.transform.y = Expr...` (continuous wire) | `SceneContinuousWireData` table entry on `SceneWiringSystem` |
| `self.on(...)` (event wire) | `SceneEventWireData` table entry on `SceneWiringSystem` |
| `payload=lambda evt: {...}` | **traced once at build** into a reflected data-extraction descriptor; lambda discarded |

The Python `Scene` class is a **compiler frontend**. Its output is the canonical artifact. Same separation HyperSyn already enforces between Expr trees and lowered dataflow output (SKILL.md lines 17–34) — just one level up.

### 3.4 Three new reflected systems

Beyond what SKILL.md already mandates, the Scene composite requires three new reflected ECS systems:

1. **`AssetSystem` + `AssetGenData` hierarchy.** Holds the list of asset generators (FunnelMesh, MeshToSdf, MeshToDrawable, NoiseTexture, ...) in topo order. Materializes at SCENE_LOAD phase. Generators reference each other by stable string name. Editor surfaces every generator's reflected fields. *Replaces* ad-hoc Python helpers like `_build_funnel_grid()` in current `col_vdb.py` for Tier 3 usage; eager Python helpers remain for Tier 1/2.

2. **`SceneParametersSystem`** — owns the canonical scene-param state (`name → default` map + live values). Per-graph mirror modules of `_DSL_SceneParameters` (one per consuming graph) lower as needed. Runtime mutation via existing `systemNotify` SET_PARAM channel; broadcast handled by the system.

3. **`SceneWiringSystem`** — continuous + event wiring tables. Continuous entries carry source descriptor + optional pre-lowered chain + target descriptor. Event entries carry source token + reflected payload-extraction descriptor + sink token. Owns the per-frame pump.

Each is a normal `SystemData` subclass — fits the existing pattern from BulletSystem / SceneGraphSystem / ParticlesGlobalSystem.

### 3.5 Time + state namespace

SKILL.md doesn't lock this; the Scene composite is the right place to formalize it:

```
Expr.sim.{time, dt}                  — wall-clock since sim start
Expr.entity.{spawn_t, age, pos, ...} — implicit entity = the one hosting this graph
Expr.slot.{t, dt, fire_count}        — per-slot (current Expr.time meaning)
Expr.ptc.{unit_age, random}          — per-particle (existing)
```

Today's `Expr.time` *already* means per-slot time. Recommendation: rename to `Expr.slot.t`, deprecate-alias `Expr.time`, introduce `Expr.sim.time` as the genuine wall-clock.

No cross-entity time references at the Expr level — that's what `scene.connect(A.x, B.y)` is for. Cross-entity refs go through the wiring system, not the per-graph Expr resolver.

### 3.6 Live mutation strategy — stage-then-swap

A `Scene` subclass produces an `ecs.SceneData` snapshot. The natural lifecycle for changes that affect a running scene is **stage-then-swap**: build a new `SceneData` off the GPU thread, hand it to the GPU thread in a single atomic operation, despawn the prior generation. The Tier 3 design adopts this as its v1 live-mutation contract; finer-grained incremental mutation is an opt-in future capability built on the same primitive.

#### v1 contract — whole-scene atomic regeneration

```
worker / authoring thread:
    new_sd = ecs.SceneData()
    NewSceneClass().build(new_sd)
    session.stage(new_sd)          # parks the new scene in a staging slot
                                   # validates eagerly; never blocks the GPU thread

GPU / sim thread (next frame):
    if session.has_staged():
        controller.stopSimulation()        # tears down the live ECS scene
        controller.bindScene(new_sd)
        controller.createSimulation()
        controller.startSimulation()       # new entities now staged + active
        # one frame: old scene gone, new scene visible
```

The full-rebuild model has three load-bearing properties:

1. **The author re-runs from scratch** — every entity expected to exist must be declared in the new `Scene` subclass. There is no implicit "preserve the cat I made 30 seconds ago." If the new scene doesn't recreate it, it's gone. This matches the trace-then-materialize idiom HyperSyn already enforces per SKILL.md §"trace-then-materialize" — the *scene* is a snapshot, the same way a *graph* is.
2. **Off-thread build** — all asset materialization, mesh extraction, graph lowering happens on the worker thread. The GPU thread sees only the finished `SceneData`. No long pause when a swap lands.
3. **Atomic transition** — the swap completes within a single frame boundary. No tearing (half-old / half-new entities), no partial reference invalidation. Either the prior scene is fully torn down and the new one fully bound, or neither happens.

#### Required engine support (already mostly present)

The orkid Controller already exposes `stopSimulation()` / `bindScene()` / `createSimulation()` / `startSimulation()` (see `controller.cpp` and the existing standalone ECS apps). The atomic-swap path needs:

- **A session/swap-coordinator type** that owns the staging slot and the prev/live/staged scene refs.
- **Frame-boundary scheduling** — the swap must land between frames, not mid-frame. Hooking into the existing onGpuUpdate begin-of-frame point is sufficient.
- **Per-frame check** — the GPU thread polls the staging slot once per frame; cost is one atomic load when nothing is staged.

No new C++ subsystems beyond `SceneData`-level primitives we already have.

#### Failure recovery — auto-revert

Authoring iteration is iterative; the swap path must survive broken inputs cleanly. The session keeps a small ring of `SceneData` history:

```
session.history = [snapshot_t0, snapshot_t1, ..., snapshot_now]
```

If the staged scene fails (build raises, first compute() asserts, runtime asserts within the first N frames), the session:
1. Reverts the controller to the most recent known-good snapshot
2. Surfaces the failure as a structured diagnostic (`Diagnostic` per SKILL.md §"validation result")
3. Marks the new candidate as failed; subsequent identical candidates are rejected without re-running

A small consecutive-failure counter (recommended N=3) triggers a hard pause: stop accepting stages until the author explicitly resumes. Prevents pathological loops where a broken candidate keeps re-failing.

This pattern is independent of the authoring driver — it applies equally to interactive editor tweaks that re-import a scene file, to scripted batch processes that iterate over parameter ranges, and to any other component that submits new scenes to the session.

#### Stage as the single mutation API

All scene changes go through the same staging primitive:

```python
session.stage(scene_data)         # any new SceneData, from any source
session.stage_from_class(MyScene) # convenience: instantiate + build + stage
session.revert()                  # explicit step back through history
session.snapshot() -> SceneData   # capture current bound scene
```

No alternate "fast path" for small changes vs. large ones in v1 — every mutation is a full rebuild + atomic swap. This keeps the contract simple and the semantics predictable: a stage either lands cleanly or doesn't land at all.

#### Future: incremental mutation (deferred)

For workflows where preserving in-flight runtime state across changes is valuable (entity refs held by external callers, particle pools mid-emission, physics objects mid-flight), incremental mutation becomes a meaningful optimization:

- **Stable entity IDs declared in the Scene** — `self.entity("dog", id="dog/0001", ...)` — so the same entity in two consecutive `SceneData` snapshots is identifiable.
- **Diff between prev and new SceneData** — at stage time, produce a `(spawn, despawn, mutate)` triple instead of "burn it all down."
- **Controller `applyDiff(diff)` primitive** — applies the triple without going through full stop/bind/create/start.

Incremental mutation is **opt-in**, gated on the Scene declaring an `incremental=True` policy flag. The default whole-scene swap path remains the simpler, more predictable model. Defer until a concrete use case demands runtime-state preservation; the v1 swap path will cover the great majority of iterative authoring workflows on its own.

#### Where this composes with the rest of HYPERECS

- **Tier 1/2 standalone particle viewer**: the same staging idiom applies one level down — re-instantiate the `ParticleSystem`, build new graph, swap into the existing layer. Cheaper since there's no Controller involved; the same revert-on-failure machinery still applies at the graphdata layer.
- **Editor round-trip (§3.3 and §5.4)**: every editor save lands as a stage. The JSON-out / JSON-in round-trip is just the same swap path with serialization on either side.
- **External-library extensibility (§5)**: any class referenced in a staged `SceneData` that isn't registered in the current Python context fails at deserialize time with the engine's existing "Class not Found" assertion. The host is responsible for importing whatever libraries the scene uses; nothing in the staging path needs to manage module imports.

The stage-then-swap pattern is the single architectural primitive on which all live-mutation workflows are built — interactive tweaking, programmatic regeneration, parameter sweeps, editor save/load, future incremental mutation. Designing it as the v1 contract from the start keeps every higher-level workflow consistent with one mental model.

---

## 4. Authoring surface — system-handle namespaces

The Tier 3 authoring surface is built from one consistent pattern: **each ECS system, once declared, becomes a Python handle whose attributes are the things that system contributes** — components it owns, factory namespaces for its sub-objects, configuration properties. A scene is composed by collecting handles, building entities out of their components, and attaching spawners. The reflected `ecs.SceneData` is the canonical artifact; the Python surface is a wrapper layer that translates terse authoring intent into the engine's existing APIs.

### 4.1 The wrapper layer is where terseness lives

Every reflected C++ type used in the Scene composite has a Python wrapper. The wrappers carry all the ergonomic conveniences:

- **Sub-object factory namespaces** — `bullet.shapes.sphere(radius=1.0)` is a wrapper-side factory that returns a `BulletShapeSphereData`. The factory accepts kwargs and applies them as reflected property assigns, so authors never type the two-step "construct, then mutate" pattern.
- **Dict-shape for user-named collections** — `nodes={"ballnode": {...}, "tip": {...}}` lowers to N `declareNodeOnLayer` calls inside the wrapper. The keys are user-defined strings (node names); the values are typed.
- **System handles as namespaces** — `bullet.component(...)` produces a `BulletObjectComponent` because the receiver knows which component the BulletSystem owns. `SG.drawables.model(...)` produces a `ModelDrawableData` because the receiver knows what drawable types the scenegraph system consumes.
- **Entity sugar** — `self.entity(name, components=[...])` expands to archetype + spawner + N component declarations; the returned handle exposes `.spawner(...)` for additional spawners off the same archetype.

The wrapper layer does not bend the engine's data model — it bends only the **author's API surface**. The reflected `SceneData` is identical to what verbose `ecs.SceneData.declareArchetype/declareComponent/declareSpawner` calls would produce. Editor round-trip, JSON serialization, and reflection-based class lookup are unchanged.

### 4.2 The FullPowerScene example

```python
class FullPowerScene(Scene):
  def __init__(self):
    super().__init__()
    SG       = self.scenegraph(external=True)             # see §4.6 — scenegraph-only flag
    bullet   = self.bullet(lin_grav=vec3(0,-9.8,0), sim_rate=240)

    ball = self.entity("ball",
      components=[
        bullet.component(mass=1.0, friction=0.3, restitution=0.45,
                         shape=bullet.shapes.sphere(radius=1.0)),
        SG.component(nodes={
          "ballnode": {"layer": "std_forward",
                       "drawable": SG.drawables.model("data://tests/pbr_calib.glb")},
        }),
      ])

    ball.spawner("ball_blue", transform=Transform(translation=vec3(0,10,0)))
    ball.spawner("ball_red",  autospawn=False)
```

~14 lines, two systems, one archetype, three spawners (implicit `"ball"` + two explicit), one component each from the bullet and scenegraph systems, one inline shape, one inline drawable. The same scene through the verbose primitive form is ~32 lines and stores 6+ explicit handles in local variables.

### 4.3 System handles and their namespaces

Each system handle is a wrapper around a `SystemData` declaration that exposes the system's own vocabulary:

| Handle attribute | What it does | Example |
|---|---|---|
| `handle.component(...)` | Constructs the system's component, returning a deferred declaration that attaches when included in `entity(components=[...])` | `bullet.component(mass=1.0, shape=...)` |
| `handle.<sub_object_namespace>.<factory>(...)` | Factory for a sub-object the system owns | `bullet.shapes.sphere(radius=1.0)` |
| `handle.<property>` | Direct attribute access to a configuration field | `bullet.sim_rate` |
| `handle.<method>(...)` | A method on the underlying SystemData that needed exposing (rare; most config is kwargs at construction) | `SG.layer("std_forward")` returns the layer handle for further use |

Each handle's surface is **defined by its wrapper**, not invented at the Scene composite level. A new system added by an external library ships its own handle with whatever attribute namespaces make sense for it (e.g. `audiolib.codecs.opus()`, `audiolib.component(...)`). The Scene composite knows nothing about the specific shape; it only knows that `self.bullet(...)` / `self.audiolib(...)` / etc. each return a handle constructed by the relevant wrapper.

### 4.4 Entity sugar and the `.spawner(...)` extension

`self.entity(name, components=[...], transform=Transform(...))` is sugar for "one archetype with one implicit spawner of the same name." The returned handle (`ball` in the example above) exposes `.spawner(name, ...)` so additional spawners off the same archetype are inline:

```python
ball = self.entity("ball", components=[...])             # implicit spawner "ball" autospawned
ball.spawner("ball_blue", transform=Transform(translation=vec3(0,10,0)))
ball.spawner("ball_red",  autospawn=False)
```

For cases where the archetype should have **no** implicit spawner (only dynamic spawners), pass `spawner=False`:

```python
proj = self.entity("projectile", components=[...], spawner=False)
proj.spawner("p_dynamic", autospawn=False)
```

Loop-friendly form for crowds:

```python
npc = self.entity("npc", components=[...], spawner=False)
for i in range(100):
    npc.spawner(f"npc_{i}", transform=Transform(translation=vec3(i*2, 0, 0)))
```

All three forms lower to the same kind of `ecs.SceneData` declarations the verbose primitive surface would produce — multiple `declareSpawner` calls bound to the same `Archetype`.

### 4.5 Sub-object construction — kwargs all the way down

Three rules govern nested data:

**(a) Typed structs use kwargs constructors via wrappers.** The wrapper layer adds kwargs support to every reflected `*Data` POD it ships. Authors write `bullet.shapes.sphere(radius=1.0)` or `Transform(translation=vec3(...), nonUniformScale=vec3(5,8,5))`, not the construct-then-mutate two-step.

**(b) Factory namespaces live under the owning system handle.** `bullet.shapes.{sphere, capsule, mesh, plane, terrain}`, `SG.drawables.{model, instanced_model, rigid_primitive, particles, string, ...}`. The handle scopes the namespace so external libraries don't fight for global names. `audiolib.codecs.opus(...)` is reachable without anything in orkid knowing about it.

**(c) Dict-shape only for user-named collections.** When a component owns an ordered named collection (SceneGraphComponent's nodes, BulletObjectComponent's instance bindings, animation tracks, etc.), the kwarg accepts a dict keyed by the user's chosen names:

```python
SG.component(nodes={
  "main":   {"layer": "std_forward", "drawable": ...},
  "shadow": {"layer": "shadow_pass", "drawable": ...},
})
```

The wrapper lowers each entry to a separate engine-side `declareNodeOnLayer` call. The values inside each entry stay typed (`drawable=` carries a real `DrawableData` object).

**Do NOT use dict-shape for typed structs with fixed fields** — `transform={"translation": vec3(...)}` loses type-safety and silently accepts typos. `Transform(translation=vec3(...))` is the same length and IDE-completable; use it.

### 4.6 `external=True` — SceneGraph only

The SceneGraph system is the one system that exists once per window/renderer and routinely outlives any single Scene's bound lifecycle. When the host (a viewer, editor, or app) already owns a SceneGraph, the Scene adopts it via the `external=True` flag:

```python
SG = self.scenegraph(external=True)
```

**What this means operationally:**

- The Scene's reflected `SceneData` still declares `SceneGraphSystem` — the C++ hook must exist for components to attach to it. `external=True` is wrapper-side intent, not a serialization difference.
- Configuration kwargs that would configure the scenegraph (`preset=`, `SkyboxTexPathStr=`, `layers=`, postfx chain, `ssaa`, etc.) are **rejected** by the wrapper with a clear authoring error. Pass them with `external=False`, or remove them and let the host supply them.
- At bind time, if the host injects its `scene_ptr_t` via `controller.createSimulation(scenegraph=<host_scene>)`, `SceneGraphSystem::_onStage()` adopts the host scene and aliases its resources (cookie atlases, etc.). Layer lookups become idempotent — declaring `layer="std_forward"` returns the host's existing layer.
- If no host injection happens at bind time, the system creates a default scene (effectively `external=False` behavior with default settings). This means a scene authored with `external=True` is still standalone-loadable; it just gets defaults instead of the host's chosen rendering setup.

**`external=True` is NOT a generic policy.** It applies only to the SceneGraph handle. Other system handles reject the kwarg with a clear error:

```python
bullet = self.bullet(external=True)   # ERROR: BulletSystem does not support external instances
```

The reason: other systems (BulletSystem, ParticlesGlobalSystem, audio, etc.) are per-scene by nature — no equivalent injection model exists in the engine today. If a use case ever surfaces, it gets its own per-system mechanism; there's no "general external policy" to abuse.

### 4.7 Full ECS expressiveness — everything still reachable

The system-handle surface is **not a cap** on expressiveness. Anything `ecs.SceneData` supports is reachable, in three layers of decreasing sugar:

1. **System-handle namespaces (recommended)** — the surface in §4.2 above. Built for the common case; covers 90%+ of authoring patterns.
2. **Explicit primitive calls** — `self.archetype("Foo")`, `self.component(arch, "FooComponent", ...)`, `self.spawner("name", arch, ...)`, `self.system_data("FooSystem", ...)`. Reach for these when the system-handle wrapper doesn't cover what you need (multiple spawners off shared archetype with NodeInstanceData wiring, post-construction property mutation, cross-archetype references requiring forward declaration).
3. **Raw escape** — `scene.on_lower(lambda sd: ...)` runs at the end of Pass 2 with the live `ecs.SceneData`. Reserved for system types Scene composite doesn't model at all (debug-only tooling, third-party components without wrappers). **Not** for filling gaps in the primitive surface — gaps are bugs in the wrapper layer.

The sugar→primitive→raw progression is purely Python-side. Every layer produces the same reflected `SceneData`. A user moving between layers within one Scene is fine; the JSON output is unaffected by which layer was used at which point.

### 4.8 Sugar→primitive equivalence rule

Every system-handle form is defined as a mechanical expansion of the primitive form. Pseudocode for the entity sugar:

```python
def entity(self, name, *, transform=None, components=(), spawner=True):
    arch = self.archetype(f"Arch_{name}")
    for comp_decl in components:
        self.component(arch, comp_decl.typename, **comp_decl.kwargs)
    if spawner:
        sp = self.spawner(name, arch, autospawn=True)
        if transform is not None:
            sp.transform = transform
    return _EntityHandle(arch)
```

The invariant: `self.entity(name, components=[X])` and the equivalent `archetype + component + spawner` sequence produce **bit-identical** `ecs.SceneData`. Property-based tests enforce this; sugar that drifts from its primitive expansion is a bug to fix in the wrapper, not a feature.

Same rule applies to system handles: `bullet.component(mass=1.0, shape=bullet.shapes.sphere(radius=1.0))` is **defined as** `self.component(<implicit_arch>, "BulletObjectComponent", mass=1.0, shape=BulletShapeSphereData(radius=1.0))`. The wrapper is doing string-keyed routing into the existing primitive APIs; nothing else.

### 4.9 Editor symmetry

Since every primitive is reflected, the editor manipulates any Scene-built artifact at the granularity the underlying type allows:

- Add/remove an archetype, add/remove a component on an archetype
- Add/remove a spawner, edit its transform / autospawn flag
- Add/remove a system, edit its reflected properties
- Edit any plug value, asset generator field, scene parameter default, wiring entry
- Re-arrange the named entries in a SceneGraphComponent's `nodes` collection

The editor sees the canonical reflected `SceneData`. Whether the author wrote the scene through system-handle sugar, primitive calls, or raw `on_lower` is invisible after `build()`. Round-trip: Python author → `Scene.build(sd)` → `ecs.serialize(sd, "scene.json")` → editor loads → user edits ANY field → editor saves JSON → `ecs.deserialize("scene.json")` → identical scene at runtime.

### 4.10 Composition — fragment functions from external libraries

Every Scene declaration method is `self.<verb>(...)` on the Scene instance. That means a library can ship a plain Python function that takes a Scene and contributes declarations to it. No registration decorators, no plugin manifest, no special imports — just function calls. This is the natural composition primitive for breaking large scenes into reusable fragments.

#### Basic pattern — pass `self` to a fragment function

```python
# library file: audiolib/scene_fragments.py  (an external library)
def declareAudio(scene, *, ambient="default"):
  audio = scene.audio(engine="opus")
  scene.entity("ambient_sound",
    components=[audio.component(loop=True, sample=ambient)])
  return audio   # return the handle so caller can use it for more

# user file:
from audiolib.scene_fragments import declareAudio

class MyScene(Scene):
  def __init__(self):
    super().__init__()
    SG    = self.scenegraph(external=True)
    audio = declareAudio(self, ambient="forest")    # fragment contributes systems + entities

    # the returned handle is usable for more declarations:
    self.entity("monster",
      components=[audio.component(sample="roar.wav", trigger="on_proximity")])
```

The fragment function is a batch of `scene.<method>(...)` calls. Anything `__init__` can do, a fragment can do — declare systems, entities, assets, scene-level params, continuous/event wiring. The same wrapper-layer ergonomics (handle namespaces, sub-object factories, dict-shape for named collections) apply identically inside fragments.

#### Richer patterns

```python
# Fragment returns an entity handle the caller can extend:
def spawnZombie(scene, *, position, hp=100):
  z = scene.entity(f"zombie_at_{position}", components=[...],
                   transform=Transform(translation=position))
  return z

# Fragment extends an existing entity:
def attachWeaponPickup(scene, target_entity, weapon_type):
  target_entity.add_component(scene.weapons.component(weapon=weapon_type))

# Class-based fragment (a "scene aspect" bundling related declarations):
class CombatRulesAspect:
  def __init__(self, scene, *, friendly_fire=False):
    bullet = scene.bullet(lin_grav=vec3(0,-9.8,0))
    self.collision_groups = bullet.collision_groups({"player": 1, "enemy": 2})
    scene.expose("FriendlyFire", default=friendly_fire)

# Composed scene:
class Level3(Scene):
  def __init__(self):
    super().__init__()
    SG = self.scenegraph(external=True)
    declareSkyAndLighting(self)
    declareTerrain(self, region=(0, 0, 1024, 1024))
    rules = CombatRulesAspect(self, friendly_fire=False)
    for pos in spawn_points:
      spawnZombie(self, position=pos)
```

The same pattern Django uses for `urlpatterns.append(include('app.urls'))` and Flask uses for `app.register_blueprint(...)`. Well-established declarative-framework composition.

#### Three properties this gets right

1. **No new mechanism.** Pure Python function calls. Each fragment is just *batch-applied-declarations* against the receiving Scene. Imports follow standard Python rules; libraries plug in as long as they're imported by the host (same contract as §5.4).
2. **External libraries plug in naturally.** An external library (e.g. `audiolib`) ships `declareAudio(scene, ...)` (and/or class-based `AudioAspect`); your scene imports it. No orkid changes needed; matches the type-registration story in §5 — each external library exports its own helpers, the Scene composite consumes them via plain function calls.
3. **Reflected JSON is identical** whether the scene was built by one monolithic `__init__` or by 20 fragment-function calls. The serializer sees the same `ecs.SceneData`. Round-trip works unchanged.

#### Conventions and gotchas

- **Namespace entity names.** A fragment that declares `scene.entity("ambient_sound", ...)` and a host that also declares `"ambient_sound"` will collide (recommend the duplicate-name error be loud — sugar-vs-primitive equivalence makes this catch trivially). Fragments should namespace (`"audio.ambient"`, `"weapons.pickup_1"`) or accept a `name_prefix=` kwarg.
- **Order matters for cross-references between fragments.** If fragment A wires against an entity created by fragment B, B must run first — or the wire must use a stable string name resolved at Pass 2 (§3.2). Both are supported; the two-pass lowering handles it.
- **Fragments call fragments.** Composition nests freely; a "city level" fragment calls "building" fragments which call "lighting" fragments. All run against the same Scene instance; everything lowers together.
- **Fragments and `external=True`.** A fragment that calls `scene.scenegraph(external=True)` will conflict with a host that also calls `scene.scenegraph(...)`. Convention: the Scene's top-level `__init__` declares scenegraph; fragments use the existing handle. Same as not redeclaring a system twice.

#### The composition surface is the same as the authoring surface

There's no separate "fragment API" — fragments use the exact same `scene.<verb>(...)`, `handle.<method>(...)`, `entity.<extension>(...)` calls as inline `__init__` code. This is intentional: anything an author can do inline, a fragment can do; anything a fragment can do, an author can copy-paste inline. The composition primitive is "function that takes a Scene" — nothing more.

---

## 5. Extensibility — external libraries contribute types

orkid is the core engine; downstream projects (application layers, third-party game / sim / authoring libraries) routinely add their own dataflow modules, ECS systems, ECS components, asset generators, and HyperSyn ops. The declarative DSL **must** be able to use any of these without changes to orkid itself, and the resulting JSON **must** round-trip through the editor on any machine where the same libraries are imported.

Core principle: **the Scene composite holds no hardcoded list of orkid-internal types.** Every type reference flows through the runtime reflection / op registry. Whatever's imported in the invoking Python context is reachable from the DSL surface.

### 5.1 What external libraries can contribute

| Contribution | How it registers | DSL surface |
|---|---|---|
| New dataflow module (force, renderer, op) | C++ `DeclareConcreteX` + `GetClassStatic()` call at pyext init + `@op(family=..., name=...)` decorator on Python wrapper | `P.<MyOp>(upstream, ...)` inside any `ParticleSystem` (Tier 1/2/3) |
| New ECS system | C++ `SystemData` subclass with `DeclareConcreteX` + pyext registration | `self.system_data("MyExternalSystem", ...)` (Tier 3) |
| New ECS component | C++ `ComponentData` subclass with `DeclareConcreteX` + pyext registration | `self.component(arch, "MyComponent", ...)` (Tier 3) |
| New asset generator | C++ `AssetGenData` subclass + pyext + `.build()` instance method | `assets.MyGen(...).build()` (Tier 1/2 eager) or `self.asset.MyGen("name", ...)` (Tier 3 declarative) |
| New HyperSyn family | `@op(family="myfamily", name=...)` decorator + (optional) family-level base class | listed in `registry.list_families()`; usable per SKILL.md family contract |

The same patterns orkid uses internally — `RingEmitterData::GetClassStatic()` in `lev2_init.cpp`, `@op` in `ork/dflow/particles/ops/*.py` — work verbatim from any external library. **No "external" code path; just code paths that happen to live outside orkid's source tree.**

### 5.2 String-keyed lookup everywhere

The Scene primitive surface uses **registered reflection names** as strings, not Python class references:

```python
# Built-in (orkid)
self.system_data("BulletSystem", linGravity=vec3(0,-9.8,0))
self.system_data("SceneGraphSystem", preset="ForwardPBR")
self.component(arch, "BulletObjectComponent", mass=1.0, ...)

# External (any third-party library)
self.system_data("MyLibNetSyncSystem", port=7070)
self.component(arch, "MyLibVoiceComponent", codec="opus")
self.component(arch, "ThirdPartyAIBrainComponent", model_path="...")
```

The reflection registry holds the `(name → factory)` mapping. Any C++ library that has been dlopen'd and whose static initializers have run contributes its registered names to the same flat namespace. Scene composite never imports specific Python symbols from orkid (`from orkengine.ecs import BulletSystemData`); it asks the registry by string name.

The same applies to `assets.<Name>`/`self.asset.<Name>` — the asset DSL uses `__getattr__` on its namespace object, which queries the reflected `AssetGenData` registry. Whatever class is registered under name `Name` is what gets constructed.

### 5.3 HyperSyn op registration from external libraries

Per SKILL.md lines 228–261, the op registry is already an extension point — `@op(family=..., name=...)` registers any class anywhere. External libraries:

```python
# in audiolib/dflow/audio_ops/granular.py
from ork.dflow.dsl import op, PlugSpec

@op(family="audiolib_audio", name="GranularEmitter")
class GranularEmitterOp:
    inputs = { "source": PlugSpec(...) }
    outputs = { "voice": PlugSpec(...) }
    def build(self, graph, source):
        ...
```

After `import audiolib.dflow.audio_ops`, the op appears in `registry.list_ops("audiolib_audio")`. A user's particle DSL can mix orkid ops and external ops freely:

```python
class MyHybridSystem(ParticleSystem):
  def __init__(self):
    super().__init__()
    self.emit = P.RingEmitter(...)                         # orkid
    self.aud  = AudioLib.GranularEmitter(self.emit)        # external; same registry
    self.render(...)
```

This is **already the SKILL.md design**. External libraries inherit it.

### 5.4 JSON round-trip across library boundaries

When the Scene composite serializes to JSON, every reflected object is dumped by its registered name. The existing reflection serializer already handles the cross-library case correctly: deserialization in a Python context that has all the required classes registered succeeds; in a context missing any class, it asserts loudly ("Class not Found"). That's the contract, and the Scene composite inherits it unchanged.

The host (whatever loads a scene — a viewer, an editor, a batch tool) is responsible for `import`-ing the right set of libraries before deserializing. There's no metadata to read, no resolution policy to implement, no fallback. If a scene was authored against an extension library, the loader must import that library; otherwise the deserialize call asserts with a clear unknown-class error pointing at the missing type. This matches how every other reflection-serialized engine asset works today.

The same applies to any validator/materializer subprocess that runs scene-level workloads: the subprocess's Python startup must import the same set of libraries the authoring context uses. Standard configuration, not new infrastructure.

### 5.5 The Scene composite's own type registry

Scene's higher-level vocabulary (`AssetGenData` subclasses, `SceneParametersSystemData` mirror modules, etc.) is itself reflection-registered, so a third-party library can subclass and extend it the same way it extends ECS:

```python
# in some external library:
# C++ side: class MyTerrainGenData : public AssetGenData with DeclareConcreteX
# Python side: ensures GetClassStatic() runs at module import

# user code:
class MyScene(Scene):
  def __init__(self):
    super().__init__()
    self.terrain = self.asset.MyTerrainGen("ter", region=(0,0,1024,1024), seed=42)
```

`self.asset.MyTerrainGen` resolves via the same `AssetGenData` registry; the JSON round-trips identically as long as the loading context has the extension library imported (same contract as §5.4).

### 5.6 What this does NOT allow

- **Pure-Python ECS systems with no C++ side** — these can't round-trip through reflection. A Python-only "system" is acceptable as a build-time DSL helper that lowers to reflected entries (the same way `Expr` trees do) but cannot itself appear in the JSON. The Scene composite enforces this: anything that should appear in the JSON must lower to a registered reflection class.
- **Closure-capturing callbacks in scene wiring** — same constraint from §3.3. External-library wiring follows the "traced once at build, lowered to reflected descriptor" rule.

### 5.7 Library-loading discipline (operational)

For the round-trip to work reliably across machines:

1. Every C++ library that registers reflected types must have a deterministic import-time initialization (already true for orkid; required of extension projects).
2. Two libraries registering the same name produce a static-init error (standard reflection-system behavior).
3. The host is responsible for importing whatever set of libraries the scene was authored against. If something's missing, deserialization fails loudly with the engine's existing "Class not Found" assertion — no silent fallback, clear error pointing at the missing class name.

Net contract: open a JSON in any Python process that has imported the same libraries the author used → scene loads identically. Open in a process missing a library → assertion at deserialize time, naming the unknown class. The Scene composite adds no new resolution machinery beyond what the reflection serializer already provides.

---

## 6. HyperSyn integration — effortless from user side (Tiers 1–3)

> **Cross-ref (2026-06-24, updated 2026-06-25).** The HyperSyn family surface this section integrates has
> grown a *structural / topology* spine — a shared `XfNodeGraph` transform-graph currency + one rewrite/grammar
> engine that make flora / cities / creatures thin specializations. That spine is specified in
> `.claude/skills/hypersyn/UNIFIED_SUBSTRATE.md` §11–§16; its **foundation LANDED 2026-06-25** (the
> `XfNodeGraph` interchange plug + the `lsystem` generator family), with the grammar generalization +
> creature/city/skinning milestones still forward. It changes nothing about the Tier-1/2/3 contract here:
> new generator families (`lsystem` shipped; creature/city grammars forward) plug into the Scene composite
> exactly as §6.3 describes — each contributes Ops, a materializer, and a Scene verb.
> The `sdf` family is now a shipped standalone family (see SKILL.md), consumable from any tier the same
> way hypermesh is. No claim in this doc is superseded.

The Scene composite must make HyperSyn graph authoring **identical** to how it works today, just hostable inside a larger structure. Three concrete promises:

### 6.1 ParticleSystem subclasses unchanged (Tier 1)

```python
class ColVdbSystem(ParticleSystem):
  def __init__(self, *, funnel_sdf=None):
    super().__init__()
    self.pool = P.PoolData(size=5000)
    self.emit = P.RingEmitter(self.pool, ...)
    self.collider = P.VdbCollider(self.emit, sdf_grid=funnel_sdf, ...)
    self.render(...)
```

A `ParticleSystem` subclass is now **parameterizable** (accepts kwargs in its `__init__`). Otherwise unchanged from today. Hosts the same way under Tier 1 (passed kwargs from viewer CLI), Tier 2 (constructed standalone), or Tier 3 (constructed by Scene with asset references).

### 6.2 Asset DSL is dual-use

Asset DSL classes (`HollowFunnelMesh`, `MeshToSdf`, ...) work in **two valid forms**:

```python
# Standalone — eager build, returns a value:
mesh = assets.HollowFunnelMesh(top_outer=8.0, ...).build()
sdf  = assets.MeshToSdf(input_mesh=mesh, voxel_size=0.08).build()

# Scene-declared — registered, reflected, materialized at SCENE_LOAD:
mesh = self.asset.HollowFunnelMesh("funnel", top_outer=8.0, ...)
sdf  = self.asset.MeshToSdf("funnel_sdf", input_mesh=mesh, voxel_size=0.08)
```

Same gen-data classes. Same materializer code. Difference is *when* `.build()` runs: immediately (standalone) vs. deferred and reflected (Scene). The eager form is just a Python convenience method on the same reflected class. **Tier 1/2 authors get the same op vocabulary as Tier 3 authors without needing AssetSystem / SceneData / any ECS plumbing.**

### 6.3 Other HyperSyn families plug in the same way (Tier 3)

`Ptex3d.LeopardSkin(...)` → returns a material that becomes a `self.material(...)`-style asset. `Hypermesh.<sdf>(...)` → asset generator. `Terrain.<heightfield>(...)` → entity with terrain-component. Each family contributes:

- A set of Ops (already enforced by SKILL.md registry)
- A materializer producing reflected runtime artifacts
- A natural Scene-composite verb (`self.material`, `self.asset.*`, `self.entity`, ...)

This per-family modularity is what makes scene-scale authoring tractable: a tool or author working on one family loads only that family's Ops, while the Scene composite owns the top-level surface that ties the families together.

### 6.4 An external authoring layer wraps the right tier

An external authoring layer (an editor, a code generator, any application built on orkid) interacts with the engine through a single serialized artifact, chosen by the asset being authored:

- **Tier 1/2 asset** (a particle system, a procedural texture): serialized `dflow::graphdata_ptr_t` in, materialized handle out.
- **Tier 3 asset** (a full scene with particles + physics + sound + scenegraph): serialized `ecs.SceneData` in, scene-controller-handle out.

Both round-trip through reflection. The authoring layer picks the right tier for the request — a "particle effect" request produces a Tier 1 graph; a "courtyard with particles and physics" request produces a Tier 3 scene.

The two missing pieces motivated by scene-scale authoring:
- **StyleContext** — a Scene-level reflected system that propagates "1980s neon" / atmospheric coherence to every materializer. Becomes a `SceneStyleSystem` analogous to `SceneParametersSystem`. Tier 3-only.
- **AssetDB bridge** — `self.asset.from_db(kind="quadruped", tag=["feline"])` returns a `MeshRef + Skeleton` from a curated library. Sits alongside HyperSyn's procedural ops; consumable from Tier 1/2 (eager) or Tier 3 (declarative).

Both are reflected; both round-trip; both extend the asset DSL surface.

---

## 7. The workflow story

| Action | Tier | Tool | Touches |
|---|---|---|---|
| Initial particle effect | 1 | Python — `class Foo(ParticleSystem)` | `.py` file |
| Parameterized particle effect (reused across scenes) | 2 | Python — `class Foo(ParticleSystem):  def __init__(self, *, ...):` | `.py` file |
| Full scene with multiple entities | 3 | Python — `class FooScene(Scene)` | `.py` file |
| Generated authoring (any tier) | 1/2/3 | external authoring layer (code generator / app) | `.py` file via `CodeBuffer` |
| Validation + materialization | 1/2/3 | zmq worker pool (per SKILL.md) | subprocess |
| Standalone persistence | 1/2 | reflection serializer → JSON | graphdata `.json` |
| Scene persistence | 3 | reflection serializer → JSON | full ECS scenedata `.json` |
| Tweak a parameter | 1/2/3 | editor — JSON-backed widgets | `.json` file |
| Add/remove entity, restructure wiring | 3 | Python (easier than editor) | `.py` file → re-build → new JSON |
| Hot reload | 1/2/3 | controller stop → rebuild → JSON → controller start | full round-trip |
| Runtime mutation (live param drag) | 1/2/3 | `controller.systemNotify(SceneParametersSystem, ...)` (Tier 3) or per-graph `set_param` (Tier 1/2) | live |

The Python `.py` file is the **seed**. The JSON is the **canonical artifact**. The editor is the **interactive tweaker**. The tier choice is set by the asset's complexity, not by an arbitrary preference.

---

## 8. Concrete implementation foothold

Build the smallest set of things that proves the round-trip — without disrupting Tier 1/2:

### 8.1 Always-on (Tier 1/2 lives forever)

- `ork.particle.viewer.py` — unchanged hosting path.
- `ParticleSystem` base class — unchanged.

### 8.2 Tier 2 ergonomic improvement

- `ParticleSystem.__init__(**kwargs)` pass-through. ~3 LOC.
- Optional `--param key=value` CLI on `ork.particle.viewer.py`. ~20 LOC.

### 8.3 Asset DSL (used by Tier 1/2 eagerly, Tier 3 declaratively)

- `AssetGenData` reflected base class + first three generators (`HollowFunnelMesh`, `MeshToSdf`, `MeshToDrawable`). ~300 LOC C++.
- `.build()` instance method on each (eager materialization, no Scene needed). Reusable from any Tier.

### 8.4 Tier 3 foundations

- `Scene` base class + two-pass lowering — skeleton with `entity()` + `.spawner()` extension, `system_data()`, `archetype()`, `component()`, `spawner()`, `material()`, `asset.*`. Plus initial system-handle wrappers (`self.scenegraph(...)`, `self.bullet(...)`, `self.particles_global(...)`) each exposing `.component(...)` + relevant factory namespaces (`SG.drawables.*`, `bullet.shapes.*`). ~250 LOC Python.
- `AssetSystem` (reflected) — wires `AssetGenData` list into `ecs.SceneData`. ~80 LOC C++.
- `vdb.gridToDrawable(grid, material, iso)` — pyext helper. ~60 LOC.
- New `ork.scene.viewer.py` — coexists with `ork.particle.viewer.py`. ~150 LOC.

### 8.5 Acid tests

1. **Tier 1 regression**: every existing `.py` in `ork.data/particles/` still loads via `ork.particle.viewer.py` unchanged.
2. **Tier 2 demo**: `col_vdb.py` ported to use `assets.HollowFunnelMesh(...).build()` instead of the hand-rolled `_build_funnel_grid()`. Still loadable by `ork.particle.viewer.py`.
3. **Tier 3 round-trip**: a new `col_vdb_scene.py` declared as a `Scene`. Run via `ork.scene.viewer.py`. Serialize the resulting `ecs.SceneData` to JSON; reload from JSON; verify identical runtime behavior.
4. **Primitive completeness**: a `Scene` subclass that declares all the same things `physics/FPS.py` does — explicit archetypes, multiple spawners per archetype, custom system parameters — entirely through the Scene primitive surface, no `on_lower` escape hatch needed.
5. **External-library round-trip**: a stub library outside orkid (literally `examples/external_stub/` with one `SystemData` subclass and one `AssetGenData` subclass) is `import`-able from a Scene-subclass `.py`; the resulting JSON loads correctly in a fresh process that imports the stub; loading WITHOUT importing the stub fails at deserialize time with the engine's existing "Class not Found" assertion naming the unknown class. This proves §5's extensibility contract end-to-end.

Wiring, scene-exposed params, and event handling are deferred to a second pass once the foothold runs.

---

## 9. Risks and open decisions

### 9.1 Decided

- **Three-tier model is permanent.** Tier 1 (`ParticleSystem`) and Tier 3 (`Scene`) coexist forever; neither replaces the other.
- **Procedural assets are dual-use** — eager `.build()` for Tier 1/2; declarative + reflected for Tier 3. Same gen-data classes.
- **Scene primitive surface must be complete** — no escape hatch needed for standard ECS shapes. Sugar is a *shorthand*, not a *cap*.
- **Two-pass lowering from day one.** Forward references must work regardless of source order.
- **No orkid-internal Python imports in the Scene type-resolution path.** Every system/component/asset-generator lookup goes through the reflection registry by string name; whatever's imported in the user's Python context contributes to the available vocabulary.
- **JSON loading inherits the reflection serializer's existing contract** — missing-class on deserialize is a loud assertion naming the unknown class. The Scene composite adds no resolution layer; the host imports whatever libraries the scene was authored against.

### 9.2 Still on the table

- **Aggressive vs. conservative `Expr.time` rename.** Recommend aggressive (rename to `slot.t`, deprecate alias) — we're in design-the-shape mode. Soft break with deprecation warning, not hard break.
- **Asset content-addressed caching to disk.** For small scenes, always-re-bake works. For heavy meshes, file-side `.vdb`/`.xgm` artifacts next to the JSON become load-bearing. Defer until a heavy use case demands it.
- **StyleContext system shape.** Scene-scale authoring use cases want it. The exact reflected schema needs concrete use cases before locking. Defer until second integration milestone.
- **Versioning policy for external-library registrations.** When an external library ships a new `AudioLibVoiceComponent` schema, what does the editor do with an old JSON? Defer the SemVer story until the second downstream library exists; for now, fail loudly on schema drift.

### 9.3 Risks to actively manage

- **Tier 1/2 must not regress.** Any change to `ParticleSystem` base class is held to a strict "no behavior change for unparameterized subclasses" bar. Visual diff test on every existing `.py` before each landing.
- **Sugar vs. primitive divergence.** If `self.entity(...)` and the equivalent `self.archetype()+self.component()+self.spawner()` sequence don't produce bit-identical scene data, that's a bug. The sugar must be a *pure expansion* over the primitive layer; ship them together with property-based tests.
- **Cross-entity Python references creating implicit ordering dependencies.** Two-pass lowering with stable IDs is the mitigation — must not skip it.
- **Python callback lifetime in event wiring.** `scene.on(..., payload=lambda evt: ...)` traces the lambda at build time into a reflected descriptor; the lambda itself is discarded. Validate eagerly that the lambda is purely descriptive (no closures over Python state).
- **Library-name collisions in the reflection registry.** Two libraries registering the same class name produce a static-init error today (standard reflection behavior). Document this so external library authors namespace their types (`AudioLibVoiceComponent`, not `VoiceComponent`).
- **Validator subprocess library set drift.** Any subprocess that loads scenes must import the same set of libraries the authoring context used; if it doesn't, deserialization asserts at the unknown-class boundary. Operationally: make whichever component spawns the workers responsible for configuring their startup imports — single configuration source, not multiple drift-prone copies.

---

## 10. What's deferred to a separate document

- The HyperSyn family rollout schedule (M0–M5) — owned by `PLAN.md`.
- Neural-op hybridization (INSTALL/SCENE_LOAD phase, content-addressed) — owned by `PLAN.md` ("Neural ops").

This doc is the bridge between SKILL.md's "future ECS instantiation" placeholder and the actual day-one shape the codebase commits to. The three-tier model is the load-bearing claim: Tier 1 (`ParticleSystem` + `ork.particle.viewer.py`) lives forever and answers "give me a single particle effect"; Tier 3 (`Scene` + `ork.scene.viewer.py`) answers "give me a full ECS scene with editor round-trip." Both are first-class.
