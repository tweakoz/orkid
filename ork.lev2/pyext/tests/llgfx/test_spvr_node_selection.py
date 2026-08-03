#!/usr/bin/env ork.python
################################################################################
# SPVR — OUTPUT-NODE SELECTION (linux/NV, offscreen).
#
# WHICH NODE DOES A GIVEN PRESET + ENVIRONMENT ACTUALLY INSTALL? Every answer
# below was previously demonstrated only by hand, in scratch runs, and every one
# of them is a SILENT wrong answer when it regresses: installing the dual-mono
# node where the single-pass node was asked for renders a correct picture, twice
# as expensively; installing the single-pass node where the caller demanded the
# literal dual-mono one breaks the committed DMVR gate's premise. Neither shows
# up in pixels, in validation, or in a frame time anybody is watching.
#
# WHY EVERY LEG IS ITS OWN PROCESS. All three env switches are read through
# function-local `static const bool` initializers — read ONCE per process, by
# design (they are announced once). Two arms in one process would measure the
# first arm twice. So each row below is a subprocess with its own environment,
# and the parent only compares the node class each one reports.
#
# THE MATRIX (each row: env -> preset -> expected output node)
#   1  (none)                          FWDPBRSPVR  -> SinglePassStereoVr
#   2  ORKID_SPVR_NO_MULTIVIEW=1       FWDPBRSPVR  -> DualMonoVr   (FALLBACK ARM:
#      the explicit ask degrades instead of failing at the first layered pass;
#      this is the only way a multiview-capable box can exercise that path)
#   3  (none)                          FWDPBRVRDM  -> DualMonoVr   (autoselect is
#      OFF by default — the preset string stays literal)
#   4  ORKID_SPVR=1                    FWDPBRVRDM  -> SinglePassStereoVr
#      (autoselect ARMED: same preset string in, better node out)
#   5  ORKID_SPVR=1 + ORKID_FORCE_DMVR FWDPBRVRDM  -> DualMonoVr
#   6  ORKID_FORCE_DMVR                FWDPBRSPVR  -> SinglePassStereoVr
#
#   Rows 5 and 6 together are what ORKID_FORCE_DMVR's LITERAL meaning is: it
#   disables AUTOSELECT and nothing else. It is not a global "no single-pass
#   stereo" switch — an explicitly named preset is not gated by it. Row 5 alone
#   would be satisfied by a knob that turned the whole feature off; row 6 is what
#   forbids that reading, and the committed DMVR gate depends on this exact
#   semantic ("this node", not "the best node available").
#
#   7  REPORTING SURFACES: Scene.hzb (the occlusion-pyramid provenance handle) is
#      present and, once built, carries sourceDepthFrame/valid; and the
#      ORKID_HZB_ALLOW_SAMEFRAME revert knob is present at its read site with its
#      announcement. Reporting semantics only — nothing culls on either, and this
#      leg claims only PRESENCE, since the knob is read in C++ during forward
#      rendering and no python surface exposes its armed state.
#
# SKIP DISCIPLINE: on a device without multiview, rows 1/4/6 have no meaningful
# answer and the whole test SKIPs loudly with the capability printed. It never
# passes by not looking.
#
# Self-configuring: ORKID_VULKAN_VALIDATE=2 in-code. Default invocation needs no
# arguments and no environment.
#
#   ork.python test_spvr_node_selection.py
################################################################################

import os
os.environ["PYTHONUNBUFFERED"] = "1"
os.environ.setdefault("ORKID_VULKAN_VALIDATE", "2")

import sys
import json
import time
import tempfile
import subprocess

sys.stdout.reconfigure(line_buffering=True)

_ROOT = os.path.abspath(__file__)
for _ in range(5):
  _ROOT = os.path.dirname(_ROOT)
sys.path.insert(0, os.path.join(_ROOT, "obt.project", "scripts"))
sys.path.insert(0, os.path.join(_ROOT, "obt.project", "bin"))

FWD_TOP_CPP = os.path.join(_ROOT, "ork.lev2", "src", "gfx", "renderer",
                           "NodeCompositor", "forward", "fwdnode_impl_top.cpp")

SPVR = "SinglePassStereoVrOutputNode"
DMVR = "DualMonoVrOutputNode"

# (name, env overrides, preset, expected node class, why)
CASES = (
    ("explicit_spvr", {}, "FWDPBRSPVR", SPVR,
     "the explicit single-pass preset on a multiview-capable device"),
    ("fallback_arm", {"ORKID_SPVR_NO_MULTIVIEW": "1"}, "FWDPBRSPVR", DMVR,
     "capability forced unavailable -> the explicit ask DEGRADES to dual-mono"),
    ("autoselect_off", {}, "FWDPBRVRDM", DMVR,
     "autoselect is off by default -> the preset string stays literal"),
    ("autoselect_on", {"ORKID_SPVR": "1"}, "FWDPBRVRDM", SPVR,
     "autoselect armed -> the same preset string resolves to the single-pass node"),
    ("force_dmvr_beats_autoselect", {"ORKID_SPVR": "1", "ORKID_FORCE_DMVR": "1"},
     "FWDPBRVRDM", DMVR,
     "ORKID_FORCE_DMVR disables autoselect"),
    ("force_dmvr_is_autoselect_only", {"ORKID_FORCE_DMVR": "1"}, "FWDPBRSPVR", SPVR,
     "...and ONLY autoselect: an explicitly named preset is not gated by it"),
)

HZB_ENV = "ORKID_HZB_ALLOW_SAMEFRAME"


################################################################################
# CHILD: build ONE scene with ONE preset and report what got installed.
################################################################################


def _probe(preset, outpath):
  from orkengine import core
  from orkengine import lev2
  from ork.testing import headless_app

  result = {"preset": preset}
  with headless_app(subsystems=['opq', 'core', 'gpu', 'lev2']) as app:
    ctx = app.ctx
    result["multiview"] = bool(ctx.supports_multiview)
    result["max_views"] = int(ctx.max_multiview_views)

    params = core.VarMap()
    params.preset = preset
    params.SkyboxIntensity = 1.0
    params.SpecularIntensity = 1.0
    params.DiffuseIntensity = 1.0
    params.AmbientLight = core.vec3(0.0)
    params.DepthFogDistance = float(1e6)
    scene = lev2.scenegraph.Scene(params)

    node = scene.compositoroutputnode
    result["node"] = repr(node).split("(")[0] if node is not None else None
    # the HZB provenance handle: present as a property before any render (null
    #  until the forward node builds a pyramid), and typed when it is not.
    result["hzb_property"] = hasattr(scene, "hzb")
    hzb = getattr(scene, "hzb", None)
    result["hzb_built"] = hzb is not None
    if hzb is not None:
      result["hzb_sourceDepthFrame"] = int(hzb.sourceDepthFrame)
      result["hzb_valid"] = bool(hzb.valid)
    print("SELECTION preset=%s node=%s multiview=%s"
          % (preset, result["node"], result["multiview"]), flush=True)

  with open(outpath, "w") as f:
    json.dump(result, f, indent=1)
  return 0


################################################################################
# DRIVER
################################################################################


def _run_case(name, envmap, preset, outdir):
  outpath = os.path.join(outdir, "case_%s.json" % name)
  env = dict(os.environ)
  env["ORKID_VULKAN_VALIDATE"] = "2"
  # start from a CLEAN slate for every switch this test owns, so a value left in
  # the caller's shell cannot decide a row.
  for k in ("ORKID_SPVR", "ORKID_SPVR_NO_MULTIVIEW", "ORKID_FORCE_DMVR"):
    env.pop(k, None)
  env.update(envmap)
  argv = [sys.executable, os.path.abspath(__file__), "--probe", preset, outpath]
  proc = subprocess.run(argv, env=env, stdout=subprocess.PIPE,
                        stderr=subprocess.STDOUT, timeout=300)
  out = proc.stdout.decode("utf-8", "replace")
  rec = {}
  if os.path.exists(outpath):
    rec = json.load(open(outpath))
  return proc.returncode, out, rec


def leg_hzb_knob():
  """PRESENCE only: the revert knob must still exist at its read site, with the
  announcement that makes an armed run provable from a transcript. Python cannot
  read the C++ static, so this is source-level and says so."""
  fails = []
  if not os.path.exists(FWD_TOP_CPP):
    return ["forward node source not found at %s" % FWD_TOP_CPP]
  src = open(FWD_TOP_CPP).read()
  if HZB_ENV not in src:
    fails.append("%s is gone from the forward node -- the same-frame-depth revert "
                 "control can no longer be armed" % HZB_ENV)
  elif ("%s=1" % HZB_ENV) not in src:
    fails.append("%s is read but no longer ANNOUNCED -- an armed control nobody can "
                 "prove was armed is not a control" % HZB_ENV)
  return fails


def main():
  outdir = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
      tempfile.gettempdir(), "spvr_node_selection")
  os.makedirs(outdir, exist_ok=True)
  t0 = time.time()
  fails = []

  # ---- row 1 first: it also answers the capability question the skip needs.
  rc, out, rec = _run_case(*CASES[0][:3], outdir)
  if rc != 0:
    print(out, flush=True)
    print("TESTVERDICT FAIL: probe child exited rc=%d" % rc, flush=True)
    return 1
  if not rec.get("multiview"):
    print("SKIP: device reports multiview=%s max_views=%s -- single-pass stereo has "
          "no meaningful answer here" % (rec.get("multiview"), rec.get("max_views")),
          flush=True)
    print("test_spvr_node_selection: SKIP in %.1fs" % (time.time() - t0), flush=True)
    return 0
  print("selection: multiview=%s max_views=%s"
        % (rec.get("multiview"), rec.get("max_views")), flush=True)

  results = {CASES[0][0]: rec}
  for case in CASES[1:]:
    rc, out, r = _run_case(case[0], case[1], case[2], outdir)
    if rc != 0:
      print(out, flush=True)
      fails.append("case %s: probe child exited rc=%d" % (case[0], rc))
    results[case[0]] = r

  for (name, envmap, preset, expected, why) in CASES:
    got = results.get(name, {}).get("node")
    envtxt = ",".join("%s=%s" % kv for kv in sorted(envmap.items())) or "(none)"
    status = "PASS" if got == expected else "FAIL"
    print("LEG_SELECT %-30s %-11s env<%s> -> %s (expected %s) %s"
          % (name, preset, envtxt, got, expected, status), flush=True)
    if got != expected:
      fails.append("SELECTION %s: preset %s with env<%s> installed %s, expected %s -- %s"
                   % (name, preset, envtxt, got, expected, why))

  # ---- row 7: the reporting surfaces
  base = results.get("explicit_spvr", {})
  print("LEG_HZB_SURFACE property=%s built=%s"
        % (base.get("hzb_property"), base.get("hzb_built")), flush=True)
  if not base.get("hzb_property"):
    fails.append("Scene.hzb provenance surface is GONE -- cull oracles lose their "
                 "only direct observable for the 1-phase invariant")
  fail_knob = leg_hzb_knob()
  print("LEG_HZB_KNOB failures=%d" % len(fail_knob), flush=True)
  for f in fail_knob:
    print("  LEG_HZB_KNOB FAIL: %s" % f, flush=True)
  fails += fail_knob

  dt = time.time() - t0
  if fails:
    print("TESTVERDICT FAIL (%d): %s" % (len(fails), "; ".join(fails[:6])), flush=True)
    print("test_spvr_node_selection: FAIL in %.1fs" % dt, flush=True)
    return 1
  print("TESTVERDICT PASS -- %d selection rows correct (fallback arm, autoselect "
        "default-off and armed, ORKID_FORCE_DMVR literal in both directions), HZB "
        "provenance surface and revert knob present (%.1fs)" % (len(CASES), dt), flush=True)
  return 0


if __name__ == "__main__":
  if len(sys.argv) > 2 and sys.argv[1] == "--probe":
    sys.exit(_probe(sys.argv[2], sys.argv[3]))
  sys.exit(main())
