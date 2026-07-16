#!/usr/bin/env ork.python
###############################################################################
# OPENXR X1 positive-seam test.
# Spawns a fresh headless lev2 GPU init with ORKID_EXTGPU_SELFTEST=1, which makes
# the Vulkan backend register a synthetic ExternalGpuRequirements exercising:
#   - instance-ext APPEND (one available ext not in the base list)
#   - instance-ext DEDUP  (one ext already in the base list -> appears once)
#   - required-physical-device selection (the default-picked device)
#   - device-ext APPEND (one available DEVICE ext not in the base list, asserted
#     present in the FINAL VkDeviceCreateInfo enabled list) — this leg guards the
#     OpenXR device-ext merge (the host_image_copy crash) from silent regression.
# The backend prints ORKID_EXTGPU_SELFTEST: verdict lines to stdout at each
# injection point; a subprocess captures + asserts them.
###############################################################################
import os, sys, subprocess

CHILD = "__x1_child__"

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
ez.mainThreadEnd()
ecs.headless_exit()
'''

if os.environ.get(CHILD) == "1":
    exec(CHILD_SRC)
    sys.exit(0)

env = dict(os.environ)
env["ORKID_EXTGPU_SELFTEST"] = "1"
env[CHILD] = "1"
proc = subprocess.run([sys.executable, __file__], env=env,
                      stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=120)
out = proc.stdout
lines = [l for l in out.splitlines() if l.startswith("ORKID_EXTGPU_SELFTEST:")]
print("=== captured selftest lines ===")
for l in lines:
    print(l)
print("=== end ===")


def has(substr):
    return any(substr in l for l in lines)

ok = True


def check(name, cond):
    global ok
    print(f"[{'PASS' if cond else 'FAIL'}] {name}")
    ok = ok and cond


req_append = next((l for l in lines if "request append instance ext" in l), "")
append_name = req_append.split("<", 1)[-1].rstrip(">") if "<" in req_append else ""
check("requested an append instance ext (non-empty)", bool(append_name))
check("append ext present after merge",
      any((f"instance ext append <{append_name}> present=1") in l for l in lines) if append_name else False)
check("dedup ext count==1",
      any(("instance ext dedup" in l and "count=1" in l) for l in lines))
check("required physical device honored", has("honored required physical device"))

# device-ext merge leg (the OpenXR host_image_copy regression guard).
req_dev_append = next((l for l in lines if "request append device ext" in l), "")
dev_append_name = req_dev_append.split("<", 1)[-1].rstrip(">") if "<" in req_dev_append else ""
check("requested an append DEVICE ext (non-empty)", bool(dev_append_name))
check("append DEVICE ext present in final VkDeviceCreateInfo list",
      any((f"device ext append <{dev_append_name}> present=1") in l for l in lines) if dev_append_name else False)

if proc.returncode != 0:
    print(f"NOTE child returncode={proc.returncode}")

print(f"=== X1 positive-seam test {'PASSED' if ok else 'FAILED'} ===")
sys.exit(0 if ok else 1)
