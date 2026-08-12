#!/usr/bin/env ork.python
################################################################################
# IMPOSTOR BILLBOARD FRAME + LEAF TONE — the two view-coupling defects, pinned.
#
# THE DEFECTS THIS CLOSES.
#   (1) THE ROLLING TREES. The billboard vertex stage built its quad basis from
#       the inverse-view ROWS (right = inv_v[0], up = inv_v[1]) — the camera's own
#       right/up. Those two columns are exactly what a ROLL rotates, so the quad
#       was glued to the screen axes: in VR every distant tree tilted with the
#       head instead of standing up in the world. Moving the impostor switch from
#       1250 m to 600 m put the band twice as close and made it plain.
#   (2) THE TWO-TONE LEAVES. The cards draw cull=off, so one of the two facings
#       must be picked for lighting, and the pick was gl_FrontFacing — SCREEN
#       WINDING. That is a per-view boolean: a card near edge-on resolves to
#       opposite normals in the two eyes and flips under head motion, so one leaf
#       shades as two discrete tones. Owner ruling 2026-08-09: tone selection is
#       frame-stable and 100% view-independent, so the resolve is against the SUN
#       (world state) instead.
#
# WHY A SOURCE GATE IS A PROOF HERE, NOT A PROXY. A camera ROLL is a rotation
# about the view axis: it rewrites inv_v columns 0 and 1 and leaves column 2 (the
# view axis) fixed. A vertex stage that never reads columns 0/1 therefore cannot
# couple to roll — the invariance is structural, and reading the emitted stage is
# how you prove a structural property. Same shape for the leaf tail: tone cannot
# depend on the view if no view-derived name appears in the expression that picks
# the normal. Phase B then compiles and selects the real thing on the GPU, so a
# stage that satisfies the text and does not build cannot pass.
#   PROVEN here: the emitted billboard basis reads only the roll-invariant view
#                axis, anchors to world up, falls back exactly as the bake does,
#                keeps the direction-keyed 3-view blend + agreement attenuation,
#                and compiles + selects in both mono and single-pass-stereo form;
#                the leaf/needle two-sided resolve names no view quantity.
#   NOT proven here: pixels. The billboard's rendered silhouette under a rolled
#                camera needs the forest content pipeline (bake + LOD tier), so
#                the look verdict stays a scene render — for the level camera it
#                was measured invisible (vista MAE 0.14, run-to-run noise 0.27).
#
# Self-configuring: no arguments, no environment.
#   ork.python test_impostor_billboard_roll_gate.py
################################################################################

import os
os.environ["PYTHONUNBUFFERED"] = "1"

import re
import sys

sys.stdout.reconfigure(line_buffering=True)

_ROOT = os.path.abspath(__file__)
for _ in range(5):
  _ROOT = os.path.dirname(_ROOT)
sys.path.insert(0, os.path.join(_ROOT, "obt.project", "scripts"))

import orkengine.core                                              # noqa: E402  (core before lev2)
from orkengine import lev2                                         # noqa: E402
from ork.testing import headless_app, verdict                      # noqa: E402
from ork.hypergraph.ptex3d import materialize_surface_fxv2         # noqa: E402
from ork.hypergraph.ptex3d.fxv2_template import generate_surface_fxv2  # noqa: E402

# the source sites the frame contract spans: the SAMPLE side (billboard basis) must
# agree with the BAKE side (which framed every atlas tile), so the bake is read too.
BAKE_SRC   = os.path.join(_ROOT, "ork.lev2", "src", "gfx", "hypermesh", "hmdflow_render.cpp")
LEAF_SRC   = os.path.join(_ROOT, "obt.project", "scripts", "ork", "hypergraph",
                          "assets", "materials", "leaf.py")
NEEDLE_SRC = os.path.join(_ROOT, "obt.project", "scripts", "ork", "hypergraph",
                          "assets", "materials", "needle.py")

MONO_TEK   = "FWD_SSBO_CUSTOM_IMPOSTOR"
STEREO_TEK = "FWD_SSBO_CUSTOM_IMPOSTOR_ST"

# the impostor billboard family is emitted by ssbo_instanced + wants_capture (the capture
# IS the atlas bake) — not by a kwarg named "impostor". Local copy on purpose: a gate that
# breaks when an unrelated test is edited is not a gate.
BODY = ("o.albedo   = vec3(uv.x, uv.y, 0.5);\n"
        "o.emissive = (wnrm * 0.5 + vec3(0.5)) * 0.55;")
IMPOSTOR_KWARGS = dict(
  ssbo_layout=("uint args[4];\n"
               "float Pos[];"),
  ssbo_vs_body=("uint i = uint(gl_VertexID);\n"
                "vec4 position = vec4(Pos[i*3u], Pos[i*3u+1u], Pos[i*3u+2u], 1.0);\n"
                "vec3 normal = vec3(0,1,0);\n"
                "vec3 binormal = vec3(1,0,0);\n"
                "vec2 uv0 = vec2(position.x*0.1+0.5, position.y*0.1+0.5);\n"
                "vec4 vtxcolor = vec4(1.0);"),
  ssbo_instanced=True,
  wants_capture=True)


def stage_body(src, name):
  """-> the text of vertex_shader `name`, brace-matched from its opening block."""
  m = re.search(r"vertex_shader\s+%s\b" % re.escape(name), src)
  if not m:
    return None
  i = src.find("{", m.end())
  if i < 0:
    return None
  depth, j = 0, i
  while j < len(src):
    if src[j] == "{":
      depth += 1
    elif src[j] == "}":
      depth -= 1
      if depth == 0:
        return src[i:j + 1]
    j += 1
  return None


def resolved(tek):
  """A technique MISS hands back a TRUTHY wrapper around a null handle; the
  `0x0:nulltek` repr is the only discriminator python gets. Phase B proves the
  discriminator still discriminates before trusting it."""
  return "0x0:nulltek" not in repr(tek)


################################################################################
# PHASE A — the emitted billboard stages (no GPU)
################################################################################

def phase_a():
  fails = []
  src = generate_surface_fxv2(BODY, **IMPOSTOR_KWARGS)
  stages = {"mono": stage_body(src, "vs_ptex_impostor"),
            "stereo": stage_body(src, "vs_ptex_impostor_stereo")}
  print("A  billboard basis:")

  # CONTROL: both stages must be found, or every assertion below is vacuous.
  missing = [k for k, v in stages.items() if not v]
  if missing:
    return ["A: billboard stage(s) %s not found in the generated material — the "
            "frame assertions would all pass vacuously" % missing]

  for key, body in stages.items():
    # (1) ROLL. Roll rewrites inv_v columns 0/1 and fixes column 2, so reading
    #     either of the first two re-couples the quad to the screen axes.
    rolled = re.findall(r"inv_v\s*\[\s*[01]\s*\]", body)
    # (2) WORLD UP. The frame must be anchored to a world-up reference crossed
    #     with the view axis — the basis the bake's lookAt produced.
    anchored = ("inv_v[2]" in body) and re.search(r"cross\s*\(\s*upref\s*,", body)
    # (3) DEGENERATE. Straight-down/up collapses the cross; the fallback must be
    #     the SAME reference the bake used for its pole tiles.
    fallback = re.search(r"abs\s*\(\s*\w+\.y\s*\)\s*>\s*0\.99", body) and "vec3(0.0, 0.0, 1.0)" in body
    print("  %-6s reads inv_v[0|1]: %-5s   world-up anchored: %-5s   pole fallback: %s"
          % (key, bool(rolled), bool(anchored), bool(fallback)))
    if rolled:
      fails.append("A %s: billboard basis reads %s — the quad rolls with the camera"
                   % (key, sorted(set(rolled))))
    if not anchored:
      fails.append("A %s: basis is not world-up anchored off the view axis" % key)
    if not fallback:
      fails.append("A %s: no pole fallback matching the bake's up reference" % key)

  # (4) BAKE AGREEMENT. The sample-side fallback threshold + reference are only
  #     correct if they are the bake's; read the bake and compare.
  bake = open(BAKE_SRC).read()
  bake_ok = re.search(r"std::abs\(dir\.y\)\s*>\s*0\.99f\s*\)\s*\?\s*fvec3\(0,\s*0,\s*1\)", bake)
  print("  bake pole reference is +Z past |dir.y|>0.99      %s" % ("OK" if bake_ok else "FAIL"))
  if not bake_ok:
    fails.append("A: the bake's up-vector fallback moved — the sample-side frame no longer "
                 "matches the frame the atlas was captured in")

  # (5) DIRECTION-KEYED FEATURES SURVIVE. The 3-view blend and the view-agreement
  #     specular attenuation key off the world view DIRECTION (roll-invariant) and
  #     must not have been disturbed by the frame change.
  blend = ("octa_uv(vdir)" in src) and ("EyePostion - frg_wpos.xyz" in src)
  agree = re.search(r"agree\s*=\s*clamp\s*\(\s*length\s*\(\s*Nb\s*\)", src)
  print("  3-view blend keyed on the view direction         %s" % ("OK" if blend else "FAIL"))
  print("  view-agreement specular attenuation intact       %s" % ("OK" if agree else "FAIL"))
  if not blend:
    fails.append("A: the hemi-oct 3-view blend no longer keys off the fragment view direction")
  if not agree:
    fails.append("A: the view-agreement specular attenuation is gone")
  return fails


################################################################################
# PHASE B — the leaf/needle two-sided resolve (no GPU)
################################################################################

def phase_b():
  fails = []
  print("B  leaf/needle two-sided resolve (owner ruling: view-independent tone):")
  for name, path in (("leaf", LEAF_SRC), ("needle", NEEDLE_SRC)):
    src = open(path).read()
    m = re.search(r"_SURF_TAIL\s*=\s*\((.*?)\)\n", src, re.S)
    tail = m.group(1) if m else ""
    if not tail:
      fails.append("B %s: no _SURF_TAIL found — the resolve assertions are vacuous" % name)
      continue
    facing = "gl_FrontFacing" in tail
    sun    = "sun_dir" in tail
    print("  %-6s picks on gl_FrontFacing: %-5s   resolves against the sun: %s"
          % (name, facing, sun))
    if facing:
      fails.append("B %s: the shading normal is picked by screen winding — one card shades "
                   "as two tones, differently in each eye" % name)
    if not sun:
      fails.append("B %s: the two-sided resolve names no world-space reference" % name)
  return fails


################################################################################
# PHASE C — GPU: the edited stages compile and both techniques still select
################################################################################

def phase_c(ctx):
  fails = []
  print("C  JIT compile + impostor technique selection (real shader path):")
  path = materialize_surface_fxv2(BODY, name_hint="impostor_roll_gate", **IMPOSTOR_KWARGS)
  mtl = lev2.PBRMaterial()
  mtl.shaderpath = path
  mtl.gpuInit(ctx)          # compiles EVERY declared stage — a bad basis edit dies here
  freestyle = mtl.freestyle
  if freestyle is not None:
    bogus = freestyle.shader.technique("FWD_NO_SUCH_TECHNIQUE_ZZZ")
    real  = freestyle.shader.technique(MONO_TEK)
    if resolved(bogus) or not resolved(real):
      return ["C: technique-resolution control broke (bogus=%s real=%s) — every "
              "assertion here would be vacuous" % (resolved(bogus), resolved(real))]

  cache = mtl.fxcache

  def select(stereo):
    permu = lev2.FxPipelinePermutation()
    permu.rendermodel = "FORWARD_PBR"
    permu.is_impostor = True
    permu.stereo = stereo
    pipe = cache.findPipeline(permu)
    return pipe.technique_name if pipe else None

  got_mo, got_st = select(False), select(True)
  print("  mono   -> %s" % got_mo)
  print("  stereo -> %s" % got_st)
  if got_mo != MONO_TEK:
    fails.append("C: mono impostor permutation selected %r, expected %r" % (got_mo, MONO_TEK))
  if got_st != STEREO_TEK:
    fails.append("C: stereo impostor permutation selected %r, expected %r" % (got_st, STEREO_TEK))
  return fails


################################################################################

def main():
  fails = phase_a() + phase_b()
  with headless_app(subsystems=['opq', 'core', 'gpu', 'lev2']) as app:
    fails += phase_c(app.ctx)
    for f in fails:
      print("  FAIL: %s" % f)
    verdict(not fails, "impostor-roll/leaf-tone: %d failure(s)" % len(fails))
  return 0 if not fails else 1


if __name__ == "__main__":
  sys.exit(main())
