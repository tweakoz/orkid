#!/usr/bin/env python3
###############################################################################
# GRASS TASK-STAGE codegen gate — the ptex3d material a grass carpet gets, and the promise
# that nothing else moved to get it. Pure TEXT: no GPU, no window, no device, so it stays
# green on any machine and fails loudly on a broken splice even where nothing can render.
#
# What it proves:
#   1. OFF-NEUTRALITY — a mesh_source that declares NO task stage generates the material it
#      generated before the task extension existed, byte for byte (sha1 recorded against the
#      pre-extension generator). Every cached shader and every content-addressed bake re-keys
#      on one moved byte, so this is the load-bearing claim of the whole slice.
#   2. ADDITIVE SPLICE — the same grass source with the task stage off vs on differs by pure
#      INSERTIONS (the payload, the task interface, the two task shaders, the payload inherit
#      on each stage) plus exactly four one-line rewrites: the four mesh pass lines gaining
#      their amplifier, and nothing but the amplifier. The mesh path the grass already had is
#      not re-authored to acquire a task stage.
#   3. NO LEAK — the taskless mesh consumers in the tree (terrain, hypermesh) carry no task
#      text at all. A shader module is created for EVERY stage in a program at load time, so a
#      device without the taskShader feature must never be handed one it did not ask for.
#   4. CAP PARITY — the emitted output limits and workgroup size equal the source's structural
#      constants, and the declared output stays under MoltenVK's native-route threshold. A split
#      pair means the layout cannot hold the blades the body writes; a fat one means Metal
#      silently runs the stage on an emulation that restarts the render pass per draw.
#   5. FIELD TEXEL CONTRACT — the packed-normal channel is decoded the one way that means
#      anything (unpack the four taps, then interpolate), and dryness comes from the baked
#      channel through a single bias function at both granularities.
#   6. ONE TASK SHADER PER VIEW MODE — the colour technique and its depth-prepass twin name the
#      SAME task shader (and the _ST pair name the same _stereo one). Two amplifiers, or one
#      pass amplifying while the other does not, is a depth prepass that disagrees with its own
#      colour pass about which blades exist — z-fighting no fragment stage could see.
#
# Self-configuring: every source is constructed with explicit shape arguments, so no external
# environment variable can change what is generated here.
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys, re, difflib, hashlib

_ROOT = os.path.abspath(__file__)
for _ in range(5):
  _ROOT = os.path.dirname(_ROOT)
sys.path.insert(0, os.path.join(_ROOT, "obt.project", "scripts"))

from orkengine import core   # core before lev2
from orkengine import lev2

from ork.hypergraph.ptex3d.fxv2_template import generate_surface_fxv2
from ork.hypergraph.dflow.terrain.gpu_chunk import TerrainChunkVertexSource
from ork.hypergraph.dflow.hypermesh import GpuMeshRenderSource
from ork.hypergraph.dflow.hypermesh import gpu_meshlet
from ork.hypergraph.dflow import grass
from ork.hypergraph.dflow.grass import GrassFieldSource

BODY = ("o.albedo   = vec3(uv.x, uv.y, 0.5);\n"
        "o.emissive = (wnrm * 0.5 + vec3(0.5)) * 0.55;")

# sha1 of the TASKLESS terrain mesh material, recorded by running this exact generation against
# the generator as it stood BEFORE the task-stage extension was added to _mesh_block. Regenerate
# ONLY with a deliberate template contract change, and say so — never to quiet a red gate.
GOLDEN_TASKLESS = "ae2fcf8358816ca34212051e118260ebfe846d5d"

# every token that may appear only in a task-amplified material
TASK_TOKENS = ("task_payload", "task_interface", "task_shader", "EmitMeshTasksEXT",
               "pld_ptex_mesh", "tif_ptex_mesh", "ts_ptex_mesh")

G = {}


def terrain_taskless():
  """A taskless mesh_source material. Fully explicit shape (dim/extent/chunk/meshlet/mode/cull
  granularity) so no ambient ORKID_TERRAIN_* setting can move the digest."""
  src = TerrainChunkVertexSource(dim=1024, extent_m=512.0, chunk=128, meshlet=8,
                                 mesh=True, mesh_indirect=False, mesh_cull_chunk=False)
  return generate_surface_fxv2(BODY, **src.as_material_kwargs())


def grass_material(task=True):
  """The grass material with the task stage on (shipping) or off (the OFF arm of the splice
  test — the same geometry source, minus the one opt-in attribute)."""
  src = GrassFieldSource()
  if not task:
    src.wants_task_stage = False
  return generate_surface_fxv2(BODY, params=src.displace_params(), **src.as_material_kwargs())


###############################################################################

def test_taskless_is_byte_identical():
  off = terrain_taskless()
  G["terrain"] = off
  digest = hashlib.sha1(off.encode("utf-8")).hexdigest()
  print(f"  taskless terrain mesh material sha1={digest}")
  assert digest == GOLDEN_TASKLESS, \
      f"the task-stage extension moved the TASKLESS output: {digest} != recorded {GOLDEN_TASKLESS}"
  assert "FWD_SSBO_CUSTOM_MESH" in off, "the taskless arm lost its mesh technique"
  return True


def test_no_task_leak_into_taskless_materials():
  hm = generate_surface_fxv2(BODY, **GpuMeshRenderSource().as_material_kwargs())
  for label, text in (("terrain", G["terrain"]), ("hypermesh", hm)):
    for tok in TASK_TOKENS:
      assert tok not in text, f"{label} (taskless) leaked task text: {tok!r}"
  print("  terrain + hypermesh materials carry no task stage")
  return True


def test_task_splice_is_additive():
  off = grass_material(task=False)
  on  = grass_material(task=True)
  G["on"] = on
  a, b = off.splitlines(), on.splitlines()
  ops = [o for o in difflib.SequenceMatcher(None, a, b, autojunk=False).get_opcodes()
         if o[0] != "equal"]
  inserted, rewritten = [], []
  for (kind, i1, i2, j1, j2) in ops:
    if kind == "insert":
      inserted += b[j1:j2]
      continue
    # the ONLY rewrite the extension is allowed: a pass line gaining its amplifier. Anything
    # else replaced or deleted would mean the mesh path itself was rewritten to get a task
    # stage, which is exactly what OFF-neutrality (test 1) promises never happens.
    assert kind == "replace" and (i2 - i1) == (j2 - j1), \
        f"task splice deleted or resized text ({kind}, {i2 - i1} -> {j2 - j1} lines)"
    for old, new in zip(a[i1:i2], b[j1:j2]):
      m = re.match(r"^(  pass p0 \{ )(task_shader = \w+; )(.*)$", new)
      assert m and (m.group(1) + m.group(3)) == old, \
          f"a non-pass line was rewritten:\n  - {old}\n  + {new}"
      rewritten.append(new)
  ins = "\n".join(inserted)
  for tok, why in [("task_payload pld_ptex_mesh {", "the payload declaration"),
                   ("task_interface tif_ptex_mesh", "the task interface"),
                   ("task_shader ts_ptex_mesh\n", "the mono task shader"),
                   ("task_shader ts_ptex_mesh_stereo\n", "the stereo task shader"),
                   ("EmitMeshTasksEXT(total, 1u, 1u);", "the dispatch verb")]:
    assert tok in ins, f"the inserted text is missing {why}: {tok!r}"
  # the four pass lines that must now be fronted by an amplifier — and ONLY those four
  assert len(rewritten) == 4, \
      f"expected exactly four rewritten pass lines (colour/dpp x mono/_ST), got {len(rewritten)}"
  # STEREO (owner design decision, 2026-08-04): the _ST twin culls ONCE against a CENTRE camera
  # derived from the stereo params, widened — never the left eye's view-projection, which would
  # drop grass out of the right eye.
  i = on.index("task_shader ts_ptex_mesh_stereo\n")
  st = on[i:on.index("\nmesh_shader ", i)]   # not "mesh_shader": GL_EXT_mesh_shader is in the decl
  assert "(spvr_vp[0] + spvr_vp[1]) * 0.5" in st, "the _ST task twin does not cull from a centre camera"
  assert "spvr_eyepos[0].xyz + spvr_eyepos[1].xyz" in st, "the _ST task twin does not use the centre eye"
  assert "GrassStereo.x" in st, "the _ST task twin does not widen its bound"
  assert "ofx_viewIndex" not in st, "a task stage has no view index to select with"
  n_ins = len([o for o in ops if o[0] == "insert"])
  print(f"  splice = {len(inserted)} inserted lines over {n_ins} insertion(s); "
        f"{len(rewritten)} pass lines amplified, nothing else touched")
  return True


def test_caps_match_the_source():
  on = G["on"]
  m = re.search(r"layout\(triangles, max_vertices = (\d+), max_primitives = (\d+)\)", on)
  assert m, "no mesh output-limits layout line"
  mv, mp = int(m.group(1)), int(m.group(2))
  assert (mv, mp) == (grass.MAX_VERTICES, grass.MAX_PRIMITIVES), \
      f"emitted caps {(mv, mp)} != source caps {(grass.MAX_VERTICES, grass.MAX_PRIMITIVES)}"
  # the caps must be exactly what the body WRITES: one blade per invocation, VERTS/PRIMS each
  assert grass.MAX_VERTICES == grass.BLADES_PER_WG * grass.VERTS_PER_BLADE, "vertex cap is not the blade budget"
  assert grass.MAX_PRIMITIVES == grass.BLADES_PER_WG * grass.PRIMS_PER_BLADE, "prim cap is not the blade budget"
  assert grass.VERTS_PER_BLADE == 2 * (grass.SEGMENTS + 1), "a ribbon is 2 corners per ring"
  assert grass.PRIMS_PER_BLADE == 2 * grass.SEGMENTS, "a ribbon is 2 triangles per segment"
  # workgroup shapes: one invocation per blade in the mesh stage, one tile per task workgroup
  i = on.index("vertex_interface vif_ptex_mesh")
  assert f"local_size_x = {grass.BLADES_PER_WG}" in on[i:on.index("}", on.index("inputs", i))], \
      "the mesh workgroup is not one invocation per blade"
  j = on.index("task_interface tif_ptex_mesh")
  assert f"local_size_x = {grass.TASK_WG}" in on[j:on.index("}", j)], \
      "the task workgroup is not one tile per workgroup"
  assert on.count("gl_MeshVerticesEXT[vi].gl_Position") == 4, \
      "expected four mesh stages (colour + depth prepass, each with its _ST peer)"
  # MOLTENVK NATIVE ROUTE: the declared output is threadgroup memory on Metal, and crossing the
  # threshold costs no error — just an emulation that restarts the render pass a thousand times
  # per draw. Modelled here on every platform (the arithmetic is portable; the route is not), so
  # a widened blade budget is a red gate rather than a field report from a mac.
  pv, pp = gpu_meshlet.VERTEX_OUTPUT_BYTES, gpu_meshlet.PRIM_OUTPUT_BYTES
  native = gpu_meshlet.MESH_NATIVE_ROUTE_BUDGET
  payload = mv * pv + mp * pp
  assert payload <= native, (
      f"grass mesh stage {mv}/{mp} = {payload}B exceeds the {native}B native-route threshold by "
      f"{payload - native}B -> MoltenVK falls back to render-pass-restart emulation")
  print(f"  caps {mv}/{mp} = {grass.BLADES_PER_WG} blades x "
        f"{grass.VERTS_PER_BLADE}v/{grass.PRIMS_PER_BLADE}p; task workgroup {grass.TASK_WG}; "
        f"{payload}B declared output (native-route margin {native - payload}B)")
  return True


def test_field_texel_contract():
  """The field texel layout is a CONTRACT with the C++ buffer lane that packs it: x height_m,
  y density01, z ground normal xz as packHalf2x16 bit-reinterpreted to float, w dryness01. The
  packed channel is the one that fails silently — read it as a plain float and every blade tilts
  by a number that is not a direction, with no error anywhere."""
  on = G["on"]
  assert "unpackHalf2x16(floatBitsToUint(t00.z))" in on, \
      "the ground normal is not decoded from the packed half2 channel"
  assert on.count("unpackHalf2x16(floatBitsToUint(") == 4, \
      "expected all four bilinear taps unpacked (lerping packed bit patterns is meaningless)"
  assert "vec2 nxz = mix(mix(n00, n10, f.x), mix(n01, n11, f.x), f.y);" in on, \
      "the normal is not interpolated in vector space after unpacking"
  assert "return vec4(b.x, b.y, b.w, 0.0);" in on, \
      "the scalar tap does not deliver (height, density, dryness) from x/y/w"
  # dryness comes from the BAKED channel through the one bias function — once in each of the four
  # mesh bodies, at the blade root. The task stage does NOT read it: a tile-level figure would be
  # coarser than the tap the mesh stage already has to make, and carrying it would cost a third
  # 1KB payload array per supertile that nothing reads.
  assert on.count("grass_dryness(fs.z, GrassDry.x)") == 4, \
      "expected the per-blade dryness in all four mesh bodies, from one baked source and one bias"
  ti = on.index("task_shader ts_ptex_mesh\n")
  assert "grass_dryness" not in on[ti:on.index("\nmesh_shader ", ti)], \
      "the task stage re-derives dryness the mesh stage taps anyway"
  print("  field texel contract: packed normal unpacked-then-lerped, dryness from .w")
  return True


def test_field_uv_convention():
  """The world->texel mapping must be TEXEL-CENTRE (* dim - 0.5), the convention the field data
  was resampled with (Image::resampledOf puts texel i's centre at (i+0.5)/scale-0.5) and the one
  the terrain render mesh puts on screen. The node-aligned form (* (dim-1)) also reads plausible
  heights everywhere — it just reads them from a point shifted by -wxz/dim metres, so the blades
  float or bury against the surface they stand on, growing to half a texel (16 m of ground) at the
  extent edge. Nothing in the pipeline errors on it, so this text is the only place it can fail."""
  on = G["on"]
  CENTRE = "uv  = (wxz / max(extent, 1.0e-4) + vec2(0.5)) * dim - vec2(0.5);"
  NODE   = "+ vec2(0.5)) * (dim - 1.0)"
  assert NODE not in on, \
      "the field tap is NODE-aligned ((dim-1)) — blades read ground shifted by up to half a texel"
  # spliced into BOTH readers: the scalar tap and the normal tap must address the same texels
  assert on.count(CENTRE) == 2, \
      (f"expected the texel-centre mapping in both field readers, found {on.count(CENTRE)} — the "
       f"scalar tap and the ground-normal tap must resolve to the same texels for a point")
  print("  field uv mapping is texel-centre (* dim - 0.5) in both readers, not node-aligned")
  return True


def test_supertile_compaction():
  """The task stage owns an S x S BLOCK of tiles and loops it, so the dispatch is (grid/S)^2 and
  the mesh workgroups it emits are the surviving set rather than the grid. Three things can only
  be checked here:

    * the payload is sized by the SAME constant the loop is bounded by (a payload one slot short
      of the loop is an out-of-bounds write into a task payload — no diagnostic, wrong grass);
    * the task stage uses NO shared memory, NO barriers and NO subgroup ops. The mesh-payload
      cliff on this platform was measured against the mesh stage; the object stage's translation
      is not a thing to gamble a silent render-pass-restart on;
    * the mesh stage demuxes its global workgroup id through the payload's cluster BASES, so a
      cluster index still means the same blades it meant when one tile owned one workgroup.
  """
  on = G["on"]
  ti = on.index("task_shader ts_ptex_mesh\n")
  tsk = on[ti:on.index("\nmesh_shader ", ti)]
  slots = grass.SUPERTILE * grass.SUPERTILE
  assert grass.SUPERTILE_SLOTS == slots, "SUPERTILE_SLOTS is not the supertile's tile count"
  assert "vec4 tile_data[%d];" % slots in on, \
      "the payload does not carry one vec4 slot per tile in the supertile"
  # PAYLOAD BYTES ARE OBJECT-STAGE OCCUPANCY on Metal (the payload is threadgroup memory), and this
  # dispatch pays them tens of thousands of times a frame. Anything the mesh stage can re-derive
  # from a bound param or a hash MUST NOT ride here — measured in ms, not in bytes.
  for gone in ("tile_aux", "tile_origin", "tile_params"):
    assert gone not in on, f"a fatter per-tile payload row is back: {gone!r}"
  payload_bytes = slots * 16 + 16   # std430: one vec4 array + the header vec4
  assert payload_bytes <= 16384, \
      f"task payload {payload_bytes}B over the portable maxTaskPayloadSize (16384B)"
  assert f"clamp(uint(GrassCull.x + 0.5), 1u, {grass.SUPERTILE}u)" in tsk, \
      "the live supertile edge is not clamped to the baked payload capacity"
  for tok in ("shared ", "barrier(", "subgroup", "gl_SubgroupInvocationID"):
    assert tok not in tsk, f"the task stage uses {tok!r} — the object stage stays scalar"
  # near-to-far: payload order is mesh workgroup order is primitive order, and this carpet has no
  # depth prepass, so front-to-back is the only early-z the blades get against each other.
  for tok in ("bool revx", "bool revy"):
    assert tok in tsk, f"the supertile walk is not near-to-far ({tok} missing)"
  # DEMUX: the mesh stage resolves (tile, cluster) from the monotone cluster bases.
  assert on.count("uint cluster = gl_WorkGroupID.x - uint(tdat.w);") == 4, \
      "the four mesh stages do not all demux their workgroup id through the payload cluster bases"
  assert on.count("uint mid = (lo + hi) >> 1u;") == 4, \
      "the demux is not the binary search the monotone cluster bases allow"
  print(f"  supertile {grass.SUPERTILE}x{grass.SUPERTILE} -> {slots} payload slots, "
        f"{payload_bytes}B payload; task stage scalar (no shared/barrier/subgroup)")
  return True


def test_hzb_occlusion_contract():
  """The tile occlusion test: the terrain chunk cull's math against the SAME pyramid, read through
  the same CamBlk c_misc.yzw header — and fail-safe in the direction that cannot lose grass."""
  on = G["on"]
  # declaration ORDER is what assigns the descriptor bindings, so the HZB block must come after
  # the field block (and both after sif_ptex_vtx) on every stage that inherits them.
  i_vtx = on.index("storage_interface sif_ptex_vtx")
  i_fld = on.index("storage_interface %s" % grass.FIELD_BLOCK_NAME)
  i_hzb = on.index("storage_interface %s" % grass.HZB_BLOCK_NAME)
  assert i_vtx < i_fld < i_hzb, \
      "storage block declaration order moved — the descriptor bindings follow it"
  assert "buffer layout(std430) hzb_blk { float HZB[]; };" in on, \
      "the HZB block is not the engine's packed float max-depth pyramid"
  # FAIL-SAFE: no pyramid (c_misc.yzw zero) or a corner behind the eye => NOT occluded.
  fn = on[on.index("bool grass_hzb_occluded"):]
  fn = fn[:fn.index("\n}")]
  assert "if (misc.y < 0.5 || misc.w < 0.5) { return false; }" in fn, \
      "a missing/zero-dimension HZB does not pass the tile through — it could black the carpet"
  assert "if (cl.w <= 0.0001) { return false; }" in fn, \
      "a box corner at/behind the eye plane is projected anyway (its NDC means nothing)"
  # A8: the bias and the on/off are BOUND, never literals in the test's own call site.
  ti = on.index("task_shader ts_ptex_mesh\n")
  tsk = on[ti:on.index("\nmesh_shader ", ti)]
  assert "grass_hzb_occluded(mvp, c_misc, bmn, bmx, GrassCull.y)" in tsk, \
      "the occlusion bias is not the bound parameter"
  assert "GrassCull.z > 0.5 &&" in tsk, "the occlusion test has no bound on/off"
  assert "0.005" not in tsk, "a tweakable landed in the task body as a literal (A8)"
  print("  HZB test: terrain's pyramid + math, bound bias/enable, fail-safe to NOT occluded")
  return True


def test_colour_and_dpp_share_one_task_shader():
  on = G["on"]
  passes = dict(re.findall(r"technique (\w+) \{\n  fxconfig = fxcfg_default;\n"
                           r"  pass p0 \{ task_shader = (\w+);", on))
  for colour, depth, want in [("FWD_SSBO_CUSTOM_MESH", "FWD_SSBO_CUSTOM_MESH_DEPTHPREPASS",
                               "ts_ptex_mesh"),
                              ("FWD_SSBO_CUSTOM_MESH_ST", "FWD_SSBO_CUSTOM_MESH_DEPTHPREPASS_ST",
                               "ts_ptex_mesh_stereo")]:
    assert passes.get(colour) == want, f"{colour} names {passes.get(colour)!r}, expected {want!r}"
    assert passes.get(depth) == want, \
        (f"{depth} names {passes.get(depth)!r}, not the colour pass's {want!r} — the prepass would "
         f"amplify to a different blade set than the pass it exists to depth-test")
  assert on.count("task_shader ts_ptex_mesh\n") == 1, "expected exactly one mono task shader"
  assert on.count("task_shader ts_ptex_mesh_stereo\n") == 1, "expected exactly one stereo task shader"
  # the payload is inherited by the STAGES, never by the vertex interface: the sema resolves a
  # task_payload inheritance only on a mesh/task shader node and drops it SILENTLY elsewhere,
  # which would leave the mesh body reading a payload that is not in scope.
  vif = on[on.index("vertex_interface vif_ptex_mesh"):]
  assert "pld_ptex_mesh" not in vif[:vif.index("}")], \
      "the payload is inherited by the vertex interface, where the sema silently drops it"
  assert on.count("  : pld_ptex_mesh\n") == 6, \
      "expected the payload on all four mesh stages and both task shaders"
  print("  colour + depth prepass share one task shader per view mode; payload on the stages")
  return True


###############################################################################

def main():
  tests = [
      test_taskless_is_byte_identical,
      test_no_task_leak_into_taskless_materials,
      test_task_splice_is_additive,
      test_caps_match_the_source,
      test_field_texel_contract,
      test_field_uv_convention,
      test_supertile_compaction,
      test_hzb_occlusion_contract,
      test_colour_and_dpp_share_one_task_shader,
  ]
  failed = []
  for t in tests:
    print(f"[{t.__name__}]", flush=True)
    try:
      if not t():
        failed.append(t.__name__)
    except Exception:
      import traceback
      traceback.print_exc()
      failed.append(t.__name__)
  ok = (len(failed) == 0)
  print("=== grass taskmesh codegen gate %s (%d/%d) ===" % (
      "PASSED" if ok else "FAILED", len(tests) - len(failed), len(tests)), flush=True)
  if failed:
    print("  failed: " + ", ".join(failed), flush=True)
  sys.exit(0 if ok else 1)


main()
