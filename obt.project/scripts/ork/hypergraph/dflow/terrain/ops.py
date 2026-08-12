###############################################################################
# ork.hypergraph.dflow.terrain.ops — named terrain ops.
#
# Generators (Fbm/Gradient/Const) and ops with no natural operator
# (Terrace/Mix/Min/Max/Clamp/Remap) are named functions; affine + blend compose
# via TerrainNode operators (see _node.py). Each call creates exactly one module
# in the active trace graph and returns a TerrainNode so it composes downstream.
###############################################################################

from orkengine.core import vec2 as _vec2
from orkengine.lev2 import terrain as _terrain
from ...units import unit_of as _unit_of   # E0 typed literals (meters()/texels())
from .._parampack import _apply_packs
from ._node import (
    TerrainNode,
    anon_name,
    graph_or_raise,
    make_const,
    make_remap,
    make_combine,
    make_maskblend,
    OP_MIX,
    OP_MIN,
    OP_MAX,
)


# --- generators (no input) ---------------------------------------------------

def _mark_field_animated():
    """E.1b: a time-driven field (offset_vel != 0) makes the HOSTING graph animated — when a
    Hypermesh asset is being composed (the selexpr build-asset binding, the S.time mechanism),
    flag it so is_animated reports True and the live render path re-evals each frame. A pure
    HeightField bake has no bound asset -> no-op (bakes are the t=0 snapshot regardless)."""
    try:
        from ork.hypergraph.dflow.hypermesh.selexpr import _build_ctx
        from .._trace import current_graph
        a = _build_ctx.get("asset")
        # the binding is a last-set global — only mark an asset whose graph IS the active
        # trace (a stale binding from a previously-composed asset must not be flagged).
        if a is not None and getattr(a, "graphdata", None) is current_graph():
            a._field_animated = True
    except Exception:
        pass


def _apply_offset_warp(g, m, offset, warp, warp_amt, offset_vel=None):
    """Wire the shared DOMAIN controls onto an Fbm/Noise module: a constant `offset`
    (a `core.vec2`, lattice-cell units — pan / reseed the field), an optional
    `offset_vel` pan VELOCITY (vec2 or (x,y), cells/second — effective offset =
    offset + offset_vel * time, fed by the family env clock; B.4: bake = t=0), and an
    optional FUSED domain `warp=(wx, wy)` — two scalar terrain nodes (typically signed,
    e.g. `T.fbm(...) - 0.5`) that displace the sample position per-texel, scaled by
    `warp_amt`. The basis is then evaluated at the warped domain analytically (no
    resample round-trip). Both warp fields must be connected for the warp to take effect."""
    # the plug is a 2-component vec2 — guard here so a wrong type/arity raises a clean
    # Python error instead of hard-aborting in the C++ pybind setter (pyext_dataflow assert).
    if not isinstance(offset, _vec2):
        raise TypeError(
            f"offset= must be a core.vec2 (got {type(offset).__name__}); the domain offset "
            f"is 2D — e.g. core.vec2(origin.x, origin.z), NOT a 3-element vec3/list")
    m.inputs.offset   = offset                       # core.vec2 -> the module's vec2 plug
    if offset_vel is not None:
        if not isinstance(offset_vel, _vec2):
            try:
                vx, vy = offset_vel
                offset_vel = _vec2(float(vx), float(vy))
            except (TypeError, ValueError):
                raise TypeError(
                    f"offset_vel= must be a core.vec2 or an (x, y) pair (got {type(offset_vel).__name__})")
        m.inputs.offset_vel = offset_vel
        if offset_vel.x != 0.0 or offset_vel.y != 0.0:
            _mark_field_animated()                   # the hosting mesh graph must re-eval per frame
    m.inputs.warp_amt = float(warp_amt)
    if warp is not None:
        try:
            wx, wy = warp
        except (TypeError, ValueError):
            raise TypeError("warp= must be a (wx, wy) pair of terrain nodes")
        g.connect(m.inputs.warp_x, wx.output_plug)
        g.connect(m.inputs.warp_y, wy.output_plug)


def fbm(*packs, frequency=4.0, amplitude=1.0, octaves=5,
        offset=_vec2(0.0, 0.0), offset_vel=None, warp=None, warp_amt=1.0, name=None, **overrides):
    """Fractal value-noise field (a GENERATOR — no input node). `frequency`/`amplitude` are
    float plugs and `octaves` is a BAKED loop bound; supply any of them via ParamPacks
    and/or keyword overrides, e.g. `T.Fbm(P1_FBM)`, `T.Fbm(T.lerp(P1_FBM, P2_FBM, t))`, or
    `T.Fbm(frequency=6.2, octaves=7)`. A lerped `octaves` snaps to the nearest int.
    `offset=core.vec2(x,y)` pans/reseeds the domain; `offset_vel=(x,y)` PANS it over TIME
    (cells/sec, env-clock-fed — a scrolling/animated field; bake = the t=0 snapshot);
    `warp=(wx,wy)` fused-warps it — e.g. `T.fbm(frequency=6, warp=(wx, wy), warp_amt=0.3)`.
    frequency/amplitude/offset are RUNTIME (params SSBO): poke m.inputs.* live, no recompile."""
    g = graph_or_raise("Fbm")
    m = g.create(name or anon_name("fbm", g), _terrain.FbmModule)
    m.octaves = int(octaves)             # baked defaults; packs/overrides may replace below
    m.inputs.frequency = float(frequency)
    m.inputs.amplitude = float(amplitude)
    _apply_offset_warp(g, m, offset, warp, warp_amt, offset_vel)
    _apply_packs(m, packs, overrides)    # frequency/amplitude plugs + octaves baked scalar
    return TerrainNode(m, m.outputs.Out)


# noise-basis PRIMITIVES (one NoiseModule, basis baked). GENERATORS (no input) — frequency
# is lattice cells across the map, amplitude scales output, octaves (default 1 = pure
# primitive) fBm-stacks. Compose with the terrain operators (mix/blend/remap/+,*) rather
# than baking composites in. Four named ops (T.perlin / T.simplex / T.worleyf1 / T.voronoi)
# over one shared module, so the basis reads at the call site.
_NOISE_BASIS = {"perlin": 0, "simplex": 1, "worleyf1": 2, "voronoi": 3}

def _noise(basis, frequency, amplitude, octaves, name, offset=_vec2(0.0, 0.0), warp=None, warp_amt=1.0,
           offset_vel=None):
    g = graph_or_raise(basis)
    m = g.create(name or anon_name("noise", g), _terrain.NoiseModule)
    m.basis   = _NOISE_BASIS[basis]      # baked primitive selector
    m.octaves = max(1, int(octaves))     # baked loop bound (1 = pure primitive)
    m.inputs.frequency = float(frequency)
    m.inputs.amplitude = float(amplitude)
    _apply_offset_warp(g, m, offset, warp, warp_amt, offset_vel)
    return TerrainNode(m, m.outputs.Out)


def perlin(frequency=4.0, amplitude=1.0, octaves=1, offset=_vec2(0.0, 0.0), warp=None, warp_amt=1.0, name=None, offset_vel=None):
    """PERLIN gradient noise (generator). frequency = lattice cells across the map; octaves
    (default 1 = pure primitive) fBm-stacks the basis. Smooth, mild grid-axis bias.
    `offset=core.vec2(x,y)` pans/reseeds; `warp=(wx,wy)`/`warp_amt` fused-warp the domain."""
    return _noise("perlin", frequency, amplitude, octaves, name, offset, warp, warp_amt, offset_vel)


def simplex(frequency=4.0, amplitude=1.0, octaves=1, offset=_vec2(0.0, 0.0), warp=None, warp_amt=1.0, name=None, offset_vel=None):
    """SIMPLEX gradient noise (generator) — fewer directional/grid artifacts than perlin.
    Args as perlin()."""
    return _noise("simplex", frequency, amplitude, octaves, name, offset, warp, warp_amt, offset_vel)


def worleyf1(frequency=4.0, amplitude=1.0, octaves=1, offset=_vec2(0.0, 0.0), warp=None, warp_amt=1.0, name=None, offset_vel=None):
    """WORLEY cellular F1 (generator) — distance to the nearest scattered feature point
    (bubbly cells; ridged-ish when fBm-stacked). Args as perlin()."""
    return _noise("worleyf1", frequency, amplitude, octaves, name, offset, warp, warp_amt, offset_vel)


def voronoi(frequency=4.0, amplitude=1.0, octaves=1, offset=_vec2(0.0, 0.0), warp=None, warp_amt=1.0, name=None, offset_vel=None):
    """VORONOI (generator) — flat per-cell random value (the cell partition / id field).
    Args as perlin()."""
    return _noise("voronoi", frequency, amplitude, octaves, name, offset, warp, warp_amt, offset_vel)


def gradient(dir_x=1.0, dir_y=0.0, scale=1.0, bias=0.0, name=None):
    """Linear ramp: dot(uv, dir) * scale + bias, with dir = (dir_x, dir_y)
    carried as a single vec2 plug."""
    g = graph_or_raise("Gradient")
    m = g.create(name or anon_name("grad", g), _terrain.GradientModule)
    m.inputs.dir = _vec2(float(dir_x), float(dir_y))
    m.inputs.scale = float(scale)
    m.inputs.bias = float(bias)
    return TerrainNode(m, m.outputs.Out)


def const(level=0.5, name=None):
    """Constant field."""
    return make_const(level, name=name)


def expr_field(surfnode, inputs=(), name=None):
    """Bake a ptex3d SurfNode (scalar) into a terrain field via the generic ExprModule
    (the unified procedural substrate). The expression is emitted to a compute shader —
    the SAME op/noise vocabulary the fragment uses (lib_pnoise) — and run over the grid.
    Backs HeightField.hfbake/hfmask/hfdisplacement; `surfnode` is a ptex3d dsl SurfNode
    (e.g. P.sin(...) over ctx.P_object). `inputs` is a list of upstream TerrainNodes wired
    to In0..In{n-1} and read in the expression via ctx.input(k) — input 0 is the current
    height (ctx.P_object.y = in0 — TRUE METERS). Bake-portable ops only (no view-dependent atoms)."""
    from ork.hypergraph.ptex3d.dsl import emit_compute_field
    from ork.hypergraph.ptex3d.compute_template import build_field_shader
    g = graph_or_raise("ExprField")
    inputs = list(inputs)
    lines, final, libsrcs, inherits, imports, params, in_idx = emit_compute_field(surfnode)
    if in_idx and max(in_idx) >= len(inputs):
        raise ValueError(
            f"expression references ctx.input({max(in_idx)}) but only {len(inputs)} "
            f"input(s) were connected (expr_field inputs=...).")
    text = build_field_shader(lines, final, libsrcs, inherits, n_inputs=len(inputs))
    m = g.create(name or anon_name("expr", g), _terrain.ExprModule)
    m.shadertext = text
    # E2.5: capture the SurfNode as canonical ExprIR — the writer re-emits real DSL from it
    # (replacing the compiled-blob escape hatch) and the cook hash keys off its bytes.
    _set_expr_tree(m, surfnode)
    for k, src in enumerate(inputs):   # In0..In{n-1}, contiguous (matches the bind order)
        g.connect(getattr(m.inputs, "In%d" % k), src.output_plug)
    return TerrainNode(m, m.outputs.Out)


def _compile_expr_source(source, n_inputs):
    """Evaluate a T.expr SOURCE STRING to a ptex3d scalar SurfNode and codegen the
    ExprModule compute shadertext. SHARED by T.expr (trace) and doc.elaborate (rebake
    recompile) so both take the identical eval+codegen path. The eval namespace is the
    fresh-ctx idiom of HeightField._eval_expr: {"ctx": SurfaceCtx(), "P": P}. Raises a
    clear TerrainDocParamError naming the problem (bad syntax / non-scalar expression /
    ctx.input(k) beyond the connected inputs) — ops self-defend, never a silent bad shader.
    Returns (shadertext, surfnode) — the SurfNode is captured to canonical ExprIR (E2.5)."""
    from ork.hypergraph.dflow.terrain.doc import TerrainDocParamError
    from ork.hypergraph.ptex3d import P
    from ork.hypergraph.ptex3d.dsl import SurfaceCtx, emit_compute_field
    from ork.hypergraph.ptex3d.compute_template import build_field_shader
    try:
        surfnode = eval(source, {"ctx": SurfaceCtx(), "P": P})  # authoring DSL string
    except Exception as e:
        raise TerrainDocParamError(
            f"T.expr: could not evaluate expression source {source!r} "
            f"({type(e).__name__}: {e}) — use ctx.input(k) / P.* over ctx.P_object.")
    try:
        lines, final, libsrcs, inherits, imports, params, in_idx = emit_compute_field(surfnode)
    except Exception as e:
        raise TerrainDocParamError(
            f"T.expr: {source!r} is not a bakeable scalar ptex3d expression "
            f"({type(e).__name__}: {e}).")
    if in_idx and max(in_idx) >= n_inputs:
        raise TerrainDocParamError(
            f"T.expr: expression references ctx.input({max(in_idx)}) but only {n_inputs} "
            f"input(s) were connected (T.expr inputs=...).")
    return build_field_shader(lines, final, libsrcs, inherits, n_inputs=n_inputs), surfnode


def _set_expr_tree(m, surfnode):
    """Capture a bake SurfNode to canonical ExprIR JSON on the ExprModule's reflected
    `expr_tree` field (E2.5 S4/S5 — the writer re-emits DSL from it, the cook hash keys off
    its bytes). Guarded on the reflected property so a pre-rebuild binary degrades cleanly
    (loud on capture failure — never a silent empty tree)."""
    if not hasattr(m, "expr_tree"):
        return
    from ork.hypergraph.ptex3d.exprir_surface import capture_json
    m.expr_tree = capture_json(surfnode)


def expr(source, inputs=(), name=None):
    """Editor-authorable EXPRESSION node: compile a ptex3d expression SOURCE STRING to a
    terrain field via the generic ExprModule. The source is evaluated with `ctx` (a fresh
    ptex3d SurfaceCtx) and `P` in scope — the SAME authoring surface as expr_field / hfbake,
    but as a STRING so it round-trips (saved .py re-emits T.expr, propsheet edits recompile).
    `inputs` are upstream TerrainNodes wired to In0..In{n-1}, read in the expression via
    ctx.input(k) (input 0 = the current height, ctx.P_object.y = in0, TRUE METERS). Both the
    compiled shadertext AND the source are recorded on the module (source drives re-authoring;
    shadertext is the bake artifact). Errors raise a clear TerrainDocParamError."""
    g = graph_or_raise("Expr")
    inputs = list(inputs)
    text, surfnode = _compile_expr_source(source, len(inputs))
    m = g.create(name or anon_name("expr", g), _terrain.ExprModule)
    m.shadertext = text
    # B1 (ExprModule.expr_source): reflected source that drives re-author + rebake recompile.
    # Feature-guarded so a pre-rebuild binary degrades to the shadertext-only path.
    if hasattr(m, "expr_source"):
        m.expr_source = source
    # E2.5: also capture the eval'd SurfNode to canonical ExprIR (the cook-hash identity).
    _set_expr_tree(m, surfnode)
    for k, src in enumerate(inputs):   # In0..In{n-1}, contiguous (matches the bind order)
        if not isinstance(src, TerrainNode):
            raise TypeError(f"T.expr input {k} must be a terrain node; got {type(src).__name__}")
        g.connect(getattr(m.inputs, "In%d" % k), src.output_plug)
    return TerrainNode(m, m.outputs.Out)


def bypass(node, flag=True):
    """Mark a node BYPASSED (structural pass-through) — the DSL / editor form of the bypass
    toggle. Sets the flag through the node's module proxy so the DOCUMENT records it (badges /
    undo / doc-JSON, validated bypassable) and it forwards to the real module; elaborate then
    forwards DocNode.bypassed onto the elaborated module, where the C++ resolveConnectedOutput
    splices it out of the dependency chain. Returns `node` so it chains. Loud on a source node
    or capture (nothing to pass through)."""
    if not isinstance(node, TerrainNode):
        raise TypeError(f"bypass expects a terrain node; got {type(node).__name__}")
    node.module.bypassed = bool(flag)
    return node


def pow(node, exponent, name=None):
    """Per-texel power: field ** exponent. Sharpens / softens a [0,1] mask (exponent > 1 pushes
    toward 0, < 1 toward 1), gamma, contrast, etc. Baked via the ptex3d expr substrate (P.pow over
    the field); `exponent` is a constant (folds into the cook hash)."""
    from ork.hypergraph.ptex3d import P
    from ork.hypergraph.ptex3d.dsl import SurfaceCtx
    if not isinstance(node, TerrainNode):
        raise TypeError(f"pow expects a terrain node; got {type(node).__name__}")
    sn = P.pow(SurfaceCtx().input(0), float(exponent))   # in0 = the input field value
    return expr_field(sn, inputs=[node], name=name)


def normalize(node, out_lo=0.0, out_hi=1.0, name=None):
    """Rescale the field's [min,max] -> [out_lo,out_hi] (default [0,1]) — the EXPLICIT,
    controllable counterpart to the bake's flush auto-exposure (put the renorm where you
    want it). GPU min/max reduction + rescale. Useful to bound an hfbake'd field, level a
    height before terracing, or pin a mask to [0,1]."""
    g = graph_or_raise("Normalize")
    if not isinstance(node, TerrainNode):
        raise TypeError(f"normalize expects a terrain node; got {type(node).__name__}")
    m = g.create(name or anon_name("normalize", g), _terrain.NormalizeModule)
    g.connect(m.inputs.In, node.output_plug)
    m.inputs.out_lo = float(out_lo)
    m.inputs.out_hi = float(out_hi)
    return TerrainNode(m, m.outputs.Out)


# --- unary -------------------------------------------------------------------

def remap(node, scale=1.0, bias=0.0, lo=0.0, hi=1.0, name=None):
    """Explicit affine remap WITH clamp: clamp(node*scale + bias, lo, hi). (The
    `*`/`+` operators use the no-clamp affine form; this is the clamping one.)"""
    return make_remap(node, scale=scale, bias=bias, lo=lo, hi=hi, name=name)


def terrace(node, step_m=100.0, sharpness=1.0, blend=1.0, name=None):
    """Quantize to plateaus `step_m` METERS apart with a `sharpness` riser (natural
    units: benches land at multiples of step_m regardless of the terrain's range).
    `blend` (0..1, default 1.0) crossfades the terraced result against the input:
    0 = passthrough, partial = softer/shallower benches. A SCALAR blend rides the
    module's RUNTIME params (params SSBO, propsheet-editable, no recompile); a
    TerrainNode (per-texel mask) routes through a post-op MaskBlend."""
    g = graph_or_raise("Terrace")
    m = g.create(name or anon_name("terr", g), _terrain.TerraceModule)
    g.connect(m.inputs.In, node.output_plug)
    m.inputs.step_m = float(step_m)
    m.inputs.sharpness = float(sharpness)
    m.inputs.blend = 1.0 if isinstance(blend, TerrainNode) else float(blend)
    out = TerrainNode(m, m.outputs.Out)
    return _blend_out(node, out, blend) if isinstance(blend, TerrainNode) else out


def clamp(node, lo=0.0, hi=1.0, name=None):
    """Clamp to [lo, hi] (Remap with unit scale)."""
    return make_remap(node, scale=1.0, bias=0.0, lo=lo, hi=hi, name=name)


# --- mask generators ("Mask by Feature") -------------------------------------
# A mask is just a [0,1] FIELD on the normal terrain plug type — no special type.
# Generators emit one; apply it compositionally with mix()/masked_by (see _node).

def slope(node, scale=1.0, radius_m=8.0, name=None):
    """Slope mask: the REAL slope (tan of the terrain angle = rise_m/run_m) of
    `node`, soft-rolled-off (Reinhard) into [0,1). High on steep faces, ~0 on flats.
    PRE-BLURRED over a `radius_m`-METER gradient baseline, so it tracks landform
    slope, not per-pixel noise — and because the param is in meters (converted to
    texels per-bake from the graph's extent), the graph is resolution-INDEPENDENT.
    `scale` tunes sensitivity (a 45deg slope reads ~0.5 at scale=1). (Mask by Feature.)"""
    g = graph_or_raise("Slope")
    if not isinstance(node, TerrainNode):
        raise TypeError(f"slope expects a terrain node; got {type(node).__name__}")
    m = g.create(name or anon_name("slope", g), _terrain.SlopeModule)
    m.radius_m = float(radius_m)
    g.connect(m.inputs.In, node.output_plug)
    m.inputs.scale = float(scale)
    return TerrainNode(m, m.outputs.Out)


# CurvatureModule._mode codes — MUST match enum CurvatureMode in hfdflow.h.
_CURV_MODE = {"convex": 0, "concave": 1, "magnitude": 2}


def curvature(node, scale=1.0, mode="magnitude", radius_m=96.0, name=None):
    """Curvature mask: a [0,1] field from the band-pass (difference-of-box ~ LoG)
    curvature of `node`, soft-rolled-off (Reinhard, no hard clamp -> magnitude
    survives). PRE-BLURRED over a `radius_m`-METER scale, so it tracks landform
    ridges/valleys, not per-pixel noise — and because the param is in meters
    (converted to texels per-bake from the graph's extent), the graph is
    resolution-INDEPENDENT. `mode`:
      "convex"    -> ridges / peaks   (e.g. snow, exposed rock)
      "concave"   -> valleys / pits   (e.g. sediment, water pooling)
      "magnitude" -> both ("where the terrain bends")
    `scale` tunes sensitivity; `radius_m` picks the curvature scale. An edge ring of
    width radius_m is 0 (curvature is undefined at the border). (Mask by Feature.)"""
    g = graph_or_raise("Curvature")
    if not isinstance(node, TerrainNode):
        raise TypeError(f"curvature expects a terrain node; got {type(node).__name__}")
    if mode not in _CURV_MODE:
        raise ValueError(f"curvature mode must be one of {sorted(_CURV_MODE)}; got {mode!r}")
    m = g.create(name or anon_name("curv", g), _terrain.CurvatureModule)
    m.mode = _CURV_MODE[mode]
    m.radius_m = float(radius_m)
    g.connect(m.inputs.In, node.output_plug)
    m.inputs.scale = float(scale)
    return TerrainNode(m, m.outputs.Out)


# --- mask generators ("Mask by Elevation / Value") ---------------------------
# Masks on the VALUE of the field (vs slope/curvature = mask by feature). These
# compose to Remap (the clamped ramp) + Combine(MUL) — no new module, JIT-only.

def smoothstep(node, e0, e1, name=None):
    """Hermite smoothstep mask: 0 where value <= e0, 1 where value >= e1, a smooth
    S-curve between (= GLSL smoothstep(e0,e1,value)). The single soft EDGE primitive —
    a rising elevation threshold. e.g. `hi = T.smoothstep(z, 0.52, 0.59)` is the terrain
    analog of the shader's `aa_band(h01, lo, hi)` rising edge (one side of `sels[6]`).
    [e0,e1] is the transition width; flip them (e0>e1) for a falling edge."""
    e0 = float(e0); e1 = float(e1)
    if e0 == e1:
        raise ValueError("smoothstep: e0 and e1 must differ (zero-width edge)")
    inv = 1.0 / (e1 - e0)
    # t = clamp((value - e0)/(e1 - e0), 0, 1); return t*t*(3 - 2t)
    t = make_remap(node, scale=inv, bias=-e0 * inv, lo=0.0, hi=1.0, name=name)
    return t * t * (t * -2.0 + 3.0)


def band(node, lo, hi, soft=0.02, name=None):
    """Soft VALUE band mask ~ the shader's `aa_band` / one slice of `aa_bands`: ~1 where
    lo <= value <= hi, rolling off over `soft` on each edge, ~0 outside. The direct terrain
    equivalent of `sels[k]` (mask the high ground, a mid elevation belt, etc.):

        hi_ground = T.band(z, 0.555, 1.0, soft=0.03)          # == shader sels[TOP] (open top)
        z = T.lpf(z, cutoff=256, units='meters', blend=hi_ground)  # smooth ONLY the high band

    hi >= 1.0 (or lo <= 0.0) drops the corresponding edge — an open-topped/bottomed band,
    exactly how `aa_bands`' first/last slices behave. Pair with a filter's `blend=<field>`
    (lpf/erox/terrace/erode_*/basin_* all accept a field mask) to confine the op."""
    lo = float(lo); hi = float(hi); soft = max(float(soft), 1e-4)
    rise = smoothstep(node, lo - soft, lo + soft, name=name) if lo > 0.0 else None
    fall = smoothstep(node, hi - soft, hi + soft) if hi < 1.0 else None
    if rise is None and fall is None:
        return make_const(1.0, name=name)        # whole range -> all-ones mask
    if fall is None:
        return rise                              # open top (== sels[TOP])
    if rise is None:
        return 1.0 - fall                        # open bottom (== sels[0])
    return rise - fall


# --- erosion -----------------------------------------------------------------

def _blend_out(inp, out, blend):
    """Crossfade an op's OUTPUT against its INPUT terrain: blend=1.0 (default) -> full op,
    0.0 -> passthrough, scalar between -> partial strength. `blend` may also be a TerrainNode
    (per-texel mask). Adds NO extra node when blend is the scalar 1.0. Implemented via T.mix,
    whose scalar factor is a runtime param (no per-value shader recompile)."""
    if isinstance(blend, TerrainNode):
        return mix(inp, out, blend)
    return out if float(blend) >= 1.0 else mix(inp, out, float(blend))


def erode_thermal(node, *packs, iterations=40, blend=1.0, name=None, **overrides):
    """THERMAL (talus) erosion — the slope-relaxation kind. Relax slopes toward the angle of repose over `iterations` steps
    -> scree/talus, softened ridges, filled hollows. Mass-conserving (material moves
    downhill, total height preserved). The PLUGS are `talus_deg` (PHYSICAL angle of
    repose, resolution-independent via the meters model) and `rate` (per-step
    relaxation, keep <~0.25 for stability) — supply them via ParamPacks and/or keyword
    overrides, e.g. `T.erode_thermal(h, ERO_P1, iterations=50)` or
    `T.erode_thermal(h, talus_deg=30, rate=0.2)`. `iterations` is the baked step count
    (keyword-only). The single op ping-pongs two buffers internally across the steps."""
    g = graph_or_raise("ThermalErode")
    if not isinstance(node, TerrainNode):
        raise TypeError(f"erode_thermal expects a terrain node; got {type(node).__name__}")
    m = g.create(name or anon_name("erode", g), _terrain.ThermalErodeModule)
    g.connect(m.inputs.In, node.output_plug)
    m.iterations = max(1, int(iterations))     # baked scalar (not a plug)
    _apply_packs(m, packs, overrides)          # talus_deg / rate from packs + overrides
    return _blend_out(node, TerrainNode(m, m.outputs.Out), blend)


def erox(node, *packs, erodibility=None, blend=1.0, name=None, **overrides):
    """PHYSICAL hydraulic erosion (Mei et al. virtual-pipes) in METERS / SECONDS, so the
    bake is RESOLUTION-INDEPENDENT. Heights are TRUE METERS; cell_size_m = extent_m/dim
    (real slope = rise_m/run_m); the timestep dt is CFL-derived
    (dt = 0.5*cell_size_m/flow_speed_max_mps) so iterations = ceil(sim_time_s/dt) scale with
    dim to hold the SAME physical time + diffusion, and every RATE is *dt -> the result
    converges across resolution. Unlike a stochastic particle sim, this is a
    DETERMINISTIC Eulerian field solver (the Houdini-grade primary; layers/materials later).
    All knobs are PHYSICAL (hashed dim-free):
      sim_time_s            total simulated erosion time (the duration knob, NOT iterations)
      rain_mps              precipitation rate (water depth added per second)
      evaporation_per_s     water removal time-constant
      flow_speed_max_mps    expected max water speed; sets the CFL dt bound (stability)
      capacity_Kc           sediment carrying-capacity coeff (C = Kc*sin(slope)*|velocity|)
      erosion_rate_per_s    dissolve rate (terrain -> suspended sediment when under capacity)
      deposition_rate_per_s settle rate (sediment -> terrain when over capacity)
      creep_m2ps            hillslope creep — a PHYSICAL diffusivity (m^2/s)
      bed_clamp_frac        per-step bed change as a fraction of the LOCAL RELIEF
      creep_max_cells       creep smoothing-length CEILING, in cells
    Cost is ~O(dim^3) for fixed sim_time_s (iterations grow with dim); the cook cache
    amortizes re-bakes.

    STABILITY — two laws, both derived per-bake from (cell_size, sim_time_s):
      * `bed_clamp_frac` bounds one step's bed change to that fraction of the local relief
        (down = height above the lowest neighbour, up = height below the highest), the same
        guard flow_erode carries. It is what makes the grid-axis 2-cell mode DECAY rather
        than ring — a checkerboard peak may only carve, a pit may only fill — so creep no
        longer has to double as the anti-checkerboard stabilizer. 0 disables the limit.
      * `creep_max_cells` caps the EFFECTIVE creep so one pass never diffuses further than
        that many CELLS: a physical diffusivity smooths the same number of METERS at every
        resolution, which erases everything a fine grid could resolve. Coarse grids sit
        under the cap and keep the authored creep verbatim; raise it for a softer, more
        resolution-independent result, lower it for sharper rills on a fine grid.

    `erodibility` is an OPTIONAL per-cell FIELD (TerrainNode) multiplying erosion_rate_per_s
    cell-by-cell — the stratigraphy hook: hard beds resist and stand out as ledges while soft
    beds cut back, so one layer field drives silhouette AND material. DIMENSIONLESS: 1.0 = the
    uniform rate (== leaving it unwired, bit-for-bit), 0 = armored, >1 = softer than nominal;
    clamped to [0,16]. Deposition is NOT scaled (only the carving term). e.g.

        hard = T.band(layer_phase, 0.0, 0.5, soft=0.05)          # 1 in the hard beds
        h    = T.erox(h, **EROX, erodibility=T.mix(1.0, 0.15, hard))

    PARAMS may be supplied as keyword args (e.g. capacity_Kc=2.0), via one or more
    ParamPacks (T.erox(node, pack)), or both (explicit kwargs override packs); unspecified
    params keep their defaults. Defaults: sim_time_s=60, rain_mps=0.05, evaporation_per_s=0.05,
    flow_speed_max_mps=8, capacity_Kc=1, erosion_rate_per_s=1, deposition_rate_per_s=1,
    creep_m2ps=4, bed_clamp_frac=0.5, creep_max_cells=4."""
    g = graph_or_raise("Erox")
    if not isinstance(node, TerrainNode):
        raise TypeError(f"erox expects a terrain node; got {type(node).__name__}")
    if erodibility is not None and not isinstance(erodibility, TerrainNode):
        raise TypeError(f"erox: erodibility must be a terrain node (a per-cell field); "
                        f"got {type(erodibility).__name__} — for a uniform strength use "
                        f"erosion_rate_per_s=")
    m = g.create(name or anon_name("erox", g), _terrain.EroxModule)
    g.connect(m.inputs.In, node.output_plug)
    if erodibility is not None:
        g.connect(m.inputs.Erodibility, erodibility.output_plug)
    _apply_packs(m, packs, overrides)
    return _blend_out(node, TerrainNode(m, m.outputs.Out), blend)


# Lpf cutoff units -> CutoffUnits enum code (MUST match enum CutoffUnits in hfdflow.h).
_CUTOFF_UNITS = {"texels": 0, "meters": 1}


def lpf(node, cutoff=8.0, units=None, blend=1.0, name=None):
    """Separable GAUSSIAN low-pass filter. `cutoff` is the wavelength below which features
    are attenuated (gaussian sigma = cutoff-in-texels/6, soft rolloff -> no ringing). `units`
    selects how `cutoff` is read: 'texels' (default, resolution-DEPENDENT) or 'meters'
    (RESOLUTION-INDEPENDENT — converted to texels per-bake from dim/extent in C++, exactly
    like slope/curvature's radius_m, so dim never enters the trace-time graph). You may also
    pass an E0 typed literal that carries its own unit — `cutoff=meters(64)` or
    `cutoff=texels(4)` — instead of a separate `units=`. A smoothing / hillslope-relaxation
    primitive — e.g. between erosion passes: `T.lpf(eroded, cutoff=64, units='meters') + base*uplift`.

    `blend` crossfades the filtered result against the ORIGINAL input — 0 = passthrough,
    1 = fully filtered, 0.5 = half-smoothed. It may be a SCALAR (0..1) or a per-texel FIELD
    (TerrainNode mask, e.g. an elevation/slope band -> smooth only there). A scalar rides the
    module's RUNTIME params (params SSBO) — sweeping cutoff/blend does NOT recompile (only the
    loop radius bakes, bucketed to next pow2, so a whole cutoff sweep shares one shader). A
    FIELD routes through a post-filter MaskBlend (the module runs full-strength, then mix(in,
    filtered, mask)) — same path every other op's `blend=<field>` takes."""
    g = graph_or_raise("Lpf")
    if not isinstance(node, TerrainNode):
        raise TypeError(f"lpf expects a terrain node; got {type(node).__name__}")
    # E0 typed literal: cutoff=meters(64)/texels(4) carries its unit -> the units enum. A
    # units= that disagrees is a LOUD conflict (pass one, not both).
    tag = _unit_of(cutoff)
    if tag is not None:
        if tag not in _CUTOFF_UNITS:
            raise TypeError(f"lpf: cutoff unit {tag!r} is not valid for a low-pass cutoff — "
                            f"use meters(...) or texels(...)")
        if units is not None and units != tag:
            raise TypeError(f"lpf: cutoff={tag}(...) disagrees with units={units!r}; pass ONE "
                            f"(drop units= when the cutoff is a typed literal)")
        units = tag
    if units is None:
        units = "texels"
    if units not in _CUTOFF_UNITS:
        raise TypeError(f"lpf: units={units!r} invalid — use 'texels' or 'meters'")
    m = g.create(name or anon_name("lpf", g), _terrain.LpfModule)
    g.connect(m.inputs.In, node.output_plug)
    m.inputs.cutoff = float(cutoff)        # preserves L.i identity (same float() claim path)
    m.cutoff_units = _CUTOFF_UNITS[units]  # reflected enum on the module
    # FIELD blend: filter full-strength, crossfade by the mask field (_blend_out -> MaskBlend).
    # SCALAR blend: stays the cheap internal runtime param (no extra node, runtime-bindable).
    m.inputs.blend = 1.0 if isinstance(blend, TerrainNode) else float(blend)
    out = TerrainNode(m, m.outputs.Out)
    return _blend_out(node, out, blend) if isinstance(blend, TerrainNode) else out


def basin_fill(node, epsilon=0.0, blend=1.0, name=None):
    """BASIN FILL (only) — raise every closed depression / pit / sink to its spill (pour)
    point so every cell has a downhill path to the boundary (no interior local minima remain;
    filled basins become flat lakes). The rest of the terrain is untouched. Priority-Flood
    (Barnes 2014), exact + one pass — a CPU op (reads the field back, floods, writes; cost
    ~O(n log n), so heavy at very high dim). `epsilon` (METERS, default
    0 = flat fill) adds a tiny per-step drainage gradient so filled flats still route to the
    outlet. Use to pre-condition terrain for flow-based erosion, fill spurious pits between
    uplift+erode passes, or carve lakes. `blend` (0..1, default 1.0) crossfades filled vs
    original: a SCALAR is a module param (propsheet-editable, partial fills); a TerrainNode
    mask routes through a post-op MaskBlend."""
    g = graph_or_raise("BasinFill")
    if not isinstance(node, TerrainNode):
        raise TypeError(f"basin_fill expects a terrain node; got {type(node).__name__}")
    m = g.create(name or anon_name("basin", g), _terrain.BasinFillModule)
    g.connect(m.inputs.In, node.output_plug)
    m.inputs.epsilon = float(epsilon)
    m.inputs.blend = 1.0 if isinstance(blend, TerrainNode) else float(blend)
    out = TerrainNode(m, m.outputs.Out)
    return _blend_out(node, out, blend) if isinstance(blend, TerrainNode) else out


from collections import namedtuple as _namedtuple
Flow3DResult = _namedtuple("Flow3DResult", ["dir", "discharge", "metrics"])  # three TerrainNodes
# fill_closed_basins outputs: .filled dem + two RGBA basin-info images.
FillBasinsResult = _namedtuple("FillBasinsResult", ["filled", "basin", "center_pit"])
# relax_uv outputs: .uv (RGBA: relaxed uv.xy + normal.x,z) and .binormal (RGBA: relaxed binormal.xyz + 1).
RelaxUvResult = _namedtuple("RelaxUvResult", ["uv", "binormal"])


def flow3d(node, exponent=1.1, iterations=0, log_compress=True, slope_scale=10.0,
           flat_scale=8.0, curv_scale=80.0, twi_scale=24.0, name=None):
    """CONTINUOUS FLOW FIELD — emits THREE outputs from one module. Returns
    Flow3DResult(dir, discharge, metrics), all TerrainNodes:
      .dir       — RGBA: R,G = downhill flow direction (continuous unit −∇z, *0.5+0.5, ANY angle,
                   NOT D8); B = PHYSICAL slope (rise/run, resolution-invariant, scaled by
                   `slope_scale`); A=1. Feed continuous erosion or view as colour.
      .discharge — mono MFD drainage area (log(1+area) default; raw via log_compress=False).
      .metrics   — RGBA derived terrain metrics: R = flatness (1/(1+slope*flat_scale), highlights
                   basins/plateaus/valley-floors), G = curvature (physical ∇²z; 0.5=flat, >0.5
                   concave valley, <0.5 convex ridge; gain `curv_scale`), B = wetness/TWI
                   (clamp(ln(A/slope)/twi_scale): glows in wet valley floors).
    Direction/slope/curvature are a continuous gradient (resolution-invariant); discharge is the
    MFD Holmgren gather. Substrate for continuous transport-limited erosion+deposition.
    (Watershed segmentation is NOT here; flow3d is the continuous gradient/discharge field only.)

        f = T.flow3d(T.basin_fill(h))
        self.capture(f.dir, "flowdir"); self.capture(f.discharge, "discharge"); self.capture(f.metrics, "metrics")
    """
    g = graph_or_raise("Flow3D")
    if not isinstance(node, TerrainNode):
        raise TypeError(f"flow3d expects a terrain node; got {type(node).__name__}")
    m = g.create(name or anon_name("flow3d", g), _terrain.Flow3DModule)
    g.connect(m.inputs.In, node.output_plug)
    m.exponent = float(exponent)
    m.iterations = int(iterations)
    m.log_compress = bool(log_compress)
    m.slope_scale = float(slope_scale)
    m.flat_scale = float(flat_scale)
    m.curv_scale = float(curv_scale)
    m.twi_scale = float(twi_scale)
    return Flow3DResult(dir=TerrainNode(m, m.outputs.Out),
                        discharge=TerrainNode(m, m.outputs.Discharge),
                        metrics=TerrainNode(m, m.outputs.Metrics))


def relax_uv(node, strength=1.0, iterations=0, name=None):
    """EQUAL-AREA UV RELAXATION (the slope-stretch fix). ONE module, TWO RGBA outputs:
      .uv       — RGBA: R,G = relaxed UV in [0,1] (warped so texels/PHYSICAL-area is ~uniform; the
                  unit-square boundary stays pinned so UVs never leave [0,1]); B,A = geometric normal
                  x,z (n.y reconstructed +sqrt in the VS — parameterization-invariant).
      .binormal — RGBA: x,y,z = the RELAXED binormal (dP/du of the relaxed UV, orthonormal to the
                  normal — the one frame axis the relaxation perturbs); A=1.
    The chunk VS reads .uv into uv0 and the precomputed normal/binormal frame (tangent = cross(N,B)),
    dropping the live finite-diff taps. `strength` is the warp gain (0 = planar UV); `iterations` the
    Poisson Jacobi sweeps (0 = auto ~ 4*dim). Pure function of the height -> cook-cached.

        r = T.relax_uv(h, strength=1.0)
        self.capture(r.uv, "relaxed_uv"); self.capture(r.binormal, "binormal")
    """
    g = graph_or_raise("RelaxUv")
    if not isinstance(node, TerrainNode):
        raise TypeError(f"relax_uv expects a terrain node; got {type(node).__name__}")
    m = g.create(name or anon_name("relaxuv", g), _terrain.RelaxUvModule)
    g.connect(m.inputs.In, node.output_plug)
    m.strength = float(strength)
    m.iterations = int(iterations)
    return RelaxUvResult(uv=TerrainNode(m, m.outputs.Out),
                         binormal=TerrainNode(m, m.outputs.Binormal))


def flow_erode(node, discharge, niter=1, dt=1.0, k_erode=0.02, k_deposit=0.02, m=0.5, n=1.0,
               dep_m=0.5, flat_k=8.0, clamp_frac=0.5, disch_log=True, blend=1.0, name=None):
    """CONTINUOUS (MFD/vector-field) erosion+deposition iteration driven by a flow map — a
    tree-free transport-limited erosion (no SFD, no implicit solve). Takes the heightfield `node` and a
    `discharge` field (A — e.g. T.flow3d(filled).discharge). Per step (slope +
    flatness recomputed from the CURRENT z, physical units): erode by stream power k_erode·Aᵐ·Sⁿ,
    deposit in flat high-flow cells k_deposit·flatness·A^dep_m; the per-cell change is CLAMPED to
    ±clamp_frac·(local relief) so a cell can never invert -> unconditionally bounded (no implicit
    solve, no spikes). Boundary held fixed (base level).

    Per cell, per step:  slope=|grad z_m|/texel_m (heights are TRUE METERS);  flatv=1/(1+slope*flat_k);
    A=discharge (exp() if disch_log);  erode=dt*k_erode*A^m*slope^n;  deposit=dt*k_deposit*flatv*A^dep_m;
    dz=clamp(deposit-erode, +/- clamp_frac*local_relief);  z+=dz  (boundary held fixed).

    ARGS / how to tune (z is METERS and A is m^2; clamp_frac*local_relief is the
    DOMINANT per-step magnitude; dt/k_* mostly set WHICH cells erode vs deposit — use dt~1, not <<1):
      niter      : internal steps with A HELD FIXED (cheap, A goes stale). Leave 1 and loop
                   flow3d->flow_erode in the DSL so A is recomputed as channels deepen.
      dt         : overall step magnitude. TOO SMALL (e.g. 0.01) => erode falls below the clamp =>
                   nothing happens. Use ~1.
      k_erode    : incision strength (deeper valleys).            ~0.05-0.3
      k_deposit  : fill strength in flats/valleys; 0 = pure incision (crispest).  0 .. 0.05
      m          : erosion area exponent; higher = incision concentrates in big rivers.  0.4-0.6
      n          : erosion slope exponent; higher = sharper canyons.  1-2
      dep_m      : deposition area exponent.  ~0.5
      flat_k     : how flat to receive deposit; higher = only very flat valley floors.  4-16
      clamp_frac : MASTER amount-per-step knob (fraction of local relief); also the stability guard.
                   lower=gentle/smooth, higher(->1)=aggressive.  0.3-0.7
      disch_log  : True if discharge is log(1+A) (T.flow3d default) -> exp() to raw A.
      blend      : crossfade eroded result vs ORIGINAL input (0..1, default 1 = full erosion).
                   A SCALAR is a module param (propsheet-editable, RUNTIME params SSBO);
                   a TerrainNode mask routes through a post-op MaskBlend.

    Crisp dendritic incision recipe (chain in the DSL, ~30 iters):
        z = T.basin_fill(z)
        for _ in range(30):
            z = T.flow_erode(z, T.flow3d(z).discharge, dt=1.0, k_erode=0.15, k_deposit=0.0,
                             m=0.5, n=1.0, clamp_frac=0.5)
    sharper canyons: n=2,k_erode=0.2 | dendritic contrast: m=0.6 | fans/fill: k_deposit=0.03,flat_k=6
    | gentler: clamp_frac=0.25 + more iters."""
    g = graph_or_raise("FlowErode")
    if not isinstance(node, TerrainNode):
        raise TypeError(f"flow_erode expects a terrain node for z; got {type(node).__name__}")
    if not isinstance(discharge, TerrainNode):
        raise TypeError(f"flow_erode expects a terrain node for discharge; got {type(discharge).__name__}")
    mod = g.create(name or anon_name("flowerode", g), _terrain.FlowErodeModule)
    g.connect(mod.inputs.In, node.output_plug)
    g.connect(mod.inputs.Discharge, discharge.output_plug)
    mod.niter = int(niter); mod.dt = float(dt)
    mod.k_erode = float(k_erode); mod.k_deposit = float(k_deposit)
    mod.m = float(m); mod.n = float(n); mod.dep_m = float(dep_m)
    mod.flat_k = float(flat_k); mod.clamp_frac = float(clamp_frac); mod.disch_log = bool(disch_log)
    mod.blend = 1.0 if isinstance(blend, TerrainNode) else float(blend)
    out = TerrainNode(mod, mod.outputs.Out)
    return _blend_out(node, out, blend) if isinstance(blend, TerrainNode) else out


def fill_closed_basins(node, min_depth=0.0, name=None):
    """Detect + fill CLOSED basins (regions where water enters but can't exit, excluding
    evaporation) with PERSISTENCE control — so nested basins don't all collapse to one level
    the way plain basin_fill does. Self-contained (CPU sorted-cell union-find = the watershed
    MERGE TREE; handles flats from terrace/lpf). Each local min is a basin (pit = its floor);
    where a cell joins two basins it's their SADDLE (pour point) and the shallower basin merges
    into the deeper. The MAP EDGE is an OUTLET, not a wall: a depression that can spill off the
    edge is open, not closed — so edge-draining regions never count.

    Returns a FillBasinsResult namedtuple (filled, basin, center_pit):
      .filled      mono: the dem with each closed basin raised to its (persistence-simplified)
                   pour point; edge-draining cells left at z. (basin_fill, but level-controlled.)
      .basin       RGBA: R=spill(pour elev) G=depth(spill-z) B=per-basin shade A=closed mask.
      .center_pit  RGBA: RGB=3D offset (meters) from the cell to its basin PIT (deepest cell) A=dist.

    min_depth (D, METERS): keep only basins whose persistence (pour - pit) >= D;
    shallower sub-basins merge into their parent. 0 = finest (every pit a basin); larger = coarser
    (the dial that fixes basin_fill's uncontrolled nesting). Independent of upstream basin_fill."""
    g = graph_or_raise("FillClosedBasins")
    if not isinstance(node, TerrainNode):
        raise TypeError(f"fill_closed_basins expects a terrain node; got {type(node).__name__}")
    m = g.create(name or anon_name("fcb", g), _terrain.FillClosedBasinsModule)
    g.connect(m.inputs.In, node.output_plug)
    m.inputs.min_depth = float(min_depth)
    return FillBasinsResult(filled=TerrainNode(m, m.outputs.Out),
                            basin=TerrainNode(m, m.outputs.Basin),
                            center_pit=TerrainNode(m, m.outputs.CenterPit))


def pha(node, *packs, octaves=5, blend=1.0, name=None, **overrides):
    """PROCEDURAL "phacelle" erosion FILTER (Rune Skovbo Johansen's "fast and gorgeous
    erosion", MPL-2.0). Unlike erox (an iterative Mei SIM), this is a SINGLE-PASS analytic
    filter: it stacks `octaves` of slope-aligned "faded gully" noise to carve drainage-like
    gullies along the downslope. Being a smooth function of continuous uv, it is naturally
    resolution-INDEPENDENT, speckle-FREE, fast (one dispatch), and predictable. Knobs:
      strength       overall erosion magnitude (affects all octaves + gully directions)
      gully_weight   gully magnitude [0,1] (0 = sharpen peaks/valleys, 1 = full gullies)
      detail         higher-freq gullies restricted to steeper slopes at lower values
      scale          horizontal+vertical scale of the erosion effect
      cell_scale     phacelle cell size relative to scale (~1 best; smaller=grainier)
      normalization  phacelle magnitude normalization [0,1]
      lacunarity     per-octave frequency multiplier
      gain           per-octave magnitude multiplier
      default_height mid-height reference (fadeTarget 0), METERS
      fade_width     valley<->peak fade window around default_height, METERS (default 600)
      octaves        baked gully-octave count (NOT a plug; stays an explicit kwarg)
    Float params accept kwargs and/or ParamPacks (explicit kwargs override packs); defaults:
    strength=0.22, gully_weight=0.5, detail=1.5, scale=0.15, cell_scale=0.7, normalization=0.5,
    lacunarity=2.0, gain=0.5, default_height=0.5, fade_width=600.0.
    `blend` (0..1 scalar OR a per-texel TerrainNode field mask) crossfades the filtered result
    against the input — 0 = passthrough, 1 = full effect, a field = erode only where it's high
    (same path every process op's `blend` takes; a field routes through MaskBlend)."""
    g = graph_or_raise("Pha")
    if not isinstance(node, TerrainNode):
        raise TypeError(f"pha expects a terrain node; got {type(node).__name__}")
    m = g.create(name or anon_name("pha", g), _terrain.PhaModule)
    m.octaves = max(1, int(octaves))  # baked scalar (not a plug) -> stays explicit
    g.connect(m.inputs.In, node.output_plug)
    _apply_packs(m, packs, overrides)
    return _blend_out(node, TerrainNode(m, m.outputs.Out), blend)


# --- in-graph scatter placement + building pads ------------------------------

from collections import namedtuple as _nt
# ScatterPlaceResult — the ScatterPlaceModule's three outputs. `pad_mask`/`pad_elev`
# feed a stock MaskBlend (height' = mix(height, pad_elev, pad_mask)); `height` is the
# input passthrough (so scatter_place composes). The .ogeo placement artifact is
# exported by the module at bake (deterministic <assetcache>/terrain/<asset>/<export_name>.ogeo).
ScatterPlaceResult = _nt("ScatterPlaceResult", ["pad_mask", "pad_elev", "height"])

_TAU_OPS = 6.283185307179586
_COLL_KINDS = {"sphere": 0, "capsule": 1, "box": 2, "cone": 3, "ring": 4}


def scatter_place(height, *, types=None, mask=None, export_name=None, density=None, count=None,
                  seed=0, align="up", yaw=(0.0, _TAU_OPS), scale=(1.0, 1.0), cutoff=0.0,
                  jitter=1.0, max_points=6000000, lift=0.0, apron_m=8.0, footprints=None,
                  yaw_from_field=None, colliders=None, assets=None, materials=None,
                  lattice_m=0.0, lane_every=0, lane_m=0.0, yaw_mode="hash",
                  cluster_pads=False, cluster_step_m=0.0, max_seam_m=1.0, name=None):
    """IN-GRAPH scatter placement + cut-and-fill building PADS (terrain family). One
    deterministic CPU module places against the PRE-flatten `height` (+ the per-type weight
    fields) and emits PadMask/PadElev; a stock MaskBlend then flattens the terrace under each
    footprint BEFORE the captures, so placement can never drift from its own pads (the fixpoint
    trap post-bake rasterization falls into). Returns ScatterPlaceResult(pad_mask, pad_elev, height):

        h = T.fbm(...) * 200.0
        place = T.scatter_place(h, export_name="buildings", density=0.001,
                                types={"house": flat_mask}, apron_m=10.0,
                                footprints={"house": (6.0, 8.0)},
                                colliders={"house": ("box", 6.0, 4.0, 8.0)})
        h = T.mix(h, place.pad_elev, place.pad_mask)   # flatten under the pads
        self.capture(h, "height")

    The module ALSO exports the ScatterSet .ogeo itself (positions/xforms carry the PAD
    elevation) — consumers (BulletShapeScatter / instance resolution) read it by export_name.

      height        : the pre-flatten terrain node the placer samples (TRUE METERS).
      types / mask  : {name: weight TerrainNode} (mutually exclusive weighted pick), or a
                      single mask (1 type). type_id = declaration index (== the W0.. wire order).
      export_name   : MANDATORY — the .ogeo artifact name consumers read (<export_name>.ogeo).
      density/count : placement amount (exactly one) — points/m^2, or total points.
      apron_m       : pad feather width (meters) around each footprint (smoothstep falloff).
      footprints    : {name: (hx, hz)} pad half-extents (meters). A missing type gets a tiny pad.
      align         : "up" (default; buildings sit flat on their pad) or "normal".
      yaw           : (lo,hi) random yaw. IGNORED when yaw_from_field is wired.
      yaw_from_field: optional TerrainNode — base yaw = hash(per-point field sample) -> a heading
                      (e.g. a worley cell-id field: one heading per cell). Truncating-nearest sample.
      colliders     : {name: ("sphere",r)|("capsule",r,h)|("box",x,y,z)|("cone",r,h)} per-item proxy.
      scale/cutoff/jitter/seed/max_points/lift : as T scatter() (placement RNG is res-independent).

    v2 aggregation controls (all default OFF -> byte-identical to v1):
      lattice_m     : >0 snaps candidates to a village-yaw-aligned grid of this pitch (m)
                      BEFORE the mask kill; same-cell candidates dedup to the strongest
                      weight -> abutment chains + block rows (real pueblo party walls).
      lane_every    : int N -> every Nth grid line widens its gap by lane_m (lanes between rows).
      lane_m        : lane gap width (m) added at each Nth grid line.
      yaw_mode      : "hash" (default) hashes the yaw_from_field value -> heading; "direct"
                      treats the field value AS radians (field-composed contour/facade align).
      cluster_pads  : True unions intersecting footprints into components with ONE grade plane
                      each (member P.y rewritten), rejecting late candidates that would seam
                      > max_seam_m against an admitted overlapping component (kills cross-level
                      party-wall interpenetration). Replaces the v1 per-texel max-coverage rule.
      cluster_step_m: >0 permits ONE terrace step when a component's natural spread exceeds it
                      (default 0 = single plane per component).
      max_seam_m    : max grade step (m) an admitted overlapping component tolerates (default 1.0).
    """
    g = graph_or_raise("ScatterPlace")
    if not isinstance(height, TerrainNode):
        raise TypeError(f"scatter_place expects a terrain node for height; got {type(height).__name__}")
    if not export_name:
        raise ValueError("scatter_place: export_name= is MANDATORY (consumers read the .ogeo by name)")
    if (density is None) == (count is None):
        raise ValueError("scatter_place: pass exactly one of density= (points/m^2) or count= (total points)")
    if (types is None) == (mask is None):
        raise ValueError("scatter_place: pass exactly one of types={name: weight} or mask=<TerrainNode>")
    if mask is not None:
        types = {"_": mask}
    type_list = list(types.items())
    if len(type_list) > 16:
        raise ValueError(f"scatter_place: at most 16 types (got {len(type_list)})")
    for tname, w in type_list:
        if not isinstance(w, TerrainNode):
            raise TypeError(f"scatter_place type {tname!r} weight must be a TerrainNode; got {type(w).__name__}")
    declared = {str(t) for (t, _w) in type_list}

    m = g.create(name or anon_name("scatterplace", g), _terrain.ScatterPlaceModule)
    g.connect(m.inputs.Height, height.output_plug)
    for k, (_tname, w) in enumerate(type_list):           # W0..W{K-1}, contiguous (type_id = index)
        g.connect(getattr(m.inputs, "W%d" % k), w.output_plug)
    if yaw_from_field is not None:
        if not isinstance(yaw_from_field, TerrainNode):
            raise TypeError(f"scatter_place yaw_from_field must be a TerrainNode; got {type(yaw_from_field).__name__}")
        g.connect(m.inputs.YawField, yaw_from_field.output_plug)

    m.seed = int(seed)
    m.align = str(align)
    m.yaw_lo = float(yaw[0]); m.yaw_hi = float(yaw[1])
    m.scale_lo = float(scale[0]); m.scale_hi = float(scale[1])
    m.cutoff = float(cutoff); m.jitter = float(jitter)
    m.max_points = int(max_points); m.lift = float(lift)
    m.apron_m = float(apron_m)
    m.lattice_m = float(lattice_m); m.lane_every = int(lane_every); m.lane_m = float(lane_m)
    m.yaw_mode = str(yaw_mode)
    m.cluster_pads = bool(cluster_pads)
    m.cluster_step_m = float(cluster_step_m); m.max_seam_m = float(max_seam_m)
    m.export_name = str(export_name)
    if density is not None:
        m.density = float(density)
    else:
        m.count = int(count)
    m.type_names = [str(t) for (t, _w) in type_list]

    def _foot(d):
        out = {}
        for tname, fp in (d or {}).items():
            if str(tname) not in declared:
                raise ValueError(f"scatter_place footprints references unknown type {tname!r}")
            hx, hz = (float(fp[0]), float(fp[1])) if len(fp) >= 2 else (float(fp[0]), float(fp[0]))
            out[str(tname)] = "%g:%g" % (hx, hz)
        return out
    m.type_footprints = _foot(footprints)

    coll_out = {}
    for tname, cspec in (colliders or {}).items():
        if str(tname) not in declared:
            raise ValueError(f"scatter_place colliders references unknown type {tname!r}")
        kind = _COLL_KINDS.get(str(cspec[0]).lower())
        if kind is None:
            raise ValueError("scatter_place collider kind must be sphere/capsule/box/cone")
        dims = [float(x) for x in cspec[1:]] + [0.0, 0.0, 0.0]
        coll_out[str(tname)] = "%d:%g:%g:%g" % (kind, dims[0], dims[1], dims[2])
    m.type_colliders = coll_out

    def _names(d, what):
        out = {}
        for tname, a in (d or {}).items():
            if str(tname) not in declared:
                raise ValueError(f"scatter_place {what} references unknown type {tname!r}")
            nm = a if isinstance(a, str) else getattr(getattr(a, "gendata", None), "asset_name", "")
            if not nm:
                raise ValueError(f"scatter_place {what}[{tname!r}] needs an asset wrapper or name string")
            out[str(tname)] = nm
        return out
    m.type_assets = _names(assets, "assets")
    m.type_materials = _names(materials, "materials")

    return ScatterPlaceResult(pad_mask=TerrainNode(m, m.outputs.PadMask),
                              pad_elev=TerrainNode(m, m.outputs.PadElev),
                              height=TerrainNode(m, m.outputs.Out))


# --- binary (join) -----------------------------------------------------------

def mix(a, b, t=0.5, name=None):
    """Linear blend mix(a, b, t). `t` may be a SCALAR (uniform blend, Combine MIX)
    or a FIELD/TerrainNode (per-texel masked blend, MaskBlend). Scalar a/b operands
    auto-wrap as Const."""
    if isinstance(t, TerrainNode):
        return make_maskblend(a, b, t, name=name)
    return make_combine(a, b, OP_MIX, t=t, name=name)


def minimum(a, b, name=None):
    """Per-texel min(a, b). (Aliased T.Min.)"""
    return make_combine(a, b, OP_MIN, name=name)


def maximum(a, b, name=None):
    """Per-texel max(a, b). (Aliased T.Max.)"""
    return make_combine(a, b, OP_MAX, name=name)
