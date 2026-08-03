#!/usr/bin/env ork.python
################################################################################
# SPVR — THE LOD-IMPOSTOR STEREO SELECTION (linux/NV, offscreen).
#
# THE DEFECT THIS CLOSES. The generated impostor material has emitted a
# FWD_SSBO_CUSTOM_IMPOSTOR_ST peer all along (fxv2_template, the mono billboard
# verbatim with the clip transform swapped for spvr_vp[ofx_viewIndex]). Nothing
# ever looked it up: there was no _tek_FWD_SSBO_CUSTOM_IMPOSTOR_ST member, and
# the impostor arm in the forward pipeline picked the MONO technique
# UNCONDITIONALLY — it never tested permu._stereo. So every impostor-LOD instance
# rendered mono into BOTH eye layers: bit-identical eyes, zero parallax, fusion
# break, on the distant trees of the owner's own forest scene.
#
# WHY THIS IS A SELECTION TEST AND WHAT THAT DOES NOT COVER. Reaching the impostor
# arm through PIXELS needs a baked hemi-oct atlas plus an LOD-tier scene far enough
# out to demote to the billboard — the forest content pipeline. This test instead
# asks the REAL pipeline cache, on a REAL generated impostor material, which
# technique an impostor draw WOULD take. That is a genuine selection proof: on the
# broken tree it returns the MONO name and this test FAILS. It is deliberately not
# the same claim as measured parallax; the honest bound is stated in the report and
# repeated here so nobody reads more into a green than it carries.
#   PROVEN here: the stereo impostor permutation SELECTS the _ST technique, and
#                the mono permutation still selects the mono one.
#   NOT proven here: that the selected _ST technique produces different PIXELS per
#                eye. Its per-view clip idiom is pinned by the ptex3d lowering gate
#                (source + JIT + 2-view render), and the arm attaches
#                createBasicStateLambda, which is what writes and binds ublk_stereo
#                — so the residual gap is narrow, but it is a gap.
#
# LEGS (all must pass)
#   (a) CONTROL   the cache must return two DIFFERENT, non-empty technique names for
#                 the two permutations — a discrimination proof, so a green can never
#                 mean "the selector answered the same thing twice".
#   (c) SELECT    the pipeline cache, handed an impostor permutation with stereo ON,
#                 returns a pipeline whose technique IS the _ST peer. THIS is the leg
#                 that was red before the wiring.
#   (d) MONO      the same cache, stereo OFF, still returns the mono technique — the
#                 wiring must not have stolen the dual-pass path.
#
# Self-configuring: no arguments, no environment.
#   ork.python test_spvr_impostor_stereo.py
################################################################################

import os
os.environ["PYTHONUNBUFFERED"] = "1"
os.environ.setdefault("ORKID_VULKAN_VALIDATE", "2")

import sys
import time

sys.stdout.reconfigure(line_buffering=True)

_ROOT = os.path.abspath(__file__)
for _ in range(5):
  _ROOT = os.path.dirname(_ROOT)
sys.path.insert(0, os.path.join(_ROOT, "obt.project", "scripts"))
sys.path.insert(0, os.path.join(_ROOT, "obt.project", "bin"))

MONO_TEK = "FWD_SSBO_CUSTOM_IMPOSTOR"
STEREO_TEK = "FWD_SSBO_CUSTOM_IMPOSTOR_ST"

# The generated surface + SSBO vertex source, mirroring the ptex3d lowering gate's
# "inst_impostor" config verbatim: the impostor billboard family is emitted by
# ssbo_instanced + wants_capture (the capture IS the atlas bake), not by a kwarg
# named "impostor". Kept a local copy rather than imported from the sibling test —
# a gate that breaks when an unrelated test is edited is not a gate.
BODY = ("o.albedo   = vec3(uv.x, uv.y, 0.5);\n"
        "o.emissive = (wnrm * 0.5 + vec3(0.5)) * 0.55\n"
        "           + vec3(uv.x, uv.y, fract(opos.y * 0.02)) * 0.45;")
SSBO_KWARGS = dict(
  ssbo_layout=("uint args[4];\n"
               "float Pos[];"),
  ssbo_vs_body=("uint i = uint(gl_VertexID);\n"
                "vec4 position = vec4(Pos[i*3u], Pos[i*3u+1u], Pos[i*3u+2u], 1.0);\n"
                "vec3 normal = vec3(0,1,0);\n"
                "vec3 binormal = vec3(1,0,0);\n"
                "vec2 uv0 = vec2(position.x*0.1+0.5, position.y*0.1+0.5);\n"
                "vec4 vtxcolor = vec4(1.0);"))


def resolved(tek):
  """A technique MISS does not return None: the binding hands back an unmanaged
  wrapper around a null FxShaderTechnique, and that wrapper is TRUTHY in python
  (it reprs as `FxShaderTechnique(0x0:nulltek)`). The null handle in the repr is
  the only discriminator python gets — leg (a) proves it still discriminates."""
  return "0x0:nulltek" not in repr(tek)


def main():
  t0 = time.time()
  fails = []

  from orkengine import core
  from orkengine import lev2
  from ork.testing import headless_app
  from ork.hypergraph.ptex3d import materialize_surface_fxv2

  with headless_app(subsystems=['opq', 'core', 'gpu', 'lev2']) as app:
    ctx = app.ctx

    # a GENERATED impostor material: ssbo_instanced + impostor is the combination the
    #  forest's distant trees use, and the only one that emits the billboard family.
    path = materialize_surface_fxv2(
        BODY, name_hint="spvr_impostor_sel",
        **dict(SSBO_KWARGS, ssbo_instanced=True, wants_capture=True))
    print("impostor: generated %s" % path, flush=True)

    # THE SELECTOR IS ALSO ITS OWN CONTROL. An earlier draft read the compiled
    #  technique TABLE through FreestyleMaterial and keyed on the `0x0:nulltek` repr
    #  that marks a lookup miss; on this material a bogus name did NOT carry that
    #  marker, so the control reported itself broken — correctly refusing to let the
    #  legs pass vacuously. Rather than chase the repr, the legs below ask the thing
    #  under test directly and read a real STRING back: the cache must return two
    #  DIFFERENT, non-empty technique names for the two permutations, which is a
    #  discrimination proof no null-handle convention can fake.

    # ---- the real selector: a PBRMaterial over the same generated shader, asked
    #      through its own pipeline cache.
    mtl = lev2.PBRMaterial()
    mtl.shaderpath = path
    mtl.gpuInit(ctx)
    cache = mtl.fxcache

    def select(stereo):
      permu = lev2.FxPipelinePermutation()
      permu.rendermodel = "FORWARD_PBR"
      permu.is_impostor = True
      permu.stereo = stereo
      pipe = cache.findPipeline(permu)
      return pipe.technique_name if pipe else None

    got_st = select(True)
    got_mo = select(False)
    print("LEG_SELECT stereo=True  -> %s" % got_st, flush=True)
    print("LEG_MONO   stereo=False -> %s" % got_mo, flush=True)

    # ---- leg (a): the selector discriminates at all
    if (not got_st) or (not got_mo):
      print("TESTVERDICT FAIL: the pipeline cache returned no technique name (st=%s mo=%s) "
            "— every assertion below would be vacuous" % (got_st, got_mo), flush=True)
      return 1
    if got_st == got_mo:
      fails.append("CONTROL: stereo and mono impostor permutations both select <%s> — the "
                   "selector cannot tell them apart, which is the defect itself" % got_st)

    # ---- leg (c): THE leg that was red before the wiring
    if got_st != STEREO_TEK:
      fails.append("SELECT: an impostor draw in a stereo pass takes <%s>, expected <%s> "
                   "— every impostor-LOD instance writes the SAME image into both eye "
                   "layers" % (got_st, STEREO_TEK))
    # ---- leg (d): the mono path must be untouched
    if got_mo != MONO_TEK:
      fails.append("MONO: a non-stereo impostor draw takes <%s>, expected <%s> — the "
                   "stereo wiring stole the dual-pass path" % (got_mo, MONO_TEK))

    verrs = int(ctx.validation_errors)
    armed = bool(ctx.validation_armed)
    print("VALIDATION armed=%s errors=%d" % (armed, verrs), flush=True)
    if armed and verrs != 0:
      fails.append("validation reported %d error(s)" % verrs)

  dt = time.time() - t0
  if fails:
    print("TESTVERDICT FAIL (%d): %s" % (len(fails), "; ".join(fails[:4])), flush=True)
    print("test_spvr_impostor_stereo: FAIL in %.1fs" % dt, flush=True)
    return 1
  print("TESTVERDICT PASS -- an impostor draw inside a single-pass-stereo pass SELECTS %s, "
        "and a mono draw still selects %s (%.1fs)" % (STEREO_TEK, MONO_TEK, dt), flush=True)
  return 0


if __name__ == "__main__":
  sys.exit(main())
