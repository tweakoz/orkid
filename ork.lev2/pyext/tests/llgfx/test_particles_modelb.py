#!/usr/bin/env python3
###############################################################################
# D.2 gate — particles MODEL B: the particle GraphData EMBEDS in ParticleSystemGenData at
# authoring (the DSL runs ONCE), and a deserialized scene materializes WITHOUT re-running
# Python — killing the named anti-pattern (recipe -> re-run DSL at load). Asserts:
#   1. authoring embeds the graph; the serialized gendata CONTAINS the new reflected state
#      (pool_size, the Parameters defaults incl. exposed names — was pyext-only),
#   2. gendata JSON round-trip: the re-serialized embedded graph is BYTE-IDENTICAL,
#   3. build() on the deserialized gendata succeeds with DSL RESOLUTION POISONED — proof of
#      zero Python re-run (model A would crash here),
#   4. ParticlesDrawableData itself round-trips (graphdata + scalars now reflected).
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys
from orkengine import core   # core before lev2
from orkengine import lev2
from orkengine import ecs
from orkengine.core import Object


def main():
  ezapp = ecs.headless_appinit(use_subsystems=['opq', 'core', 'gpu', 'lev2'])
  ezapp.mainThreadBegin()
  ctx = ezapp.bindGfxToCurrentThread()
  assert ctx, "bindGfxToCurrentThread() returned null"
  ok = False
  try:
    from ork.hypergraph.ecs.scene.assets import ParticleSystem

    # 1. authoring: the DSL runs ONCE here; the graph embeds
    wrap = ParticleSystem(dsl_file="elliptical_exposed")
    gd = wrap.gendata
    gd.asset_name = "modelb_psys"
    assert gd.graph is not None, "authoring did not embed the graph"
    js = gd.serializeJson()
    for needle, why in [
        ("pool_size",  "ParticlePoolData._poolSize (was pyext-only)"),
        ("defaults",   "ParametersModuleData._defaults (was pyext-only)"),
        ("Turb",       "an exposed parameter name"),
        ("Rate",       "the second exposed parameter name"),
        ("dsl_file",   "provenance retained"),
    ]:
      assert needle in js, "serialized gendata missing %s (%r)" % (why, needle)
    print("MODELB embed+reflect PASS", flush=True)

    # 2. round-trip: the embedded graph reproduces byte-identically
    gd2 = Object.deserializeJson(js)
    assert gd2.graph is not None
    assert gd2.graph.serializeJson() == gd.graph.serializeJson(), "embedded graph round-trip diverged"
    print("MODELB graph roundtrip PASS (byte-identical)", flush=True)

    # 3. THE ANTI-PATTERN KILL: build from the deserialized gendata with DSL resolution poisoned.
    import ork.hypergraph.dflow.particles.resolve as _resolve
    real = _resolve.resolve_dsl_file
    def _poisoned(arg):
      raise AssertionError("model A regression: build() re-ran the Python DSL at load (%r)" % arg)
    _resolve.resolve_dsl_file = _poisoned
    try:
      dd = ParticleSystem.from_gendata(gd2, artifacts={}).build()
    finally:
      _resolve.resolve_dsl_file = real
    assert dd is not None and dd.graphdata is not None
    print("MODELB no-python-at-load PASS (built with DSL resolution poisoned)", flush=True)

    # 4. the drawable data itself round-trips (graphdata + scalars now reflected)
    djs = dd.serializeJson()
    dd2 = Object.deserializeJson(djs)
    assert dd2.graphdata is not None
    assert dd2.graphdata.serializeJson() == dd.graphdata.serializeJson()
    print("MODELB drawabledata roundtrip PASS", flush=True)
    ok = True
  except Exception:
    import traceback
    traceback.print_exc()
  finally:
    ezapp.mainThreadEnd()
    print("=== particles model-B gate %s ===" % ("PASSED" if ok else "FAILED"), flush=True)
    ecs.headless_exit()
    sys.exit(0 if ok else 1)


main()
