###############################################################################
# hypermesh — GPU mesh compute-dataflow DSL (v2: INDEXED attribute mesh).
#
# Compose the C++ hypermesh modules (RipplePrimitive / Box / SubdivideModule, each a DgModuleData
# owning its runtime shadlang compute) into a dflow.GraphData. materialize() bakes the graph
# (DgSorter + GraphInst + pow2 SSBO pool) to a GpuMesh = an INDEXED polygon mesh: vertex-domain
# channels (P/N/B/uv/color, shared verts) + topology (vidx corner->vert + CSR face_offsets, mixed
# tri/quad/ngon) + face-domain attrs. make_drawable() renders it through a ptex3d material: a small
# on-GPU compute fan-triangulates the faces -> tri index buffer -> DrawIndexedIndirect, and the
# FWD_SSBO_CUSTOM pull VS reads vert attrs by gl_VertexID (== gl_VertexIndex, index-aware).
#
#   from ork.hypergraph.dflow import hypermesh as H
#   m = H.Hypermesh(); n = m.ripple(grid=64); m.output(m.subdivide(n))
#   live = m.materialize_live(ctx); cdd, mtl = H.make_drawable(live, ctx, animated=True)
###############################################################################
from enum import IntEnum
from orkengine.core import dataflow as _dflow
from orkengine.core import CrcStringProxy as _CrcStringProxy
from orkengine import lev2 as _lev2

# named fx-pipeline provider tokens (fx_pipeline.cpp FxPipelineNamedParamProviders). A shader_param
# whose value is one of these is fed PER-FRAME by the engine, not baked: _tokens.RCFD_TIME = the scene
# clock. Used as a vertex-displace param default so a clock-driven displace needs no per-frame host code.
_tokens = _CrcStringProxy()

# vertex-channel semantic ids (match the C++ MeshChannel enum order)
P, N, B, UV, COLOR = 0, 1, 2, 3, 4


class Archetype(IntEnum):
  """L-system growth model — SELECTS a preset grammar emitter (GR1.d): the four stock
  growth models are combinator-DSL grammars in ork.hypergraph.dflow.lsystem.presets
  (keep in sync with presets.SYMPODIAL/CONIFER/SAGUARO/OCOTILLO), derived by the C++
  LRuleSet evaluator. There is no C++ archetype enum anymore — species are data."""
  SYMPODIAL = 0   # repeated forking — trees, shrubs, cholla
  CONIFER   = 1   # monopodial leader + whorls of drooping laterals
  SAGUARO   = 2   # columnar trunk + arms that curl up (children=0 -> barrel)
  OCOTILLO  = 3   # many basal whips splaying out


class LeafStyle(IntEnum):
  """Leaf-card geometry — keep in sync with the C++ LeafScatterModuleData::_style (hmdflow.h)."""
  SINGLE = 0   # one quad per leaf (cheapest; alpha-mask / A2C gives the silhouette)
  CROSS  = 1   # two perpendicular quads per leaf (fuller, holds up at grazing angles)


class LeafSource(IntEnum):
  """WHERE the organ placements come from — keep in sync with C++ LeafScatterModuleData::_source."""
  NODES = 0   # phyllotaxis on skeleton nodes passing the min_gen gate (the default)
  SLOTS = 1   # the grammar's own SLOT ops (XfNodeGraph._slots) — instance_at_slots


# per-archetype known-good chaos-channel defaults (effective chaos = jitter * jit_X) — the
# data lives WITH the preset emitters (lsystem/presets.py PRESET_JIT, int-keyed; IntEnum keys
# hash-equal). A jit_* kwarg left None picks the value for that archetype here.
from ork.hypergraph.dflow.lsystem.presets import PRESET_JIT as _ARCH_JIT  # noqa: E402

# the selection DSL (SelExpr atoms/builders + MaskOp factories) — re-export so assets can write
#   from ork.hypergraph.dflow.hypermesh import S, sel_normal_dir, group, replace, add, POLY
from ork.hypergraph.dflow.hypermesh.selexpr import (  # noqa: F401
  POINT, POLY, LINE, SelExpr, S,
  sl_smoothstep, sl_step, sl_clamp, sl_min, sl_max, sl_sin, sl_cos, sl_fract, sl_select, vexpr, param, collect_params,
  sel_normal_dir, sel_id_range, sel_area_gt, sel_dihedral_gt, sel_length_gt, sel_dist_point, sel_height_band,
  MaskOp, group, groups, add, remove, toggle, isolate, replace, _bind_build_asset)
# E2.5 (Q6): capture the SelExpr as a canonical ExprIR TREE (JSON) alongside its GLSL, stored in the
# reflected SelectData/ExtrudeFacesData `*_tree` fields (author-intent round-trip + cook identity).
from ork.hypergraph.dflow.hypermesh import exprir_selexpr as _xse

_DOMAIN_ID = {POLY: 0, POINT: 1, LINE: 2}   # C++ Select shell: POLY (faces), POINT (verts), LINE (edges via MeshEdges)

# BitOp operation codes (match the C++ cs_bitop shell) + the 5-bit "bank" convention: bank n occupies
# bit offset 5n — bank 0 = the working/viz band (bits 0..4, what GroupView shows), banks 1.. = the 27
# storage bits. Bit-banking saves/restores/combines named selection masks WITHOUT a shader recompile.
BIT_COPY, BIT_NOT, BIT_AND, BIT_OR, BIT_XOR = 0, 1, 2, 3, 4

def bank(n):
  """bit offset of 5-bit bank `n` (0 = working/viz band, 1.. = storage)."""
  return 5 * int(n)

def _field_predicate(value, var, domain=POLY, prefix="_se"):
  """GLSL assigning `var` from a SelExpr (flat SSA, parser-shallow) or a constant (scalar / vec3). '' for None.
  Used by extrude_faces to drive per-face distance/inset/direction from expressions or constants. `prefix`
  must differ per field — all three predicates are spliced into one cs_field body, so shared temp names collide."""
  if value is None:
    return ""
  if isinstance(value, SelExpr):
    stmts, res = value.emit_block(domain, prefix=prefix)
    return "\n".join(stmts) + ("\n  %s = %s;" % (var, res))
  if hasattr(value, "x") and hasattr(value, "y") and hasattr(value, "z"):    # a vec3 constant
    return "  %s = vec3(%r, %r, %r);" % (var, float(value.x), float(value.y), float(value.z))
  return "  %s = %r;" % (var, float(value))                                  # a scalar constant

###############################################################################
# MeshNode — a fluent wrapper over a mesh-producing module (the SdfNode analogue for
# the mesh side). Mesh GENERATORS (icosphere/box/...) return one; it delegates EVERY
# attribute (.inputs/.outputs/.set_mask/...) to the wrapped module, so it is transparent
# everywhere a raw module was used (all verbs consume `src` via src.outputs.Out). On top
# of that it exposes the verbs as CHAINED methods, so:
#     ball = self.icosphere(radius=4)
#     skin = ball.conformToSdf(sdf=blob, conform_steps=8, relax_steps=8)
# reads the same as ball.subdivide().smooth_normals(). Each method re-wraps its result so
# chains stay fluent without the underlying Hypermesh verb having to know about MeshNode.
###############################################################################

class MeshNode:
  def __init__(self, hm, module):
    self.__dict__["_hm"]     = hm
    self.__dict__["_module"] = module

  def __getattr__(self, name):
    # only reached for attributes MeshNode doesn't define -> delegate to the module
    return getattr(self.__dict__["_module"], name)

  def __setattr__(self, name, value):
    setattr(self.__dict__["_module"], name, value)

  def __repr__(self):
    return "MeshNode(%r)" % (self._module,)

  # ---- fluent mesh ops (each delegates to the Hypermesh verb with self as src) ----
  def conformToSdf(self, sdf, conform_steps=4, relax_steps=0, relax_lambda=0.5,
                   relax_passband=0.1, iso_level=0.0, smooth=True):
    """SHRINKWRAP / CONFORM RETOPO: pull every vertex onto `sdf`'s iso-surface (`iso_level` shifts the
    target iso-level — a signed world-distance offset shell), then (relax_steps>0) run a NON-SHRINKING
    Taubin SURFACE FAIRING that smooths high-freq ridging out of the surface while preserving the gross
    shape + genuine saddles. `sdf` is an SdfNode (run .redistance() first for a true |grad|=1 field).
    Fairing controls: `relax_steps` (passes — main smoothness/temporal-calm knob), `relax_lambda`
    (per-pass strength, <=0.6), `relax_passband` (low-pass CUTOFF: LOWER = smooth more/broader). With
    fairing on, normals are recomputed from the FAIRED MESH (smooth=, default True); relax_steps==0
    keeps the exact analytic grad-phi normal. Sugar over displace_by_sdf(mode='conform')."""
    node = MeshNode(self._hm, self._hm.displace_by_sdf(
        self, field=sdf, mode="conform", conform_steps=conform_steps, relax_steps=relax_steps,
        relax_lambda=relax_lambda, relax_passband=relax_passband, iso_level=iso_level))
    if relax_steps > 0 and smooth:
      node = node.smooth_normals()    # shading matches the faired surface (not the faceted grad-phi)
    return node

  def displaceBySdf(self, field, mode="conform", amount=1.0, iso_level=0.0,
                    conform_steps=3, relax_steps=0, relax_lambda=0.5, relax_passband=0.1):
    return MeshNode(self._hm, self._hm.displace_by_sdf(
        self, field=field, mode=mode, amount=amount, iso_level=iso_level, conform_steps=conform_steps,
        relax_steps=relax_steps, relax_lambda=relax_lambda, relax_passband=relax_passband))

  def temporalSmooth(self, alpha=None, tau=None):
    """Inter-frame EMA to damp cross-frame jitter — `alpha` (per-frame) or `tau` (seconds, framerate-
    independent). Chain after conformToSdf on a fixed base. See Hypermesh.temporal_smooth."""
    return MeshNode(self._hm, self._hm.temporal_smooth(self, alpha=alpha, tau=tau))

  def subdivide(self, smooth=False, slot=None, level=None):
    return MeshNode(self._hm, self._hm.subdivide(self, smooth=smooth, slot=slot, level=level))

  def smooth_normals(self, slot=None):
    return MeshNode(self._hm, self._hm.smooth_normals(self, slot=slot))

  def face_normals(self, slot=None):
    return MeshNode(self._hm, self._hm.face_normals(self, slot=slot))

  def transform(self, **kw):
    return MeshNode(self._hm, self._hm.transform(self, **kw))


class Hypermesh:
  """Build a hypermesh graph by composing modules. The last-added (or output()) node is the
  terminal; materialize() bakes it to a GpuMesh."""

  MATERIAL_CLASS = None   # ptex3d material to render with (like terrain assets); None -> Solid. Assign
                          # a hypermesh material (e.g. assets.materials.hypermesh.TopoView) to override.

  def materials(self):
    """The material(s) this asset renders ITSELF with — a list of HmMaterial (applied by the hypermesh
    viewer + scene). Default = ONE derived from MATERIAL_CLASS (back-compat; None -> Solid). Override to
    declare 0 (let the viewer pick its own material) or MORE (partition the mesh into per-gid draws via
    assign_gid). A material may carry a `displace` (VS-side animation, e.g. Wind(...)) — see ls_anim."""
    return [HmMaterial(self.MATERIAL_CLASS)]

  def __init__(self):
    self.graphdata = _dflow.GraphData.createShared()
    self._terminal = None
    self._n        = 0
    self._param_sinks = []               # MaterialParamSink modules (viewer drains them per-frame)
    self._time_param = None              # S.time lazily attaches the asset's single shared time param here
    self._field_animated = False         # set by T.* ops with offset_vel != 0 (E.1b time-driven fields)
    _bind_build_asset(self)              # so S.time / animated-field marking resolves to THIS asset
    # E.1 CROSS-FAMILY: open a trace context on this graph (the HeightField pattern), so terrain
    # expression ops (T.fbm / T.voronoi / the TerrainNode algebra) called during composition emit
    # their modules INTO this mesh graph — they become field inputs for displace(). The trace
    # closes at generatedflow()/materialize*()/save() (idempotent).
    from .._trace import enter_trace
    self._prev_trace = enter_trace(self.graphdata)

  def _close_trace(self):
    from .._trace import current_graph, leave_trace
    if getattr(self, "_prev_trace", None) is not None and current_graph() is self.graphdata:
      leave_trace(self._prev_trace)
      self._prev_trace = None
    # E.6/2.19 — STATIC graphs participate in the per-node disk cook cache (the
    # terrain pattern): reloads/re-authorings restore each node's mesh from
    # <staging>/dflowcache instead of recomputing (content-only hashes — a
    # re-authored graph with the same values HITS despite fresh uuids). Animated
    # graphs recompute per-frame by definition -> never cacheable.
    self.graphdata.cacheable = not self.is_animated

  def _add(self, mod, base):
    self.graphdata.addModule(mod, "%s_%d" % (base, self._n))
    self._n += 1
    self._terminal = mod
    # generators `return self._add(...)` -> become fluent MeshNodes; SDF verbs and ops
    # `self._add(m,...); ...; return m` keep returning the raw module (SdfNode wraps those,
    # and MeshNode's __getattr__ makes the raw form interchangeable as a verb `src`).
    return MeshNode(self, mod)

  # ---- generators ---- (each takes an optional `mask` MaskOp -> whole-mesh __tags, RUNTIME; the
  #      upstream-free analogue of select(): the generated faces start tagged. None -> untagged.)
  @staticmethod
  def _set_gen_mask(m, mask):
    if mask is not None:
      m.set_mask(mask.triple())     # value per face = OR ^ XOR (base 0); identity inherit doesn't apply

  def ripple(self, grid=64, amp=1.2, freq=2.2, extent=8.0, mask=None):
    m = _lev2.hypermesh.RipplePrimitive.createShared()
    m.inputs.grid   = int(grid)      # INT plug
    m.inputs.amp    = float(amp)
    m.inputs.freq   = float(freq)
    m.inputs.extent = float(extent)
    self._set_gen_mask(m, mask)
    return self._add(m, "ripple")

  def box(self, size=1.0, mask=None):
    m = _lev2.hypermesh.Box.createShared()
    m.inputs.size = float(size)
    self._set_gen_mask(m, mask)
    return self._add(m, "box")

  def lsystem(self, archetype=Archetype.SYMPODIAL, depth=7, seg_len=0.5, base_radius=0.08, sides=6,
              budget=4000, children=2, internodes=1, seed=1, branch_angle=35.0,
              roll=137.5, len_decay=0.78, rad_decay=0.72, taper=0.0, tropism=0.0,
              jitter=0.0, apical=0.0, cap_segments=3, cap_round=1.0,
              jit_azimuth=None, jit_pitch=None, jit_length=None, jit_spacing=None,
              jit_drop=None, jit_wave=None, grammar=None):
    # L-system FAMILY (M1/GR1): an LSystemModule derives its reflected LRuleSet grammar into
    # an XfNodeGraph branch skeleton, an LSweepModule skins it to a swept-tube GpuMesh.
    # Returns the (mesh) sweep node.
    #   grammar=  : a reflected LRuleSet (from the ork.hypergraph.dflow.lsystem DSL: an
    #               LRuleSet, an Lsystem subclass/instance, or a builder). The C++ evaluator
    #               (derive() in _buildSkeleton) rewrites+turtle-interprets it into the
    #               XfNodeGraph. The 21 scalar params below stay live as the grammar's PARAM
    #               environment (A8).
    #   archetype=: selects a PRESET GRAMMAR EMITTER (GR1.d) when grammar is None — the four
    #               stock growth models (0 sympodial 1 conifer 2 saguaro 3 ocotillo) authored
    #               as combinator grammars in ork.hypergraph.dflow.lsystem.presets. Same DSL
    #               surface as the deleted C++ enum; ints (depth/children/internodes) shape
    #               the emitted grammar, floats stay live via the PARAM env (with the
    #               per-preset module-scalar remaps presets.py documents).
    ls = _lev2.hypermesh.LSystemModule.createShared()
    _preset_overrides = {}
    if grammar is None:
      from ork.hypergraph.dflow.lsystem.presets import build_preset
      grammar, _preset_overrides = build_preset(int(archetype),
                                                depth=depth,
                                                children=children,
                                                internodes=internodes,
                                                seed=seed,
                                                budget=budget,
                                                tropism=tropism)
    from ork.hypergraph.dflow.lsystem import resolve_grammar
    ls.grammar = resolve_grammar(grammar)     # reflected LRuleSet -> LSystemModuleData._grammar
    ls.depth        = int(depth)
    ls.budget       = int(budget)
    ls.children     = int(children)
    ls.internodes   = int(internodes)
    ls.seed         = int(seed)
    ls.seg_len      = float(seg_len)
    ls.base_radius  = float(base_radius)
    ls.branch_angle = float(branch_angle)
    ls.roll         = float(roll)
    ls.len_decay    = float(len_decay)
    ls.rad_decay    = float(rad_decay)
    ls.taper        = float(taper)
    ls.tropism      = float(tropism)
    ls.jitter       = float(jitter)
    ls.apical       = float(apical)
    # chaos channels: a None kwarg falls back to this archetype's known-good default
    _jd = _ARCH_JIT.get(Archetype(int(archetype)), _ARCH_JIT[Archetype.SYMPODIAL])
    _pick = lambda v, key: _jd[key] if v is None else v
    ls.jit_azimuth  = float(_pick(jit_azimuth, "jit_azimuth"))
    ls.jit_pitch    = float(_pick(jit_pitch,   "jit_pitch"))
    ls.jit_length   = float(_pick(jit_length,  "jit_length"))
    ls.jit_spacing  = float(_pick(jit_spacing, "jit_spacing"))
    ls.jit_drop     = float(_pick(jit_drop,    "jit_drop"))
    ls.jit_wave     = float(_pick(jit_wave,    "jit_wave"))
    # per-preset module-scalar remaps (conifer/saguaro tropism — see presets.py header)
    for _k, _v in _preset_overrides.items():
      setattr(ls, _k, float(_v))
    self._skeleton = self._add(ls, "lsystem")                 # produces XfNodeGraph (the skeleton hub —
                                                              # organs (leaves/needles/thorns) branch off it)
    sw = _lev2.hypermesh.LSweepModule.createShared()
    sw.sides        = int(sides)
    sw.cap_segments = int(cap_segments)
    sw.cap_round    = float(cap_round)
    self.graphdata.connect(sw.inputs.In, ls.outputs.Out)      # XfNodeGraph edge
    return self._add(sw, "lsweep")                            # produces the GpuMesh

  def leaves(self, skeleton=None, *, style=LeafStyle.SINGLE, source=LeafSource.NODES, per_node=3,
             min_gen=4.0, max_radius=0.0, size=0.35, aspect=0.6, roll=137.5, pitch=50.0,
             embed=0.0, twist=0.0, up_bias=0.0, jitter=0.25, jitter_deg=0.0, seed=1):
    # BROADLEAF ORGAN placer — reads the L-system SKELETON (XfNodeGraph; defaults to the last lsystem()'s)
    # and emits a leaf-card GpuMesh: per high-generation node, `per_node` cards by phyllotaxis (golden-angle
    # `roll` around the node heading, drooped `pitch`). `style` LeafStyle.SINGLE (quad) | CROSS (2 quads). The
    # card UV0 lets the MATERIAL texture or proceduralize the leaf; COLOR.x = flutter weight (0..1 tip).
    # `source` LeafSource.SLOTS instead places at the grammar's own SLOT attachment points (areoles,
    # blooms) — their world frames, `min_gen` inapplicable, `per_node` cards per slot.
    #   max_radius=: 0 = off; else a node whose segment radius EXCEEDS it bears no cards (trunk/bough
    #                wood — a monopodial leader is ONE generation from thick base to thin apex, so
    #                min_gen cannot make that cut).
    #   embed=     : 0 = off (base edge on the skeleton centerline); else the base edge anchors on the
    #                outward radial, recessed this fraction of the local radius UNDER the bark, so the
    #                blade emerges through the surface instead of hovering beside it.
    #   twist=     : card rotation about its own length axis (deg; 0 = the flat faces point radially out)
    #   up_bias=   : -1..1, blends the length axis toward world up (+) / down (-) after pitch; the base
    #                anchor does not move (the emergence DIRECTION bends, not the emergence point).
    #   jitter_deg=: +/- degrees of per-card jitter on azimuth / pitch / twist, hashed from
    #                (seed, placement, card) — deterministic, so two cooks are byte-identical.
    skel = skeleton if skeleton is not None else getattr(self, "_skeleton", None)
    if skel is None:
      raise ValueError("leaves(): no skeleton — call lsystem() first, or pass skeleton=<lsystem node>")
    m = _lev2.hypermesh.LeafScatterModule.createShared()
    m.style    = int(style)                                   # LeafStyle.SINGLE / CROSS (IntEnum -> 0/1)
    m.source   = int(source)                                  # LeafSource.NODES / SLOTS (IntEnum -> 0/1)
    m.per_node   = int(per_node)
    m.min_gen    = float(min_gen)
    m.max_radius = float(max_radius)
    m.size       = float(size)
    m.aspect     = float(aspect)
    m.roll       = float(roll)
    m.pitch      = float(pitch)
    m.embed      = float(embed)
    m.twist      = float(twist)
    m.up_bias    = float(up_bias)
    m.jitter     = float(jitter)
    m.jitter_deg = float(jitter_deg)
    m.seed       = int(seed)
    self.graphdata.connect(m.inputs.In, skel.outputs.Out)     # XfNodeGraph edge (the skeleton)
    return self._add(m, "leaves")                             # produces the leaf-card GpuMesh

  def merge(self, a, b, *, gid_a=0, gid_b=1):
    # CONCAT two meshes into one, stamping each source's faces with its own gid (`a` -> gid_a, `b` -> gid_b)
    # so a multi-material drawable routes them to distinct materials. The canonical use is BAKING a leaf-card
    # mesh (b) INTO a trunk mesh (a): the result is ONE mesh -> one instance, one frustum-cull, two materials.
    # gid_a / gid_b = None PRESERVES that source's existing gids (e.g. a trunk that already assign_gid'd its
    # upper branches into gid 1 keeps that split; pass gid_b=2 for the leaves -> bark/branch/leaf = 3 gids).
    m = _lev2.hypermesh.MergeMesh.createShared()
    m.gid_a = -1 if gid_a is None else int(gid_a)
    m.gid_b = -1 if gid_b is None else int(gid_b)
    self.graphdata.connect(m.inputs.A, a.outputs.Out)         # mesh edge (gid_a faces)
    self.graphdata.connect(m.inputs.B, b.outputs.Out)         # mesh edge (gid_b faces)
    return self._add(m, "merge")                             # produces the combined GpuMesh

  def sorttest(self, n=252):
    # Regression harness for the MeshSort GPU primitive (verts encode a stably-sorted key/payload
    # stream; see hmdflow_module_sorttest.cpp). Not a modeling op.
    m = _lev2.hypermesh.SortTest.createShared()
    m.n = int(n)
    return self._add(m, "sorttest")

  def edgetest(self, src):
    # Regression harness for MeshEdges (GPU edge enumeration). Output verts encode each unique edge as
    # (va, vb, count) in XYZ; sentinels (-1,-1,-1) fill unused slots. Not a modeling op.
    m = _lev2.hypermesh.EdgeTest.createShared()
    self.graphdata.connect(m.inputs.In, src.outputs.Out)
    return self._add(m, "edgetest")

  def bevel(self, src, amount=0.1, slot=0, part_masks=None, precheck=True, postcheck=True):
    # EDGE CHAMFER. `src` must carry an edge selection (an upstream select(..., domain=LINE, ...) on bit
    # `slot`); bevel chamfers those edges by `amount` (runtime float plug). CPU-rebuilds the chamfer topology
    # (offset corner-verts + chamfer quads + corner caps), GPU recomputes positions each frame.
    # precheck  : ASSERT if the INPUT isn't a good mesh (welded/manifold — a bevel needs it; e.g. caught using
    #             face_normals (splits) where smooth_normals (welds) was needed). Default True.
    # postcheck : ASSERT if the OUTPUT isn't a good mesh (the chamfer/cap created holes the input didn't have —
    #             e.g. a MIXED edge selection the bevel can't seal). Default True. Set False to inspect a partial.
    m = _lev2.hypermesh.Bevel.createShared()
    m.inputs.amount = float(amount)
    m.slot = int(slot)
    m.precheck  = bool(precheck)
    m.postcheck = bool(postcheck)
    if part_masks is not None:
      flat = []
      for mo in part_masks: flat += mo.triple()
      m.set_part_masks(flat)
    self.graphdata.connect(m.inputs.In, src.outputs.Out)
    return self._add(m, "bevel")

  def verttest(self, src):
    # Regression harness for POINT (vertex) selection. Output verts encode (vertid, group-0 selbit, 0).
    # Not a modeling op.
    m = _lev2.hypermesh.VertTest.createShared()
    self.graphdata.connect(m.inputs.In, src.outputs.Out)
    return self._add(m, "verttest")

  def uvsphere(self, radius=1.5, segments=32, rings=16, mask=None):
    m = _lev2.hypermesh.UvSphere.createShared()   # MIXED quad/tri
    m.inputs.radius   = float(radius)
    m.inputs.segments = int(segments)             # INT plug (baked topology)
    m.inputs.rings    = int(rings)
    self._set_gen_mask(m, mask)
    return self._add(m, "uvsphere")

  def icosphere(self, radius=1.5, subdivisions=2, mask=None):
    m = _lev2.hypermesh.IcoSphere.createShared()  # all-tri; midpoint-subdivided + sphere-projected
    m.inputs.radius       = float(radius)
    m.inputs.subdivisions = max(0, int(subdivisions))   # INT plug (runtime); 0 = base icosahedron
    self._set_gen_mask(m, mask)
    return self._add(m, "icosphere")

  def cone(self, radius=1.5, height=2.5, sides=24, mask=None):
    m = _lev2.hypermesh.Cone.createShared()       # MIXED: `sides` side tris + 1 base n-gon
    m.inputs.radius = float(radius)
    m.inputs.height = float(height)
    m.inputs.sides  = max(3, int(sides))          # INT plug (baked); >=3
    self._set_gen_mask(m, mask)
    return self._add(m, "cone")

  # ---- transforms (1-in) ----
  def subdivide(self, src, smooth=False, slot=None, level=None):
    # smooth=False: LINEAR midpoint refinement (the original; tri->4tri, ngon->n-quad). smooth=True: true
    # Catmull-Clark (uniform tri->3-quad topology + CC position rules -> rounds the surface). The `level`
    # int plug is runtime; positions recompute on the GPU each frame, so an animated source is tracked.
    # `level=N` seeds that plug inline (else it keeps its default); still live-settable via m.inputs.level.
    # slot=<bit>: subdivide ONLY faces tagged with that __tags bit (from select(...)); unselected faces stay
    # put but absorb the boundary midpoints -> watertight n-gon seam (smooth -> the boundary is a crease).
    m = _lev2.hypermesh.SubdivideModule.createShared()
    m.smooth = bool(smooth)
    m.slot = -1 if slot is None else int(slot)
    if level is not None:
      m.inputs.level = int(level)
    self._add(m, "subdivide")
    self.graphdata.connect(m.inputs.In, src.outputs.Out)
    return m

  def assign_gid(self, src, gid, slot=None):
    """E.3 — sets the persistent gid (the LOCKED top 12 __tags bits [20:32), the per-face
    material/semantic-class key the render bucketing partitions on) on the selected faces
    (slot=<bit> from select(), None = ALL faces — creates the __tags channel on an untagged
    mesh). This is the ONLY verb that may write those bits; every other tag write (select
    MaskOps, FaceTagger partitions, bitop bands) is hard-masked to the free region [0:20).
    Read back in expressions via S.gid; bind materials per gid via
    drawable_data(materials={gid: material_asset}). Both params RUNTIME (no recompile)."""
    m = _lev2.hypermesh.GidAssign.createShared()
    m.gid  = int(gid) & 0xFFF
    m.slot = -1 if slot is None else (int(slot) & 31)
    self._add(m, "assign_gid")
    self.graphdata.connect(m.inputs.In, src.outputs.Out)
    return m

  def section_unwrap(self, src, padding=2, max_layers=64):
    """O3 — PER-SECTION UV unwrap for the baked ptex3d texture-ARRAY path. Runs AFTER the gid
    partition (select+assign_gid): each distinct gid becomes one SECTION unwrapped by xatlas into
    its OWN full 0-1 UV domain (more texel budget than one shared atlas), and the section's dense
    LAYER index (0..K-1 in sorted-gid order) is written into UV0.z of every one of its verts. The
    forward material then samples a sampler2DArray at layer=frg_uv0.z (ctx.texArray). Faces are
    emitted grouped by section (contiguous ranges) so the bake driver renders one section per layer;
    the per-face gid is preserved. `padding` = xatlas chart padding; `max_layers` = fail-loud cap on
    distinct sections. Insert as: sdf_to_mesh_clean(unwrap=False) -> select+assign_gid... ->
    section_unwrap(...)."""
    m = _lev2.hypermesh.SectionUnwrap.createShared()
    m.padding    = int(padding)
    m.max_layers = int(max_layers)
    self._add(m, "section_unwrap")
    self.graphdata.connect(m.inputs.In, src.outputs.Out)
    return m

  def sdf(self, dim=64, extent=4.0, center=(0.0, 0.0, 0.0)):
    """Open a fluent SDF builder bound to this hypermesh, sharing the brick framing:
        s = self.sdf(dim=128, extent=4.0)
        cut = s.box(size=(1,1,1)) - s.sphere(0.75)   # - subtract / | union / & intersect
        self.output(cut.to_mesh())
    Sugar over sdf_eval / csg / sdf_to_mesh; the shape nodes expose `.inputs` (poke
    `.inputs.offset` to animate a shape's position)."""
    from ork.hypergraph.dflow.sdf import SdfContext
    return SdfContext(self, dim, extent, center)

  def sdf_eval(self, expr, dim=64, extent=4.0, center=(0.0, 0.0, 0.0)):
    """E.7/M0 — bake an analytic SDF expression (ork.hypergraph.dflow.sdf algebra, or a raw
    GLSL string of `vec3 p`) into a DENSE BRICK on the SdfGrid interchange plug: a cubic
    dim³ float field over the world cube (center, extent). dim/extent/center are RUNTIME
    plugs (a dim poke reallocs the brick from the pool, no recompile); the expression
    bakes into the kernel. Consumers: csg / sdf_to_mesh (M2), displace_by_sdf (M4)."""
    from orkengine.core import vec3 as _vec3
    m = _lev2.sdf.SdfEval.createShared()
    m.expression    = expr._glsl if hasattr(expr, "_glsl") else str(expr)
    m.inputs.dim    = int(dim)
    m.inputs.extent = float(extent)
    m.inputs.center = _vec3(float(center[0]), float(center[1]), float(center[2]))
    return self._add(m, "sdf_eval")

  def mesh_to_sdf(self, src, dim=48, extent=-1.0, center=(0.0, 0.0, 0.0), pad=0.12, sign_mode=-1):
    """E.7/M1 — GPU voxelize: the mesh -> a dense SDF brick on the SdfGrid plug.
    extent<=0 = AUTO-fit the mesh bound (+pad margin; refits when the source re-emits —
    animated meshes prefer an explicit extent for a stable brick). sign_mode: -1 AUTO
    (the boundary-edge probe picks pseudonormal on closed input, winding on dirty),
    0 = force pseudonormal, 1 = force winding. dim/extent/center/pad are RUNTIME plugs."""
    from orkengine.core import vec3 as _vec3
    m = _lev2.sdf.MeshToSdf.createShared()
    m.sign_mode     = int(sign_mode)
    m.inputs.dim    = int(dim)
    m.inputs.extent = float(extent)
    m.inputs.center = _vec3(float(center[0]), float(center[1]), float(center[2]))
    m.inputs.pad    = float(pad)
    self._add(m, "mesh_to_sdf")
    self.graphdata.connect(m.inputs.In, src.outputs.Out)
    return m

  _CSG_OP = {"union": 0, "intersect": 1, "subtract": 2, "smooth_union": 3}

  def csg(self, a, b, op="union", k=0.25):
    """E.7/M2 — boolean composite of two SDF bricks (sdf_eval / mesh_to_sdf outputs).
    Output rides A's frame; B is sampled trilinearly at A's positions (frames may differ).
    op in union/intersect/subtract/smooth_union; k = smooth-union fillet (runtime plug)."""
    m = _lev2.sdf.Csg.createShared()
    m.op = self._CSG_OP[op] if isinstance(op, str) else int(op)
    m.inputs.k = float(k)
    self._add(m, "csg")
    self.graphdata.connect(m.inputs.A, a.outputs.Out)
    self.graphdata.connect(m.inputs.B, b.outputs.Out)
    return m

  def sdf_to_mesh(self, src, weld=True, blocky=False):
    """E.7/M2 — an SDF brick -> an INDEXED GpuMesh. Default = MARCHING TETRAHEDRA (smooth, sub-voxel
    interpolated). `blocky=True` = CUBERILLE: pure axis-aligned voxel-block faces (no interpolation/
    smoothing, Minecraft look; block size = the brick voxel) — handles ANY topology (disconnected /
    high-genus) since it's not a deformed sheet. The output is an ordinary hypermesh (gid-assignable,
    bevelable, instanceable, standard triangulator). `weld` dedups coincident verts (shared/watertight);
    it is FORCED OFF for blocky (welding would merge the hard 90deg per-face normals)."""
    m = _lev2.sdf.SdfToMesh.createShared()
    m.blocky = bool(blocky)
    m.weld   = bool(weld) and not bool(blocky)
    self._add(m, "sdf_to_mesh")
    self.graphdata.connect(m.inputs.In, src.outputs.Out)
    return m

  def sdf_to_mesh_clean(self, src, adaptivity=0.5, unwrap=True, isovalue=0.0, weld_tol=0.0):
    """E.7/M2 — SHAPE-AWARE clean remesh: an SDF brick -> a CLEAN, LOW-POLY, QUAD-DOMINANT
    INDEXED GpuMesh, optionally UV-unwrapped. Where sdf_to_mesh (marching tets) emits a
    uniform-density soup (~voxel^2 faces — 1M+ for a dim=192 building), this routes the brick
    through openvdb's curvature-ADAPTIVE volumeToMesh (flat regions -> few big faces, detail
    at curvature) for ~20k clean faces, then (unwrap=True) xatlas UV-unwraps for texture baking.
    `adaptivity` 0..1 (0 = max detail, 1 = flattest). `unwrap=True` writes UV0.xy and TRIANGULATES
    (xatlas reindexes along seams); `unwrap=False` keeps quads-as-quads with UV0 = 0. `weld_tol`
    optional pre-unwrap position weld (0 = none). ALL work is ONE-SHOT CPU at cook (openvdb is
    CPU/bake-time only) — NOT for per-frame animated SDF input. gids do NOT survive the remesh
    (re-gid by position/normal band AFTER, the sdf_to_mesh pattern)."""
    m = _lev2.sdf.SdfToMeshClean.createShared()
    m.adaptivity = float(adaptivity)
    m.unwrap     = bool(unwrap)
    m.isovalue   = float(isovalue)
    m.weld_tol   = float(weld_tol)
    self._add(m, "sdf_to_mesh_clean")
    self.graphdata.connect(m.inputs.In, src.outputs.Out)
    return m

  def redistance(self, src, max_iterations=0):
    """E.7/M4a — JFA eikonal redistance: re-normalize an SDF brick to a TRUE
    |grad|=1 field, PRESERVING the zero-set. Use before displace_by_sdf(conform) or
    any field-MAGNITUDE consumer — CSG output (esp. smooth_union) is not a true SDF
    away from the surface. `max_iterations` 0 = auto (ceil(log2 next-pow2 dim))."""
    m = _lev2.sdf.Redistance.createShared()
    if max_iterations:
      m.max_iterations = int(max_iterations)
    self._add(m, "redistance")
    self.graphdata.connect(m.inputs.In, src.outputs.Out)
    return m

  _DISPLACE_SDF_MODE = {"conform": 0, "offset": 1, "scalar": 2}

  def displace_by_sdf(self, src, field, mode="conform", amount=1.0, iso_level=0.0,
                      conform_steps=3, relax_steps=0, relax_lambda=0.5, relax_passband=0.1):
    """E.7/M4b — displace a mesh's vertices by an SDF FIELD (the cross-family SDF edge).
      conform : shrinkwrap each vertex onto the field's iso-surface (true-distance step; run
                field.redistance() first for a true |grad|=1 field — THE retopo use).
      offset  : P += amount * normalize(grad)        (inflate / shell along the gradient)
      scalar  : P += amount * (phi-iso_level) * N    (the heightfield idiom)
    Produces POSITION (displaced) AND NORMAL (= grad phi). conform+`relax_steps` runs a NON-SHRINKING
    Taubin SURFACE FAIRING (full 1-ring Laplacian, sign alternated per pass) that smooths high-freq
    ridging out of the SURFACE while preserving the gross shape. Controls: `relax_steps` (passes — the
    main smoothness/calm knob), `relax_lambda` (per-pass strength, keep <=0.6 for stability),
    `relax_passband` (the Taubin low-pass CUTOFF on the Laplacian spectrum [0,2]: features below it are
    preserved, above are smoothed — so LOWER it to smooth MORE/broader wobble, raise toward ~1 for
    gentler/finest-only). uv/color + topology pass through. `field` is an SdfNode (or any SDF module)."""
    m = _lev2.hypermesh.DisplaceBySdf.createShared()
    m.mode          = self._DISPLACE_SDF_MODE[mode] if isinstance(mode, str) else int(mode)
    m.conform_steps = int(conform_steps)
    m.relax_steps   = int(relax_steps)
    m.inputs.amount         = float(amount)
    m.inputs.iso_level     = float(iso_level)
    m.inputs.relax_lambda   = float(relax_lambda)
    m.inputs.relax_passband = float(relax_passband)
    self._add(m, "displace_by_sdf")
    self.graphdata.connect(m.inputs.In, src.outputs.Out)
    self.graphdata.connect(m.inputs.Field, field.outputs.Out)
    return m

  def temporal_smooth(self, src, alpha=None, tau=None):
    """Inter-frame EMA on POSITION + NORMAL — a TIME low-pass that damps cross-frame jitter on a
    deforming mesh. Pass `alpha` (per-frame weight of the NEW frame; small = heavy smoothing, lag
    ~(1-a)/a frames, framerate-dependent) OR `tau` (seconds; framerate-INDEPENDENT time constant,
    a=1-exp(-dt/tau)). Holds a persistent previous-frame buffer; REQUIRES stable vertex correspondence
    (true after conform-on-a-fixed-base) and SELF-DEFENDS by re-priming (pass-through) on an
    nv/topology change. Topology + uv/color pass through; only POSITION/NORMAL are produced."""
    m = _lev2.hypermesh.TemporalSmooth.createShared()
    if tau is not None:
      m.mode = 1
      m.inputs.tau = float(tau)
    else:
      m.mode = 0
      m.inputs.alpha = float(alpha if alpha is not None else 0.4)
    self._add(m, "temporal_smooth")
    self.graphdata.connect(m.inputs.In, src.outputs.Out)
    return m

  def material_param(self, src, name, value=0.0):
    """E.6/2.12 — drive a bound material's UBO param `name` (a ptex3d ctx.param)
    from the graph. Mesh PASSTHROUGH (chain position is ergonomic only); the float
    `value` plug is a pokeable DATA plug — set it live via m.inputs.value (works on
    a STATIC mesh, no geometry recompute). The drawable applies it per-frame to
    EVERY bound material (main + per-gid buckets) whose shader declares the param;
    a name matching nothing logs LOUDLY and the sink is inert."""
    m = _lev2.hypermesh.MaterialParamSink.createShared()
    m.param_name   = str(name)
    m.inputs.value = float(value)
    self._add(m, "material_param")
    self.graphdata.connect(m.inputs.In, src.outputs.Out)
    self._param_sinks.append(m)
    return m

  # ---- selection: evaluate a SelExpr predicate over `domain` and apply the MaskOp `op` to __tags ----
  # `expr` is a SelExpr (S.* atoms / sel_* builders / & | ^ ~); `op` is a MaskOp (the two-triple bit
  # transform — matched elements get the sel-triple, unmatched the unsel-triple). 32 boolean named
  # groups (bits) ride the mesh's uint32 __tags channel + recompute each frame (animate with the mesh).
  def select(self, src, expr, domain=POLY, op=None):
    if domain not in _DOMAIN_ID:
      raise NotImplementedError("hypermesh select domain '%s' not wired yet (have: %s)"
                                % (domain, ", ".join(_DOMAIN_ID)))
    if op is None:
      op = replace(group(0))                       # default: group 0 becomes exactly the matched set
    m = _lev2.hypermesh.Select.createShared()
    # SelExpr -> FLAT SSA GLSL (one op per temp; constants baked; reads fP/fN/fArea/_tags). Flat (not one
    # deeply-nested expr) so shadlang's PEG parser stays shallow for arbitrarily-complex predicates.
    _stmts, _res = expr.emit_block(domain)
    m.predicate = "\n".join(_stmts) + ("\n  _sel = %s;" % _res)
    m.predicate_tree = _xse.capture_json(expr)   # E2.5 (Q6): the ExprIR tree of the SAME predicate
    m.domain    = _DOMAIN_ID[domain]
    m.sel_and,   m.sel_or,   m.sel_xor   = op.sel_and,   op.sel_or,   op.sel_xor
    m.unsel_and, m.unsel_or, m.unsel_xor = op.unsel_and, op.unsel_or, op.unsel_xor
    self._add(m, "select")
    self.graphdata.connect(m.inputs.In, src.outputs.Out)
    return m

  # ---- __tags BIT-BANKING: rewrite a width-bit destination band from one/two source bands, per face.
  #      band offsets (use bank(n)) + op + width are ALL RUNTIME — mutating them on the returned module
  #      takes effect next frame with NO shader recompile (tweak/animate banking live). ----
  def bitop(self, src, dst, a, b=0, op=BIT_COPY, width=5):
    m = _lev2.hypermesh.BitOp.createShared()
    m.dst   = int(dst) & 31
    m.a     = int(a) & 31
    m.b     = int(b) & 31
    m.op    = int(op)
    m.width = int(width)
    self._add(m, "bitop")
    self.graphdata.connect(m.inputs.In, src.outputs.Out)
    return m

  def save_bank(self, src, into, frm=0, width=5):
    """copy band `frm` (default the working bank 0) into band `into`. Offsets: use bank(n)."""
    return self.bitop(src, dst=into, a=frm, op=BIT_COPY, width=width)

  def restore_bank(self, src, frm, into=0, width=5):
    """copy band `frm` back into band `into` (default the working bank 0). Offsets: use bank(n)."""
    return self.bitop(src, dst=into, a=frm, op=BIT_COPY, width=width)

  def combine_banks(self, src, a, b, op=BIT_OR, into=0, width=5):
    """into-band = a-band OP b-band (op in BIT_AND/BIT_OR/BIT_XOR). Offsets: use bank(n)."""
    return self.bitop(src, dst=into, a=a, b=b, op=op, width=width)

  # ---- selection consumer: extrude the faces tagged in bit `slot` (REGION) along N by `distance`.
  #      Output __tags = inherit input + per-partition MaskOp: mask_base (untouched faces), mask_cap
  #      (the raised/lifted selected faces), mask_wall (the new side quads). Each defaults to identity
  #      (the upstream selection survives the extrude unchanged). All RUNTIME (no recompile). ----
  _EXTRUDE_MODES = {"vertex": 0, "face": 1}
  def extrude_faces(self, src, distance=0.5, slot=0, mode="face", inset=0.0, direction=None,
                    keep_base=False, mask_wall=None, mask_cap=None, mask_base=None,
                    segments=1, twist=0.0, scale=1.0):
    # mode "face" (default): each selected face lifts along its OWN face normal -> faces SEPARATE, walls PLANAR
    #   (winding trivially correct, no see-through holes). mode "vertex": REGION extrude -> welded along shared
    #   edges, lift along the vertex normal, walls only on the region boundary (non-planar).
    #
    # Per-face EXPRESSIONS (face mode only) — each accepts a SelExpr (traced per-face on GPU, reads S.P/S.N/
    # S.area/S.tag, animates for free) OR a plain constant:
    #   distance  : float | SelExpr(float) — lift amount (a SelExpr replaces the scalar; else it's the scalar plug)
    #   inset     : float | SelExpr(float) — in-plane shrink of the cap toward the face centroid (0 = none)
    #   direction : vec3  | SelExpr(vec3)  — lift direction (default = the face normal)
    #
    # MULTI-SEGMENT (face mode): `segments` (int >= 1) subdivides the lift into N stacked rings. Then the field
    # exprs are evaluated PER RING at the ring parameter S.t in (0..1] / index S.seg, and:
    #   distance/direction — ACCUMULATE: each ring steps by distance*dir(t) from the previous, so a per-t
    #                        `direction` BENDS the strand (subsumes a chained droop into one op).
    #   twist : float | SelExpr(float) — ABSOLUTE rotation (RADIANS) of the ring's cross-section about the axis.
    #   scale : float | SelExpr(float) — ABSOLUTE in-plane profile scale about the face centroid (1 = unchanged).
    # segments==1 reproduces the legacy single-lift exactly (twist/scale ignored).
    if mode not in self._EXTRUDE_MODES:
      raise ValueError("extrude_faces mode must be 'face' or 'vertex', got %r" % (mode,))
    segments = int(segments)
    _twist_active = isinstance(twist, SelExpr) or float(twist) != 0.0
    _scale_active = isinstance(scale, SelExpr) or float(scale) != 1.0
    _inset_active = isinstance(inset, SelExpr) or float(inset) != 0.0
    _has_expr     = isinstance(distance, SelExpr) or _inset_active or (direction is not None) \
                    or _twist_active or _scale_active or segments > 1
    if mode != "face" and _has_expr:
      raise ValueError("extrude_faces per-face distance/inset/direction/twist/scale/segments need mode='face' "
                       "(vertex/region is scalar-only)")
    m = _lev2.hypermesh.ExtrudeFaces.createShared()
    m.slot = int(slot) & 31
    m.mode = self._EXTRUDE_MODES[mode]
    m.segments = segments
    # collect runtime params used across the exprs; assign each a vec4 slot (BEFORE emitting, so the
    # GLSL bakes the slot), register the (module, slot) binding so param.set() can rebind live, seed defaults.
    prms = collect_params(distance, inset, direction, twist, scale)
    if len(prms) > 8:                            # kMaxExprParams: the FIXED exprp0..7 vec4 plug set
      raise ValueError("extrude_faces: at most 8 runtime params per extrude (got %d)" % len(prms))
    for slot_i, p in enumerate(prms):
      p._slot = slot_i
      p._bindings.append((m, slot_i))
      if p._pname == "__time":                   # S.time -> the module feeds this slot from the C++ clock
        m.time_slot = slot_i                     # (declarative time: zero per-frame Python; serializes)
    # distance: a SelExpr -> per-face field (the scalar plug becomes a 1.0 global multiplier); else the scalar plug.
    # each field: the GLSL pred is the EVAL form; the ExprIR tree (Q6) is the storage-form identity,
    # captured from the SAME value (a SelExpr or a baked constant) -> set both in lockstep.
    if isinstance(distance, SelExpr):
      m.dist_predicate = _field_predicate(distance, "_dist", prefix="_sd")
      m.dist_tree = _xse.capture_json(distance)
    if _inset_active:
      m.inset_predicate = _field_predicate(inset, "_inset", prefix="_si")
      m.inset_tree = _xse.capture_json(inset)
    if direction is not None:
      m.dir_predicate = _field_predicate(direction, "_dir", prefix="_sr")
      m.dir_tree = _xse.capture_json(direction)
    if _twist_active:
      m.twist_predicate = _field_predicate(twist, "_twist", prefix="_st")
      m.twist_tree = _xse.capture_json(twist)
    if _scale_active:
      m.scale_predicate = _field_predicate(scale, "_scale", prefix="_sc")
      m.scale_tree = _xse.capture_json(scale)
    if prms:
      flat = []
      for p in prms:
        flat += p._default4()
      m.set_expr_params(flat)
    m.keep_base = bool(keep_base)
    _ident = MaskOp()
    masks  = []                                    # partition-id order: BASE(0), CAP(1), WALL(2), FLOOR(3)
    for mo in (mask_base or _ident, mask_cap or _ident, mask_wall or _ident):
      masks += mo.triple()
    if keep_base:                                  # the re-closed footprint inherits the parent's tags but
      masks += remove(group(slot)).triple()        # CLEARS the slot bit (so a later same-slot pass won't re-extrude it)
    m.set_masks(masks)
    self._add(m, "extrude")
    self.graphdata.connect(m.inputs.In, src.outputs.Out)
    m.inputs.distance = 1.0 if isinstance(distance, SelExpr) else float(distance)
    return m

  # ---- inset: replace each selected face (bit `slot`) with an INSET — a smaller inner polygon plus a collar
  #      ring bridging the boundary to it. Coplanar (no lift). The standard modeling "inset", generalized:
  #      `sides` resamples the inner loop to a regular N-gon (quad->octagon etc.), `fill=False` leaves a hole. ----
  def inset(self, src, amount=0.3, sides=None, fill=True, rotate=0.0, slot=0, mask_inner=None, mask_collar=None,
            precheck=True, postcheck=True, cpu=False):   # cpu=False: GPU build+positions (C.4c default); True = CPU reference
    # amount : how far in (0 = on the boundary, ->1 = collapsed to center). RUNTIME plug -> animate, no recompile.
    # sides  : None = keep the face's side count (a plain inset); N = resample the inner loop to an N-gon. BAKED.
    # fill   : True = inner poly is a face (an extrudable cap); False = leave it open (a hole). BAKED.
    # rotate : (sides!=None only) orientation of the resampled N-gon in DEGREES about the face normal, measured
    #          from the face's first-edge (u) axis. EXPLICIT (default 0 = a ring vertex on +u; mirror-symmetric
    #          about both axes of a rectangular face). BAKED.
    # mask_inner / mask_collar : MaskOps tagging the inner cap / the collar ring (tag the cap so the next
    #          extrude can slot=it, exactly like extrude's mask_cap). RUNTIME (FaceTagger).
    m = _lev2.hypermesh.Inset.createShared()
    m.slot  = int(slot) & 31
    m.fill  = bool(fill)
    m.precheck  = bool(precheck)    # assert if the INPUT has degenerate faces
    m.postcheck = bool(postcheck)   # assert if the inset/collar CREATED open/non-manifold edges
    m.cpu       = bool(cpu)         # True = pure-CPU reference path (correct); False = GPU
    _ident = MaskOp()
    masks  = []                                    # partition-id order: OUTER(0), COLLAR(1), INNER(2)
    for mo in (_ident, mask_collar or _ident, mask_inner or _ident):
      masks += mo.triple()
    m.set_masks(masks)
    self._add(m, "inset")
    self.graphdata.connect(m.inputs.In, src.outputs.Out)
    # amount + rotate: RUNTIME float plugs (set m.inputs.amount / m.inputs.rotate in onUpdate to animate, no rebuild).
    # sides: int plug (0 = keep face's count; N>=3 = resample to an N-gon) -> changing it triggers a rebuild.
    m.inputs.amount = float(amount)
    m.inputs.rotate = float(rotate)
    m.inputs.sides  = 0 if sides is None else max(3, int(sides))
    return m

  # ---- recompute the NORMAL + BINORMAL channels per vertex from the CURRENT positions (tracks deformation).
  #      smooth = area-weighted average over the faces sharing each vertex (smooth shading; for SHARED meshes
  #      — vertex-mode bosses, the raw sphere); flat = the vertex's face normal (for UN-SHARED / face-mode
  #      output). UV-aligned binormal (tangent = cross(N,B)). Passthrough (aliases position/uv/color/topo). ----
  def _normals(self, src, smooth, slot):
    m = _lev2.hypermesh.Normals.createShared()
    m.smooth = bool(smooth)
    m.slot   = -1 if slot is None else (int(slot) & 31)
    self._add(m, "normals")
    self.graphdata.connect(m.inputs.In, src.outputs.Out)
    return m

  def smooth_normals(self, src, slot=None):
    """SMOOTH shading: weld coincident verts -> area-weighted average + UV binormal. `slot` restricts to the
    faces tagged in that __tags bit (unselected pass through; the selection border becomes a hard crease)."""
    return self._normals(src, smooth=True, slot=slot)

  def face_normals(self, src, slot=None):
    """FLAT shading: unweld (each face its own verts) -> per-face normal + UV binormal. `slot` restricts to
    the tagged faces (unselected pass through; the border is a crease — the split verts already make it one)."""
    return self._normals(src, smooth=False, slot=slot)

  def transform(self, src, matrix=None, translate=(0, 0, 0), rotate=None, scale=(1, 1, 1),
                pivot=(0, 0, 0), slot=None):
    # Affine transform of a vertex selection (verts of faces tagged `slot`; None = whole mesh). The matrix
    # is composed HERE in Python (axis-angle quats) and stored as the module's `matrix` PARAM -> animate by
    # re-setting m.matrix in onUpdate (no recompile). Either pass a ready `matrix` (core.mtx4) or the
    # translate/rotate/scale/pivot convenience: rotate = a core.quat, or (axis_vec3, degrees), or a list of
    # those (composed left-to-right). Scale/rotate happen about `pivot`, then translate.
    from orkengine.core import vec3, quat, mtx4
    import math
    m = _lev2.hypermesh.Transform.createShared()
    m.slot = -1 if slot is None else (int(slot) & 31)
    if matrix is not None:
      m.matrix = matrix
    else:
      def _toquat(r):
        if r is None:                       return quat()
        if isinstance(r, quat):             return r
        axis, deg = r                       # (axis_vec3, degrees)
        return quat(vec3(*axis) if not isinstance(axis, vec3) else axis, math.radians(deg))
      q = quat()
      for r in (rotate if isinstance(rotate, (list, tuple)) and rotate and
                isinstance(rotate[0], (quat, list, tuple)) else [rotate]):
        q = q * _toquat(r)
      rs = mtx4.composed(vec3(0, 0, 0), q, vec3(*scale))     # rotate*scale about origin
      pv = vec3(*pivot)
      Tp  = mtx4(); Tp.compose(pv, quat(), 1.0)
      Tpi = mtx4(); Tpi.compose(pv * -1.0, quat(), 1.0)
      Tt  = mtx4(); Tt.compose(vec3(*translate), quat(), 1.0)
      m.matrix = Tt * Tp * rs * Tpi
    self._add(m, "transform")
    self.graphdata.connect(m.inputs.In, src.outputs.Out)
    return m

  # ---- GENERIC per-vertex GPU compute: run an arbitrary fxv2 compute kernel (shadertext) over the
  #      input mesh's vertices. The kernel binds iP@0 (input POSITION), oP@1 (produced POSITION),
  #      EXPRP@2 (8 vec4 runtime params), control@3 ({uint p_nv; uint p_mode; ...}). Topology + every
  #      OTHER channel pass through; only POSITION is produced (chain smooth_normals to refresh shading).
  #      New per-vertex behaviors (ripple/twist/bend/noise-displace) are authored as shader TEXT here —
  #      NO new C++ (the GpuCompute counterpart of terrain's ExprModule). `params` = up to 8 (x,y,z,w)
  #      tuples seeded into the exprp plugs (poke m.set_expr_param4(slot,x,y,z,w) to animate live);
  #      `time_slot` >= 0 = the EXPRP slot the module feeds from the C++ clock (abstime/dt -> .x/.y). ----
  def gpu_compute(self, src, shadertext, kernel="cs_main", params=None, time_slot=None):
    m = _lev2.hypermesh.GpuCompute.createShared()
    m.shadertext = str(shadertext)
    m.kernel     = str(kernel)
    if time_slot is not None:
      m.time_slot = int(time_slot)
    if params:
      if len(params) > 8:
        raise ValueError("gpu_compute: at most 8 vec4 runtime params (got %d)" % len(params))
      flat = []
      for p in params:
        p = list(p) + [0.0] * (4 - len(p))
        flat += [float(p[0]), float(p[1]), float(p[2]), float(p[3])]
      m.set_expr_params(flat)
    self._add(m, "gpu_compute")
    self.graphdata.connect(m.inputs.In, src.outputs.Out)
    return m

  # ---- E.1 CROSS-FAMILY: displace vertex positions by a TERRAIN FIELD authored in the SAME graph.
  #      `field` is a TerrainNode — the output of T.* ops / TerrainNode algebra called while this
  #      Hypermesh is composing (its trace context routes them into this graph):
  #          fld = T.fbm(frequency=6.0) * 0.5
  #          d   = m.displace(grid, field=fld, amount=2.0, extent=8.0)
  #      Mapping: the terrain planar convention — uv = clamp(P.xz/extent + 0.5) (extent = the XZ
  #      span the field covers, centered at the origin), bilinear-sampled. mode "normal" lifts
  #      along the vertex normal, "y" along world +Y. amount/extent are RUNTIME plugs (animate
  #      free). Only POSITION changes — chain smooth_normals/face_normals to refresh shading.
  #      `field_dim` is the field subgraph's bake resolution — CARRIED BY THIS MODULE (the
  #      consumer that brings terrain into the graph; the generic drivers know nothing of it). ----
  _DISPLACE_MODES = {"normal": 0, "y": 1}
  def displace(self, src, field, amount=1.0, extent=8.0, mode="normal", field_dim=512):
    if not hasattr(field, "output_plug"):
      raise TypeError(
          "displace(field=...) expects a terrain field node (a T.* op result traced into this "
          "hypermesh graph); got %s" % type(field).__name__)
    if mode not in self._DISPLACE_MODES:
      raise ValueError("displace mode must be 'normal' or 'y', got %r" % (mode,))
    m = _lev2.hypermesh.DisplaceByField.createShared()
    m.mode      = self._DISPLACE_MODES[mode]
    m.field_dim = int(field_dim)
    self._add(m, "displace")
    self.graphdata.connect(m.inputs.In, src.outputs.Out)
    self.graphdata.connect(m.inputs.Field, field.output_plug)
    m.inputs.amount = float(amount)
    m.inputs.extent = float(extent)
    return m

  # ---- E.2 TYPED INSTANCE EDGE: bring a baked ScatterSet into the graph as an InstanceSet.
  #      The render draws the graph's terminal mesh ONCE per instance in a single indirect call,
  #      placed by the set's matrices; per-instance data (x=type_id, y=seed01) reaches the FS as
  #      frg_clr through the typed attrs SSBO (the matrix-bottom-row smuggle is retired).
  #      Reference the set PORTABLY (scatter_asset= the HeightField asset name + sink= the
  #      scatter sink — resolves to <assetcache>/terrain/<asset>/<sink>.ogeo, which the C++
  #      HF materializer places at load) or directly (ogeo_path=, tools/viewers).
  #      type_id filters to one type (one typed hypermesh node per type). ----
  def instance_source(self, scatter_asset="", sink="", ogeo_path="", type_id=None):
    if not ogeo_path and not (scatter_asset and sink):
      raise ValueError("instance_source needs scatter_asset= + sink= (portable) or ogeo_path= (direct)")
    # REPLACE semantics: one InstanceSet per graph (the live driver's discovery is last-wins;
    # two sources would silently shadow). Re-calling updates the existing module — so a shared
    # asset re-materialized per type (install_hypermeshes) re-filters between materialize calls.
    m = getattr(self, "_instsrc", None)
    if m is None:
      m = _lev2.hypermesh.ScatterSource.createShared()
      # NOT self._add — an InstanceSet output must never become the graph's mesh terminal;
      # the live driver discovers it by plug type.
      self.graphdata.addModule(m, "instsrc_%d" % self._n)
      self._n += 1
      self._instsrc = m
    m.scatter_asset = str(scatter_asset)
    m.sink          = str(sink)
    m.ogeo_path     = str(ogeo_path)
    m.type_id       = -1 if type_id is None else int(type_id)
    return m

  def delete_faces(self, src, slot=0):
    # Drop the faces tagged with `slot` (from an upstream select). GPU-native (shared count->scan->scatter):
    # deleted faces become zero-length in the output CSR (the render skips them) and the surviving corners
    # are packed; num_faces is unchanged (a later `compact` repacks). No selection channel -> no-op.
    m = _lev2.hypermesh.DeleteFaces.createShared()
    m.slot = int(slot) & 31
    self._add(m, "delete_faces")
    self.graphdata.connect(m.inputs.In, src.outputs.Out)
    return m

  def mirror(self, src, axis="x", weld=True, eps=1e-4):
    # Reflect across the X/Y/Z=0 plane and append the reversed-winding copy (model-half -> symmetric whole).
    # weld=True shares the on-plane seam verts (one loop, smooth across); else duplicates them (crease).
    # GPU-native (shared scan compacts the welded mirror-vert indices). Pair with delete_faces to cut a half.
    m = _lev2.hypermesh.Mirror.createShared()
    m.axis = {"x": 0, "y": 1, "z": 2}[axis] if isinstance(axis, str) else int(axis)
    m.weld = bool(weld)
    m.eps  = float(eps)
    self._add(m, "mirror")
    self.graphdata.connect(m.inputs.In, src.outputs.Out)
    return m

  def compact(self, src):
    # Garbage-collect ORPHAN vertices (left by delete_faces / mirror's over-allocation) and remap vidx.
    # GPU-native (shared scan); faces/attrs unchanged. Reads back one uint (the new vert count). Drop this
    # after a delete/mirror chain to tighten the mesh. (Face compaction is a follow-up.)
    m = _lev2.hypermesh.Compact.createShared()
    self._add(m, "compact")
    self.graphdata.connect(m.inputs.In, src.outputs.Out)
    return m

  def output(self, node):
    self._terminal = node
    return node

  @property
  def is_animated(self):
    """True if this asset needs per-frame graph re-evaluation: it defines its own onUpdate (plug pokes),
    OR it uses S.time (driven by the C++ clock via the extrude time_slot bridge), OR it carries a
    time-driven terrain field (a T.* op with offset_vel != 0 — the E.1b env-clock pan)."""
    return (hasattr(self, "onUpdate")
            or (getattr(self, "_time_param", None) is not None)
            or bool(getattr(self, "_field_animated", False)))

  def generatedflow(self):
    self._close_trace()        # composition is over (idempotent — the HeightField contract)
    return self.graphdata

  def materialize(self, ctx, vtx_budget=1 << 20):
    self._close_trace()
    return _lev2.hypermesh.materialize(self.graphdata, ctx, vtx_budget)

  def materialize_live(self, ctx, vtx_budget=1 << 20):
    # persistent GraphInst; call .recompute(ctx) per frame after changing a module's plug.
    self._close_trace()
    return _lev2.hypermesh.materialize_live(self.graphdata, ctx, vtx_budget)

  def save(self, path):
    """Serialize the traced graphdata to JSON at `path` — THE portable artifact (ratified decision #3:
    persist the trace OUTPUT — post-trace GLSL predicates, baked matrices, flat mask/param vectors —
    never the Python recipe). A saved graph reloads + bakes with NO DSL re-run via load_graphdata();
    imperative onUpdate is app logic and is deliberately NOT part of the artifact."""
    self._close_trace()
    js = self.graphdata.serializeJson()
    with open(path, "w") as f:
      f.write(js)
    return path


def load_graphdata(path):
  """Deserialize a Hypermesh.save() artifact -> dflow graphdata, with NO Python DSL re-run. Feed it to
  materialize_graph()/materialize_live_graph() (or a future C++ host) to bake/render."""
  from orkengine.core import Object
  with open(path) as f:
    return Object.deserializeJson(f.read())


def materialize_graph(graphdata, ctx, vtx_budget=1 << 20):
  """Bake a (possibly deserialized) hypermesh graphdata -> GpuMesh (one-shot)."""
  return _lev2.hypermesh.materialize(graphdata, ctx, vtx_budget)


def materialize_live_graph(graphdata, ctx, vtx_budget=1 << 20):
  """LIVE-materialize a (possibly deserialized) hypermesh graphdata (persistent GraphInst)."""
  return _lev2.hypermesh.materialize_live(graphdata, ctx, vtx_budget)

###############################################################################
# render: FWD_SSBO_CUSTOM multi-block SoA vertex source (P in sif_ptex_vtx, N/B/uv/color in extra
# blocks). The mesh is INDEXED: a render-owned on-GPU compute (setupMeshRender) fan-triangulates the
# faces into a tri vert-index buffer + a DrawIndexedIndirect command, so the pull VS just reads vert
# attrs by gl_VertexID (== gl_VertexIndex, the index-aware builtin -> dereferences the index buffer).
###############################################################################

# --- VERTEX DISPLACE (opt-in, generalized) ----------------------------------------------------------
# A VertexDisplace primitive is a DSL-authored object-space vertex deformation that the render source
# GENERATES into the pull VS (it appends `position.xyz += f(...)` before the mvp transform). It is OFF
# by default — wind/ripple/etc. make no sense for most hypermeshes, so you only attach one explicitly:
#   GpuMeshRenderSource(vtx_displace=Wind(amp=0.06, freq=2.0, dir=(1,0,0)))   # a tree
#   GpuMeshRenderSource(vtx_displace=[Wind(...), Ripple(...)])                 # composable
# Protocol (each primitive implements): glsl_func() = a pure lib function; params() = the bindable
# uniforms it reads as [(name, gtype, default)] specs (merged into the material's ublk_ptex_params);
# glsl_call() = the VS-body line. NOTHING is baked — values ride as uniforms, so the generated shader
# text is value-INDEPENDENT (changing amp/freq/dir is a param rebind, not a recompile). A param whose
# default is a provider token (_tokens.RCFD_TIME, a core CrcStringProxy) is fed per-frame BY THE ENGINE via the
# standard fx-pipeline provider — so a clock-driven displace needs ZERO per-frame host code anywhere
# (viewer, scene, zero-Python player all animate identically). The mesh stays STATIC + cacheable.
class VertexDisplace:
  wants_inst_data = False          # True -> the codegen exposes a `vec4 inst_data` local to glsl_call:
                                   # the per-instance attr (_instance_attrs[gl_InstanceIndex]) in the
                                   # INSTANCED VS, vec4(0) otherwise. For per-tree modulation (wind phase).
  def glsl_func(self): return ""   # lib function(s), uniform-free (values passed as args)
  def params(self):    return []   # [(name, gtype, default), ...] -> ublk_ptex_params members
  def glsl_call(self): return ""   # one line, modifies `position` (reads the params below + inst_data)


class Wind(VertexDisplace):
  """Time-varying horizontal sway, weighted by height so the trunk base stays planted and the canopy
  moves most (a cantilever shear). amp = radians-ish per unit height at peak; freq = oscillation rate;
  dir = world sway direction. The clock is the standard RCFD_TIME provider (the scene clock, fed by the
  engine each frame) — no per-frame host code. PER-TREE variation (instanced): the per-instance attr
  vec4 carries (phase01, amp_delta, freq_delta) -> each tree sways at its own phase/amp/freq off the
  material base (0 = neutral, so a single non-instanced tree is unchanged). Set it in the instance
  matrix bottom row (m[0..2].w) -> _instance_attrs.xyz."""
  wants_inst_data = True
  def __init__(self, amp=0.06, freq=2.0, dir=(1.0, 0.0, 0.0)):
    self.amp = float(amp); self.freq = float(freq); self.dir = tuple(float(c) for c in dir)
  def params(self):
    # WindDir/WindParams default to the DSL values (bound ONCE at material creation, like albedo).
    # Time's default is the RCFD_TIME provider TOKEN: the engine binds the scene clock into it every
    # frame through fx_pipeline's named-param providers — the SAME standard path as MatMVP/modcolor.
    # (fxv2_template pads every ublk_ptex_params member to vec4; Time is read as .x in glsl_call.)
    return [("WindDir",    "vec4",  (self.dir[0], self.dir[1], self.dir[2], 0.0)),
            ("WindParams", "vec4",  (self.amp, self.freq, 0.0, 0.0)),
            ("Time",       "float", _tokens.RCFD_TIME)]
  def glsl_func(self):
    return ("vec3 hm_wind(vec3 p, float h, vec3 dir, float amp, float freq, float t, vec3 inst){\n"
            "  // per-tree: inst.x = phase (cycles), inst.y/z = amp/freq DELTAS (0 = neutral).\n"
            "  amp  *= (1.0 + inst.y);\n"
            "  freq *= (1.0 + inst.z);\n"
            "  return dir * (amp * h * sin(t*freq + inst.x*6.2831853 + p.x*0.3 + p.z*0.3));\n}")
  def glsl_call(self):
    return ("position.xyz += hm_wind(position.xyz, max(position.y,0.0), "
            "WindDir.xyz, WindParams.x, WindParams.y, Time.x, inst_data.xyz);")


class LeafFlutter(VertexDisplace):
  """Per-LEAF high-frequency flutter LAYERED ON TOP of the bulk Wind (compose them:
  vtx_displace=[Wind(...), LeafFlutter(...)]). Flaps each card along its OWN normal, scaled by the tip
  weight (COLOR.x: 0 at the petiole .. 1 at the tip, so the blade flexes and the base stays put) and
  phased per-leaf (COLOR.y hash) so neighbouring leaves desync — the canopy shimmers RELATIVE to the
  branches it rides. Same RCFD_TIME clock as Wind (the shared Time UBO member dedups). amp = flap depth
  (world units at the tip), freq = shimmer rate."""
  def __init__(self, amp=0.03, freq=8.0):
    self.amp = float(amp); self.freq = float(freq)
  def params(self):
    return [("FlutterParams", "vec4",  (self.amp, self.freq, 0.0, 0.0)),
            ("Time",          "float", _tokens.RCFD_TIME)]
  def glsl_func(self):
    return ("vec3 hm_flutter(vec3 nrm, float w, float ph, float amp, float freq, float t){\n"
            "  // w = tip weight (0 base..1 tip); ph = per-leaf phase hash. flap along the card normal.\n"
            "  return nrm * (amp * w * sin(t*freq + ph*6.2831853));\n}")
  def glsl_call(self):
    return ("position.xyz += hm_flutter(normal, vtxcolor.x, vtxcolor.y, "
            "FlutterParams.x, FlutterParams.y, Time.x);")


class HmMaterial:
  """A material a hypermesh asset declares for ITSELF (so the viewer / a scene render it without the
  caller authoring one). `material_cls` = a ptex3d material class (None -> Solid). `vtx_displace` = an
  optional VertexDisplace (e.g. Wind(...)) for VS-side vertex animation. `gid` (optional, int) = the
  assign_gid bucket this material paints — declaring MULTIPLE HmMaterials with distinct gids PARTITIONS
  the mesh into separate per-gid draws. albedo/roughness/metallic = surface knobs (None -> default)."""
  def __init__(self, material_cls=None, *, albedo=None, roughness=0.55, metallic=None,
               vtx_displace=None, gid=None, name="asset"):
    self.material_cls = material_cls
    self.albedo       = albedo
    self.roughness    = roughness
    self.metallic     = metallic
    self.vtx_displace = vtx_displace
    self.gid          = gid
    self.name         = name


class GpuMeshRenderSource:
  """The VERTEX side of the hypermesh render: the FWD_SSBO_CUSTOM pull VS that reads P/N/B/uv/color
  from the SoA channel SSBOs by gl_VertexID (== gl_VertexIndex for the indexed draw). Material-agnostic
  — the surface fragment comes from whichever ptex3d material the asset selects (Solid, TopoView, ...).
  `instanced=True` ALSO generates the FWD_SSBO_CUSTOM_INSTANCED technique: each vertex is placed by a
  per-instance matrix (storage_inst_mtx[gl_InstanceIndex]); the matrix bottom row carries 3 data floats
  -> frg_clr. The same SoA geometry is shared by all instances (one graph eval, one triangulate, N draws).
  `vtx_displace=` (None | a VertexDisplace | list) opts a DSL-authored object-space VS deformation in."""
  def __init__(self, instanced=False, vtx_displace=None):
    self._instanced = bool(instanced)
    if vtx_displace is None:
      self._vtx_displace = []
    else:
      self._vtx_displace = list(vtx_displace) if isinstance(vtx_displace, (list, tuple)) else [vtx_displace]

  def displace_params(self):
    """[(name, gtype, default), ...] across all displace primitives -> merged into the material's
    ublk_ptex_params (the bindable block the VS inherits). Static specs (WindDir/WindParams) bind once
    at creation; a provider-token default (Time = tokens.RCFD_TIME) is fed per-frame by the engine via
    fx_pipeline's named-param providers — no per-frame host code, no displace-specific render plumbing.
    Params shared by several primitives (notably Time) collapse to ONE UBO member (dedup by name)."""
    seen, out = set(), []
    for d in self._vtx_displace:
      for spec in d.params():
        if spec[0] in seen:
          continue
        seen.add(spec[0])
        out.append(spec)
    return out

  def _decode(self, index_expr):
    """The per-vertex decode, parameterized by the INDEX EXPRESSION. The pull VS reads
    gl_VertexID (the indexed draw renames it to gl_VertexIndex, i.e. the index buffer's vertex);
    the mesh stage reads the meshlet's vertex-list entry. ONE decode, two callers — so a channel
    added here reaches both paths or neither."""
    return (
      "uint i = %s;\n" % index_expr +
      "vec4 position = Pd[i];\n"
      "vec3 normal   = Nd[i].xyz;\n"
      "vec3 binormal = Bd[i].xyz;\n"
      "vec2 uv0      = UVd[i].xy;\n"
      "float uv0z    = UVd[i].z;\n"   # O3: SectionUnwrap's per-section LAYER index -> forwarded to frg_uv0z (ctx.layer)
      "vec4 vtxcolor = Cd[i];")

  def as_material_kwargs(self):
    from .gpu_meshlet import MeshletMeshSource, meshmode
    extra = (
      "storage_interface sif_N   (descriptor_set 0) { buffer layout(std430) hm_nb { vec4 Nd[];  }; }\n"
      "storage_interface sif_B   (descriptor_set 0) { buffer layout(std430) hm_bb { vec4 Bd[];  }; }\n"
      "storage_interface sif_uv  (descriptor_set 0) { buffer layout(std430) hm_ub { vec4 UVd[]; }; }\n"
      "storage_interface sif_clr (descriptor_set 0) { buffer layout(std430) hm_cb { vec4 Cd[];  }; }\n")
    inherits = ["sif_N", "sif_B", "sif_uv", "sif_clr"]
    body = self._decode("uint(gl_VertexID)")
    lib = ""
    displace = ""
    if self._vtx_displace:  # opt-in: the displace uniforms live in ublk_ptex_params (a REAL bindable param
                        # block — pipeline block-state + bindParam by name; see displace_params()). The VS
                        # inherits that block, plus the pure lib funcs + the appended position calls.
      inherits.append("ublk_ptex_params")
      lib = "\n".join(d.glsl_func() for d in self._vtx_displace)
      displace = "\n".join(d.glsl_call() for d in self._vtx_displace)
      body += "\n" + displace
    wants_inst = any(getattr(d, "wants_inst_data", False) for d in self._vtx_displace)
    mesh_source = None
    if meshmode() >= 1:
      if self._instanced:
        # the mesh path draws ONE partition of ONE mesh (no per-instance matrix in the mesh stage),
        # so an instanced material never carries a mesh technique — and the drawable's capability
        # check then refuses LOUDLY instead of quietly rendering something else.
        print("[hypermesh] ORKID_HYPERMESH_MESHSHADER is on but this material is INSTANCED — "
              "no mesh technique emitted (pull-VS path stands)", flush=True)
      else:
        # the mesh stage decodes the SAME vertex, indexed through the meshlet's vertex list.
        mesh_source = MeshletMeshSource(vertex_decode=self._decode("MLVerts[v_off + vi]"),
                                        displace_calls=displace,
                                        wants_inst_data=wants_inst)
        # the source decides its own block set: the per-cluster bounds contract is present only when
        # the stage emits stored positions (a displacing stage cannot be bounded ahead of time).
        extra += mesh_source.blocks()
    return dict(
      ssbo_layout="vec4 Pd[];",   # P lives in sif_ptex_vtx (runtime array; indexed draw -> gl_VertexIndex)
      ssbo_extra_blocks=extra,
      ssbo_lib=lib,               # displace functions (empty when no displace -> output unchanged)
      ssbo_vs_inherits=tuple(inherits),
      ssbo_vs_body=body,
      ssbo_instanced=self._instanced,
      # any displace that reads per-instance data (Wind: per-tree phase/amp/freq) -> the codegen
      # exposes `inst_data` to the VS body (the per-instance attr when instanced, vec4(0) otherwise).
      ssbo_wants_inst_data=wants_inst,
      # MESH PATH (ORKID_HYPERMESH_MESHSHADER): hand the template the meshlet mesh source so it
      # emits FWD_SSBO_CUSTOM_MESH (+ its depth-prepass twin) around the SAME varying contract.
      # None -> no mesh technique and text byte-identical to the pull-VS-only generator.
      mesh_source=mesh_source,
      ssbo_compute="")

# block name (in the generated material) -> GpuMesh vertex-channel id, in render-bind order.
_RENDER_CHANNELS = [("sif_ptex_vtx", P), ("sif_N", N), ("sif_B", B), ("sif_uv", UV), ("sif_clr", COLOR)]

# make_drawable(instance_from=...) default sentinel: UNSPECIFIED -> the drawable is NOT instanced by a
# graph-carried InstanceSet (explicit-scoping law — never a graph-wide any-set sniff). Callers that want
# the graph's set pass instance_from=live (or the ScatterSource); instance_from=None is the same as the
# default (explicit "un-instanced"), kept for readability at the road/mesh call sites.
_INSTANCE_AUTO = object()


class GpuMeshWireSource:
  """Vertex side for the LINE overlay/primitive: pull VS reads P + N and applies `bias` as a CLIP-SPACE
  depth bias toward the viewer (`gl_Position.z -= bias*gl_Position.w`, after projection). Position-only
  (so coincident/welded verts get the SAME offset -> shared edges stay shared) and always toward the
  camera. Reads P (sif_ptex_vtx) + N (sif_N). Pairs with the Lines material."""
  def __init__(self, bias=0.0, instanced=False):
    self._bias = float(bias)
    self._instanced = bool(instanced)
  def as_material_kwargs(self):
    return dict(
      ssbo_layout="vec4 Pd[];",
      ssbo_extra_blocks=(
        "storage_interface sif_N (descriptor_set 0) { buffer layout(std430) hm_nb { vec4 Nd[]; }; }\n"),
      ssbo_vs_inherits=("sif_N",),
      ssbo_vs_body=(
        "uint i = uint(gl_VertexID);\n"
        "vec4 position = Pd[i];\n"                          # NO object-space displacement (welded verts stay put)
        "vec3 normal   = Nd[i].xyz;\n"
        "vec3 binormal = vec3(0.0, 0.0, 1.0);\n"
        "vec2 uv0      = vec2(0.0);\n"
        "vec4 vtxcolor = vec4(1.0);"),
      ssbo_instanced=self._instanced,   # instanced overlay: place each line copy by its per-instance matrix
      # clip-space depth bias toward the viewer (after gl_Position is set) — position-only, view-uniform.
      ssbo_vs_post=("gl_Position.z -= float(%r) * gl_Position.w;" % self._bias),
      ssbo_compute="")


def make_drawable(live, ctx, *, animated=False, material_cls=None, roughness=0.55, albedo=None,
                  metallic=None, wireframe=False, wire_color=None, wire_bias=0.0006, instances=None,
                  vtx_displace=None, gid_materials=None, cull=False, cull_bound=None,
                  instance_from=_INSTANCE_AUTO):
  """ComputeDrawableData that renders a LIVE hypermesh through a ptex3d material (auto-selected
  FWD_SSBO_CUSTOM pipeline). Binds each vertex-channel SSBO to its block, then installs the on-GPU
  render-time triangulator + per-frame in-frame hook via setupMeshRender (which also sets the
  DrawIndexedIndirect index/args). `animated` re-evaluates the graph each frame. `albedo`/`roughness`/
  `metallic` override the material's surface knobs when given (ignored by group/face-viz materials whose
  FS post sets the surface). If `material_cls` declares WANTS_FACE_ID (e.g. TopoView), the per-triangle
  face-id buffer is wired + bound to the FS. Returns (cdd, material). Keep `live` alive.

  `instance_from` EXPLICITLY SCOPES the graph-carried InstanceSet (an instance_source/ScatterSource
  output that materialize discovers graph-wide, live.instance_count): the caller OPTS THIS drawable in
  by passing the set / the live graph (`instance_from=live` — the forest/scatter pairing); the default
  and `instance_from=None` render un-instanced even when the graph carries an unrelated set (a road
  ribbon sharing one graph with building-seed lots must NOT tile by the lot transforms). Never a
  graph-wide any-InstanceSet sniff."""
  from orkengine.lev2 import ComputeDrawableData
  from ork.hypergraph.ecs.scene.assets import Ptex3d as Ptex3dAsset
  if material_cls is None:
    from ork.hypergraph.assets.materials.terrain.solid import Solid as material_cls
  face_viz = bool(getattr(material_cls, "WANTS_FACE_ID", False))  # material opts into per-face id data
  tag_viz  = bool(getattr(material_cls, "WANTS_TAGS", False))     # material opts into the __tags channel
  if tag_viz:
    face_viz = True                                    # tag viz indexes __tags by the per-triangle face id
  kw = dict(roughness=roughness)
  if albedo is not None:
    kw["albedo"] = albedo
  if metallic is not None:
    kw["metallic"] = metallic
  # O3 stage 2 — a capture material (e.g. SectionArray) declares PTEX_MODE="stored"/PTEX_CAPTURE=True so its
  # forward is surface_stored() (samples the baked array) AND it emits the FWD_SSBO_CUSTOM_CAPTURE technique
  # the section bake driver renders through. Absent -> proc (unchanged for every non-capture material).
  _ptex_mode = getattr(material_cls, "PTEX_MODE", None)
  if _ptex_mode is not None:
    kw["mode"] = _ptex_mode
  if bool(getattr(material_cls, "PTEX_CAPTURE", False)):
    kw["capture"] = True
  # INSTANCES: a flat / (N,16) / (N,4,4) sequence of COLUMN-MAJOR mat4 floats -> N copies in ONE draw call,
  # each placed by its per-instance matrix (FWD_SSBO_CUSTOM_INSTANCED). The matrix bottom row (m[0..2].w)
  # carries 3 free per-instance data floats -> frg_clr. The geometry is shared (one graph eval, N draws).
  inst_floats, inst_count = [], 1
  if instances is not None:
    import numpy as _np
    _arr = _np.asarray(instances, dtype="float32").reshape(-1)
    if _arr.size == 0 or _arr.size % 16 != 0:
      raise ValueError("make_drawable(instances=): need N*16 column-major mat4 floats, got %d" % _arr.size)
    inst_floats, inst_count = _arr.tolist(), _arr.size // 16
  # E.2: a graph-carried InstanceSet (instance_source -> ScatterSource). It is discovered graph-wide
  # during materialize (live.instance_count), but is applied to THIS drawable ONLY when the caller
  # opts in via instance_from (truthy) — never a graph-wide sniff (a road ribbon must not tile by an
  # unrelated building-seed set). None / the default -> render un-instanced regardless of the set.
  has_graph_set = int(getattr(live, "instance_count", 0)) > 0
  opted_in = (instance_from is not _INSTANCE_AUTO) and (instance_from is not None)
  graph_instanced = has_graph_set and opted_in
  if graph_instanced and instances is not None:
    raise ValueError("make_drawable: instance_from opts into the graph InstanceSet AND instances= "
                     "floats were passed — one instance source per drawable")
  instanced = graph_instanced or inst_count > 1
  mesh = live.mesh
  wrap = Ptex3dAsset(dsl_class=material_cls,
                     vertex_source=GpuMeshRenderSource(instanced=instanced, vtx_displace=vtx_displace), **kw)
  wrap._ctx = ctx
  gmtl = wrap.as_gfx_material
  fs   = gmtl.freestyle
  cdd  = ComputeDrawableData()
  cdd.material = gmtl                                   # findPipeline(RCID,_isSSBOSourced) -> FWD_SSBO_CUSTOM[_INSTANCED]
  cdd.instanced = instanced                            # -> RCID._isInstanced (picks the instanced technique)
  for (blockname, chan) in _RENDER_CHANNELS:            # vertex channels -> graphics-storage 0..4
    cdd.addGraphicsStorage(fs.storage(blockname), mesh.channel_ssbo(chan))
  # on-GPU fan-triangulate -> DrawIndexedIndirect + the per-frame in-frame recompute/refresh hook.
  # wireframe also builds a per-edge LINE index buffer + configures the drawable's OVERLAY draw (below).
  # E.3 — per-gid bucket materials (the make_drawable port of the scene path): build each gid's
  # material like the main, collect bound_gids so setupMeshRender allocates a per-gid args slot.
  gid_mtls = {}
  if gid_materials:
    from ork.hypergraph.assets.materials.terrain.solid import Solid as _Solid
    for _g, _spec in gid_materials.items():
      _gkw = dict(roughness=getattr(_spec, "roughness", 0.55))
      if getattr(_spec, "albedo", None)   is not None: _gkw["albedo"]   = _spec.albedo
      if getattr(_spec, "metallic", None) is not None: _gkw["metallic"] = _spec.metallic
      # PER-GID vtx_displace: a gid material uses its OWN displace if it declared one, else inherits the
      # main's. CAVEAT: gid faces that SHARE verts with the main draw (e.g. an upper-branch split off the
      # trunk) MUST match the main's displace or the shared boundary verts TEAR — that's the author's
      # responsibility. SEPARATE geometry (leaf cards baked in) shares no verts, so it can carry its own
      # displace (e.g. + LeafFlutter) freely. None -> inherit the main displace (safe default).
      _gvd = getattr(_spec, "vtx_displace", None)
      if _gvd is None:
        _gvd = vtx_displace
      _gwrap = Ptex3dAsset(dsl_class=(getattr(_spec, "material_cls", None) or _Solid),
                           vertex_source=GpuMeshRenderSource(instanced=instanced, vtx_displace=_gvd), **_gkw)
      _gwrap._ctx = ctx
      gid_mtls[int(_g)] = _gwrap.as_gfx_material
  # E.4 per-view GPU frustum cull (instanced only): cull_bound = object-space sphere (cx,cy,cz,r);
  # None / w<=0 -> setupMeshRender AUTO-computes it from a one-time mesh position readback (+5% pad).
  from orkengine.core import vec4 as _vec4
  _cb = _vec4(0, 0, 0, 0) if cull_bound is None else (
        cull_bound if isinstance(cull_bound, _vec4) else _vec4(*cull_bound))
  # on-GPU fan-triangulate -> DrawIndexedIndirect + the per-frame in-frame recompute/refresh hook.
  # use_graph_instances is only NEEDED to SUPPRESS a graph-carried set the caller did not opt
  # into (the road-beside-seeds case); the default C++ behavior already matches the other cases
  # (no set, or opted in). Pass it only when suppressing, and tolerate a pre-fix binary that
  # lacks the kwarg (retry once, loud) so the adapter runs before the pyext is rebuilt.
  _smr_kw = {}
  if has_graph_set and not opted_in:
    _smr_kw["use_graph_instances"] = False
  try:
    triface, instmtx, instattr = _lev2.hypermesh.setupMeshRender(
        cdd, live, ctx, animated, face_viz, tag_viz, wireframe,
        instance_count=inst_count, instance_matrices=inst_floats, bound_gids=sorted(gid_mtls.keys()),
        cull=bool(cull), cull_bound=_cb, **_smr_kw)
  except TypeError:
    if not _smr_kw:
      raise
    print("[hypermesh] make_drawable: setupMeshRender lacks use_graph_instances (pyext not rebuilt) "
          "— this drawable will be instanced by the graph's InstanceSet (scope fix inert until rebuild)",
          flush=True)
    triface, instmtx, instattr = _lev2.hypermesh.setupMeshRender(
        cdd, live, ctx, animated, face_viz, tag_viz, wireframe,
        instance_count=inst_count, instance_matrices=inst_floats, bound_gids=sorted(gid_mtls.keys()),
        cull=bool(cull), cull_bound=_cb)
  if face_viz:                                          # per-triangle face-id buffer -> FS (graphics-storage 5)
    cdd.addGraphicsStorage(fs.storage("sif_triface"), triface)
  if tag_viz:                                           # the __tags FACE channel -> FS (graphics-storage 6)
    cdd.addGraphicsStorage(fs.storage("sif_seltags"), mesh.face_ssbo("__tags"))
  if instanced and instmtx is not None:                 # per-instance matrices + TYPED attrs (E.2) -> the VS blocks.
    cdd.addGraphicsStorage(fs.storage("storage_inst_mtx"), instmtx)  # bound LAST (the live-refresh loop only touches <=slot 6)
    if instattr is not None:                            # vec4[i]: x=type_id, y=seed01 -> frg_clr (replaces the bottom-row smuggle)
      cdd.addGraphicsStorage(fs.storage("storage_inst_attr"), instattr)
  if gid_mtls:   # E.3: one extra indexed-indirect draw per bound gid, each with its OWN material + storage
    _lev2.hypermesh.addGidBuckets(cdd, live, gid_mtls, instanced, instmtx, instattr)
  if wireframe:                                         # OVERLAY: draw the polygon edges as flat LINES on top
    from orkengine.core import vec3 as _vec3
    from ork.hypergraph.assets.materials.hypermesh.lines import Lines
    wc    = wire_color if wire_color is not None else _vec3(0.0, 0.0, 0.0)
    wwrap = Ptex3dAsset(dsl_class=Lines, vertex_source=GpuMeshWireSource(wire_bias, instanced=instanced), color=wc)
    wwrap._ctx = ctx
    wmtl  = wwrap.as_gfx_material                        # flat Lines material drawn as the overlay (LINES)
    wfs   = wmtl.freestyle
    cdd.overlay_material = wmtl
    cdd.addOverlayGraphicsStorage(wfs.storage("sif_ptex_vtx"), mesh.channel_ssbo(P))  # overlay slot 0 = P
    cdd.addOverlayGraphicsStorage(wfs.storage("sif_N"),        mesh.channel_ssbo(N))  # overlay slot 1 = N (bias/shade)
    if instanced and instmtx is not None:               # place each LINE copy by its per-instance matrix too
      cdd.addOverlayGraphicsStorage(wfs.storage("storage_inst_mtx"), instmtx)  # overlay slot 2 (refresh touches <=1)
      if instattr is not None:
        cdd.addOverlayGraphicsStorage(wfs.storage("storage_inst_attr"), instattr)  # overlay slot 3
  # the SHARED vtx_displace (above) gives every gid bucket material the same Time provider param, so a
  # clock-driven deformation (Wind) sways all buckets identically (no tear) with NO per-frame host code.
  return cdd, gmtl
