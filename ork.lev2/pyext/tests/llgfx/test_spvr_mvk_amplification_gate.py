#!/usr/bin/env ork.python
###############################################################################
# Gate — MoltenVK renders 2-view multiview CORRECTLY, and (when asked) does it
# through Metal VERTEX AMPLIFICATION rather than by replaying the draw once per
# view through the instance index.
#
# WHY THIS TEST IS A SUBPROCESS WRAPPER AND NOT AN ENGINE SCENE
# -------------------------------------------------------------
# The thing under test is a DRIVER, not the engine: which Metal encoding
# MoltenVK picks for a multiview draw. Proving that needs a render whose shader
# reads gl_ViewIndex and whose vertex-invocation count is directly observable —
# neither is reachable through the engine's material stack today, and routing it
# through a scene would put the engine's own compositor between the claim and
# the evidence. So the proof instrument is a raw-Vulkan micro-test that lives
# with the driver patch, in the MoltenVK fork:
#
#     <MoltenVK>/Tests/multiview-amplification/{mvtest.c,mv.vert,mv.frag}
#     <MoltenVK>/Tests/multiview-amplification/run_microtest.sh
#
# This file is the COMMITTED, owner-runnable face of that instrument: it builds
# and runs the micro-test against a named MoltenVK dylib and asserts its verdict
# lines. It takes seconds, needs no scene, and needs no VR hardware.
#
# WHAT THE MICRO-TEST RENDERS
# ---------------------------
# One triangle into a 2-LAYER offscreen color target through a multiview subpass
# (viewMask 0b11), then reads BOTH layers back. Each pixel encodes
#   R = the view index as seen by the VERTEX stage (carried on a flat varying)
#   G = the view index as seen by the FRAGMENT stage (native ViewIndex path)
# so layer 0 must read R=G=64 and layer 1 must read R=G=191. That asserts both
# stages agree AND that the two views actually diverged.
#
# COVERAGE: TWO AXES, NOT ONE
# ---------------------------
# MoltenVK reaches its multiview-encoding decision by one code path per
# DECLARATION route, and carries the view count into Metal at a different place
# per DRAW COMMAND. So the gate sweeps both axes:
#
#   declaration route x  classic (VkRenderPass + VkRenderPassMultiviewCreateInfo)
#                        dynamic (vkCmdBeginRendering + VkRenderingInfo.viewMask)
#   draw command     x  direct          vkCmdDraw            (the base run)
#                       indexed         vkCmdDrawIndexed
#                       indirect        vkCmdDrawIndirect
#                       indexed-indirect vkCmdDrawIndexedIndirect
#
# The indirect pair is the one production leans on: hypermesh and terrain geometry
# draw indirectly, and on the replay path MoltenVK rewrites the indirect buffer's
# instanceCount with a compute kernel before the draw — an encoder site the direct
# draw never touches. Each draw-command leg runs as its OWN subprocess (the shader
# dump is keyed by SPIR-V hash, so legs sharing a process overwrite each other's
# MSL) and both routes are swept, which stays cheap: ~0.03s per leg run.
#
# WHY EVERY LEG ASSERTS AN INVOCATION COUNT, NOT JUST PIXELS
# ----------------------------------------------------------
# A double-encoded draw is PIXEL-INVISIBLE: drawing the same triangle twice into
# the same layers reads back exactly the correct colors. The vertex-invocation
# counter is what catches it — at viewCount 2 a correct leg reads exactly
# vertexCount*2, an encoder that amplified AND replayed reads 4x, and one that did
# neither reads 1x with layer 1 left at the clear color. Pixels prove correctness;
# the count proves how the correctness was obtained.
#
# WHAT PASSES vs WHAT DISCRIMINATES — read this before trusting a green run
# ------------------------------------------------------------------------
#  * VISUAL PARITY passes on the stock instanced-replay driver AND on the
#    amplification driver. That is the REQUIREMENT (bug-for-bug identity), so it
#    is deliberately NOT evidence that amplification engaged. A green visual leg
#    on an unpatched dylib is a correct result, not a false pass.
#  * The VERTEX-INVOCATION COUNT is likewise not a standalone discriminator:
#    Metal executes the vertex function once per amplification ID, so a 2-view
#    draw reads 2x vertexCount on BOTH paths. Asserting "2x means replay" would
#    be wrong, and this gate does not.
#  * The CATEGORICAL discriminator is the GENERATED MSL, which MoltenVK will dump
#    for us (MVK_CONFIG_SHADER_DUMP_DIR). The amplification build's vertex entry
#    point takes `uint gl_ViewIndex [[amplification_id]]` and contains NO
#    instance-index division; the replay build derives the view index from the
#    instance index and has no amplification_id anywhere. That is a hard artifact
#    read off the driver's own output, and it is what this gate asserts when a
#    caller declares which path it expects.
#
# The remaining leg — that the ENCODER half is live, not just the codegen — is
# proven in the MoltenVK lane by a negative control (disable
# setVertexAmplificationCount, keep the amplification codegen: layer 1 goes to
# the clear color and the invocation count drops to 1x, pinning instanceCount at
# 1x). That control mutates driver source, so it is not reproducible from here;
# this gate asserts the two halves it CAN see and says so.
#
# HOW TO RUN (owner)
# ------------------
#   ork.python ork.lev2/pyext/tests/llgfx/test_spvr_mvk_amplification_gate.py
#
# Bare, with no environment at all. The gate DISCOVERS the dylibs to exercise
# among the lane's durable artifacts (see DYLIB_PROBES below) and, per dylib,
# derives what to expect from the file's own name — the patched build must
# generate the amplification MSL, the archived stock build must generate replay.
# Nothing found -> SKIP with a stated sentinel; the gate never falls back to
# whatever driver happens to be installed, because that would report on something
# nobody named. Every environment variable below is an OPTIONAL override:
#
#   ORKID_SPVR_MVK_DYLIB   Exercise exactly this dylib instead of discovering.
#   ORKID_SPVR_MVK_EXPECT  'amplification' | 'replay' | 'any'. Selects whether the
#                          MSL discriminator is an ASSERTION or just a report;
#                          overrides the name-derived expectation. Default: the
#                          name-derived one when discovering, 'any' when a dylib is
#                          named without one. 'any' still prints which path ran.
#   ORKID_SPVR_MVK_TESTDIR Override the micro-test source dir. Default probes
#                          the usual fork locations next to this checkout, then
#                          the durable prebuilt artifact.
#   ORKID_SPVR_MVK_SDK     Vulkan loader/header prefix (default $OBT_STAGE, then
#                          the jul24 staging) — needs include/vulkan + lib.
#
# DRIVER-IDENTITY TRAP THIS GATE DEFENDS AGAINST
# ----------------------------------------------
# An ICD manifest names the driver by ABSOLUTE path, but dyld resolves dlopen by
# LEAF NAME against DYLD_LIBRARY_PATH first. ork.python exports
# DYLD_LIBRARY_PATH=<staging>/lib, and staging ships its own libMoltenVk.dylib —
# which, on a case-insensitive filesystem, shadows any libMoltenVK.dylib you
# name. The observed effect is silent and total: the gate runs the STAGING
# driver while reporting on the dylib you asked for, so a patched build reads
# back as unpatched. Defended twice below: the dylib is copied to a unique leaf
# name that nothing can shadow, and the DYLD_* variables are scrubbed from the
# subprocess. Do not "simplify" either one away.
#
# SKIPS CLEANLY (verdict PASS + a stated sentinel) when: not macOS, no dylib
# discovered or named, the named dylib is missing, the micro-test sources and the
# prebuilt artifact are both missing, or the toolchain to build it (cc / glslc /
# vulkan loader) is absent. A skip never masks a failure — once the micro-test
# RUNS, every leg is asserted.
###############################################################################

import os
os.environ["PYTHONUNBUFFERED"] = "1"
import sys
sys.stdout.reconfigure(line_buffering=True)

import glob
import platform
import re
import shutil
import subprocess
import tempfile
import time

# repo root = five levels up; prepend THIS checkout's scripts dir so ork.testing
# resolves from the same tree as this test (the sibling worktree-shadowing idiom).
_ROOT = os.path.abspath(__file__)
for _ in range(5):
  _ROOT = os.path.dirname(_ROOT)
sys.path.insert(0, os.path.join(_ROOT, "obt.project", "scripts"))

from ork.testing import verdict

# The micro-test's own expectations, restated here so a drift in either side is
# a failure rather than a silently-agreeing pair.
VIEW_COUNT = 2
VERT_COUNT = 3
EXPECT_LAYER_VALUE = {0: 64, 1: 191}   # 0.25 and 0.75 in 8-bit UNORM
CHANNEL_TOLERANCE = 2

# The draw-command legs, one subprocess each (shared processes overwrite one
# another's MSL dump), swept through both declaration routes.
DRAW_LEGS = ("indirect", "idxindirect", "dirindexed")
DECL_ROUTES = ("classic", "dynamic")

SKIP_ENV = "ORKID_SPVR_MVK_DYLIB"

# Durable lane artifacts: dylibs and a prebuilt micro-test that OUTLIVE temp
# cleans, which the scratch copies below do not. Probed first for exactly that
# reason — a gate whose inputs evaporate on reboot is not owner-runnable.
_HOME = os.path.expanduser("~")
DURABLE_DIR = os.path.join(_HOME, ".obt-coord", "m5maxtozx", "coordination",
                           "artifacts", "spvr-mvk")
DURABLE_BIN = os.path.join(DURABLE_DIR, "mvtest-d4c6bf8f")
DYLIB_PROBES = (
    os.path.join(DURABLE_DIR, "libMoltenVK-patched-*.dylib"),
    os.path.join(DURABLE_DIR, "libMoltenVK-baseline-*.dylib"),
    # scratch/staging fallbacks: same naming convention, shorter lifetime.
    os.path.join(_HOME, ".spvr-mvk", "libMoltenVK-*.dylib"),
    "/private/tmp/claude-*/*/*/scratchpad/spvr-mvk/libMoltenVK-*.dylib",
)

###############################################################################

def _skip(reason):
  print("SKIP: %s" % reason)
  return verdict(True, "SPVR MoltenVK amplification gate SKIPPED — %s" % reason)

def _expect_for(path):
  """A build's name states what MSL it must generate; unnamed builds report only."""
  leaf = os.path.basename(path).lower()
  if "patch" in leaf:
    return "amplification"
  if "baseline" in leaf or "stock" in leaf:
    return "replay"
  return "any"

def _find_dylibs():
  """[(path, expect)] — the named dylib, else the discovered durable artifacts."""
  override = os.environ.get("ORKID_SPVR_MVK_EXPECT")
  named = os.environ.get(SKIP_ENV)
  if named:
    return [(named, (override or "any").strip().lower())]
  found, seen = [], set()
  for pat in DYLIB_PROBES:
    for p in sorted(glob.glob(pat)):
      real = os.path.realpath(p)
      if real in seen or not os.path.isfile(p):
        continue
      seen.add(real)
      found.append((p, (override or _expect_for(p)).strip().lower()))
  return found

def _find_testdir():
  """Locate the micro-test sources that ship with the MoltenVK patch."""
  override = os.environ.get("ORKID_SPVR_MVK_TESTDIR")
  if override:
    return override if os.path.isdir(override) else None
  home = os.path.expanduser("~")
  for cand in (os.path.join(home, "projects", "MoltenVK"),
               os.path.join(os.path.dirname(_ROOT), "MoltenVK")):
    d = os.path.join(cand, "Tests", "multiview-amplification")
    if os.path.isdir(d):
      return d
  return None

def _find_prebuilt():
  """The durable micro-test binary + its SPIR-V, for boxes with no fork checkout."""
  if not os.path.isfile(DURABLE_BIN) or not os.access(DURABLE_BIN, os.X_OK):
    return None
  for spv in ("mv.vert.spv", "mv.frag.spv"):
    if not os.path.isfile(os.path.join(DURABLE_DIR, spv)):
      return None
  return DURABLE_BIN

def _find_sdk():
  """Vulkan loader + headers. The micro-test links the loader, not MoltenVK."""
  override = os.environ.get("ORKID_SPVR_MVK_SDK")
  cands = [override] if override else []
  cands += [os.environ.get("OBT_STAGE"),
            os.path.join(os.path.expanduser("~"), ".staging-jul24")]
  for c in cands:
    if c and os.path.isfile(os.path.join(c, "include", "vulkan", "vulkan.h")):
      libdir = os.path.join(c, "lib")
      if glob.glob(os.path.join(libdir, "libvulkan*.dylib")):
        return c
  return None

###############################################################################

def _build(testdir, sdk, work):
  """Compile the shaders and the host. Returns the binary path, or None."""
  glslc = shutil.which("glslc") or shutil.which("glslangValidator")
  if not glslc or glslc.endswith("glslangValidator"):
    # Only glslc's CLI is used below; glslangValidator takes different flags and
    # is not worth a second code path for a gate that skips cleanly without it.
    glslc = shutil.which("glslc")
  if not glslc:
    return None, "glslc not on PATH"
  cc = os.environ.get("CC") or shutil.which("cc")
  if not cc:
    return None, "no C compiler"

  for stage in ("vert", "frag"):
    src = os.path.join(testdir, "mv.%s" % stage)
    rc = subprocess.run([glslc, "--target-env=vulkan1.1",
                         "-o", os.path.join(work, "mv.%s.spv" % stage), src],
                        capture_output=True, text=True)
    if rc.returncode != 0:
      return None, "shader compile failed (%s): %s" % (stage, rc.stderr.strip())

  binpath = os.path.join(work, "mvtest")
  rc = subprocess.run([cc, "-O2", "-o", binpath, os.path.join(testdir, "mvtest.c"),
                       "-I%s" % os.path.join(sdk, "include"),
                       "-L%s" % os.path.join(sdk, "lib"),
                       "-lvulkan", "-Wl,-rpath,%s" % os.path.join(sdk, "lib")],
                      capture_output=True, text=True)
  if rc.returncode != 0:
    return None, "host compile failed: %s" % rc.stderr.strip()[:400]
  return binpath, None

def _stage_dylib(work, dylib, tag):
  """Write an ICD manifest naming a shadow-proof copy of exactly this dylib."""
  # Copy to a leaf name nothing on any DYLD search path can shadow (see the
  # driver-identity trap in the header). The copy, not the original, is named.
  shadowproof = os.path.join(work, "libMoltenVK_spvrgate_%d_%s.dylib" % (os.getpid(), tag))
  shutil.copy2(dylib, shadowproof)

  icd = os.path.join(work, "icd_%s.json" % tag)
  with open(icd, "w") as f:
    f.write('{\n  "file_format_version": "1.0.0",\n  "ICD": {\n'
            '    "library_path": "%s",\n'
            '    "api_version": "1.4.0",\n'
            '    "is_portability_driver": true\n  }\n}\n' % shadowproof)
  return icd

def _run(binpath, shaderdir, icd, dumpdir, mode=None, route=None):
  """Run one micro-test process against the ICD-named driver."""
  env = dict(os.environ)
  # Belt to the unique-leaf braces: with no DYLD_* override in play, dlopen of an
  # absolute path resolves to that path.
  for k in ("DYLD_LIBRARY_PATH", "DYLD_FALLBACK_LIBRARY_PATH", "DYLD_FRAMEWORK_PATH"):
    env.pop(k, None)
  env["VK_ICD_FILENAMES"] = icd     # older loaders
  env["VK_DRIVER_FILES"] = icd      # current loaders
  env["MVK_CONFIG_SHADER_DUMP_DIR"] = dumpdir
  env["MVT_VALIDATION"] = "1"
  env["MVT_VERTS"] = str(VERT_COUNT)
  if mode:
    env["MVT_MODE"] = mode
  if route:
    env["MVT_PASS"] = route
  return subprocess.run([binpath, shaderdir], capture_output=True, text=True,
                        env=env, timeout=180)

###############################################################################

def _parse_layers(out):
  """layer N center rgba = R G B A ... -> {N: (r,g,b,a)}"""
  found = {}
  for m in re.finditer(r"layer\s+(\d+)\s+center\s+rgba\s*=\s*"
                       r"(\d+)\s+(\d+)\s+(\d+)\s+(\d+)", out):
    found[int(m.group(1))] = tuple(int(m.group(i)) for i in range(2, 6))
  return found

def _parse_leg(out, mode):
  """VERDICT-LEG: name=.. api=.. pass=.. visual=.. invocations=N dbg_errors=N"""
  m = re.search(r"VERDICT-LEG:\s*name=%s\s+api=(\S+)\s+pass=(\S+)\s+visual=(\S+)"
                r"\s+invocations=(\d+)\s+dbg_errors=(-?\d+)" % re.escape(mode), out)
  if not m:
    return None
  return {"api": m.group(1), "route": m.group(2), "visual": m.group(3),
          "invocations": int(m.group(4)), "dbg_errors": int(m.group(5))}

def _classify_msl(dumpdir):
  """Read the driver's own generated MSL and say which multiview path it took."""
  vs = sorted(glob.glob(os.path.join(dumpdir, "shader-vs-*.metal")))
  if not vs:
    return None, "no vertex MSL dumped"
  src = open(vs[-1]).read()
  amp = "[[amplification_id]]" in src
  # The replay path's signature: view index recovered from the instance index.
  replay = ("% spvViewMask[1]" in src) or ("/ spvViewMask[1]" in src)
  if amp and not replay:
    return "amplification", None
  if replay and not amp:
    return "replay", None
  if amp and replay:
    return None, "MSL shows BOTH amplification_id and instance-index recovery"
  return None, "MSL shows neither multiview path (multiview may be inactive)"

###############################################################################

def _exercise(dylib, expect, binpath, shaderdir, work, tag):
  """One driver build: the direct-draw base run, then every draw-command leg."""
  fails, notes = [], []
  leaf = os.path.basename(dylib)
  icd = _stage_dylib(work, dylib, tag)
  dumpdir = os.path.join(work, "msl_%s" % tag)
  os.makedirs(dumpdir, exist_ok=True)

  print("\n" + "=" * 79)
  print("=== driver: %s   expect=%s" % (leaf, expect))
  print("=" * 79)

  try:
    proc = _run(binpath, shaderdir, icd, dumpdir)
  except subprocess.TimeoutExpired:
    return ["%s: micro-test timed out" % leaf], notes

  out = proc.stdout + proc.stderr
  print(out.rstrip())

  # ---- leg 1: the micro-test's own verdict --------------------------------
  if "VERDICT: visual=PASS" not in out:
    fails.append("%s: micro-test did not report visual=PASS (rc=%d)" % (leaf, proc.returncode))
  if proc.returncode != 0:
    fails.append("%s: micro-test rc=%d" % (leaf, proc.returncode))

  # ---- leg 2: per-layer values, re-asserted here ---------------------------
  layers = _parse_layers(out)
  if len(layers) != VIEW_COUNT:
    fails.append("%s: expected %d layers in output, parsed %d" % (leaf, VIEW_COUNT, len(layers)))
  for idx, want in EXPECT_LAYER_VALUE.items():
    px = layers.get(idx)
    if px is None:
      fails.append("%s: layer %d absent from output" % (leaf, idx))
      continue
    # R = vertex-stage view index, G = fragment-stage view index; both must
    # land on the same expected value or the two stages disagree.
    if abs(px[0] - want) > CHANNEL_TOLERANCE:
      fails.append("%s: layer %d vertex-stage view index: R=%d want %d" % (leaf, idx, px[0], want))
    if abs(px[1] - want) > CHANNEL_TOLERANCE:
      fails.append("%s: layer %d fragment-stage view index: G=%d want %d" % (leaf, idx, px[1], want))
  if len(layers) == VIEW_COUNT and layers.get(0) == layers.get(1):
    fails.append("%s: layers are identical — the two views never diverged" % leaf)

  # ---- leg 3: invocation count, as a RANGE check, not a path claim ---------
  want_inv = VERT_COUNT * VIEW_COUNT
  m = re.search(r"VS invocations measured\s*:\s*(\d+)", out)
  if not m:
    fails.append("%s: vertex-invocation count missing from output" % leaf)
  else:
    got = int(m.group(1))
    # Both supported paths run the vertex function once per view. A 1x reading
    # means the second view never ran (the negative-control signature); more
    # than 2x means the encoder amplified AND replayed, i.e. double-counted.
    if got != want_inv:
      fails.append("%s: vertex invocations=%d want %d (1x => a view never ran; "
                   ">2x => amplified AND replayed)" % (leaf, got, want_inv))
    notes.append("direct invocations=%d (%dx)" % (got, got // VERT_COUNT if VERT_COUNT else 0))

  # ---- leg 4: the categorical discriminator, off the driver's own MSL ------
  path, why = _classify_msl(dumpdir)
  if path is None:
    if expect == "any":
      notes.append("multiview path UNDETERMINED (%s)" % why)
    else:
      fails.append("%s: cannot classify the multiview path: %s" % (leaf, why))
  else:
    notes.append("multiview path=%s" % path)
    if expect != "any" and path != expect:
      fails.append("%s: expected the %s path, driver generated %s" % (leaf, expect, path))

  # ---- leg 5: the rest of the draw-command family, one process per leg -----
  # Same triangle, same SPIR-V, same expectations — reached through the encoder
  # sites the direct draw never touches, on both declaration routes. The pixel
  # check alone cannot see a double-encode here; the invocation count can.
  print("\n--- draw-command legs (%s) ---" % leaf)
  for mode in DRAW_LEGS:
    for route in DECL_ROUTES:
      legdump = os.path.join(work, "msl_%s_%s_%s" % (tag, mode, route))
      os.makedirs(legdump, exist_ok=True)
      try:
        lp = _run(binpath, shaderdir, icd, legdump, mode=mode, route=route)
      except subprocess.TimeoutExpired:
        fails.append("%s: leg %s/%s timed out" % (leaf, mode, route))
        print("SPVR-DRAWLEG: dylib=%s mode=%s route=%s verdict=FAIL reason=timeout"
              % (leaf, mode, route))
        continue
      lout = lp.stdout + lp.stderr
      leg = _parse_leg(lout, mode)
      legfails = []
      if leg is None:
        legfails.append("no VERDICT-LEG line for %s" % mode)
      else:
        if leg["visual"] != "PASS":
          legfails.append("visual=%s" % leg["visual"])
        if leg["route"] != route:
          legfails.append("route=%s asked %s" % (leg["route"], route))
        if leg["invocations"] != want_inv:
          legfails.append("invocations=%d want %d" % (leg["invocations"], want_inv))
        if leg["dbg_errors"] != 0:
          legfails.append("dbg_errors=%d" % leg["dbg_errors"])
      if lp.returncode != 0:
        legfails.append("rc=%d" % lp.returncode)
      print("SPVR-DRAWLEG: dylib=%s mode=%s route=%s api=%s visual=%s invocations=%s "
            "dbg_errors=%s rc=%d verdict=%s"
            % (leaf, mode, route,
               leg["api"] if leg else "?", leg["visual"] if leg else "?",
               leg["invocations"] if leg else "?", leg["dbg_errors"] if leg else "?",
               lp.returncode, "FAIL" if legfails else "PASS"))
      if legfails:
        # A failing leg's whole transcript, once — the sentinel says what, this says why.
        print(lout.rstrip())
        fails.append("%s: leg %s/%s %s" % (leaf, mode, route, ", ".join(legfails)))
  notes.append("draw legs %d/%d PASS"
               % (len(DRAW_LEGS) * len(DECL_ROUTES) - len([f for f in fails if ": leg " in f]),
                  len(DRAW_LEGS) * len(DECL_ROUTES)))
  return fails, notes


def main():
  t0 = time.time()
  if platform.system() != "Darwin":
    sys.exit(_skip("not macOS — MoltenVK multiview encoding is a mac-only concern"))

  dylibs = _find_dylibs()
  if not dylibs:
    sys.exit(_skip("no MoltenVK build discovered under %s (and %s not set) — nothing named "
                   "to exercise" % (DURABLE_DIR, SKIP_ENV)))
  for path, expect in dylibs:
    if not os.path.isfile(path):
      sys.exit(_skip("%s=%s does not exist" % (SKIP_ENV, path)))
    if expect not in ("any", "amplification", "replay"):
      sys.exit(verdict(False, "expectation %r is not any|amplification|replay" % expect))

  sdk = _find_sdk()
  if not sdk:
    sys.exit(_skip("no Vulkan loader+headers found — set ORKID_SPVR_MVK_SDK"))

  fails, notes = [], []

  with tempfile.TemporaryDirectory() as work:
    # Build once from the fork sources; fall back to the durable prebuilt binary
    # on a box with no MoltenVK checkout (its SPIR-V ships beside it).
    testdir = _find_testdir()
    binpath, shaderdir, err = None, work, None
    if testdir:
      binpath, err = _build(testdir, sdk, work)
    if not binpath:
      binpath = _find_prebuilt()
      shaderdir = DURABLE_DIR
      if not binpath:
        sys.exit(_skip("cannot build the micro-test (%s) and no prebuilt at %s"
                       % (err or "sources not found — set ORKID_SPVR_MVK_TESTDIR",
                          DURABLE_BIN)))
      print("NOTE: using the prebuilt micro-test %s" % binpath)

    for i, (dylib, expect) in enumerate(dylibs):
      f, n = _exercise(dylib, expect, binpath, shaderdir, work, "d%d" % i)
      fails += f
      notes += ["%s: %s" % (os.path.basename(dylib), x) for x in n]

  elapsed = time.time() - t0
  print("\nSPVR-GATE: drivers=%d draw_legs=%d runs=%d runtime=%.1fs"
        % (len(dylibs), len(dylibs) * len(DRAW_LEGS) * len(DECL_ROUTES),
           len(dylibs) * (1 + len(DRAW_LEGS) * len(DECL_ROUTES)), elapsed))
  detail = "SPVR MoltenVK amplification gate | %s | %s" % (
      ", ".join("%s=%s" % (os.path.basename(d), e) for d, e in dylibs),
      "; ".join(notes) if notes else "no notes")
  code = verdict(not fails, detail if not fails else detail + " | " + "; ".join(fails))
  sys.exit(code)


main()
