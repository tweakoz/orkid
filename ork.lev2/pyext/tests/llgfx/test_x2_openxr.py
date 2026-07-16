#!/usr/bin/env ork.python
###############################################################################
# OPENXR X2 tests (mac has NO XR runtime — this covers the two mac-observable
# facets; the live session/frame/composite are gated on a real runtime).
#
#  A) pose/FOV self-test: a fresh headless lev2 init with ORKID_OPENXR_SELFTEST=1
#     runs the pure C++ xrPoseToFmtx4 / fovToVrFrustum fixtures in GfxInit and
#     prints "ORKID_OPENXR_SELFTEST:" verdict lines; this subprocess asserts them.
#
#  B) loader fallback: a fresh headless lev2 init with ORKID_VR_DRIVER=openxr and
#     no runtime present must emit EXACTLY ONE "[OPENXR]" failure line, not crash,
#     and proceed (ctx bound, clean exit) — proving the fallback is NoVR-equivalent.
###############################################################################
import os, sys, subprocess

CHILD = "__x2_child__"

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
print("X2_CHILD: ctx bound ok")
ez.mainThreadEnd()
ecs.headless_exit()
print("X2_CHILD: clean exit")
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
# A) pose / FOV self-test
###############################################################################
print("=== X2.A pose/FOV self-test ===")
procA = run_child({"ORKID_OPENXR_SELFTEST": "1"})
linesA = [l for l in procA.stdout.splitlines() if l.startswith("ORKID_OPENXR_SELFTEST:")]
for l in linesA:
    print(l)
verdicts = {
    "pose_identity": None, "pose_translation": None, "pose_yaw90": None,
    "pose_combined": None, "fov_symmetric_matches_engine_helper": None,
    # XR projection polarity vs native/NoVR (cross-convention): the eye projection's
    # Y-scale sign + determinant sign must match fmtx4::perspective, else the projection
    # is Y-flipped -> inverted winding -> inside-out backface culling in XR mode.
    "proj_y_sign_matches_native": None,
    # ext-string splitter (the producer parse the X1 device-ext merge consumes) —
    # the runtime's real strings can't repro on mac, but the split logic can.
    "splitexts_basic": None, "splitexts_ws": None,
    "splitexts_trailing_nul": None, "splitexts_empty": None,
    # degenerate-FOV self-defense: zero fov -> finite fallback projection (proxy for
    # the pre-first-frame all-zero-FOV NaN-bind failure that has no mac runtime).
    "fov_degenerate_fallback": None,
    # per-eye handoff guard: no session -> __compositeStereo early-returns (mac-
    # observable facet of the runtime-owned FWDPBRVRDM path; no runtime needed).
    "compositestereo_guard_no_session": None,
    # depth-layer math (XR_KHR_composition_layer_depth): standard-Z -> reverse-Z + the
    # coverage clamp that keeps the background opaque, and near/far self-defense. Pure
    # math — the GPU copy + runtime reprojection are gated on a real runtime.
    "depth_stdz_to_revz": None,
    "depth_nearfar_selfdefense": None,
}
for l in linesA:
    for name in verdicts:
        if name in l:
            verdicts[name] = ("[PASS]" in l)
check("self-test ran (verdict lines present)", len(linesA) > 0)
for name, res in verdicts.items():
    check(f"selftest {name}", res is True)
check("A child exit clean", procA.returncode == 0)

###############################################################################
# B) loader fallback (no runtime): exactly one [OPENXR] line, no crash, proceeds
###############################################################################
print("=== X2.B loader fallback ===")
# Ensure no runtime is discoverable.
envB = {"ORKID_VR_DRIVER": "openxr"}
if "XR_RUNTIME_JSON" in os.environ:
    envB["XR_RUNTIME_JSON"] = "/nonexistent/orkid_x2_no_runtime.json"
procB = run_child(envB)
xr_lines = [l for l in procB.stdout.splitlines() if l.startswith("[OPENXR]")]
print("--- [OPENXR] lines ---")
for l in xr_lines:
    print(l)
print("--- end ---")
# The single failure line is the pre-graphics fallback; the wide-swapchain bring-up
# line only appears on a real session, so on mac exactly one [OPENXR] line is expected.
check("B: exactly one [OPENXR] fallback line", len(xr_lines) == 1)
check("B: fallback line mentions NoVR", any("NoVR" in l for l in xr_lines))
check("B: child bound ctx (engine proceeded)", "X2_CHILD: ctx bound ok" in procB.stdout)
check("B: child clean exit (no crash)", "X2_CHILD: clean exit" in procB.stdout and procB.returncode == 0)

print(f"=== X2 test {'PASSED' if ok else 'FAILED'} ===")
sys.exit(0 if ok else 1)
