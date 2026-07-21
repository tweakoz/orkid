#!/usr/bin/env ork.python
###############################################################################
# OPENXR hand-tracking (generic XR_EXT_hand_tracking) headless-degraded gate.
#
# mac has NO XR runtime, so live joints are OWNER-run on a real runtime. This
# gate covers the two facets observable WITHOUT a runtime:
#
#  A) C++ engine-surface self-test: a fresh headless lev2 init with
#     ORKID_OPENXR_SELFTEST=1 runs the pure-C++ hand-mirror degrade fixture in
#     GfxInit and prints the "handtracking_unavailable_degrades_clean" verdict.
#
#  B) python hand API on the degraded OpenXrDevice: with ORKID_VR_DRIVER=openxr
#     and no runtime, orkidvr.device() is the OpenXrDevice (inactive). Its hand
#     API must report unavailable cleanly — hand_tracking_supported False, both
#     hands non-null / unsupported / inactive with 26 cleared joints — no crash,
#     one device-level fallback log line.
#
# SKIP-LOUD: the API additions need the coordinated build. If the binding is
# absent (pre-build binary), the child prints X3: SKIP and exits 0 so this file
# is runnable both before AND after the build.
###############################################################################
import os, sys, subprocess

CHILD = "__x3_child__"

CHILD_SRC = '''
import os
os.environ["PYTHONUNBUFFERED"] = "1"
from orkengine import core
from orkengine import lev2
from orkengine import ecs
ez = ecs.headless_appinit(use_subsystems=["opq","core","gpu","lev2"])
ez.mainThreadBegin()
ctx = ez.bindGfxToCurrentThread()
assert ctx, "bindGfxToCurrentThread() returned null"

dev = lev2.orkidvr.device()
print("X3: active device present:", dev is not None)

# skip-loud when the binding is not in this build yet (pre-build)
if dev is None or not hasattr(dev, "hand_tracking_supported"):
    print("X3: SKIP hand-tracking binding absent in this build (re-run post-build)")
    ez.mainThreadEnd()
    ecs.headless_exit()
    print("X3_CHILD: clean exit")
    raise SystemExit(0)

ok = True
def chk(name, cond):
    global ok
    print("X3: [%s] %s" % ("PASS" if cond else "FAIL", name))
    ok = ok and bool(cond)

# no runtime on mac -> feature unavailable, degraded cleanly
chk("device.hand_tracking_supported is False", dev.hand_tracking_supported == False)

for label, hand in (("left", dev.left_hand), ("right", dev.right_hand)):
    chk("%s hand non-null" % label, hand is not None)
    chk("%s hand unsupported" % label, hand.supported == False)
    chk("%s hand inactive" % label, hand.active == False)
    chk("%s hand joint_count==26" % label, hand.joint_count == 26)
    joints = hand.joints
    chk("%s hand joints list==26" % label, len(joints) == 26)
    all_clear = all((not j.valid) and (not j.position_valid) and (not j.orientation_valid) for j in joints)
    chk("%s hand joints all-cleared (no stale data)" % label, all_clear)

# handState(side) parity with the left/right properties
chk("handState(0) matches left", dev.handState(0).supported == dev.left_hand.supported)
chk("handState(1) matches right", dev.handState(1).supported == dev.right_hand.supported)
# a single joint read is well-formed
j0 = dev.left_hand.joint(0)
chk("joint(0) readable", (j0 is not None) and (j0.radius == 0.0) and (not j0.valid))

print("X3_RESULT:", "PASS" if ok else "FAIL")
ez.mainThreadEnd()
ecs.headless_exit()
print("X3_CHILD: clean exit")
raise SystemExit(0)
'''

if os.environ.get(CHILD) == "1":
    exec(CHILD_SRC)
    sys.exit(0)


def run_child(extra_env):
    env = dict(os.environ)
    env[CHILD] = "1"
    env.update(extra_env)
    return subprocess.run([sys.executable, __file__], env=env,
                          stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                          text=True, timeout=180)


ok = True


def check(name, cond):
    global ok
    print(f"[{'PASS' if cond else 'FAIL'}] {name}")
    ok = ok and cond


###############################################################################
# A) C++ engine-surface self-test verdict
###############################################################################
print("=== X3.A hand-mirror self-test ===")
procA = run_child({"ORKID_OPENXR_SELFTEST": "1"})
linesA = [l for l in procA.stdout.splitlines() if l.startswith("ORKID_OPENXR_SELFTEST:")]
verdict_line = [l for l in linesA if "handtracking_unavailable_degrades_clean" in l]
if verdict_line:
    for l in verdict_line:
        print(l)
    check("A: hand-mirror degrade verdict PASS", any("[PASS]" in l for l in verdict_line))
else:
    print("[SKIP] A: hand-mirror verdict absent (pre-build binary) — re-run post-build")
check("A: child exit clean", procA.returncode == 0)

###############################################################################
# B) python hand API on the degraded OpenXrDevice (no runtime)
###############################################################################
print("=== X3.B python hand API (degraded) ===")
envB = {"ORKID_VR_DRIVER": "openxr"}
if "XR_RUNTIME_JSON" in os.environ:
    envB["XR_RUNTIME_JSON"] = "/nonexistent/orkid_x3_no_runtime.json"
procB = run_child(envB)
x3_lines = [l for l in procB.stdout.splitlines() if l.startswith("X3:") or l.startswith("X3_RESULT:")]
for l in x3_lines:
    print(l)

skipped = any("SKIP hand-tracking binding absent" in l for l in x3_lines)
if skipped:
    print("[SKIP] B: hand-tracking binding not present in this build — re-run after the coordinated build")
    check("B: child clean exit (no crash)", "X3_CHILD: clean exit" in procB.stdout and procB.returncode == 0)
else:
    child_pass = any(l == "X3_RESULT: PASS" for l in x3_lines)
    no_fail = not any("[FAIL]" in l for l in x3_lines)
    check("B: all in-child hand-API checks pass", child_pass and no_fail)
    check("B: child clean exit (no crash)", "X3_CHILD: clean exit" in procB.stdout and procB.returncode == 0)

print(f"=== X3 test {'PASSED' if ok else 'FAILED'} ===")
sys.exit(0 if ok else 1)
