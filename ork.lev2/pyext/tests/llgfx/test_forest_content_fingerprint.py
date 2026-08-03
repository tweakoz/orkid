#!/usr/bin/env python3
###############################################################################
# FOREST CONTENT fingerprint gate — pure author phase (no device, no window).
#
# ForestScene was PROMOTED out of ork.data/scenes/scn_forest.py into the scene
# library (ork.hypergraph.ecs.scene.content.forest) with all of its content:
# species grammars, material recipes, terrain/bake config, LOD + cull constants,
# post-fx chain. The move was required to be BEHAVIOR-PRESERVING, and this gate
# is that requirement made permanent: it rebuilds the forest with every
# device-touching call recorded instead of executed, and compares
#
#   * the DECLARATION FINGERPRINT — systems, scenegraph params + sub-calls,
#     archetypes/components, spawners, and every asset/terrain/projectile call
#     in order;
#   * the MATERIAL DIGESTS — one per declared asset (a generated material is a
#     pure function of its dsl_class + kwargs, and a tree variant's seed rides
#     the closure of the per-variant class, so the 16 variants digest apart).
#
# The GOLDEN below was first recorded from ork.data/scenes/scn_forest.py at
# 79fc875ab — the file this content was promoted from, evaluated with this same
# harness. A content edit legitimately moves it: re-record the printed value and
# say so in the commit message. A move, a re-import or a refactor must NOT.
#
# RE-RECORDED at a540a49f0, same 19 assets / 19 distinct digests, absorbing four
# owner-ratified content changes and nothing else:
#   354ee4f22, 1daba3f9b   bench tweaks — the forest's cloud-shadow arming and
#                          time_scale, the swest values
#   d8d09e0f0              the deck night-radiance wiring
#   41b7d5e62              bench tweak — forest terrain bake 4096 -> 8192
# (the value f7f70cc82e6ee04a covered the first three, at 1bd21cac4; the bake
# tweak landed on top of it the same evening and moved it again).
#
# Values are canonicalized for exactly two things and nothing else: heap
# addresses (a repr detail) and the DEFINING MODULE NAME (the one thing a
# promotion changes). Class identity is name + bases + bound closure cells, so
# unrelated library growth does not perturb the digest.
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import hashlib
import json
import re
import sys

# workspace-anchored (never cwd-derived): <ws>/ork.lev2/pyext/tests/llgfx/<this>
_WS = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                   "..", "..", "..", ".."))
for _p in (os.path.join(_WS, "ork.data/scenes"), os.path.join(_WS, "obt.project/scripts")):
  if _p in sys.path:
    sys.path.remove(_p)
  sys.path.insert(0, _p)

from orkengine import core   # core before lev2

from ork.hypergraph.ecs.scene import Scene

# re-recorded at a540a49f0 (see the header)
GOLDEN_FINGERPRINT = "f86d3b2ae467a030"
GOLDEN_ASSET_COUNT = 19

_ADDR = re.compile(r"(?: at )?0x[0-9a-fA-F]+")
_MODULE_NAMES = ("scn_forest", "ork.hypergraph.ecs.scene.content.forest")


def canon(v, depth=0):
  if depth > 4:
    return "..."
  if isinstance(v, type):
    # name + bases + whatever the class body BOUND (a per-variant tree class
    # carries its seed in a closure cell and nowhere else)
    cells = []
    for _name, val in sorted(vars(v).items()):
      fn = getattr(val, "__func__", val)
      for cell in (getattr(fn, "__closure__", None) or ()):
        try:
          cells.append(canon(cell.cell_contents, depth + 1))
        except ValueError:
          cells.append("<empty>")
    return "class:%s(%s)[%s]" % (v.__name__,
                                 ",".join(b.__name__ for b in v.__bases__),
                                 ",".join(cells))
  if isinstance(v, (int, float, str, bool, type(None))):
    return repr(v)
  if isinstance(v, (list, tuple)):
    return "[" + ",".join(canon(x, depth + 1) for x in v) + "]"
  if isinstance(v, dict):
    return "{" + ",".join("%s=%s" % (k, canon(x, depth + 1))
                          for k, x in sorted(v.items(), key=lambda kv: str(kv[0]))) + "}"
  d = getattr(v, "__dict__", None)
  if d:
    return "obj:%s%s" % (type(v).__name__, canon(dict(d), depth + 1))
  out = _ADDR.sub("", repr(v))
  for m in _MODULE_NAMES:
    out = out.replace(m, "<forestmodule>")
  return out


class _Stub:
  """a built asset: records every call made ON it (fork / imposter /
  drawable_data) — the forest's LOD + scatter wiring rides those."""

  def __init__(self, log, name):
    self._log  = log
    self._name = name
    self.built = "built:" + name

  def __getattr__(self, attr):
    def call(*a, **kw):
      self._log.append(("CALL", self._name + "." + attr,
                        [canon(x) for x in a],
                        {k: canon(v) for k, v in sorted(kw.items())}))
      return _Stub(self._log, "%s.%s()" % (self._name, attr))
    return call

  def __repr__(self):
    return "<%s>" % self._name


class _Recorder:
  def __init__(self, log):
    self._log = log

  def __getattr__(self, gen):
    def call(name, **kw):
      self._log.append(("ASSET", gen, name,
                        {k: canon(v) for k, v in sorted(kw.items())}))
      return _Stub(self._log, name)
    return call


def fingerprint(cls):
  """(fingerprint text, {asset name: digest}) for a Scene subclass, built with
  the GPU-touching calls recorded: the asset factory (materials, hypermeshes),
  the terrain bake and the projectile pool."""
  log   = []
  saved = (Scene.__init__, Scene.terrain, Scene.projectile_pool)

  def _init(self, *a, **kw):
    saved[0](self, *a, **kw)
    self.asset = _Recorder(log)

  def _rec(tag):
    return lambda self, *a, **kw: log.append(
        (tag, [canon(x) for x in a], {k: canon(v) for k, v in sorted(kw.items())}))

  Scene.__init__        = _init
  Scene.terrain         = _rec("TERRAIN")
  Scene.projectile_pool = _rec("PROJECTILES")
  try:
    scene = cls()
  finally:
    Scene.__init__, Scene.terrain, Scene.projectile_pool = saved

  out = [("LOG", log)]
  sg  = scene._systems.get("SceneGraphSystem")
  if sg is not None:
    out.append(("SG_KWARGS", sorted((k, canon(v)) for k, v in sg.kwargs.items())))
    out.append(("SG_SUBCALLS", [(c, [canon(x) for x in a],
                                 sorted((k, canon(v)) for k, v in kw.items()))
                                for c, a, kw in sg.sub_calls]))
  out.append(("SYSTEMS", sorted(scene._systems)))
  out.append(("ARCH", [(n, [(c.typename,
                             sorted((k, canon(v)) for k, v in c.kwargs.items()))
                            for c in a._components])
                       for n, a in sorted(scene._archetypes.items())]))
  out.append(("SPAWNERS", sorted(scene._spawners)))
  text = json.dumps(out, indent=1, default=repr)

  digests = {}
  for rec in log:
    if rec[0] == "ASSET":
      _tag, gen, name, kw = rec
      digests[name] = hashlib.sha256(
          json.dumps([gen, kw], sort_keys=True).encode()).hexdigest()[:16]
  return text, digests


def test_forest_fingerprint():
  from ork.hypergraph.ecs.scene.content import ForestScene
  text, digests = fingerprint(ForestScene)
  got = hashlib.sha256(text.encode()).hexdigest()[:16]
  bad = []
  if got != GOLDEN_FINGERPRINT:
    bad.append("declaration fingerprint %s, golden %s (%d bytes) — a MOVE or a "
               "refactor must not change this; a content edit re-records it"
               % (got, GOLDEN_FINGERPRINT, len(text)))
  if len(digests) != GOLDEN_ASSET_COUNT:
    bad.append("%d declared assets, golden %d" % (len(digests), GOLDEN_ASSET_COUNT))
  if len(set(digests.values())) != len(digests):
    dupes = [n for n in digests
             if list(digests.values()).count(digests[n]) > 1]
    bad.append("assets share a digest (a variant lost its seed): %s" % sorted(dupes))
  print("[forest fingerprint] %s (sha %s, %d assets, %d distinct digests)" % (
      "OK" if not bad else "BAD", got, len(digests), len(set(digests.values()))),
      flush=True)
  for b in bad:
    print("  " + b, flush=True)
  return not bad


def test_forest_is_library_content():
  """the promotion itself: the content lives in the library, the runnable scene
  file is gone, and the surviving scene imports it from there."""
  bad = []
  from ork.hypergraph.ecs.scene.content import forest as _mod
  if not _mod.__file__.startswith(os.path.join(_WS, "obt.project/scripts")):
    bad.append("library forest resolved from %s" % _mod.__file__)
  retired = os.path.join(_WS, "ork.data/scenes/scn_forest.py")
  if os.path.exists(retired):
    bad.append("retired scene file is back: %s" % retired)
  src = open(os.path.join(_WS, "ork.data/scenes/scn_forest.py")).read()
  if "from ork.hypergraph.ecs.scene.content import ForestScene" not in src:
    bad.append("scn_forest does not import the library forest")
  if "from scn_forest import" in src:
    bad.append("scn_forest still imports the retired scene file")
  print("[forest promotion] %s" % ("OK" if not bad else "BAD"), flush=True)
  for b in bad:
    print("  " + b, flush=True)
  return not bad


def main():
  tests = [test_forest_fingerprint, test_forest_is_library_content]
  failed = []
  for t in tests:
    try:
      if not t():
        failed.append(t.__name__)
    except Exception:
      import traceback
      traceback.print_exc()
      failed.append(t.__name__)
  ok = (len(failed) == 0)
  print("=== forest content fingerprint gate %s (%d/%d) ===" % (
      "PASSED" if ok else "FAILED", len(tests) - len(failed), len(tests)), flush=True)
  if failed:
    print("  failed: " + ", ".join(failed), flush=True)
  sys.exit(0 if ok else 1)


main()
