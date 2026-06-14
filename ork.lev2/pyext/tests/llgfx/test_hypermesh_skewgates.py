#!/usr/bin/env python3
###############################################################################
# A.3 gate — the FAIL-LOUD protections must actually fire (negative tests, subprocess-based since
# each protection aborts the process via OrkAssert):
#   1. PLUG-LAYOUT SKEW: a serialized plug whose name doesn't match the reshapeIOs plug at that index
#      (simulates an engine-side plug reorder/rename since save) -> loud abort, NOT silent scrambling.
#   2. CONNECTION DESERIALIZE: an edge whose module name no longer resolves -> loud abort, NOT a
#      silently half-wired graph.
# Builds + saves the artifact in-process, then corrupts a copy per case and loads it in a CHILD
# process, asserting the child dies with the right message.
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
os.environ["ORK_DFLOW_ENFORCE_TYPED_CONNECT"] = "1"   # gates ENFORCE typed connections (runtime default = WARN)
import sys, subprocess, tempfile
from orkengine import core
from orkengine import lev2
from orkengine import ecs
from orkengine.core import vec3

from ork.hypergraph.dflow.hypermesh import Hypermesh, S, isolate, group, POLY

ART = "/tmp/hm_skewgate.hmgraph.json"

_CHILD = r"""
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys
from orkengine import core, lev2, ecs
from ork.hypergraph.dflow.hypermesh import load_graphdata
ezapp = ecs.headless_appinit(use_subsystems=['opq', 'core', 'gpu', 'lev2'])
ezapp.mainThreadBegin()
ezapp.bindGfxToCurrentThread()
g = load_graphdata(sys.argv[1])     # expected to ABORT inside deserialize on the corrupted artifact
print("CHILD_LOADED_WITHOUT_ERROR")  # reaching here = the gate FAILED to fire
ezapp.mainThreadEnd()
core.coreappexit()
"""


class GateAsset(Hypermesh):
  def __init__(self):
    super().__init__()
    n = self.box(size=1.0)
    n = self.select(n, S.N.dot(vec3(0, 1, 0)) > 0.6, domain=POLY, op=isolate(group(2)))
    n = self.extrude_faces(n, distance=0.5, slot=2)
    self.output(n)


def _expect_child_abort(tag, corrupted_json, expect_msg):
  art = os.path.join(tempfile.gettempdir(), "hm_skewgate_%s.json" % tag)
  with open(art, "w") as f:
    f.write(corrupted_json)
  child = os.path.join(tempfile.gettempdir(), "hm_skewgate_child.py")
  with open(child, "w") as f:
    f.write(_CHILD)
  r = subprocess.run([sys.executable, child, art], capture_output=True, text=True, timeout=120)
  out = r.stdout + r.stderr
  assert "CHILD_LOADED_WITHOUT_ERROR" not in out, \
      f"[{tag}] the corrupted artifact loaded WITHOUT error — the gate did not fire"
  assert r.returncode != 0, f"[{tag}] child exited 0 on a corrupted artifact"
  assert expect_msg in out, \
      f"[{tag}] child died but without the expected diagnostic {expect_msg!r}\n--- child output tail:\n{out[-800:]}"
  print(f"  gate[{tag}] fired correctly (rc={r.returncode}): {expect_msg!r} present", flush=True)


def main():
  ezapp = ecs.headless_appinit(use_subsystems=['opq', 'core', 'gpu', 'lev2'])
  ezapp.mainThreadBegin()
  ezapp.bindGfxToCurrentThread()
  try:
    return _body()
  finally:
    ezapp.mainThreadEnd()
    core.coreappexit()


def _body():
  a = GateAsset()
  a.save(ART)
  js = open(ART).read()

  # control: the UNCORRUPTED artifact must load fine in a child
  ctl = os.path.join(tempfile.gettempdir(), "hm_skewgate_child.py")
  with open(ctl, "w") as f:
    f.write(_CHILD)
  r = subprocess.run([sys.executable, ctl, ART], capture_output=True, text=True, timeout=120)
  assert "CHILD_LOADED_WITHOUT_ERROR" in (r.stdout + r.stderr), \
      f"control load failed — environment problem, not a gate test\n{(r.stdout + r.stderr)[-800:]}"
  print("  control: pristine artifact loads in a child", flush=True)

  # 1. PLUG-LAYOUT SKEW: rename the extrude 'distance' plug's serialized name
  assert '"name": "distance"' in js, "artifact schema changed — fix this test's corruption targets"
  _expect_child_abort("plugskew", js.replace('"name": "distance"', '"name": "distancex"', 1),
                      "PLUG-LAYOUT SKEW")

  # 2. CONNECTION fail-loud: point an edge at a module name that doesn't exist. The connections
  #    section serializes AFTER the Modules map, so corrupting the LAST occurrence of the module
  #    name hits only the edge reference (the module map key stays intact).
  i = js.rfind("select_1")
  assert i > 0
  _expect_child_abort("conndrop", js[:i] + "select_Z" + js[i + len("select_1"):],
                      "CONNECTION DESERIALIZE FAILED")

  print("HYPERMESH_SKEWGATES_RESULT=PASS", flush=True)
  return 0


if __name__ == "__main__":
  sys.exit(main())
