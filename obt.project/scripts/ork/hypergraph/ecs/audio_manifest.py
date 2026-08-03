################################################################################
# ECS scene manifest — audio opt-out peek
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
#
# Stdlib-only (no orkengine import) so this can be exercised by a fast,
# hardware-free unit test without booting the engine.
################################################################################

import os, json

def scene_disables_audio(scene_path):
  """Peek the .ecs manifest's TOP-LEVEL "audio" key WITHOUT invoking the
  reflected deserializer (EcsRuntime.load_scene runs post-GPU-init, far too
  late to gate subsystem creation at createEzApp time — same timing
  constraint that forces ork.ecs.player.exe's rapidjson peek, main.cpp
  A0a). ork.ecsplay.py enables audio by DEFAULT (back-compat); this mirrors
  the C++ player's peek inverted into an opt-OUT: True only when the
  top-level key is present and is exactly the JSON literal `false`. The
  peek PARSES rather than greps — a substring scan would false-positive on
  any nested component property that also happens to be named "audio".
  Missing key / true / non-bool value / unparseable-or-missing file all
  leave the default-on behavior untouched (the real deserializer reports
  load failures loudly elsewhere)."""
  if not scene_path or not os.path.exists(scene_path):
    return False
  try:
    with open(scene_path, "r") as f:
      doc = json.load(f)
  except (OSError, ValueError):
    return False
  if not isinstance(doc, dict):
    return False
  return doc.get("audio", True) is False
