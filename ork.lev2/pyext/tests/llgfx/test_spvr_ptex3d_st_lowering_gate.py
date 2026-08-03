#!/usr/bin/env ork.python
################################################################################
# SPVR _ST LOWERING gate — the GENERATED (ptex3d/DSL) material families emit
# single-pass-stereo techniques, by the authored convention, without moving a
# byte of the mono output.
#
# THE CONVENTION BEING ENFORCED (authored in pbrtools.i2 vs_forward_*_stereo +
# pbr.fxv2 FWD_*_ST; ublk_stereo declared in stereotools.i2, imported DIRECTLY):
#   naming   <mono>_MO -> <mono>_ST; a generated name with no mono suffix gets
#            _ST appended (FWD_SSBO_CUSTOM -> FWD_SSBO_CUSTOM_ST).
#   transform the clip-space matrix is spvr_vp[ofx_viewIndex] * m — the per-VIEW
#            view-projection index-selected by the multiview view selector
#            (gl_ViewIndex on vulkan), times the draw's own model matrix. Never a
#            branch. mvp is per-DRAW and ublk_stereo per-FRAME, so the product is
#            formed in the shader rather than stored per view.
#   sharing  everything else is the mono stage's — same interfaces, same varyings,
#            same fragment stage, same state block.
#
# PHASE A (no GPU) — text.
#   A1 MONO BYTE-IDENTITY. Generated with emit_stereo=False, every config still
#      produces its recorded pre-lowering sha1. This is the whole safety claim of
#      the lowering: existing materials' cached shaders and every downstream
#      content-addressed bake stay exactly where they were. The digests were taken
#      from the generator AS IT WAS at 2de7a0768. Regenerate ONLY with a deliberate
#      template contract change, and say so in the commit — do not "fix" a red gate
#      by pasting new numbers.
#   A2 FAMILY COVERAGE. Each config emits exactly the _ST set its mono families
#      imply — a family that silently stops lowering fails here.
#   A3 CONVENTION. Every _ST stage inherits ublk_stereo, transforms through
#      spvr_vp[ofx_viewIndex], carries no leftover bare `mvp *`, and pairs with the
#      SAME fragment stage as its mono twin.
#   A4 GUARD TEETH. The lowering's fail-loud check (`_st_clip`) rejects a body with
#      no clip transform to redirect, and a disarmed generation emits no _ST at all.
#
# PHASE B (GPU) — JIT COMPILE through the real shader path. Each family's material
#   is materialized to the staging shader cache and loaded with
#   FreestyleMaterial.gpuInit, which compiles EVERY declared stage in the file
#   (modules are created at load, not lazily at pipeline time) — so a malformed
#   _ST stage fails here, loudly, and every expected _ST technique must resolve on
#   the loaded shader. This is the gate's primary catch.
#
# PHASE C (GPU) — 2-VIEW RENDER. The SSBO-pull depth-prepass pair rendered into one
#   2-layer multiview RtGroup with two distinct eye view-projections:
#     C1  the _ST technique -> the two layers DIFFER (per-view transform is live)
#     C2  the MONO technique, same RtGroup, same frame shape -> the two layers are
#         IDENTICAL. Without C2, C1 proves only that multiview plumbing works; with
#         it, the difference is attributable to the lowering itself.
#   The depth-prepass pair is the subject deliberately: it declares no samplers, so
#   the leg needs no scene lighting state to be a truthful render of the generated
#   vertex stage. Skips clean (visible SKIP, never a silent pass) where the device
#   reports no multiview.
#
# Self-configuring, offscreen, no arguments, no environment:
#   ork.python test_spvr_ptex3d_st_lowering_gate.py
################################################################################

import os
os.environ["PYTHONUNBUFFERED"] = "1"
import sys
sys.stdout.reconfigure(line_buffering=True)
import hashlib
import re

# repo root = five levels up; prepend THIS checkout's scripts dir so ork.testing +
# the ptex3d template resolve from the same tree as this test.
_ROOT = os.path.abspath(__file__)
for _ in range(5):
  _ROOT = os.path.dirname(_ROOT)
sys.path.insert(0, os.path.join(_ROOT, "obt.project", "scripts"))

import numpy

from orkengine import core   # core before lev2
from orkengine import lev2
from ork.testing import headless_app, verdict
from ork.hypergraph.dflow.terrain.gpu_chunk import TerrainChunkVertexSource
from ork.hypergraph.dflow.hypermesh import GpuMeshRenderSource
from ork.hypergraph.ptex3d.fxv2_template import (
    generate_surface_fxv2, materialize_surface_fxv2, _st_clip, _st_name, _ST_IMPORTS)

tokens = core.CrcStringProxy()

################################################################################
# The generator population.
################################################################################

# reads the varyings the stereo stages must still produce (uv / world normal /
# object position), so a lowering that dropped part of the tail shows up as a
# compile error rather than as a quiet pixel change.
BODY = ("o.albedo   = vec3(uv.x, uv.y, 0.5);\n"
        "o.emissive = (wnrm * 0.5 + vec3(0.5)) * 0.55\n"
        "           + vec3(uv.x, uv.y, fract(opos.y * 0.02)) * 0.45;")
CAP_BODY = "c.base = vec4(uv, 0.0, 1.0);\nc.nrmao = vec4(wnrm, 1.0);"

# a minimal SSBO vertex source: a VkDrawIndirectCommand header then packed float3
# positions, pulled by gl_VertexID. Phase C draws through this exact layout.
ARGS_SLOTS = 4
POS_OFF    = ARGS_SLOTS * 4
SSBO_KWARGS = dict(
  ssbo_layout=("uint args[4];\n"
               "float Pos[];"),
  ssbo_vs_body=("uint i = uint(gl_VertexID);\n"
                "vec4 position = vec4(Pos[i*3u], Pos[i*3u+1u], Pos[i*3u+2u], 1.0);\n"
                "vec3 normal = vec3(0,1,0);\n"
                "vec3 binormal = vec3(1,0,0);\n"
                "vec2 uv0 = vec2(position.x*0.1+0.5, position.y*0.1+0.5);\n"
                "vec4 vtxcolor = vec4(1.0);"))


def _terrain_kwargs(mesh):
  vs = TerrainChunkVertexSource(dim=1024, extent_m=1000.0, chunk=128, mesh=mesh)
  return vs.as_material_kwargs()


def _hypermesh_kwargs():
  """The OTHER mesh source, and the reason it is a separate config: the two derive the
  per-cluster frustum reject differently. Terrain reads its own bound c_vp; hypermesh
  takes the clip matrix as an EXPRESSION and builds Gribb-Hartmann rows out of it — so
  the hypermesh _ST mesh stage is the only place the per-view matrix is substituted into
  a plane derivation, and the only place that shape gets compiled. The env toggle the
  source reads is set explicitly here rather than inherited, so the config is the same
  on any shell."""
  prev = os.environ.get("ORKID_HYPERMESH_MESHSHADER")
  os.environ["ORKID_HYPERMESH_MESHSHADER"] = "1"
  try:
    return GpuMeshRenderSource().as_material_kwargs()
  finally:
    if prev is None:
      os.environ.pop("ORKID_HYPERMESH_MESHSHADER", None)
    else:
      os.environ["ORKID_HYPERMESH_MESHSHADER"] = prev


# the DECK-SCOPE unlit shape verbatim (_cloud_deck.py: unlit + prema + depth-test-on,
# no z-write, no cull, an opacity discard appended). This config is the NAMED CONTROL for
# the lean unlit main-view + sun-cookie fragments merged at 6ed21180c: those two blocks are
# emitted only for surface_mode != "lit", so they are exercised by nothing else in this
# population, and the lowering must leave their text alone byte for byte.
DECK_KWARGS = dict(surface_mode="unlit", blend="prema", depth_test="leq",
                   depth_write=False, cull="off",
                   surf_body_append="if (s.opacity < 0.02) discard;")

CONFIGS = {
  "rigid":         dict(),
  "rigid_unlit":   dict(surface_mode="unlit"),
  "deck_unlit":    dict(DECK_KWARGS),
  "rigid_masked":  dict(masked_dpp_expr="_a", masked_dpp_body="float _a = uv.x;"),
  "ssbo":          dict(SSBO_KWARGS),
  "ssbo_masked":   dict(SSBO_KWARGS, masked_dpp_expr="_a", masked_dpp_body="float _a = uv.x;"),
  "inst_impostor": dict(SSBO_KWARGS, ssbo_instanced=True, wants_capture=True),
  "cap_named":     dict(SSBO_KWARGS, wants_capture=True, capture_targets=("base", "nrmao"),
                        capture_body=CAP_BODY),
  "terr_mesh":     _terrain_kwargs(True),
  "hyper_mesh":    _hypermesh_kwargs(),
}

# sha1 of each config's text generated with the lowering DISARMED, recorded from the
# generator at 2de7a0768 (the commit before the lowering). See the A1 note above.
MONO_GOLDEN = {
  "cap_named":     "9a24c55a63e25c2d000ca907bda9cc5c53e53349",
  "deck_unlit":    "e56802f8e66f32ea02784b7c208f90c257af4272",
  "hyper_mesh":    "da267591c520c4986cef1756b93bc76a6826e16d",
  "inst_impostor": "054ac79e606f91adbbd5a6514e09672709696aba",
  "rigid":         "c425ba580eeb512709b2797d3004daa2acda2a1b",
  "rigid_masked":  "7ffac99824206035f1d42861b1dde451c568a2b0",
  "rigid_unlit":   "d0007141ddc0f8dee220ed54779144ac82fd03b5",
  "ssbo":          "20a76ec5a7a4c43de7c769590d13c253009bc644",
  "ssbo_masked":   "c45c0adf660b71c1cd98b0f4a18911c421196775",
  "terr_mesh":     "fcbc649c83ae2b65f6be43ffcdc964c03e104869",
}

# The FRAGMENT stages that must be byte-untouched even in ARMED output. A whole-file
# digest only constrains the DISARMED path; these are checked inside the armed text, so
# a lowering that edited a shared fragment while adding its _ST vertex stages is caught.
# ps_ptex_unlit + ps_ptex_cookie are the lean unlit main-view / sun-cookie fragments
# merged at 6ed21180c — emitted for surface_mode != "lit" only, hence the deck_unlit
# config carrying them.
FROZEN_FRAGMENTS = {
  "rigid":       ("ps_ptex_forward", "ps_ptex_dpp"),
  "deck_unlit":  ("ps_ptex_unlit", "ps_ptex_cookie", "ps_ptex_dpp"),
  "rigid_unlit": ("ps_ptex_unlit", "ps_ptex_cookie"),
  "ssbo_masked": ("ps_ptex_forward", "ps_ptex_dpp", "ps_ptex_dpp_masked"),
  "cap_named":   ("ps_ptex_forward", "ps_ptex_capture"),
  "inst_impostor": ("ps_ptex_forward", "ps_ptex_impostor", "ps_ptex_capture"),
}

# the _ST techniques each config's mono families imply.
ST_RIGID = ("FWD_CV_NM_RI_NI_ST", "FWD_CT_NM_RI_NI_ST", "FWD_DEPTHPREPASS_RI_NI_ST")
ST_SSBO  = ("FWD_SSBO_CUSTOM_ST", "FWD_SSBO_CUSTOM_DEPTHPREPASS_ST")
ST_INST  = ("FWD_SSBO_CUSTOM_INSTANCED_ST", "FWD_SSBO_CUSTOM_INSTANCED_DEPTHPREPASS_ST")
ST_MESH  = ("FWD_SSBO_CUSTOM_MESH_ST", "FWD_SSBO_CUSTOM_MESH_DEPTHPREPASS_ST")

EXPECT_ST = {
  "rigid":         set(ST_RIGID),
  "rigid_unlit":   set(ST_RIGID),
  "deck_unlit":    set(ST_RIGID),
  "rigid_masked":  set(ST_RIGID),
  "ssbo":          set(ST_RIGID + ST_SSBO),
  "ssbo_masked":   set(ST_RIGID + ST_SSBO),
  "inst_impostor": set(ST_RIGID + ST_SSBO + ST_INST + ("FWD_SSBO_CUSTOM_IMPOSTOR_ST",)),
  "cap_named":     set(ST_RIGID + ST_SSBO),
  "terr_mesh":     set(ST_RIGID + ST_SSBO + ST_MESH),
  # GpuMeshRenderSource() is non-instanced here, so no instanced family to lower.
  "hyper_mesh":    set(ST_RIGID + ST_SSBO + ST_MESH),
}

# The bake and cookie passes are deliberately NOT lowered: the impostor/atlas capture
# rasterizes from a fixed offline viewpoint and the sun cookie renders from the SUN.
# Neither has an eye, so an _ST peer of either would be meaningless — and one appearing
# means someone lowered a pass by pattern instead of by meaning.
NEVER_ST = ("FWD_SSBO_CUSTOM_CAPTURE_ST", "FWD_SUNCOOKIE_ST")

################################################################################
# text helpers
################################################################################

_TEK_RE = re.compile(r"^technique\s+(\S+)\s*\{(.*?)^\}", re.S | re.M)


def techniques(text):
  """-> {name: body} for every technique in the generated file."""
  return {m.group(1): m.group(2) for m in _TEK_RE.finditer(text)}


def stages_of(tek_body):
  """-> (vertex-or-mesh stage name, fragment stage name) for a technique body.
  Handles both emitted forms: `vf_pass = { vs, ps, sb }` and the explicit
  `pass p0 { mesh_shader = ms; fragment_shader = ps; ... }`."""
  m = re.search(r"vf_pass\s*=\s*\{\s*([^,]+),\s*([^,]+),", tek_body)
  if m:
    return m.group(1).strip(), m.group(2).strip()
  vtx = re.search(r"(?:mesh_shader|vertex_shader)\s*=\s*(\w+)", tek_body)
  frg = re.search(r"fragment_shader\s*=\s*(\w+)", tek_body)
  assert vtx and frg, "unparseable technique body: %r" % tek_body[:120]
  return vtx.group(1), frg.group(1)


def stage_source(text, name, kinds="vertex_shader|mesh_shader"):
  """The full declaration of a shader stage, inherit list through closing brace.
  Stages close at column 0, which is what bounds the match."""
  m = re.search(r"^(?:%s)\s+%s\b(.*?)^\}" % (kinds, re.escape(name)), text, re.S | re.M)
  assert m, "no stage declaration for %s" % name
  return m.group(1)


def mono_twin(name, teks):
  """The mono technique an _ST name peers with — the exact inverse of the naming
  rule (_MO twin first, bare base second)."""
  base = name[:-3]
  if base + "_MO" in teks:
    return base + "_MO"
  return base if base in teks else None


################################################################################
# PHASE A — text
################################################################################

def phase_a():
  fails, notes = [], []
  mono_digests, armed = {}, {}
  for name, kw in sorted(CONFIGS.items()):
    mono = generate_surface_fxv2(BODY, emit_stereo=False, **kw)
    mono_digests[name] = hashlib.sha1(mono.encode("utf-8")).hexdigest()
    armed[name] = generate_surface_fxv2(BODY, **kw)

  ##############################################
  # A1 — mono byte-identity
  ##############################################
  print("A1 mono byte-identity (lowering disarmed == pre-lowering generator):")
  for name in sorted(mono_digests):
    got, want = mono_digests[name], MONO_GOLDEN.get(name)
    mark = "RECORD" if want is None else ("OK" if got == want else "MISMATCH")
    print("  %-16s %s  %s" % (name, got, mark))
    if want is not None and got != want:
      fails.append("A1 %s: mono text moved (%s != recorded %s)" % (name, got, want))
    if want is None:
      notes.append("A1 %s: no recorded digest (RECORD slot)" % name)

  ##############################################
  # A1b — FROZEN FRAGMENTS. The whole-file digests above constrain only the DISARMED
  # path; these compare the shared fragment stages BETWEEN the armed and disarmed
  # texts, so a lowering that edited a fragment while adding its _ST vertex stages
  # is caught. Named control for the lean unlit main-view + sun-cookie fragments
  # (6ed21180c), which the deck-scope config is here to carry.
  ##############################################
  print("A1b frozen fragment stages (armed text == disarmed text, stage for stage):")
  for name in sorted(FROZEN_FRAGMENTS):
    mono = generate_surface_fxv2(BODY, emit_stereo=False, **CONFIGS[name])
    same = []
    for frg in FROZEN_FRAGMENTS[name]:
      a = stage_source(armed[name], frg, kinds="fragment_shader")
      b = stage_source(mono, frg, kinds="fragment_shader")
      same.append(a == b)
      if a != b:
        fails.append("A1b %s: fragment %s moved when the lowering armed" % (name, frg))
    print("  %-16s %d/%d frozen  (%s)"
          % (name, sum(same), len(same), " ".join(FROZEN_FRAGMENTS[name])))

  ##############################################
  # A2 — family coverage
  ##############################################
  print("A2 family coverage (_ST technique set per config):")
  for name in sorted(armed):
    teks = techniques(armed[name])
    got = {t for t in teks if t.endswith("_ST")}
    want = EXPECT_ST[name]
    print("  %-16s %d/%d  %s" % (name, len(got), len(want), " ".join(sorted(got))))
    if got != want:
      fails.append("A2 %s: _ST set %s != expected %s"
                   % (name, sorted(got), sorted(want)))
    for never in NEVER_ST:
      if never in teks:
        fails.append("A2 %s: %s emitted — that pass has no eye to lower to" % (name, never))

  ##############################################
  # A3 — convention
  ##############################################
  print("A3 convention (ublk_stereo + spvr_vp[ofx_viewIndex] + shared fragment):")
  checked = 0
  for name in sorted(armed):
    text = armed[name]
    teks = techniques(text)
    for tek in sorted(t for t in teks if t.endswith("_ST")):
      vtx, frg = stages_of(teks[tek])
      src = stage_source(text, vtx)
      if ": ublk_stereo" not in src:
        fails.append("A3 %s/%s: stage %s does not inherit ublk_stereo" % (name, tek, vtx))
      if "spvr_vp[ofx_viewIndex]" not in src:
        fails.append("A3 %s/%s: stage %s has no per-view clip transform" % (name, tek, vtx))
      if re.search(r"\bmvp\s*\*", src):
        fails.append("A3 %s/%s: stage %s still transforms by the mono mvp" % (name, tek, vtx))
      twin = mono_twin(tek, teks)
      if twin is None:
        fails.append("A3 %s/%s: no mono twin technique" % (name, tek))
      else:
        _, mfrg = stages_of(teks[twin])
        if mfrg != frg:
          fails.append("A3 %s/%s: fragment %s differs from mono twin's %s"
                       % (name, tek, frg, mfrg))
      checked += 1
  print("  %d _ST techniques checked" % checked)
  if checked == 0:
    fails.append("A3: nothing checked — the coverage leg is vacuous")

  ##############################################
  # A3b — DIRECT IMPORT of the stereo block's home. ublk_stereo must arrive by THIS
  # file's own import line, never as a side effect of the forward-PBR imports: the
  # transitive path compiles, so a re-pointed upstream import would take the per-view
  # block away silently. Absent when disarmed, so the mono import list is unmoved.
  ##############################################
  print("A3b direct stereo import:")
  for name in sorted(armed):
    fxcfg = re.search(r"fxconfig\s+fxcfg_default\s*\{(.*?)\}", armed[name], re.S)
    assert fxcfg, "%s: no fxconfig block" % name
    for imp in _ST_IMPORTS:
      line = 'import "%s";' % imp
      if line not in fxcfg.group(1):
        fails.append("A3b %s: armed fxconfig does not import %s DIRECTLY" % (name, imp))
      if line in generate_surface_fxv2(BODY, emit_stereo=False, **CONFIGS[name]):
        fails.append("A3b %s: disarmed generation still imports %s" % (name, imp))
  print("  %d configs import %s directly when armed, none when disarmed"
        % (len(armed), ", ".join(_ST_IMPORTS)))

  ##############################################
  # A4 — guard teeth
  ##############################################
  print("A4 guard teeth:")
  try:
    _st_clip("gl_Position = someOtherMatrix * position;")
    fails.append("A4: _st_clip accepted a body with no clip transform to redirect")
    print("  _st_clip refuses a body with no `mvp *`   FAIL")
  except ValueError:
    print("  _st_clip refuses a body with no `mvp *`   OK")
  if _st_name("FWD_CT_NM_RI_NI_MO") != "FWD_CT_NM_RI_NI_ST":
    fails.append("A4: _MO -> _ST naming rule broken")
  if _st_name("FWD_SSBO_CUSTOM") != "FWD_SSBO_CUSTOM_ST":
    fails.append("A4: suffix-append naming rule broken")
  print("  naming rule _MO->_ST / append              %s"
        % ("OK" if not any(f.startswith("A4:") and "naming" in f for f in fails) else "FAIL"))
  leaked = 0
  for name, kw in sorted(CONFIGS.items()):
    disarmed = generate_surface_fxv2(BODY, emit_stereo=False, **kw)
    leaked += len([t for t in techniques(disarmed) if t.endswith("_ST")])
    leaked += disarmed.count("spvr_vp")
  if leaked:
    fails.append("A4: disarmed generation leaked %d stereo artifacts" % leaked)
  print("  disarmed generation emits zero stereo      %s" % ("OK" if not leaked else "FAIL"))

  return fails, notes


################################################################################
# GPU-phase preconditions
################################################################################

def resolved(tek):
  """True iff a technique lookup actually FOUND one.

  A miss does not return None: FreestyleMaterial.technique() hands back an
  unmanaged_const_ptr wrapper around a null FxShaderTechnique, and that wrapper is
  TRUTHY in python — it reprs as `FxShaderTechnique(0x0:nulltek)`. So the natural
  `assert mtl.technique(name)` can never fire, and a leg written that way passes for
  a technique the shader does not have. The null handle in the repr is the only
  discriminator python is given; check_resolver() below proves it still discriminates
  before any leg trusts it."""
  return "0x0:nulltek" not in repr(tek)


def check_resolver(mtl):
  """The resolver's own control: a name no generator emits must read UNRESOLVED, and
  a name every generated material has must read RESOLVED. If either flips, every
  technique assertion below is vacuous and says so instead of passing quietly."""
  bogus = mtl.shader.technique("FWD_NO_SUCH_TECHNIQUE_ZZZ")
  real  = mtl.shader.technique("FWD_CT_NM_RI_NI_MO")
  if resolved(bogus) or not resolved(real):
    return ["B: technique-resolution control broke (bogus=%s real=%s) — every "
            "technique assertion in this run would be vacuous"
            % (resolved(bogus), resolved(real))]
  return []


def stereo_import_missing():
  """-> [unresolvable orkshader:// imports the armed emission requires].

  ublk_stereo's declaration is being relocated into its own .i2 and the generated
  files import it DIRECTLY (never transitively — see the generator's note). Until
  that file lands, loading an armed material would trap inside the shader loader
  with no python-visible error, so the GPU phases check first and SKIP loudly."""
  return [i for i in _ST_IMPORTS if not core.Path(i).exists]


################################################################################
# PHASE B — JIT compile through the real shader path
################################################################################

def phase_b(ctx):
  fails, notes = [], []
  print("B  JIT compile + technique resolution (real shader path):")
  first = True
  for name, kw in sorted(CONFIGS.items()):
    if name == "terr_mesh" and not ctx.supports_mesh_shader:
      notes.append("B %s: SKIP — device reports no VK_EXT_mesh_shader" % name)
      print("  %-16s SKIP (no mesh shader)" % name)
      continue
    path = materialize_surface_fxv2(BODY, name_hint="st_lowering_%s" % name, **kw)
    mtl = lev2.FreestyleMaterial()
    mtl.gpuInit(ctx, path)      # compiles every declared stage; a bad _ST stage dies here
    if first:
      fails += check_resolver(mtl)
      first = False
    missing = [t for t in sorted(EXPECT_ST[name]) if not resolved(mtl.shader.technique(t))]
    if missing:
      fails.append("B %s: compiled material lacks %s" % (name, missing))
    print("  %-16s %d/%d _ST techniques resolved%s"
          % (name, len(EXPECT_ST[name]) - len(missing), len(EXPECT_ST[name]),
             "" if not missing else "   MISSING %s" % missing))
  return fails, notes


################################################################################
# PHASE C — 2-view render
################################################################################

W = H = 256
ASPECT = float(W) / float(H)
# eyes far enough apart, at the same target, that a per-view transform moves every
# pixel of the ramp; a mono transform moves none.
EYES = ((-1.20, 0.25, 6.0), (1.20, -0.25, 6.0))
# a depth RAMP: two triangles spanning z -8..-2, so both the screen position AND the
# depth the fragment writes are functions of the view.
RAMP = ((-3.0, -2.0, -8.0), (3.0, -2.0, -8.0), (3.0, 2.0, -2.0),
        (-3.0, -2.0, -8.0), (3.0, 2.0, -2.0), (-3.0, 2.0, -2.0))
# 0..1 luma bars. A per-view transform on this geometry moves far more than 2% of
# pixels; the mono control must move none beyond 8-bit quantization.
DIFF_FRAC_FLOOR = 0.02
DIFF_EPS = 2.0 / 255.0
MONO_DIFF_CEIL = 0.001
COVERAGE_FLOOR = 0.05   # a blank capture would pass "identical" for the wrong reason


def phase_c(app):
  ctx = app.ctx
  fails, notes = [], []
  if not ctx.supports_multiview:
    notes.append("C: SKIP — device reports no multiview (max_views=%d)" % ctx.max_multiview_views)
    print("C  2-view render: SKIP (no multiview)")
    return fails, notes

  fxi = ctx.FXI
  path = materialize_surface_fxv2(BODY, name_hint="st_lowering_render", **SSBO_KWARGS)
  mtl = lev2.FreestyleMaterial()
  mtl.gpuInit(ctx, path)
  mtl.rasterstate.culltest  = tokens.OFF
  mtl.rasterstate.depthtest = tokens.LEQUALS

  verts = numpy.array([c for v in RAMP for c in v], dtype=numpy.float32)
  ssbo = fxi.createShaderStorageBufferWithLength(POS_OFF + verts.nbytes)
  fxi.copyDataIntoShaderStorageBuffer(verts, ssbo, POS_OFF)
  for i, v in enumerate((len(RAMP), 1, 0, 0)):   # VkDrawIndirectCommand
    fxi.copyDataIntoShaderStorageBuffer(int(v), ssbo, i * 4)

  cams = []
  for eye in EYES:
    cam = lev2.CameraData()
    cam.perspective(0.5, 60.0, 45.0)
    cam.lookAt(core.vec3(*eye), core.vec3(0, 0, 0), core.vec3(0, 1, 0))
    cm = lev2.CameraMatrices()
    cm.setCustomView(cam.vMatrix())
    cm.setCustomProjection(cam.pMatrix(ASPECT))
    cams.append((cam, cm))

  pipes = {}
  for key, tek_name in (("stereo", "FWD_SSBO_CUSTOM_DEPTHPREPASS_ST"),
                        ("mono",   "FWD_SSBO_CUSTOM_DEPTHPREPASS")):
    tek = mtl.shader.technique(tek_name)
    assert resolved(tek), "generated material has no technique %s" % tek_name
    permu = lev2.FxPipelinePermutation()
    permu.rendermodel = "CUSTOM"
    permu.technique = tek
    pipe = mtl.fxcache.findPipeline(permu)
    assert pipe, "no pipeline for %s" % tek_name
    pipe.bindStorage(mtl.storage("sif_ptex_vtx"), ssbo)
    # BOTH stages get both matrices bound: the mono stage reads mvp, the _ST stage
    # reads m (and spvr_vp from the block). Binding both keeps the two legs
    # identical in every respect except which technique runs.
    pipe.bindParam(mtl.param("mvp"), cams[0][0].vpMatrix(ASPECT))
    pipe.bindParam(mtl.param("m"), core.mtx4())
    pipes[key] = (tek, pipe)

  # ONE 2-layer multiview RtGroup. numLayers + multiview BEFORE the first
  # createBuffer: the layer count is copied into each RtBuffer at construction.
  rtg = lev2.RtGroup(ctx, W, H)
  rtg.name = "StLoweringStereo"
  rtg.numLayers = 2
  rtg.multiview = True
  rtb = rtg.createBuffer(tokens.RGBA8, tokens.color)
  rtb.clearColor = core.vec4(0, 0, 0, 1)
  rtg.createDepthBuffer(tokens.Z32F, False)
  print("C  2-view render: viewMask=0x%x numLayers=%d" % (rtg.viewMask, rtg.numLayers))

  def capture(key):
    tek, pipe = pipes[key]
    caps = [lev2.CaptureBuffer(), lev2.CaptureBuffer()]
    caps[0].capture_layer = 0
    caps[1].capture_layer = 1
    ctx.beginFrame()
    ctx.FBI.rtGroupPush(rtg)
    ctx.FBI.rtGroupClear(rtg)
    RCFD = lev2.RenderContextFrameData(ctx)
    RCID = lev2.RenderContextInstData(RCFD)
    RCID.forceTechnique(tek)
    RCID.genMatrix(lambda: core.mtx4())

    def _draw():
      # inside the draw call the pass is current, which is what the stereo block
      # bind requires. Published for BOTH legs so the mono control differs from
      # the stereo leg in exactly one thing: the technique.
      mtl.publishStereoBlock(RCFD, cams[0][1], cams[1][1])
      ctx.GBI.drawIndirect(args=ssbo, primtype=tokens.TRIANGLES, args_offset=0)

    pipe.wrappedDrawCall(RCID, _draw)
    futures = [ctx.FBI.captureAsFormat(rtb, caps[i], "RGBA8") for i in (0, 1)]
    ctx.FBI.rtGroupPop()
    ctx.endFrame()
    frames = 0
    for i, fut in enumerate(futures):
      while not fut.is_ready:
        app.run_frames(1)
        frames += 1
        assert frames < 600, "%s layer %d capture never became ready" % (key, i)
    return [numpy.array(c, dtype=numpy.uint8).reshape(c.height, c.width, 4) for c in caps]

  def stats(imgs, key):
    a = imgs[0][..., :3].astype(numpy.float32) / 255.0
    b = imgs[1][..., :3].astype(numpy.float32) / 255.0
    frac = float((numpy.abs(a - b).max(axis=2) > DIFF_EPS).mean())
    cov = float(max((a.max(axis=2) > DIFF_EPS).mean(), (b.max(axis=2) > DIFF_EPS).mean()))
    print("  %-7s layer-diff frac=%.4f  coverage=%.4f" % (key, frac, cov))
    return frac, cov

  st_frac, st_cov = stats(capture("stereo"), "stereo")
  mo_frac, mo_cov = stats(capture("mono"), "mono")

  if st_cov < COVERAGE_FLOOR:
    fails.append("C1: stereo capture is blank (coverage %.4f) — the leg proves nothing"
                 % st_cov)
  if st_frac < DIFF_FRAC_FLOOR:
    fails.append("C1: _ST layers agree (diff frac %.4f < %.4f) — no per-view transform"
                 % (st_frac, DIFF_FRAC_FLOOR))
  if mo_cov < COVERAGE_FLOOR:
    fails.append("C2: mono control is blank (coverage %.4f) — the control proves nothing"
                 % mo_cov)
  if mo_frac > MONO_DIFF_CEIL:
    fails.append("C2: MONO technique produced differing layers (diff frac %.4f) — the "
                 "stereo leg's difference is not attributable to the lowering" % mo_frac)
  return fails, notes


################################################################################

def main():
  fails, notes = phase_a()
  with headless_app(subsystems=['opq', 'core', 'gpu', 'lev2']) as app:
    blocked = stereo_import_missing()
    if blocked:
      # BLOCKED, not passed: the emission is complete and asserted by phase A, but no
      # armed material can be LOADED until the stereo block's own .i2 exists. A visible
      # sentinel, never a silent skip — see the generator's direct-import note.
      msg = ("GPU phases BLOCKED — armed materials import %s, which does not resolve "
             "on this tree yet (ublk_stereo relocation not landed). Phase A stands."
             % ", ".join(blocked))
      print("B  %s" % msg)
      print("C  2-view render: BLOCKED (same cause)")
      notes.append(msg)
    else:
      f, n = phase_b(app.ctx)
      fails += f
      notes += n
      f, n = phase_c(app)
      fails += f
      notes += n
    for note in notes:
      print("  NOTE: %s" % note)
    for f in fails:
      print("  FAIL: %s" % f)
    # verdict() takes a BOOLEAN — handing it the PASS/FAIL string constants would be
    # truthy either way and report PASS over a red run.
    verdict(not fails,
            "st-lowering: %d failure(s), %d note(s)" % (len(fails), len(notes)))
  return 0 if not fails else 1


if __name__ == "__main__":
  sys.exit(main())
