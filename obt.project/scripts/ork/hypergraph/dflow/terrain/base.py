###############################################################################
# ork.hypergraph.dflow.terrain.base — HeightField family base class.
#
# User code subclasses HeightField and builds the heightfield DAG in __init__ via
# the expression-first terrain DSL. The base class:
#  - allocates a fresh dflow.GraphData on construction
#  - opens a trace context so DSL ops/operators target that graph
#  - exposes self.capture(node, channel) to record output channels (multi-sink:
#    a terrain bake emits several channels — height, slope, masks, ...)
#  - generatedflow() closes the trace and returns the populated GraphData
#
# Materialization is a BAKE: lev2.terrain.bake_heightfield(graphdata, ctx, dim)
# runs the compute DAG once and writes one EXR/PNG per capture channel. The base
# stays out of the bake — the asset wrapper (later) assigns per-channel paths via
# set_capture_path() then calls the driver.
#
# __init__ takes **kwargs so the same class is reusable parameterized: loop
# bounds / conditionals in __init__ read kwargs the asset/scene supplies (Python
# control flow runs at TRACE time and unrolls into DAG topology).
###############################################################################

from collections import namedtuple as _namedtuple
from orkengine.core import dataflow as _dflow
from orkengine.lev2 import terrain as _terrain
from .._trace import enter_trace, leave_trace, current_graph
from ._node import TerrainNode, anon_name

_TAU = 6.28318530718

# A SCATTER sink (mask-driven placement). Recorded at trace time by HeightField.scatter();
# consumed POST-bake by the CPU placer (scatter.place) to produce a ScatterSet (.ogeo).
# MESH-AGNOSTIC — carries no mesh; only how/where to place + per-instance variation.
# `types` = ordered tuple of (type_name, weight_channel): one SHARED point set, per-point
# weighted pick among the types -> mutually exclusive (no overlap). type_id = the index.
# OVERLAPPING layers are just separate scatter() calls (independent point sets).
# `assets`/`materials` (D.4): optional reflected type→asset-name bindings ({tname: asset_name})
# that ride ScatterSinkData into the scene JSON so a consumer can resolve mesh + material per
# type_id from the AssetSystem registry at load. Empty = the legacy live-recipe path
# (e.g. scatter_hypermeshes()) supplies them.
ScatterSpec = _namedtuple(
    "ScatterSpec",
    "name types density count seed align yaw scale cutoff jitter max_points lift assets materials colliders",
    defaults=({}, {}, {}))


class HeightField:
    """Family base for HyperSyn terrain heightfield graphs (expression-first DSL).

    Subclass and implement __init__:

        from ork.hypergraph.dflow.terrain import HeightField
        from ork.hypergraph.dflow import terrain as T

        class RollingHills(HeightField):
          def __init__(self, octaves=5, steps=6):
            super().__init__()                       # opens the trace
            h = T.Fbm(frequency=3.0, octaves=octaves) * 0.5 + 0.5
            self.capture(T.Terrace(h, steps=steps), "height")

    Then:

        hf = RollingHills()
        g  = hf.generatedflow()                      # the populated dflow.GraphData
        hf.set_capture_path("height", "/tmp/h.exr")  # asset/bake harness supplies paths
        stats = lev2.terrain.bake_heightfield(g, ctx, 1024)
    """

    # ---- authored world scale (PHYSICAL) ---------------------------------------
    # A terrain DSL OWNS its world dimensions; consumers (the viewer, the asset
    # wrapper, segmentation/placement tools) READ these instead of hardcoding.
    # Subclasses override. These are the FINAL PHYSICAL scale — what the mesh renders
    # at and what every measurement (normals, slope, curvature, segmentation) uses.
    # The EROSION vertical exaggeration is NOT here: it is scoped to the erosion ops
    # (erode_thermal/erox `exaggerated_height_m`), authored where erosion is.
    EXTENT_M = 32768.0   # XZ meters the heightfield spans (centered at origin)
    HEIGHT_M = 4000.0    # FINAL physical meters that a normalized height of 1.0 represents

    # ---- suggested display material (authoring metadata; the bake ignores it) --------
    # The terrain "knows" how it wants to be shaded. MATERIAL names a known material
    # ("hmview"); MATERIAL_PARAMS are that material's ctx.param / DSL kwargs (e.g.
    # strata_freq, scale, colors). Co-locate params that MUST agree with the terrain —
    # e.g. a strata terrace period derived from the same scale/strata_freq. Viewers/tools
    # READ these; -M / explicit args override. MATERIAL is resolved by name from the
    # terrain asset's OWN folder first, then materials/ (so a bespoke material can live
    # next to its terrain). MATERIAL_CLASS, if set, is a Ptex3d subclass used directly
    # (define it inline in the DSL or import it) — takes precedence over the name.
    MATERIAL = "hmview"
    MATERIAL_PARAMS = {}
    MATERIAL_CLASS = None

    def __init__(self, **kwargs):
        # Fresh empty graph; DSL ops populate it during user __init__.
        self.graphdata = _dflow.GraphData.createShared()
        # terrain graphs opt in to the per-node cook cache (content-addressed,
        # disk-backed). The flag round-trips with the (embedded) graph.
        self.graphdata.cacheable = True
        # channel name -> CaptureModule (the sink). dict preserves declaration
        # order so the bake's FieldStats list lines up with channels().
        self._captures = {}
        # name -> ScatterSpec: POST-bake placement sinks (see scatter()). Lives on the
        # python instance (bake-time only); the persisted artifact is the ScatterSet .ogeo.
        self._scatters = {}
        # **kwargs accepted + ignored at the base — subclasses opt in by
        # declaring their own signature (parameterized terrain). Open the trace;
        # generatedflow() closes it.
        self._prev_trace = enter_trace(self.graphdata)

    def capture(self, node, channel, cache=False):
        """Record an output channel. Creates a CaptureModule fed by `node`,
        keyed by `channel`. Repeatable for multi-channel bakes; the on-disk path
        is supplied later via set_capture_path() (asset wrapper / bake harness).

        cache=False disables the disk cook cache for the whole bake (the cache is
        whole-bake / per-node, so any capture opting out turns it off): the bake
        recomputes every node every run and never reads/writes <staging>/dflowcache.
        The per-op GPU sync is unaffected — only disk I/O is skipped."""
        if not isinstance(node, TerrainNode):
            raise TypeError(
                f"HeightField.capture() expects a terrain node (output of a T.* op "
                f"or operator); got {type(node).__name__}")
        if not self._is_tracing():
            raise RuntimeError(
                f"capture({channel!r}) called outside a trace context — call "
                f"super().__init__() at the top of your HeightField subclass __init__.")
        # `channel` is a single name ("height") or a list (["height","normal"]). The
        # list form emits one image PER channel from a SINGLE source readback (more
        # efficient than two separate capture() calls). Synthesized channels: "normal"
        # (Scharr normal of the height field); any other name is the scalar field itself.
        chans = [str(c) for c in channel] if isinstance(channel, (list, tuple)) else [str(channel)]
        for c in chans:
            if c in self._captures:
                raise ValueError(f"duplicate capture channel {c!r}")
        g = self.graphdata
        cap = g.create(anon_name("capture", g), _terrain.CaptureModule)
        g.connect(cap.inputs.In, node.output_plug)
        cap.channel = ",".join(chans)  # comma-joined; the bake splits + emits one image/channel
        cap.cache = bool(cache)        # per-bake cook-cache opt-out (round-trips)
        for c in chans:
            self._captures[c] = cap    # each channel name resolves to this (shared) cap

    def hfbake(self, expr, channel, *, cache=True):
        """Bake a ptex3d EXPRESSION to a data channel (the unified procedural substrate).
        `expr` is a ptex3d SurfNode, or a callable f(ctx)->SurfNode (ctx = ptex3d
        SurfaceCtx — the SAME authoring surface as a Ptex3d material, so ONE function can
        SHADE and BAKE). It runs over the grid via a generic ExprModule and is captured as
        `channel`: a data field other shaders sample or other hf modules consume (masks,
        strata/terrace id, ...). Bake-portable ops only (no view-dependent NV/eye/Cd)."""
        from .ops import expr_field
        self.capture(expr_field(self._eval_expr(expr)), channel, cache=cache)

    def hfmask(self, expr, channel, *, cache=True):
        """Like hfbake but saturate()s the field to [0,1] (a blend mask)."""
        from ork.hypergraph.ptex3d import P
        from .ops import expr_field
        self.capture(expr_field(P.saturate(self._eval_expr(expr))), channel, cache=cache)

    def relax_uv(self, node, *, strength=1.0, iterations=0):
        """EQUAL-AREA UV RELAXATION (the slope-stretch fix). Adds a T.relax_uv on the height
        `node` and captures its two channels — "relaxed_uv" (RGBA: relaxed uv.xy + geometric
        normal.x,z) and "binormal" (RGBA: relaxed binormal.xyz). The chunk drawable loads + (decouple)
        downsamples these into per-vertex SSBO arrays so the VS reads uv0 + the precomputed tangent
        frame: steep faces get an equal texel budget (texels/physical-area ~uniform) and the VS goes
        tap-light. Pure function of the height -> cook-cached. One line after capturing height:

            self.capture(h, "height")
            self.relax_uv(h)
        """
        from .ops import relax_uv as _relax_op
        r = _relax_op(node, strength=strength, iterations=iterations)
        # cache=True: the relax is a pure function of the height -> cook-cacheable. capture() defaults to
        # cache=False which disables the disk cook cache for the WHOLE bake (any one cache=False turns it
        # off) — so without this the entire erosion graph recomputes every run (the slow non-cached startup).
        self.capture(r.uv, "relaxed_uv", cache=True)
        self.capture(r.binormal, "binormal", cache=True)
        return r

    def hfdisplacement(self, expr, into, *extra, mask=None):
        """Displace the height field `into` by a ptex3d expression that READS the current
        height: the bake sets ctx.P_object.y / ctx.P.y = in0 * height_m (PHYSICAL), so the
        SAME strata(ctx) that shades a Ptex3d material also DISPLACES here — baked terraces
        coincide with the shaded bands. `into` wires to ctx.input(0); `extra` TerrainNodes
        wire to ctx.input(1).. (masks/warps/flow). Returns the displaced height node (capture
        it as 'height'); pass mask= (a TerrainNode) to blend old<->new by it.

            h = self.hfdisplacement(terrace_strata, h)        # snap to strata-band elevations
            self.capture(h, 'height')
        """
        from .ops import expr_field
        node = expr_field(self._eval_expr(expr), inputs=[into, *extra])
        if mask is not None:
            node = into.masked_by(node, mask)   # mix(into, node, mask) per-texel
        return node

    def scatter(self, name, *, types=None, mask=None, density=None, count=None, seed=0,
                align="normal", yaw=(0.0, _TAU), scale=(1.0, 1.0),
                cutoff=0.0, jitter=1.0, max_points=6000000, lift=0.0,
                assets=None, materials=None, colliders=None):
        """Record a SCATTER sink — a mask-driven placement set baked POST-bake (CPU) into a
        ScatterSet (.ogeo): per-point WORLD TRS matrices (translate · align-to-normal · random
        yaw · per-point scale, SCALE BAKED INTO the matrix) + an extensible instance-attribute
        table (type_id, variant_seed, ...).

        MESH-AGNOSTIC: scatter produces ONLY placement data — no mesh, no instancing. A SEPARATE
        consumer (an instance_to_mesh node, or a direct instanced-drawable / mesh-task handoff)
        reads the ScatterSet and binds meshes per type_id.

        MULTI-TYPE / HOLISTIC: `types={name: weight}` places ONE shared point set and picks a type
        per point by WEIGHTED-RANDOM over the per-type weight fields — so the types are MUTUALLY
        EXCLUSIVE (no overlap), and composition is holistic (types compete per point). Coverage
        follows the TOTAL weight W=Σwₖ (gated by `cutoff`); the type follows the relative weight
        wₖ/W. `mask=<TerrainNode>` is 1-type sugar (types={"_": mask}). For types that SHOULD
        overlap, use SEPARATE scatter() calls (independent point sets). Each weight is any
        TerrainNode (slope/band/curvature/noise/...), auto-captured as a hidden channel.

          density/count : placement amount (exactly one) — points/m^2, or total points.
          types / mask  : {name: weight TerrainNode} (exclusive), or a single mask (1 type).
          cutoff        : reject where total weight W < cutoff (0 = keep, weighted by W).
          seed          : int; into the placement RNG + cook key (deterministic, res-independent).
          align         : "normal" (up -> surface normal) or "up" (world +Y).
          yaw           : (lo,hi) random rotation about the up axis, radians.
          scale         : (lo,hi) uniform per-point scale, BAKED INTO the matrix.
          jitter        : 0..1 in-cell positional jitter (mask-weighted jittered grid).
          max_points    : hard cap (the instanced-drawable engine limit is 262144 = InstancedDrawable::k_max_instances).
          lift          : METERS to offset each instance along its UP axis (the align axis: surface normal for
                          align="normal", world +Y for align="up"), BAKED into the matrix translation. A uniform
                          (scale-independent) pre-applied offset — e.g. lift the prop so its base sits on the ground.
          assets        : optional {type_name: asset} — the MESH asset each type renders (an asset
                          wrapper with .gendata.asset_name, or a bare name string). Reflected (D.4):
                          rides the scene JSON; a consumer resolves it from the AssetSystem registry.
          materials     : optional {type_name: material asset} — same convention, the per-type material.
        """
        if not self._is_tracing():
            raise RuntimeError(
                f"scatter({name!r}) called outside a trace context — call super().__init__() "
                f"at the top of your HeightField subclass __init__.")
        if (density is None) == (count is None):
            raise ValueError(f"scatter({name!r}): pass exactly one of density= (points/m^2) "
                             f"or count= (total points).")
        if (types is None) == (mask is None):
            raise ValueError(f"scatter({name!r}): pass exactly one of types={{name: weight}} "
                             f"or mask=<TerrainNode> (1-type sugar).")
        if name in self._scatters:
            raise ValueError(f"duplicate scatter sink {name!r}")
        if mask is not None:
            types = {"_": mask}                          # 1-type sugar
        # auto-capture each type's WEIGHT field as a hidden channel the CPU placer reads
        # post-bake; type_id = the declaration index (0..K-1), name preserved for the consumer.
        type_list = []
        for i, (tname, w) in enumerate(types.items()):
            if not isinstance(w, TerrainNode):
                raise TypeError(f"scatter({name!r}) type {tname!r} weight must be a TerrainNode "
                                f"(slope/band/curvature/noise/...); got {type(w).__name__}.")
            ch = "_scatter_%s_w%d" % (name, i)
            self.capture(w, ch, cache=True)
            type_list.append((str(tname), ch))
        declared = {t for (t, _c) in type_list}
        def _names(d, what):
            # accept asset wrappers (with .gendata.asset_name) or bare name strings
            out = {}
            for tname, a in (d or {}).items():
                if str(tname) not in declared:
                    raise ValueError(f"scatter({name!r}) {what} references unknown type {tname!r}")
                nm = a if isinstance(a, str) else getattr(getattr(a, "gendata", None), "asset_name", "")
                if not nm:
                    raise ValueError(f"scatter({name!r}) {what}[{tname!r}] needs an asset wrapper "
                                     f"(with .gendata.asset_name) or a name string")
                out[str(tname)] = nm
            return out
        # colliders: {type: ("sphere", r) | ("capsule", r, h) | ("box", x, y, z)
        #             | ("cone", r, h)} -> the PER-ITEM physics proxy. Encoded
        # "kind:d0:d1:d2" into the reflected map; the placer bakes kind+dims PER
        # POINT into the ScatterSet, so the collider shape rides the DATA
        # (BulletShapeScatter reads items, hardcodes nothing). cone follows the
        # hypermesh cone convention: BASE at the item origin, apex up local +Y.
        _KINDS = {"sphere": 0, "capsule": 1, "box": 2, "cone": 3}
        coll_out = {}
        for tname, cspec in (colliders or {}).items():
            if str(tname) not in declared:
                raise ValueError(f"scatter({name!r}) colliders references unknown type {tname!r}")
            kind = _KINDS.get(str(cspec[0]).lower())
            if kind is None:
                raise ValueError(f"scatter({name!r}) collider kind must be sphere/capsule/box/cone")
            dims = [float(x) for x in cspec[1:]] + [0.0, 0.0, 0.0]
            coll_out[str(tname)] = "%d:%g:%g:%g" % (kind, dims[0], dims[1], dims[2])
        self._scatters[name] = ScatterSpec(
            name=str(name), types=tuple(type_list),
            density=(None if density is None else float(density)),
            count=(None if count is None else int(count)),
            seed=int(seed), align=str(align), yaw=(float(yaw[0]), float(yaw[1])),
            scale=(float(scale[0]), float(scale[1])), cutoff=float(cutoff),
            jitter=float(jitter), max_points=int(max_points), lift=float(lift),
            assets=_names(assets, "assets"), materials=_names(materials, "materials"),
            colliders=coll_out)

    @property
    def scatters(self):
        """The declared scatter sinks: {name -> ScatterSpec}."""
        return dict(self._scatters)

    def _eval_expr(self, expr):
        """A ptex3d SurfNode, or a callable f(ctx)->SurfNode evaluated against a fresh
        SurfaceCtx (the same ctx a Ptex3d material __init__ receives)."""
        if callable(expr):
            from ork.hypergraph.ptex3d.dsl import SurfaceCtx
            return expr(SurfaceCtx())
        return expr

    @property
    def channels(self):
        """The declared channel names, in declaration order (one EXR each)."""
        return tuple(self._captures.keys())

    def set_capture_path(self, channel, path):
        """Assign a channel's on-disk output path before bake_heightfield()."""
        if channel not in self._captures:
            raise KeyError(f"no such capture channel {channel!r}; have {self.channels}")
        self._captures[channel].path = str(path)

    def generatedflow(self):
        """Close the trace and return the populated dflow.GraphData. Idempotent —
        re-calling just returns the same graph."""
        if self._is_tracing():
            leave_trace(self._prev_trace)
            self._prev_trace = None
        return self.graphdata

    def _is_tracing(self):
        return current_graph() is self.graphdata
