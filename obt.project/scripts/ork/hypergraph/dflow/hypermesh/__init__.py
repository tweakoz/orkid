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
from orkengine.core import dataflow as _dflow
from orkengine import lev2 as _lev2

# vertex-channel semantic ids (match the C++ MeshChannel enum order)
P, N, B, UV, COLOR = 0, 1, 2, 3, 4

# the selection DSL (SelExpr atoms/builders + MaskOp factories) — re-export so assets can write
#   from ork.hypergraph.dflow.hypermesh import S, sel_normal_dir, group, replace, add, POLY
from ork.hypergraph.dflow.hypermesh.selexpr import (  # noqa: F401
  POINT, POLY, LINE, SelExpr, S,
  sl_smoothstep, sl_step, sl_clamp, sl_min, sl_max, sl_sin, sl_cos, sl_fract, sl_select, vexpr, param, collect_params,
  sel_normal_dir, sel_id_range, sel_area_gt, sel_dihedral_gt, sel_length_gt, sel_dist_point, sel_height_band,
  MaskOp, group, groups, add, remove, toggle, isolate, replace, _bind_build_asset)

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
    if isinstance(distance, SelExpr):
      m.dist_predicate = _field_predicate(distance, "_dist", prefix="_sd")
    if _inset_active:
      m.inset_predicate = _field_predicate(inset, "_inset", prefix="_si")
    if direction is not None:
      m.dir_predicate = _field_predicate(direction, "_dir", prefix="_sr")
    if _twist_active:
      m.twist_predicate = _field_predicate(twist, "_twist", prefix="_st")
    if _scale_active:
      m.scale_predicate = _field_predicate(scale, "_scale", prefix="_sc")
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

class GpuMeshRenderSource:
  """The VERTEX side of the hypermesh render: the FWD_SSBO_CUSTOM pull VS that reads P/N/B/uv/color
  from the SoA channel SSBOs by gl_VertexID (== gl_VertexIndex for the indexed draw). Material-agnostic
  — the surface fragment comes from whichever ptex3d material the asset selects (Solid, TopoView, ...).
  `instanced=True` ALSO generates the FWD_SSBO_CUSTOM_INSTANCED technique: each vertex is placed by a
  per-instance matrix (storage_inst_mtx[gl_InstanceIndex]); the matrix bottom row carries 3 data floats
  -> frg_clr. The same SoA geometry is shared by all instances (one graph eval, one triangulate, N draws)."""
  def __init__(self, instanced=False):
    self._instanced = bool(instanced)
  def as_material_kwargs(self):
    return dict(
      ssbo_layout="vec4 Pd[];",   # P lives in sif_ptex_vtx (runtime array; indexed draw -> gl_VertexIndex)
      ssbo_extra_blocks=(
        "storage_interface sif_N   (descriptor_set 0) { buffer layout(std430) hm_nb { vec4 Nd[];  }; }\n"
        "storage_interface sif_B   (descriptor_set 0) { buffer layout(std430) hm_bb { vec4 Bd[];  }; }\n"
        "storage_interface sif_uv  (descriptor_set 0) { buffer layout(std430) hm_ub { vec4 UVd[]; }; }\n"
        "storage_interface sif_clr (descriptor_set 0) { buffer layout(std430) hm_cb { vec4 Cd[];  }; }\n"),
      ssbo_vs_inherits=("sif_N", "sif_B", "sif_uv", "sif_clr"),
      ssbo_vs_body=(
        "uint i = uint(gl_VertexID);\n"   # indexed draw: gl_VertexID renames to gl_VertexIndex (vert index)
        "vec4 position = Pd[i];\n"
        "vec3 normal   = Nd[i].xyz;\n"
        "vec3 binormal = Bd[i].xyz;\n"
        "vec2 uv0      = UVd[i].xy;\n"
        "vec4 vtxcolor = Cd[i];"),
      ssbo_instanced=self._instanced,
      ssbo_compute="")

# block name (in the generated material) -> GpuMesh vertex-channel id, in render-bind order.
_RENDER_CHANNELS = [("sif_ptex_vtx", P), ("sif_N", N), ("sif_B", B), ("sif_uv", UV), ("sif_clr", COLOR)]


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
                  metallic=None, wireframe=False, wire_color=None, wire_bias=0.0006, instances=None):
  """ComputeDrawableData that renders a LIVE hypermesh through a ptex3d material (auto-selected
  FWD_SSBO_CUSTOM pipeline). Binds each vertex-channel SSBO to its block, then installs the on-GPU
  render-time triangulator + per-frame in-frame hook via setupMeshRender (which also sets the
  DrawIndexedIndirect index/args). `animated` re-evaluates the graph each frame. `albedo`/`roughness`/
  `metallic` override the material's surface knobs when given (ignored by group/face-viz materials whose
  FS post sets the surface). If `material_cls` declares WANTS_FACE_ID (e.g. TopoView), the per-triangle
  face-id buffer is wired + bound to the FS. Returns (cdd, material). Keep `live` alive."""
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
  # E.2: a graph-carried InstanceSet (instance_source -> ScatterSource) instances AUTOMATICALLY —
  # the typed edge needs no instances= argument (setupMeshRender binds the set's own SSBOs).
  graph_instanced = int(getattr(live, "instance_count", 0)) > 0
  if graph_instanced and instances is not None:
    raise ValueError("make_drawable: the graph carries an InstanceSet (instance_source) AND instances= "
                     "floats were passed — one instance source per drawable")
  instanced = graph_instanced or inst_count > 1
  mesh = live.mesh
  wrap = Ptex3dAsset(dsl_class=material_cls, vertex_source=GpuMeshRenderSource(instanced=instanced), **kw)
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
  triface, instmtx, instattr = _lev2.hypermesh.setupMeshRender(
      cdd, live, ctx, animated, face_viz, tag_viz, wireframe,
      instance_count=inst_count, instance_matrices=inst_floats)
  if face_viz:                                          # per-triangle face-id buffer -> FS (graphics-storage 5)
    cdd.addGraphicsStorage(fs.storage("sif_triface"), triface)
  if tag_viz:                                           # the __tags FACE channel -> FS (graphics-storage 6)
    cdd.addGraphicsStorage(fs.storage("sif_seltags"), mesh.face_ssbo("__tags"))
  if instanced and instmtx is not None:                 # per-instance matrices + TYPED attrs (E.2) -> the VS blocks.
    cdd.addGraphicsStorage(fs.storage("storage_inst_mtx"), instmtx)  # bound LAST (the live-refresh loop only touches <=slot 6)
    if instattr is not None:                            # vec4[i]: x=type_id, y=seed01 -> frg_clr (replaces the bottom-row smuggle)
      cdd.addGraphicsStorage(fs.storage("storage_inst_attr"), instattr)
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
  return cdd, gmtl
