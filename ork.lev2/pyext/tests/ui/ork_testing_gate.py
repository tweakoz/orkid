#!/usr/bin/env ork.python
################################################################################
# ork_testing_gate — the gate battery for the ork.testing harness.
#
# The driver (no --role) is pure stdlib: it spawns each engine-booting check as its
# OWN ork.python subprocess (one engine boot per process — the harness is exactly the
# thing that makes a clean single-boot lifecycle reusable) and asserts the observable.
# Pure checks (verdict classification) run in-process.
#
# Coverage:
#   verdict     PASS/FAIL bodies -> correct lines + rcs; read_verdict classifies
#               PASS / FAIL / PASS_WITH_TEARDOWN_BUG / NO_VERDICT / CRASH_NO_VERDICT.
#   watchdog    an intentional wedge is SAMPLED (file non-empty) + killed with the
#               distinct rc (subprocess).
#   guards      a missing output dir is created; a bogus asset path fails LOUD pre-init
#               with the path in the message.
#   lifecycle   a trivial offscreen headless_app runs N frames + exits rc=0, 5x (flake).
#   capture     one real capture_app offscreen grab -> a non-empty, non-black PNG.
#   migration   test_rtg_resize_extent.py passes, observable line byte-identical.
#   neutrality  test_terrain_battery.py still N/N suites.
################################################################################

import os
os.environ["PYTHONUNBUFFERED"] = "1"
import sys
import argparse
import subprocess
import time

# prepend THIS checkout's scripts dir so ork.testing resolves from the same tree.
_ROOT = os.path.abspath(__file__)
for _ in range(5):
  _ROOT = os.path.dirname(_ROOT)
sys.path.insert(0, os.path.join(_ROOT, "obt.project", "scripts"))

SELF = os.path.abspath(__file__)
TESTDIR = os.path.dirname(SELF)
LLGFX = os.path.normpath(os.path.join(TESTDIR, "..", "llgfx"))
TMP = os.path.join(os.environ.get("TMPDIR", "/tmp"), "ork_testing_gate")


################################################################################
# ROLES — each is a self-contained child body selected by --role.
################################################################################

def _role_verdict_pass():
  from ork.testing import verdict
  code = verdict(True, "pass-body")
  sys.exit(code)


def _role_verdict_fail():
  from ork.testing import verdict
  code = verdict(False, "fail-body")
  sys.exit(code)


def _role_verdict_teardown_bug():
  # PASS verdict emitted+flushed, THEN a simulated teardown SIGSEGV (hard nonzero exit).
  from ork.testing import verdict
  verdict(True, "pass-then-crash")
  os._exit(139)   # emulates a post-verdict teardown crash (128+SIGSEGV)


def _role_guards():
  # dir-creation (#71) + asset preflight fail-loud (#54-class), both pure logic.
  from ork.testing import ensure_parent_dir, preflight_assets
  ok = True
  nested = os.path.join(TMP, "made", "by", "harness", "out.png")
  # ensure the leaf dir does not pre-exist
  import shutil
  shutil.rmtree(os.path.join(TMP, "made"), ignore_errors=True)
  ensure_parent_dir(nested)
  created = os.path.isdir(os.path.dirname(nested))
  print(f"[guards] dir-created={created} ({os.path.dirname(nested)})", flush=True)
  ok = ok and created

  bogus = "/no/such/ork/testing/asset_xyz.glb"
  raised = False
  msg = ""
  try:
    preflight_assets([bogus])
  except FileNotFoundError as ex:
    raised = True
    msg = str(ex)
  has_path = bogus in msg
  print(f"[guards] preflight-raised={raised} path-in-msg={has_path}", flush=True)
  ok = ok and raised and has_path
  print("GUARDS=%s" % ("PASS" if ok else "FAIL"), flush=True)
  sys.exit(0 if ok else 1)


def _role_watchdog_wedge(sample_path):
  from ork.testing import Watchdog
  wd = Watchdog(1.5, sample_path=sample_path, label="gate_wedge").arm()
  # intentional wedge past the deadline — the watchdog must sample + kill us.
  t_end = time.time() + 60.0
  while time.time() < t_end:
    time.sleep(0.1)
  wd.disarm()
  print("WEDGE_ESCAPED", flush=True)   # must NOT happen
  sys.exit(0)


def _role_headless_life():
  from ork.testing import headless_app, verdict
  with headless_app(subsystems=['opq', 'core', 'gpu', 'lev2']) as app:
    n = app.run_frames(5)
    ok = (n == 5 and app.ctx is not None)
    print(f"[lifecycle] frames={n} ctx={app.ctx is not None}", flush=True)
  verdict(ok, f"frames={n}")
  sys.exit(0 if ok else 1)


def _role_capture(png):
  from ork.testing import capture_app, verdict
  with capture_app(width=320, height=240) as cap:
    r = cap.capture(png)
  verdict(r["nonblack"] and r["wrote"], f"mean={r['mean']:.2f} max={r['max']} {png}")
  sys.exit(0 if (r["nonblack"] and r["wrote"]) else 1)


################################################################################
# DRIVER
################################################################################

def _spawn(role, *args, timeout=110):
  cmd = ["ork.python", SELF, "--role", role] + list(args)
  p = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)
  return p.returncode, (p.stdout or "") + (p.stderr or "")


def _spawn_script(path, *args, timeout=170):
  cmd = ["ork.python", path] + list(args)
  p = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)
  return p.returncode, (p.stdout or "") + (p.stderr or "")


def driver():
  from ork.testing import (
      read_verdict, PASS, FAIL, PASS_WITH_TEARDOWN_BUG, NO_VERDICT, CRASH_NO_VERDICT,
      WATCHDOG_RC)
  os.makedirs(TMP, exist_ok=True)
  results = {}

  def record(name, ok, detail=""):
    results[name] = ok
    print(f"[GATE] {'PASS' if ok else 'FAIL'} {name}  {detail}", flush=True)

  # ---- 1. verdict protocol bodies (subprocess) ----
  rc, out = _spawn("verdict-pass")
  record("verdict.pass",
         rc == 0 and read_verdict(out) == "PASS" and read_verdict(out, rc) == PASS,
         f"rc={rc} class={read_verdict(out, rc)}")

  rc, out = _spawn("verdict-fail")
  record("verdict.fail",
         rc == 1 and read_verdict(out) == "FAIL" and read_verdict(out, rc) == FAIL,
         f"rc={rc} class={read_verdict(out, rc)}")

  rc, out = _spawn("verdict-teardown-bug")
  cls = read_verdict(out, rc)
  record("verdict.teardown_bug",
         read_verdict(out) == "PASS" and rc != 0 and cls == PASS_WITH_TEARDOWN_BUG,
         f"rc={rc} class={cls}")

  # ---- 2. read_verdict pure classification (in-process) ----
  synth = [
      (read_verdict("TESTVERDICT=PASS detail=a", 0), PASS),
      (read_verdict("TESTVERDICT=PASS detail=a", 139), PASS_WITH_TEARDOWN_BUG),
      (read_verdict("TESTVERDICT=FAIL detail=b", 1), FAIL),
      (read_verdict("no verdict emitted", 0), NO_VERDICT),
      (read_verdict("died early", 139), CRASH_NO_VERDICT),
  ]
  record("read_verdict.classify", all(got == want for got, want in synth),
         str([g for g, _ in synth]))

  # ---- 3. guards: dir-creation + asset preflight (subprocess) ----
  rc, out = _spawn("guards")
  record("guards.dir_and_preflight", rc == 0 and "GUARDS=PASS" in out, f"rc={rc}")

  # ---- 4. watchdog wedge: sampled + killed with distinct rc (subprocess) ----
  sample = os.path.join(TMP, "wedge_sample.txt")
  if os.path.exists(sample):
    os.remove(sample)
  rc, out = _spawn("watchdog-wedge", sample, timeout=30)
  sample_ok = os.path.isfile(sample) and os.path.getsize(sample) > 0
  record("watchdog.sample_then_kill",
         rc == WATCHDOG_RC and sample_ok and "WEDGE_ESCAPED" not in out,
         f"rc={rc} want={WATCHDOG_RC} sample_bytes="
         f"{os.path.getsize(sample) if os.path.isfile(sample) else 0}")

  # ---- 5. headless_app lifecycle 5x (flakiness check) ----
  life_rcs = []
  for i in range(5):
    rc, out = _spawn("headless-life")
    life_rcs.append(rc)
  record("lifecycle.5x_rc0", all(r == 0 for r in life_rcs), f"rcs={life_rcs}")

  # ---- 6. capture smoke: non-empty, non-black PNG (READ it) ----
  png = os.path.join(TMP, "capture_smoke.png")
  if os.path.exists(png):
    os.remove(png)
  rc, out = _spawn("capture", png)
  nonblack_read = False
  detail = f"rc={rc}"
  if os.path.isfile(png) and os.path.getsize(png) > 0:
    try:
      import numpy
      from PIL import Image
      arr = numpy.asarray(Image.open(png).convert("RGB"))
      mean = float(arr.mean())
      nonblack_read = int(arr.max()) > 0 and mean > 1.0
      detail = f"rc={rc} png_bytes={os.path.getsize(png)} mean={mean:.2f}"
    except Exception as e:
      detail = f"rc={rc} png_read_error={e!r}"
  record("capture.nonblack_png", rc == 0 and nonblack_read, detail)

  # ---- 7. migration: test_rtg observable line byte-identical ----
  rtg = os.path.join(LLGFX, "test_rtg_resize_extent.py")
  rc, out = _spawn_script(rtg, timeout=110)
  line = "=== rtg resize extent oracle PASSED ==="
  record("migration.rtg_observable", rc == 0 and line in out, f"rc={rc} line_present={line in out}")

  # ---- 8. neutrality: terrain battery still N/N ----
  # ORK_TESTING_GATE_NO_BATTERY skips ONLY this slow (~2min) neutrality run so the fast
  # harness checks can iterate under a short timeout; the default runs the full battery.
  if os.environ.get("ORK_TESTING_GATE_NO_BATTERY"):
    print("[GATE] SKIP neutrality.terrain_battery (ORK_TESTING_GATE_NO_BATTERY set)", flush=True)
  else:
    batt = os.path.join(LLGFX, "test_terrain_battery.py")
    try:
      rc, out = _spawn_script(batt, timeout=170)
      battery_ok = rc == 0 and "terrain battery PASSED" in out
      suites = ""
      for ln in out.splitlines():
        if "terrain battery PASSED" in ln:
          suites = ln.strip()
      record("neutrality.terrain_battery", battery_ok, suites or f"rc={rc}")
    except subprocess.TimeoutExpired:
      record("neutrality.terrain_battery", False, "TIMEOUT")

  # ---- verdict ----
  from ork.testing import verdict
  all_ok = all(results.values())
  fails = [k for k, v in results.items() if not v]
  verdict(all_ok, "ok" if all_ok else ("fails=" + ",".join(fails)))
  sys.exit(0 if all_ok else 1)


def main():
  ap = argparse.ArgumentParser()
  ap.add_argument("--role", default=None)
  ap.add_argument("rest", nargs="*")
  args = ap.parse_args()

  role = args.role
  if role is None:
    driver()
  elif role == "verdict-pass":
    _role_verdict_pass()
  elif role == "verdict-fail":
    _role_verdict_fail()
  elif role == "verdict-teardown-bug":
    _role_verdict_teardown_bug()
  elif role == "guards":
    _role_guards()
  elif role == "watchdog-wedge":
    _role_watchdog_wedge(args.rest[0])
  elif role == "headless-life":
    _role_headless_life()
  elif role == "capture":
    _role_capture(args.rest[0])
  else:
    print(f"unknown role {role!r}", flush=True)
    sys.exit(2)


if __name__ == "__main__":
  main()
