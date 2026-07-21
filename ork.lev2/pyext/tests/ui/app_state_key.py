#!/usr/bin/env ork.python
################################################################################
# Gate: ork.ui.app_state per-app persisted-state slot derivation.
#
#  The slot is keyed by the ezapp identity (app name + realpath of the entry
#  script). Asserts:
#    - deterministic: same (name, path) -> same slot/path across calls,
#    - distinct: different names OR different paths -> different slots,
#    - user-local: the path lives under obt.path.user_global() (OUTSIDE the repo),
#    - the text-input-focus suppression predicate is well-defined for None.
################################################################################

import os
os.environ["PYTHONUNBUFFERED"] = "1"
import sys

import obt.path
from ork.ui.app_state import (
    state_slot, session_state_dir, dock_layout_path, text_input_has_focus)


def main():
  problems = []

  # deterministic for a given (name, path)
  a1 = dock_layout_path("terrainedit", entry_path="/a/b/ork.terrain.edit.py", create=False)
  a2 = dock_layout_path("terrainedit", entry_path="/a/b/ork.terrain.edit.py", create=False)
  if a1 != a2:
    problems.append(f"non-deterministic for same (name,path): {a1} != {a2}")

  # distinct app names -> distinct slots (same path)
  b = dock_layout_path("dflowedit", entry_path="/a/b/ork.terrain.edit.py", create=False)
  if b == a1:
    problems.append("distinct app names collided to the same slot")

  # distinct entry paths -> distinct slots (same name) — relocating the tree re-keys
  c = dock_layout_path("terrainedit", entry_path="/a/b/OTHER/ork.terrain.edit.py", create=False)
  if c == a1:
    problems.append("distinct entry paths collided to the same slot")

  # two fully-distinct mock (name, path) pairs -> distinct
  m1 = state_slot("editorX", entry_path="/opt/x/x.py")
  m2 = state_slot("editorY", entry_path="/opt/y/y.py")
  if m1 == m2:
    problems.append(f"distinct mock pairs collided: {m1} == {m2}")

  # user-local: under obt.path.user_global(), OUTSIDE any repo checkout
  root = str(obt.path.user_global())
  d = session_state_dir("terrainedit", entry_path="/a/b/ork.terrain.edit.py", create=False)
  if not d.startswith(root):
    problems.append(f"state dir not under user_global(): {d} (root={root})")
  if "orkid/appstate" not in d.replace(os.sep, "/"):
    problems.append(f"state dir missing orkid/appstate namespace: {d}")

  # default entry (sys.argv[0]) path derivation must not raise
  try:
    _ = dock_layout_path("terrainedit", create=False)
  except Exception as e:
    problems.append(f"default-entry derivation raised: {e}")

  # focus predicate: None is safe/False
  if text_input_has_focus(None) is not False:
    problems.append("text_input_has_focus(None) should be False")

  print(f"a1={a1}", flush=True)
  print(f"slot(editorX,...)={m1}  slot(editorY,...)={m2}", flush=True)
  print(f"user_global root={root}", flush=True)

  if problems:
    print("=== app_state key-derivation gate FAILED ===", flush=True)
    for p in problems:
      print("  - " + p, flush=True)
    sys.exit(1)
  print("=== app_state key-derivation gate PASSED ===", flush=True)
  sys.exit(0)


if __name__ == "__main__":
  main()
