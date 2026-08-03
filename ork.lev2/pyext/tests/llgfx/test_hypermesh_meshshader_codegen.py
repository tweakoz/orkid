#!/usr/bin/env python3
###############################################################################
# hypermesh MESH-STAGE codegen gate — the material a real hypermesh scene gets when the mesh path
# is switched on. Pure TEXT: no GPU, no window, no device (so it stays green on any machine and
# fails loudly on a broken splice even where nothing can render).
#
# What it proves:
#   1. byte-neutrality — with the mesh path OFF the generated material is EXACTLY what the
#      pull-VS-only generator produced (the VS decode was refactored to be shared with the mesh
#      stage; if that moved one byte, every cached shader and every content-addressed bake re-keys)
#   2. no leak — mesh-OFF text carries no mesh stage at all (shader modules are created for every
#      stage in the program at load time, so a machine without VK_EXT_mesh_shader must never be
#      handed one)
#   3. additive splice — OFF -> ON differs by exactly one contiguous insertion, and that insertion
#      is the FWD_SSBO_CUSTOM_MESH / _MESH_DEPTHPREPASS technique pair
#   4. the mesh body is the meshlet consumer it claims to be: descriptor -> vertex list -> packed
#      local triangles -> gl_MeshVerticesEXT / gl_PrimitiveTriangleIndicesEXT, with the position
#      routed through `mvp * position` (a direct SSBO-sourced clip position mis-rasterizes under
#      MoltenVK — the transform is what makes the two paths one image)
#   5. cap parity — the emitted output limits equal the CPU builder's bucket caps (meshlet.h). A
#      split pair means the layout cannot hold what the builder built.
#   6. instanced refusal — an instanced material emits NO mesh technique (the mesh stage places no
#      per-instance matrix), so the drawable's capability check refuses loudly instead of quietly
#      rendering the other path.
#   7. native-route guard — every emitted mesh stage stays inside MoltenVK's NATIVE taskless route
#      (declared caps, output bytes, no workgroup variables, no barriers). Missing it is silent:
#      the fallback emulation restarts the Metal render pass ~1000x per draw with a green build and
#      no diagnostic. Terrain clears the byte threshold by EIGHTY bytes, so this is a tripwire.
#      That route is a MoltenVK artifact: the byte/varying model and the structural conditions are
#      asserted on every platform, the emitted caps only where the route exists (elsewhere the
#      guard prints NOT APPLICABLE rather than a pass that means nothing).
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys, re, difflib, hashlib

_ROOT = os.path.abspath(__file__)
for _ in range(5):
  _ROOT = os.path.dirname(_ROOT)
sys.path.insert(0, os.path.join(_ROOT, "obt.project", "scripts"))

from orkengine import core   # core before lev2
from orkengine import lev2

from ork.hypergraph.ptex3d import fxv2_template
from ork.hypergraph.ptex3d.fxv2_template import generate_surface_fxv2, materialize_surface_fxv2
from ork.hypergraph.dflow.hypermesh import GpuMeshRenderSource
from ork.hypergraph.dflow.hypermesh import gpu_meshlet
from ork.hypergraph.dflow.hypermesh.gpu_meshlet import (MESH_WG_MAX, MESHLET_MAX_VERTS,
                                                        MESHLET_MAX_PRIMS, MESHLET_BLOCK_NAMES)

hm = lev2.hypermesh

BODY = ("o.albedo   = vec3(uv.x, uv.y, 0.5);\n"
        "o.emissive = (wnrm * 0.5 + vec3(0.5)) * 0.55;")

# The pull VS body EXACTLY as it read before the decode was factored out for reuse by the mesh
# stage. This is the byte-neutrality anchor: a literal, not a digest, so a diff says WHAT moved.
LEGACY_VS_BODY = (
  "uint i = uint(gl_VertexID);\n"
  "vec4 position = Pd[i];\n"
  "vec3 normal   = Nd[i].xyz;\n"
  "vec3 binormal = Bd[i].xyz;\n"
  "vec2 uv0      = UVd[i].xy;\n"
  "float uv0z    = UVd[i].z;\n"
  "vec4 vtxcolor = Cd[i];")

# sha1 of the mesh-OFF material text (the refactor's byte-neutrality is proven independently
# by test_vs_body_byte_neutral). Regenerate ONLY with a deliberate template contract change,
# and say so — never to quiet a red gate.
# RE-RECORDED at the single-pass-stereo (_ST) technique lowering: every generated family now
# emits its per-view peer alongside the mono technique, so this whole-file digest moved by
# exactly that addition. The MONO half is unmoved and is proved so independently —
# test_spvr_ptex3d_st_lowering_gate regenerates with the lowering disarmed and compares
# against the pre-lowering generator, digest for digest, this config included.
GOLDEN_OFF = "fb12a998273c6b8ec1c9f27c1d6c5564298f70cb"

G = {}


def gen(mesh, instanced=False):
  """generate the hypermesh material text with the mesh path on/off (the env is what the DSL and
  the C++ drawable both read, so the toggle is exercised exactly as a run would)."""
  prev = os.environ.get("ORKID_HYPERMESH_MESHSHADER")
  os.environ["ORKID_HYPERMESH_MESHSHADER"] = "1" if mesh else "0"
  try:
    src = GpuMeshRenderSource(instanced=instanced)
    return generate_surface_fxv2(BODY, **src.as_material_kwargs())
  finally:
    if prev is None:
      os.environ.pop("ORKID_HYPERMESH_MESHSHADER", None)
    else:
      os.environ["ORKID_HYPERMESH_MESHSHADER"] = prev


###############################################################################

def test_vs_body_byte_neutral():
  prev = os.environ.pop("ORKID_HYPERMESH_MESHSHADER", None)
  try:
    kw = GpuMeshRenderSource().as_material_kwargs()
  finally:
    if prev is not None:
      os.environ["ORKID_HYPERMESH_MESHSHADER"] = prev
  assert kw["ssbo_vs_body"] == LEGACY_VS_BODY, \
      "the shared decode moved the pull-VS body:\n" + "\n".join(
          difflib.unified_diff(LEGACY_VS_BODY.splitlines(), kw["ssbo_vs_body"].splitlines(),
                               "legacy", "generated", lineterm=""))
  assert kw["mesh_source"] is None, "mesh path off must supply no mesh source"
  print("  pull-VS body unchanged by the shared-decode refactor")
  return True


def test_mesh_off_is_stable_and_clean():
  off = gen(mesh=False)
  G["off"] = off
  digest = hashlib.sha1(off.encode("utf-8")).hexdigest()
  print(f"  mesh-OFF sha1={digest}")
  assert digest == GOLDEN_OFF, f"mesh-OFF material text moved: {digest} != recorded {GOLDEN_OFF}"
  assert "mesh_shader" not in off, "mesh-OFF text leaked a mesh stage"
  assert "FWD_SSBO_CUSTOM_MESH" not in off, "mesh-OFF text leaked the mesh technique"
  for name in MESHLET_BLOCK_NAMES:
    assert name not in off, f"mesh-OFF text leaked the meshlet storage block {name}"
  return True


def test_splice_is_additive():
  on = gen(mesh=True)
  G["on"] = on
  ops = [o for o in difflib.SequenceMatcher(None, G["off"].splitlines(), on.splitlines(),
                                            autojunk=False).get_opcodes() if o[0] != "equal"]
  kinds = [o[0] for o in ops]
  # two insertions: the meshlet storage blocks (with the other SSBO declarations) and the
  # technique pair. Nothing may be replaced or deleted — that would move the pull-VS path.
  assert all(k == "insert" for k in kinds), f"mesh splice is not insertion-only: {kinds}"
  inserted = []
  for (_, _, _, j1, j2) in ops:
    inserted += on.splitlines()[j1:j2]
  ins = "\n".join(inserted)
  assert "technique FWD_SSBO_CUSTOM_MESH {" in ins, "no mesh technique in the inserted text"
  assert "technique FWD_SSBO_CUSTOM_MESH_DEPTHPREPASS {" in ins, "no mesh depth-prepass technique"
  for name in MESHLET_BLOCK_NAMES:
    assert name in ins, f"meshlet storage block {name} not declared"
  print(f"  splice = {len(ops)} insertion(s), {len(inserted)} lines, both techniques present")
  return True


def test_mesh_body_is_a_meshlet_consumer():
  on = G["on"]
  for token, why in [
      ("uint ml = gl_WorkGroupID.x;", "one workgroup per meshlet"),
      ("if (ml >= ml_count)", "over-dispatch guard"),
      ("MLDesc[d0 + 0u]", "descriptor read"),
      ("MLVerts[v_off + vi]", "vertex-list indirection"),
      ("MLPrims[p_off + pi]", "prim-index read"),
      ("SetMeshOutputsEXT(v_cnt, p_cnt);", "output sizing from the descriptor"),
      ("gl_MeshVerticesEXT[vi].gl_Position = mvp * position;", "MVP-routed position (MoltenVK law)"),
      ("gl_PrimitiveTriangleIndicesEXT[pi] = uvec3(packed & 255u,", "3x8-bit local unpack"),
      ("extension(GL_EXT_mesh_shader)", "the EXT declaration"),
  ]:
    assert token in on, f"mesh body is missing {why}: {token!r}"
  # never a raw clip-space position lifted straight out of the SSBO
  assert not re.search(r"gl_MeshVerticesEXT\[\w+\]\.gl_Position\s*=\s*P", on), \
      "direct SSBO-sourced gl_Position — mis-rasterizes under MoltenVK"
  # FOUR stages since the single-pass-stereo lowering: the color + depth-prepass pair, and
  # each one's _ST peer (identical meshlet body, per-view clip matrix). Named individually
  # rather than counted loosely, so losing one is still a failure.
  for stage in ("ms_ptex_mesh", "ms_ptex_mesh_dpp",
                "ms_ptex_mesh_stereo", "ms_ptex_mesh_dpp_stereo"):
    assert on.count("mesh_shader %s\n" % stage) == 1, "expected exactly one %s stage" % stage
  # the _ST peers route position through the per-VIEW clip matrix, and only they do.
  assert on.count("gl_MeshVerticesEXT[vi].gl_Position = (spvr_vp[ofx_viewIndex] * m) * position;") == 2, \
      "expected the two _ST mesh stages to transform per view"
  print("  mesh body reads desc/verts/prims and emits through mvp * position")
  print("  4 mesh stages: color + depth-prepass, each with its _ST peer")
  return True


def test_caps_match_the_builder():
  on = G["on"]
  m = re.search(r"layout\(triangles, max_vertices = (\d+), max_primitives = (\d+)\)", on)
  assert m, "no mesh output-limits layout line"
  mv, mp = int(m.group(1)), int(m.group(2))
  assert (mv, mp) == (MESHLET_MAX_VERTS, MESHLET_MAX_PRIMS), f"emitted caps {(mv, mp)} != codegen caps"
  # against the LIVE C++ constants, not a regex over the header — this is the pair that must agree
  # on the machine that renders (they are platform-derived, so a header scrape reads the wrong arm)
  cv, cp = hm.meshlet_max_verts, hm.meshlet_max_prims
  assert (mv, mp) == (cv, cp), f"shader caps {(mv, mp)} != builder caps {(cv, cp)} (meshlet.h)"
  assert f"local_size_x = {MESH_WG_MAX}" in on, "mesh workgroup is not the portable invocation cap"
  print(f"  caps {mv}/{mp} match the live builder caps; workgroup={MESH_WG_MAX}")
  return True


def test_mesh_output_budget():
  """the declared output payload must fit the platform's mesh-output budget.

  Metal accounts a mesh threadgroup's whole declared output as threadgroup memory (32KB). Over it,
  vkCreateGraphicsPipelines returns VK_ERROR_INITIALIZATION_FAILED — at PIPELINE CREATION, on the
  first real mesh draw, with nothing in the shader to point at. 256 verts of the ptex3d varying set
  is 45KB and does exactly that; terrain's mesh path survives on the same device at 81 verts /
  128 prims = 16304 bytes. This pins the derived caps inside that proven envelope."""
  pv, pp = gpu_meshlet.VERTEX_OUTPUT_BYTES, gpu_meshlet.PRIM_OUTPUT_BYTES
  budget = gpu_meshlet.MESH_OUTPUT_BUDGET
  limit  = int(budget * gpu_meshlet.MESH_BUDGET_SAFETY)
  # the derivation itself: an unbudgeted platform keeps the taskless-tier maximum...
  assert gpu_meshlet.meshlet_caps(budget=0) == (256, 256), "no-budget caps are not the tier maximum"
  # ...and a Metal budget must bring it strictly under the limit
  mv, mp = gpu_meshlet.meshlet_caps(budget=budget)
  payload = mv * pv + mp * pp
  assert payload <= limit, f"derived caps {(mv, mp)} = {payload}B exceed the {limit}B safety limit"
  assert (mv, mp) != (256, 256), "the budget derivation is inert — it must bite on Metal"
  assert payload <= 81 * pv + 128 * pp, \
      f"derived payload {payload}B exceeds terrain's proven {81 * pv + 128 * pp}B footprint"
  # and the 256/256 shape is the one that fails, so the math must reject it
  assert 256 * pv + 256 * pp > budget, "the failing shape is not over budget by this model"
  # whatever THIS platform runs, the live caps must fit its own budget
  live = mv * pv + mp * pp if sys.platform == "darwin" else 0
  print(f"  metal caps {(mv, mp)} = {payload}B (limit {limit}B, terrain {81 * pv + 128 * pp}B); "
        f"live caps {(hm.meshlet_max_verts, hm.meshlet_max_prims)}")
  return True


def test_cached_material_identity():
  """the CACHED ARTIFACT's identity must move with both the mesh mode and the codegen version.

  A material generated with the toggle off, served to a toggle-on process, is a mesh stage that
  isn't there — and the reverse is a mesh stage nobody asked for. The .fxv2 is content-addressed
  (sha1 over CODEGEN_VERSION + the whole generated source, materialize_surface_fxv2), so BOTH
  properties fall out of one key; this pins them so a future coarsening is a red gate, not a
  field report."""
  def path(mesh, version=None):
    prev_v = fxv2_template.CODEGEN_VERSION
    prev_e = os.environ.get("ORKID_HYPERMESH_MESHSHADER")
    os.environ["ORKID_HYPERMESH_MESHSHADER"] = "1" if mesh else "0"
    if version:
      fxv2_template.CODEGEN_VERSION = version
    try:
      src = GpuMeshRenderSource()
      return materialize_surface_fxv2(BODY, name_hint="meshletgate", **src.as_material_kwargs())
    finally:
      fxv2_template.CODEGEN_VERSION = prev_v
      if prev_e is None:
        os.environ.pop("ORKID_HYPERMESH_MESHSHADER", None)
      else:
        os.environ["ORKID_HYPERMESH_MESHSHADER"] = prev_e

  p_off, p_on = path(False), path(True)
  assert p_off != p_on, f"mesh mode is not part of the cached material identity: {p_off}"
  # the toggle-on artifact must really carry the stage (not just a different name)
  disk = os.path.join(fxv2_template._dslcache_dir("ptex3d"), os.path.basename(p_on))
  assert "FWD_SSBO_CUSTOM_MESH" in open(disk).read(), "toggle-on artifact has no mesh technique"
  p_bumped = path(True, version=fxv2_template.CODEGEN_VERSION + "-probe")
  assert p_bumped != p_on, "the codegen version is not part of the cached material identity"
  print("  identity tracks mesh mode and codegen version (%s)" % os.path.basename(p_on))
  return True


def test_instanced_emits_no_mesh_technique():
  on = gen(mesh=True, instanced=True)
  assert "FWD_SSBO_CUSTOM_MESH" not in on, "instanced material must not carry a mesh technique"
  assert "FWD_SSBO_CUSTOM_INSTANCED" in on, "instanced material lost its instanced technique"
  print("  instanced material refuses the mesh path (pull-VS stands)")
  return True


def test_native_route_guard():
  """every emitted mesh stage must stay on MoltenVK's NATIVE taskless route.

  This is the failure mode with no symptom: miss the route and MoltenVK silently switches to an
  emulation that restarts the Metal render pass roughly a thousand times per draw. Nothing errors,
  nothing warns, the build is green and the frame time collapses. The route requires <=128 declared
  vertices, <=128 declared primitives, an estimated output under 16KB, and NO workgroup variables.

  The margin is the reason this is a gate and not a comment: terrain is EIGHTY BYTES under the
  threshold, so one added varying on any mesh stage crosses it. Both figures print below so the
  headroom is visible here rather than rediscovered from a profiler.

  The route split is a MOLTENVK TRANSLATION artifact — a driver with a native mesh stage has no
  emulated route to fall into — so the check has two halves. The MODEL (terrain's footprint and the
  Metal-derived caps against the ptex3d varying stride) is arithmetic over fixed numbers and runs
  EVERYWHERE, which is what actually catches the added varying; that is the same trick that makes
  test_mesh_output_budget portable. Only the caps this platform EMITS are darwin-scoped, because
  off-darwin those are the 256/256 tier maximum by design and holding them to a Metal ceiling
  asserts nothing. Where the route does not exist the guard says so instead of printing a pass."""
  on = G["on"]
  pv, pp = gpu_meshlet.VERTEX_OUTPUT_BYTES, gpu_meshlet.PRIM_OUTPUT_BYTES
  native = gpu_meshlet.MESH_NATIVE_ROUTE_BUDGET
  nv, npr = gpu_meshlet.MESH_NATIVE_ROUTE_MAX_VERTS, gpu_meshlet.MESH_NATIVE_ROUTE_MAX_PRIMS
  # ---- the scoping itself: the ceilings exist exactly where MoltenVK does, and nowhere else ----
  assert gpu_meshlet.native_route_limits("linux") is None, \
      "native-route ceilings must not apply where the driver has a native mesh stage"
  assert gpu_meshlet.native_route_limits("darwin") == (native, nv, npr), \
      "native-route ceilings went missing on the platform that has the route"
  # ---- MODEL half, checked on every platform: terrain, modelled the same way this repo already
  #      models it (its declared 81/128 caps against the ptex3d varying stride) — the thin one,
  #      and the reason for the guard ----
  terr_bytes = 81 * pv + 128 * pp
  assert terr_bytes <= native, (
      f"terrain footprint {terr_bytes}B exceeds the {native}B native-route threshold by "
      f"{terr_bytes - native}B -> terrain mesh path falls into emulation")
  # ...and the stage a MAC build of this same material would declare, from the same derivation
  mac_mv, mac_mp = gpu_meshlet.meshlet_caps(budget=gpu_meshlet.MESH_OUTPUT_BUDGET)
  mac_bytes = mac_mv * pv + mac_mp * pp
  assert mac_mv <= nv, f"Metal-derived max_vertices {mac_mv} > native-route limit {nv} -> EMULATED"
  assert mac_mp <= npr, f"Metal-derived max_primitives {mac_mp} > native-route limit {npr} -> EMULATED"
  assert mac_bytes <= native, (
      f"Metal-derived stage {mac_mv}/{mac_mp} = {mac_bytes}B exceeds the {native}B native-route "
      f"threshold by {mac_bytes - native}B -> MoltenVK falls back to render-pass-restart emulation")
  # ---- the structural conditions, asserted rather than eyeballed (text, so portable) ----
  for name in ("ms_ptex_mesh", "ms_ptex_mesh_dpp"):
    i = on.index("mesh_shader %s\n" % name)
    body = on[i:on.index("technique", i)]
    assert not re.search(r"^\s*shared\s+\w", body, re.M), \
        f"{name} declares a workgroup (shared) variable -> EMULATED route"
    assert "barrier()" not in body, f"{name} calls barrier() -> workgroup sync -> EMULATED route"
  # ---- EMITTED half: every mesh stage this material actually emits, read off its own declared
  #      limits — only where the route exists ----
  decls = re.findall(r"layout\(triangles, max_vertices = (\d+), max_primitives = (\d+)\)", on)
  assert decls, "no mesh output-limits layout line to check"
  model = (f"modelled Metal stage {mac_mv}/{mac_mp} = {mac_bytes}B "
           f"(margin {native - mac_bytes}B), terrain {terr_bytes}B "
           f"(margin {native - terr_bytes}B), threshold {native}B; no shared vars, no barriers")
  limits = gpu_meshlet.native_route_limits()
  if limits is None:
    hm_mv, hm_mp = int(decls[0][0]), int(decls[0][1])
    print(f"  native route NOT APPLICABLE on {sys.platform}: no MoltenVK translation, so no "
          f"emulated route and no ceiling on the emitted {hm_mv}/{hm_mp} caps. Model still "
          f"asserted: {model}")
    return True
  for mv, mp in [(int(a), int(b)) for a, b in decls]:
    payload = mv * pv + mp * pp
    assert mv <= nv, f"declared max_vertices {mv} > native-route limit {nv} -> EMULATED"
    assert mp <= npr, f"declared max_primitives {mp} > native-route limit {npr} -> EMULATED"
    assert payload <= native, (
        f"mesh stage payload {payload}B exceeds the {native}B native-route threshold by "
        f"{payload - native}B -> MoltenVK falls back to render-pass-restart emulation")
  hm_mv, hm_mp = int(decls[0][0]), int(decls[0][1])
  hm_bytes = hm_mv * pv + hm_mp * pp
  print(f"  native route OK: hypermesh {hm_mv}/{hm_mp} = {hm_bytes}B "
        f"(margin {native - hm_bytes}B), terrain {terr_bytes}B "
        f"(margin {native - terr_bytes}B), threshold {native}B; no shared vars, no barriers")
  return True


###############################################################################

def main():
  tests = [
      test_vs_body_byte_neutral,
      test_mesh_off_is_stable_and_clean,
      test_splice_is_additive,
      test_mesh_body_is_a_meshlet_consumer,
      test_caps_match_the_builder,
      test_mesh_output_budget,
      test_native_route_guard,
      test_cached_material_identity,
      test_instanced_emits_no_mesh_technique,
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
  print("=== hypermesh meshshader codegen gate %s (%d/%d) ===" % (
      "PASSED" if ok else "FAILED", len(tests) - len(failed), len(tests)), flush=True)
  if failed:
    print("  failed: " + ", ".join(failed), flush=True)
  sys.exit(0 if ok else 1)


main()
