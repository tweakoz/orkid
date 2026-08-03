#!/usr/bin/env ork.python
################################################################################
# test_ecsplay_audio_manifest — canary for ork.ecsplay.py's audio opt-out
# (audio lane, ecsplay inconsistency fix).
#
# WHAT IT PROVES (needs NO audio hardware, NO display, NO engine boot):
#   ork.ecsplay.py enables audio ON BY DEFAULT (owner-ruled back-compat). The
#   ONLY way a scene opts itself out is a TOP-LEVEL "audio": false beside
#   "root" in its .ecs manifest, peeked (ork.hypergraph.ecs.audio_manifest.
#   scene_disables_audio) with an EXACT JSON parse of the top level —
#   mirroring ork.ecs.player.exe's rapidjson peek (main.cpp, A0a) inverted for
#   ecsplay's default-on semantics. A substring/grep-style scan would
#   false-positive on any nested component property also named "audio"; this
#   is the negative case this canary exists to pin.
#
#   Checks: missing key / true / non-bool value / unparseable / missing file
#   all leave audio ON; exact top-level `false` (and ONLY that) turns it OFF;
#   a nested "audio": false inside a component does NOT trip the peek; both
#   committed scenes (ecsscn2.ecs, ecsscn3.ecs — neither declares the key)
#   stay audio-ON.
#
# SHAPE: pure stdlib + the peeked module only (no orkengine import at all —
# audio_manifest.py is deliberately engine-independent) — fast, hardware-free,
# single process.
################################################################################

import os
os.environ["PYTHONUNBUFFERED"] = "1"
import sys
import json
import tempfile

# prepend THIS checkout's scripts dir so ork.testing / ork.hypergraph resolve
# from the same tree (mirrors sibling singularity canaries).
_ROOT = os.path.abspath(__file__)
for _ in range(5):
  _ROOT = os.path.dirname(_ROOT)
sys.path.insert(0, os.path.join(_ROOT, "obt.project", "scripts"))

from ork.testing import verdict
from ork.hypergraph.ecs.audio_manifest import scene_disables_audio


def _write(tmpdir, name, obj_or_text):
  path = os.path.join(tmpdir, name)
  with open(path, "w") as f:
    if isinstance(obj_or_text, str):
      f.write(obj_or_text)
    else:
      json.dump(obj_or_text, f)
  return path


def _main():
  failures = []

  def check(label, path, expect_disables):
    got = scene_disables_audio(path)
    if got != expect_disables:
      failures.append("%s: expected disables=%s got=%s (path=%s)" %
                       (label, expect_disables, got, path))

  with tempfile.TemporaryDirectory() as tmp:
    check("missing_file", os.path.join(tmp, "nope.ecs"), False)
    check("no_key", _write(tmp, "no_key.ecs", {"root": {}}), False)
    check("audio_true", _write(tmp, "audio_true.ecs", {"root": {}, "audio": True}), False)
    check("audio_false", _write(tmp, "audio_false.ecs", {"root": {}, "audio": False}), True)
    # exact-type check: a string "false" is NOT the JSON bool literal.
    check("audio_string", _write(tmp, "audio_string.ecs", {"root": {}, "audio": "false"}), False)
    # exact-type check: 0 is falsy in Python but must NOT match (is False).
    check("audio_zero", _write(tmp, "audio_zero.ecs", {"root": {}, "audio": 0}), False)
    # THE negative case: nested "audio" inside a component must not false-positive
    # (the substring-scan bug class this peek is written to avoid).
    check("nested_audio_false", _write(tmp, "nested.ecs", {
        "root": {"entities": [{"components": [{"class": "SomeComponentData", "audio": False}]}]}
    }), False)
    check("unparseable", _write(tmp, "bad.ecs", "{not json"), False)

  # committed scenes: neither declares "audio" -> default stays on.
  ecsscenes = os.path.join(_ROOT, "ork.data", "ecsscenes")
  check("ecsscn2", os.path.join(ecsscenes, "ecsscn2.ecs"), False)
  check("ecsscn3", os.path.join(ecsscenes, "ecsscn3.ecs"), False)

  passed = not failures
  detail = "8 synthetic + 2 committed-scene checks" if passed else "; ".join(failures)
  sys.exit(verdict(passed, detail))


if __name__ == "__main__":
  _main()
