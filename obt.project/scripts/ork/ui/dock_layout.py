################################################################################
# ork/ui/dock_layout.py — python-side JSON persistence for ui.DockSpace.
#
#   save_layout(dock)        -> dict (versioned; panel ids keyed by DockPanel.name)
#   to_json(layout)          -> byte-deterministic str (repo-checkable)
#   load_layout(dock, data)  -> apply a saved arrangement to a DockSpace whose
#                               panels (by id) already exist.
#
# Owner decision 2: NO reflection in ui::. The C++ side exposes only tree
# introspection (serializeLayout) and the reconstruction primitives
# (moveChild / setSplitProportion / activatePanel / allPanels); the JSON format
# and the save/load logic live here.
#
# Interior node  : {"split": "V"|"H", "proportion": f, "a": <node>, "b": <node>}
#                  V = left|right (a=left,  b=right); H = top/bottom (a=top, b=bottom)
# Leaf node      : {"leaf": [id,...] (name-sorted), "active": id}
################################################################################

import json
from orkengine.core import CrcStringProxy

_tokens = CrcStringProxy()

LAYOUT_VERSION = 1

################################################################################

def _is_leaf(node):
  return "leaf" in node

def _collect_ids(node, out):
  if _is_leaf(node):
    out.extend(node["leaf"])
  else:
    _collect_ids(node["a"], out)
    _collect_ids(node["b"], out)

def _canon(node):
  # canonical form: leaf ids kept in tab order (explicit, reorderable) + rounded
  # proportion -> byte-determinism (order is stable + restored on load)
  if _is_leaf(node):
    return {"leaf": list(node["leaf"]), "active": node["active"]}
  return {
    "split": node["split"],
    "proportion": round(float(node["proportion"]), 6),
    "a": _canon(node["a"]),
    "b": _canon(node["b"]),
  }

################################################################################

def save_layout(dock):
  raw  = dock.serializeLayout()
  tree = _canon(raw) if raw else {}

  # loud refusal on duplicate panel ids
  ids = []
  if tree:
    _collect_ids(tree, ids)
  seen, dups = set(), set()
  for i in ids:
    (dups if i in seen else seen).add(i)
  if dups:
    raise ValueError(f"save_layout: duplicate panel ids in DockSpace: {sorted(dups)}")

  return {"version": LAYOUT_VERSION, "tree": tree}

def to_json(layout):
  return json.dumps(layout, sort_keys=True, separators=(",", ":"))

def panel_ids(data):
  """The panel ids referenced by a saved layout dict (a save_layout result or its JSON).
  Used by cross-window recreation to know which member panels a persisted window hosts."""
  if isinstance(data, (str, bytes)):
    data = json.loads(data)
  tree = data.get("tree", {}) or {}
  ids = []
  if tree:
    _collect_ids(tree, ids)
  return ids

################################################################################

def load_layout(dock, data):
  if isinstance(data, (str, bytes)):
    data = json.loads(data)
  tree = data.get("tree", {}) or {}

  live = {p.name: p for p in dock.allPanels()}
  need = []
  if tree:
    _collect_ids(tree, need)

  missing = sorted(i for i in need if i not in live)
  if missing:
    raise ValueError(f"load_layout: missing panel ids in live DockSpace: {missing}")
  extra = [pid for pid in live.keys() if pid not in need]

  if not tree:
    return

  # collapse everything into a single leaf (all panels tabbed together)
  order  = need + extra
  anchor = live[order[0]]
  for pid in order[1:]:
    dock.moveChild(panel=live[pid], to=anchor, zone=_tokens.CENTER)

  # realize the saved tree
  _realize(dock, live, tree)

  # extra live panels (not in the layout) -> append to the last leaf (documented default)
  if extra and need:
    last = live[need[-1]]
    for pid in extra:
      dock.moveChild(panel=live[pid], to=last, zone=_tokens.CENTER)

def _realize(dock, live, node):
  if _is_leaf(node):
    # restore the explicit tab order, then the active tab
    for i, pid in enumerate(node["leaf"]):
      if pid in live:
        dock.reorderPanel(live[pid], i)
    active = node.get("active")
    if active and active in live:
      dock.activatePanel(live[active])
    return
  a_ids, b_ids = [], []
  _collect_ids(node["a"], a_ids)
  _collect_ids(node["b"], b_ids)
  a_rep = live[a_ids[0]]
  b_rep = live[b_ids[0]]
  zone = _tokens.RIGHT if node["split"] == "V" else _tokens.BOTTOM
  # split off the b subtree, then gather the rest of its panels into that leaf
  dock.moveChild(panel=b_rep, to=a_rep, zone=zone)
  for bid in b_ids[1:]:
    dock.moveChild(panel=live[bid], to=b_rep, zone=_tokens.CENTER)
  # restore the divider proportion (clamped), then recurse (order matters: the
  # a/b leaves are direct siblings only until 'a' is split further)
  dock.setSplitProportion(a_rep, b_rep, node["proportion"])
  _realize(dock, live, node["a"])
  _realize(dock, live, node["b"])
