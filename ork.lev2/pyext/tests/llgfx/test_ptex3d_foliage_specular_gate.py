#!/usr/bin/env ork.python
################################################################################
# FOLIAGE ENVIRONMENT-SPECULAR + IMPOSTOR SILHOUETTE — the generator contract.
#
# THE DEFECTS THIS CLOSES.
#   (1) THE "COTTON". Distant tree canopies whitened uniformly across a whole
#       distance band, and survived with the sun and the IBL diffuse forced to
#       zero. The surviving term is the IBL ENVIRONMENT SPECULAR: it has a floor
#       and deliberately NO ceiling (fwdtools.i2 pbrEnvironmentLightingWithF0 —
#       a ceiling there is range the tone curve can never get back), and a flat
#       foliage card standing in for a leaf cluster hits grazing Fresnel over the
#       whole canopy and mirrors the sky. The honest scope for the correction is
#       the MATERIAL: surface-level env_specular, NOT the scene's SpecularIntensity
#       (which also darkens terrain and rock) and NOT a global clamp.
#   (2) THE IMPOSTOR SILHOUETTE. The billboard fragment computes a soft coverage
#       alpha that ONLY alpha-to-coverage consumes, but the technique used the
#       material's own state block (A2C off) — and the effective-rasterstate
#       resolution hands an equal-priority TECHNIQUE state block the win, so A2C
#       set anywhere else was discarded every draw. The silhouette was a hard
#       discard cut. The billboard now carries its OWN state block.
#   (3) THE IMPOSTOR MIP INVARIANT. All three bake MRTs (albedo+coverage,
#       normal+ao, metal-rough) store PREMULTIPLIED by coverage against a zeroed
#       background, and the billboard unpremultiplies by the coverage sampled from
#       the SAME mip. That is what keeps albedo AND roughness steady as the atlas
#       minifies; measuring the stored RGB without the unpremultiply reads a
#       ~-67% "drain" that is the measurement, not the pipeline. Leg (E) pins the
#       arithmetic; legs (C)/(D) pin the code that relies on it.
#
# WHY A GENERATOR GATE. These are codegen contracts: which lighting entry a
# material's fragment calls, which state block its technique names, whether a
# tweakable landed in the UBO or got baked into the text. That is exactly what
# reading the generated .fxv2 proves, with no GPU and no scene.
#   PROVEN here: the emitted shader text + param specs.
#   NOT proven here: pixels. The look verdict is an owner render review of the
#                    forest scene; the A2C edge change is measured there.
#
# LEGS (all must pass)
#   (A) IDENTITY   a material that declares no env_specular still emits the stock
#                  _forward_lightingZ call and no EnvSpecularGain — the knob must
#                  not perturb every other material in the engine.
#   (B) BINDABLE   env_specular= lands as a ublk_ptex_params member with its value
#                  in the param specs (A8: tweakables live in a UBO), the shade
#                  routes through the entry that carries it, and the VALUE ITSELF
#                  never appears baked into the lighting call.
#   (C) A2C        the impostor techniques (mono AND the single-pass-stereo peer)
#                  name their own state block, and it declares AlphaToCoverage ON
#                  with BlendMode OFF (VR law: A2C, never alpha blend).
#   (D) AGREEMENT  the billboard attenuates the environment specular by the length
#                  of the blended normal — the 3-view/footprint agreement — so
#                  silhouette pixels whose normals cancel cannot Fresnel-bloom.
#   (E) PREMUL     the bake writes every MRT premultiplied by coverage and the
#                  billboard unpremultiplies; a box filter over that encoding is
#                  proven value-preserving for albedo AND roughness, while the
#                  same filter read WITHOUT the unpremultiply is not.
#
# Self-configuring: no arguments, no environment, no GPU.
#   ork.python test_ptex3d_foliage_specular_gate.py
################################################################################

import os
os.environ["PYTHONUNBUFFERED"] = "1"

import sys
import time

sys.stdout.reconfigure(line_buffering=True)

_ROOT = os.path.abspath(__file__)
for _ in range(5):
  _ROOT = os.path.dirname(_ROOT)
sys.path.insert(0, os.path.join(_ROOT, "obt.project", "scripts"))

import orkengine.core                                        # noqa: E402  (core before lev2)
from orkengine.core import vec3                              # noqa: E402

from ork.hypergraph.assets.materials.terrain.solid import Solid          # noqa: E402
from ork.hypergraph.dflow.hypermesh import GpuMeshRenderSource           # noqa: E402
from ork.hypergraph.ptex3d import materialize_ptex3d_full               # noqa: E402
from ork.hypergraph.ptex3d.fxv2_template import _dslcache_dir           # noqa: E402

ENVSPEC_VALUE = 0.15
PARAM_NAME    = "EnvSpecularGain"
IMPOSTOR_SB   = "sb_ptex_impostor"


def _gen(**kw):
  """Generate a material and return (generated .fxv2 text, param specs)."""
  path, pspecs, _lobes, _caps = materialize_ptex3d_full(
      Solid, name_hint="gate_foliage",
      albedo=vec3(0.10, 0.25, 0.09), roughness=0.45, **kw)
  # the generator returns the relocatable "<staging>/dslshadercache/ptex3d/<f>" token;
  # resolve it against the same cache dir the writer used (a <token> path's .exists lies).
  local = os.path.join(_dslcache_dir("ptex3d"), os.path.basename(path))
  with open(local) as fp:
    return fp.read(), list(pspecs)


def _technique_body(text, name):
  """The brace body of `technique <name> { ... }` (exact name, not a prefix)."""
  head = "technique %s {" % name
  i = text.find(head)
  if i < 0:
    return None
  j = text.find("}", i)
  return text[i + len(head):j]


def _stateblock_body(text, name):
  head = "state_block %s : " % name
  i = text.find(head)
  if i < 0:
    return None
  i = text.find("{", i)
  j = text.find("}", i)
  return text[i + 1:j]


def _box_filter(img):
  """2x2 box downsample of an HxWxC float array (list-of-lists free: pure python)."""
  h = len(img) - (len(img) % 2)
  w = len(img[0]) - (len(img[0]) % 2)
  out = []
  for y in range(0, h, 2):
    row = []
    for x in range(0, w, 2):
      row.append([0.25 * (img[y][x][c] + img[y + 1][x][c] + img[y][x + 1][c] + img[y + 1][x + 1][c])
                  for c in range(len(img[0][0]))])
    out.append(row)
  return out


def main():
  t0 = time.time()
  fails = []

  ##############################################################################
  # (A) IDENTITY — no env_specular declared: stock lighting entry, no new uniform
  ##############################################################################
  plain, plain_specs = _gen()
  if "_forward_lightingZ(" not in plain:
    fails.append("IDENTITY: a material with no env_specular does not call _forward_lightingZ — "
                 "the stock lighting entry moved for every material in the engine")
  if PARAM_NAME in plain or "_forward_lightingZQE(" in plain:
    fails.append("IDENTITY: a material with no env_specular carries %s / the env-spec lighting "
                 "entry — the opt-in leaked into every other material" % PARAM_NAME)
  if any(n == PARAM_NAME for (n, _g, _d) in plain_specs):
    fails.append("IDENTITY: %s appears in the param specs of a material that never asked for it"
                 % PARAM_NAME)
  print("LEG_A identity: stock entry present, no %s (%d param specs)" % (PARAM_NAME, len(plain_specs)))

  ##############################################################################
  # (B) BINDABLE — env_specular= is a UBO member, not a baked constant (A8)
  ##############################################################################
  es, es_specs = _gen(env_specular=ENVSPEC_VALUE)
  got = [(n, g, d) for (n, g, d) in es_specs if n == PARAM_NAME]
  if not got:
    fails.append("BINDABLE: env_specular=%s produced no %s param spec — the value cannot be "
                 "rebound at runtime (A8: tweakables live in a UBO, never in the shader text)"
                 % (ENVSPEC_VALUE, PARAM_NAME))
  elif abs(float(got[0][2]) - ENVSPEC_VALUE) > 1e-6:
    fails.append("BINDABLE: %s default is %s, expected %s" % (PARAM_NAME, got[0][2], ENVSPEC_VALUE))
  if ("uniform_block ublk_ptex_params" not in es) or ("vec4 %s;" % PARAM_NAME) not in es:
    fails.append("BINDABLE: %s is not declared in ublk_ptex_params" % PARAM_NAME)
  call_i = es.find("_forward_lightingZQE(")
  if call_i < 0:
    fails.append("BINDABLE: the surface fragment does not route through the lighting entry that "
                 "carries the env-spec gain (_forward_lightingZQE)")
  else:
    call = es[call_i:es.find(";", call_i)]
    if "%s.x" % PARAM_NAME not in call:
      fails.append("BINDABLE: the lighting call does not read %s.x: <%s>" % (PARAM_NAME, call.strip()))
    if str(ENVSPEC_VALUE) in call:
      fails.append("BINDABLE: the lighting call BAKED the literal %s into the shader text (A8) — "
                   "the value must arrive through the uniform" % ENVSPEC_VALUE)
  print("LEG_B bindable: %s in ublk_ptex_params, default %s, read as a uniform"
        % (PARAM_NAME, got[0][2] if got else "MISSING"))

  ##############################################################################
  # (C) A2C — the billboard's OWN state block, on both the mono and stereo peers
  ##############################################################################
  imp, _ = _gen(vertex_source=GpuMeshRenderSource(instanced=True), impostor=True)
  sb = _stateblock_body(imp, IMPOSTOR_SB)
  if sb is None:
    fails.append("A2C: no `state_block %s` in an impostor material — the billboard is back on the "
                 "material's block, where its computed coverage alpha is dead code" % IMPOSTOR_SB)
  else:
    if "AlphaToCoverage = ON;" not in sb:
      fails.append("A2C: %s does not declare AlphaToCoverage ON — the silhouette is a hard "
                   "discard cut again" % IMPOSTOR_SB)
    if "BlendMode = OFF;" not in sb:
      fails.append("A2C: %s does not declare BlendMode OFF — VR law is A2C, never alpha blend"
                   % IMPOSTOR_SB)
  for tek in ("FWD_SSBO_CUSTOM_IMPOSTOR", "FWD_SSBO_CUSTOM_IMPOSTOR_ST"):
    body = _technique_body(imp, tek)
    if body is None:
      fails.append("A2C: technique %s is not emitted for an instanced impostor material" % tek)
    elif IMPOSTOR_SB not in body:
      fails.append("A2C: technique %s does not name %s — its coverage alpha is unconsumed"
                   % (tek, IMPOSTOR_SB))
  print("LEG_C a2c: %s declared and named by both impostor techniques" % IMPOSTOR_SB)

  ##############################################################################
  # (D) AGREEMENT — env specular scaled by the blended-normal length
  ##############################################################################
  if "float agree" not in imp or "length(Nb)" not in imp:
    fails.append("AGREEMENT: the billboard fragment does not derive a view-agreement scalar from "
                 "the blended normal — disagreeing silhouette normals Fresnel-bloom again")
  imp_call_i = imp.find("_forward_lightingZQE(")
  if imp_call_i < 0:
    fails.append("AGREEMENT: the billboard does not route through the env-spec lighting entry")
  else:
    imp_call = imp[imp_call_i:imp.find(";", imp_call_i)]
    if "agree" not in imp_call:
      fails.append("AGREEMENT: the billboard's lighting call ignores `agree`: <%s>"
                   % " ".join(imp_call.split()))
  print("LEG_D agreement: billboard env specular scaled by the blended-normal length")

  ##############################################################################
  # (E) PREMUL — the bake/sample encoding, and the arithmetic it depends on
  ##############################################################################
  for frag in ("out_albedo_cov  = vec4(s.albedo * _cov, _cov);",
               "out_normal_ao   = vec4((normalize(s.normal) * 0.5 + 0.5) * _cov, s.ao * _cov);",
               "out_metal_rough = vec4(s.metallic * _cov, s.roughness * _cov, 0.0, _cov);"):
    if frag not in imp:
      fails.append("PREMUL: the impostor bake no longer writes <%s> — an MRT that is not "
                   "coverage-weighted drains (metal-rough background roughness 0 = mirror)"
                   % frag.split("=")[0].strip())
  if "float inv = 1.0 / cov;" not in imp or "mr.y * inv" not in imp:
    fails.append("PREMUL: the billboard no longer unpremultiplies every channel by the sampled "
                 "coverage — albedo and roughness drain as the atlas minifies")

  # the arithmetic, on the population a distant impostor actually shades: the texels whose
  # coverage clears the billboard's cutoff. Two materials (albedo .2/.8, roughness .3/.9) at
  # mixed coverage, stored premultiplied against a zeroed background, minified by box filter.
  W, CUT = 16, 0.25
  img = []
  for y in range(W):
    row = []
    for x in range(W):
      cov = 0.0 if (x + y) % 3 == 0 else (0.4 if x < W // 2 else 1.0)
      alb = 0.2 if y < W // 2 else 0.8
      rgh = 0.3 if y < W // 2 else 0.9
      row.append([alb * cov, rgh * cov, cov])    # premultiplied albedo, premultiplied rough, coverage
    img.append(row)

  def means(level):
    """(unpremultiplied albedo, unpremultiplied roughness, RAW stored albedo) over covered texels."""
    px = [t for row in level for t in row if t[2] >= CUT]
    n  = float(len(px))
    return (sum(t[0] / t[2] for t in px) / n,
            sum(t[1] / t[2] for t in px) / n,
            sum(t[0] for t in px) / n)

  a0, r0, raw0 = means(img)
  lvl = img
  while len(lvl) > 2:
    lvl = _box_filter(lvl)
  aN, rN, rawN = means(lvl)
  d_a   = (aN - a0) / a0
  d_r   = (rN - r0) / r0
  d_raw = (rawN - raw0) / raw0
  if abs(d_a) > 0.05 or abs(d_r) > 0.05:
    fails.append("PREMUL: minifying the premultiplied encoding and unpremultiplying by the same "
                 "mip's coverage does NOT hold the value (albedo %+.1f%%, roughness %+.1f%%) — "
                 "the invariant the bake relies on is broken" % (d_a * 100.0, d_r * 100.0))
  if abs(d_raw) < 0.10:
    fails.append("PREMUL: the CONTROL is vacuous — reading the stored RGB WITHOUT the "
                 "unpremultiply drifted only %+.1f%%, so a green here would not distinguish the "
                 "encoding from doing nothing" % (d_raw * 100.0))
  print("LEG_E premul: unpremultiplied albedo %+.2f%% / roughness %+.2f%% over the mip chain; "
        "the same chain read raw (the measurement that reported a 'drain') moves %+.1f%%"
        % (d_a * 100.0, d_r * 100.0, d_raw * 100.0))

  dt = time.time() - t0
  if fails:
    print("TESTVERDICT FAIL (%d): %s" % (len(fails), "; ".join(fails[:4])), flush=True)
    print("test_ptex3d_foliage_specular_gate: FAIL in %.1fs" % dt, flush=True)
    return 1
  print("TESTVERDICT PASS -- env_specular is an opt-in UBO param (no other material moved), the "
        "impostor billboard owns an A2C state block and attenuates environment specular by view "
        "agreement, and the premultiplied atlas encoding is mip-invariant (%.1fs)" % dt, flush=True)
  return 0


if __name__ == "__main__":
  sys.exit(main())
