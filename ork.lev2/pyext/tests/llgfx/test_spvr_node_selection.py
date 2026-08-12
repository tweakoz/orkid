#!/usr/bin/env ork.python
################################################################################
# SPVR — OUTPUT-NODE SELECTION (offscreen).
#
# WHICH NODE DOES A GIVEN PRESET + ENVIRONMENT ACTUALLY INSTALL? Single-pass stereo
# is the ONLY VR output model, and BOTH VR preset strings route to it. That routing
# is a silent thing when it regresses: a preset string that stopped resolving, or a
# device that quietly rendered something else, shows up in neither pixels nor
# validation nor a frame time anybody is watching. The no-multiview answer is a
# THROW, and a throw that stops throwing is just as silent.
#
# WHY EVERY LEG IS ITS OWN PROCESS. The capability switch is read through a
# function-local `static const bool` initializer — read ONCE per process, by design
# (it announces itself once). Two arms in one process would measure the first arm
# twice. So each row below is a subprocess with its own environment, and the parent
# only compares what each one reports.
#
# THE MATRIX (each row: env -> preset -> expected outcome)
#   1  (none)                       FWDPBRSPVR -> SinglePassStereoVr
#   2  (none)                       FWDPBRVRDM -> SinglePassStereoVr (SYNONYM: the
#      older VR preset string names the same one VR output node)
#   3  ORKID_SPVR_NO_MULTIVIEW=1    FWDPBRSPVR -> THROW, child exits NONZERO
#   4  ORKID_SPVR_NO_MULTIVIEW=1    FWDPBRVRDM -> THROW, child exits NONZERO
#
#   Rows 3/4 are the FAIL-LOUD contract: a device that cannot run single-pass stereo
#   gets a named runtime_error (the preset asked for + the concrete capability
#   reason), never a degrade to some other renderer. ORKID_SPVR_NO_MULTIVIEW=1 is
#   the only way a multiview-capable box can exercise that path.
#
#   5  REPORTING SURFACES: Scene.hzb (the occlusion-pyramid provenance handle) is
#      present and, once built, carries sourceDepthFrame/valid; and the
#      ORKID_HZB_ALLOW_SAMEFRAME revert knob is present at its read site with its
#      announcement. Reporting semantics only — nothing culls on either, and this
#      leg claims only PRESENCE, since the knob is read in C++ during forward
#      rendering and no python surface exposes its armed state.
#
# SKIP DISCIPLINE: on a device without multiview, rows 1/2 have no meaningful answer
# and the whole test SKIPs loudly with the capability printed — it never passes by
# not looking. Note that rows 3/4 ARE the default behaviour on such a device, so
# they would pass trivially there; that is exactly why the skip covers the run
# rather than letting the fail-loud rows carry it alone.
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

# the expectation sentinel for a row that must FAIL: the child exits nonzero and its
# transcript carries the resolver's named message.
FAILLOUD = "<FAIL-LOUD>"
# the distinctive substring of that runtime_error (scenegraph.cpp). Asserted, not
# just "some exception happened" — a different failure must not read as this one.
THROW_TEXT = "single-pass stereo is the only VR output model"

# (name, env overrides, preset, expectation, why)
CASES = (
    ("explicit_spvr", {}, "FWDPBRSPVR", SPVR,
     "the explicit single-pass preset on a multiview-capable device"),
    ("synonym_vrdm", {}, "FWDPBRVRDM", SPVR,
     "the older VR preset string is a SYNONYM -- same one VR output node"),
    ("no_multiview_spvr", {"ORKID_SPVR_NO_MULTIVIEW": "1"}, "FWDPBRSPVR", FAILLOUD,
     "capability forced unavailable -> the ask THROWS by name, never degrades"),
    ("no_multiview_vrdm", {"ORKID_SPVR_NO_MULTIVIEW": "1"}, "FWDPBRVRDM", FAILLOUD,
     "the synonym string throws the same way -- one routing, one failure mode"),
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
    # the resolver THROWS on a device that cannot run single-pass stereo. Caught here
    #  (and reported, then re-signalled through the exit code) so the app still tears
    #  down through the harness -- an exception escaping the context manager leaves a
    #  wedged teardown, which reads as a hang instead of a verdict.
    scene = None
    try:
      scene = lev2.scenegraph.Scene(params)
    except Exception as e:                      # noqa: BLE001
      result["throw"] = str(e)
      print("SELECTION preset=%s THROW <%s>" % (preset, result["throw"]), flush=True)

    if scene is not None:
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
  return 0 if scene is not None else 3


################################################################################
# DRIVER
################################################################################


def _run_case(name, envmap, preset, outdir):
  outpath = os.path.join(outdir, "case_%s.json" % name)
  env = dict(os.environ)
  env["ORKID_VULKAN_VALIDATE"] = "2"
  # start from a CLEAN slate for the switch this test owns, so a value left in the
  # caller's shell cannot decide a row.
  env.pop("ORKID_SPVR_NO_MULTIVIEW", None)
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

  # ---- row 1 first: it also answers the capability question the skip needs. Its
  #  child records the capability BEFORE it builds the scene, so the record is
  #  readable even when the build threw.
  rc, out, rec = _run_case(*CASES[0][:3], outdir)
  if not rec.get("multiview"):
    print(out, flush=True)
    print("SKIP: device reports multiview=%s max_views=%s -- single-pass stereo has "
          "no meaningful answer here, and the fail-loud rows below are simply this "
          "device's default behaviour" % (rec.get("multiview"), rec.get("max_views")),
          flush=True)
    print("test_spvr_node_selection: SKIP in %.1fs" % (time.time() - t0), flush=True)
    return 0
  if rc != 0:
    print(out, flush=True)
    print("TESTVERDICT FAIL: probe child exited rc=%d on a multiview-capable device"
          % rc, flush=True)
    return 1
  print("selection: multiview=%s max_views=%s"
        % (rec.get("multiview"), rec.get("max_views")), flush=True)

  results = {CASES[0][0]: (rc, out, rec)}
  for case in CASES[1:]:
    results[case[0]] = _run_case(case[0], case[1], case[2], outdir)

  for (name, envmap, preset, expected, why) in CASES:
    rc, out, r = results.get(name, (None, "", {}))
    envtxt = ",".join("%s=%s" % kv for kv in sorted(envmap.items())) or "(none)"
    if expected is FAILLOUD:
      # the contract is BOTH halves: a nonzero exit AND the named message. A child
      #  that died for some other reason must not read as this row passing.
      thrown = r.get("throw") or ""
      named = (THROW_TEXT in thrown) or (THROW_TEXT in out)
      ok = (rc != 0) and named
      got = ("rc=%s throw<%s>" % (rc, thrown[:90])) if thrown else ("rc=%s (no throw text)" % rc)
      print("LEG_SELECT %-20s %-11s env<%s> -> %s (expected FAIL-LOUD) %s"
            % (name, preset, envtxt, got, "PASS" if ok else "FAIL"), flush=True)
      if not ok:
        if rc == 0:
          print(out, flush=True)
        fails.append("SELECTION %s: preset %s with env<%s> gave %s, expected a nonzero "
                     "exit naming <%s> -- %s" % (name, preset, envtxt, got, THROW_TEXT, why))
      continue
    got = r.get("node")
    ok = (rc == 0) and (got == expected)
    print("LEG_SELECT %-20s %-11s env<%s> -> %s (expected %s) %s"
          % (name, preset, envtxt, got, expected, "PASS" if ok else "FAIL"), flush=True)
    if not ok:
      print(out, flush=True)
      fails.append("SELECTION %s: preset %s with env<%s> installed %s (rc=%s), expected "
                   "%s -- %s" % (name, preset, envtxt, got, rc, expected, why))

  # ---- row 5: the reporting surfaces
  base = results.get("explicit_spvr", (None, "", {}))[2]
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
  print("TESTVERDICT PASS -- %d selection rows correct (both VR preset strings install "
        "the single-pass stereo node; both throw by name when multiview is forced "
        "unavailable), HZB provenance surface and revert knob present (%.1fs)"
        % (len(CASES), dt), flush=True)
  return 0


if __name__ == "__main__":
  if len(sys.argv) > 2 and sys.argv[1] == "--probe":
    sys.exit(_probe(sys.argv[2], sys.argv[3]))
  sys.exit(main())
