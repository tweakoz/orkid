#!/usr/bin/env python3
###############################################################################
# JUL09 S1.x gate — editor viewer keys (post-fx / envmap) SURVIVE a rebuild, OFFSCREEN.
#
# Runs the ACTUAL editor app class offscreen (createEzApp(offscreen=True)) and drives
# the real key HANDLERS (handler-level): G (gamma) must measurably shift the captured
# luma AND persist across a per-edit scene rebuild (the SAME held HSVG node is re-spliced
# into the fresh scenegraph); E (envmap) must SWAP the radiance-maps object (identity)
# without crashing — LIT-after-swap render verify is windowed (the player lane's known
# offscreen flakiness), so it is asserted as swap + non-crash here.
#
# Also asserts the camera chords (X pan / C dolly / V zoom) FALL THROUGH to the uicam —
# _on_viewport_event must NOT consume them (they are camera-reserved, not editor keys).
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys

from orkengine import core   # core before lev2
from orkengine import lev2
from orkengine import ecs

from ork.editor.terrainedit import TerrainEditor

tokens = core.CrcStringProxy()
_ui = lev2.ui


class _FakeKeyEvent:
  """Minimal KEY_DOWN uievent stand-in for the camera-fallthrough assert (the real
  handler reads .code / .keycode and the modifier flags)."""
  def __init__(self, keycode):
    self.code = tokens.KEY_DOWN.hashed
    self.keycode = keycode
    self.super = False
    self.ctrl = False
    self.shift = False


def main():
  app = TerrainEditor("voronoi", extent_m=512.0, dsl_kwargs={"amplitude": 60.0},
                      preview_dim=256, chunk=128, keytest=True)
  app.ezapp.mainThreadLoop()
  app.ezapp.shutdown()

  r = getattr(app, "_keytest_results", {})
  print(f"\n[keys] results = {r}", flush=True)

  # camera-chord fall-through: X/C/V (and Z rotate / F focus) must NOT be consumed by the
  # editor key handler — it delegates to runtime.handle_camera_event for anything but E/G/T/H/M.
  # We prove non-consumption by monkeypatching the camera handler and pressing the chords.
  fell_through = []
  app.runtime.handle_camera_event = lambda ev: fell_through.append(ev.keycode)
  for kc in (ord("X"), ord("C"), ord("V"), ord("Z"), ord("F")):
    app._on_viewport_event(_FakeKeyEvent(kc))
  chords_ok = (set(fell_through) == {ord("X"), ord("C"), ord("V"), ord("Z"), ord("F")})
  print(f"[keys.camera] chords reaching uicam = {sorted(fell_through)} -> {chords_ok}", flush=True)

  # [M] wrap-to-declared: cycling PAST the last debug mode must NOTIFY mode 0 at the
  # LIVE sim (the regression: an early-return skipped mode 0, so the render stuck on
  # the last debug look). Handler-level: record every systemNotify through a stub
  # controller and drive one full cycle.
  notified = []
  class _NotifyRecorder:
    def systemNotify(self, sysref, token, table):
      notified.append(dict(table))
  app.runtime.controller = _NotifyRecorder()
  app.runtime._sys_ref = object()
  app._mat_mode = 0
  for _ in range(len(app._mat_modes)):
    app._cycleMaterialMode()
  wrap_ok = (len(notified) == len(app._mat_modes)
             and list(notified[-1].values()) == [0])
  print(f"[keys.matmode] notifies per cycle = {[list(t.values())[0] for t in notified]} "
        f"(last must be 0) -> {wrap_ok}", flush=True)

  # delete-chain UX: deleting a node must SELECT the row that took its place so
  # repeated Delete works through a run. Handler-level on a real chain document
  # (voronoi is just noise->capture; open a fixture with a mid-chain run).
  import tempfile, textwrap
  fx = os.path.join(tempfile.mkdtemp(prefix="tered_keys_chain_"), "chainfx.py")
  with open(fx, "w") as f:
    f.write(textwrap.dedent("""
        from ork.hypergraph.dflow.terrain import HeightField
        from ork.hypergraph.dflow import terrain as T
        class ChainFx(HeightField):
            def __init__(self):
                super().__init__()
                h = T.Fbm(frequency=5.0, octaves=4) * 0.5 + 0.5
                h = T.terrace(h, step_m=6.0, sharpness=3.0)
                h = T.lpf(h, cutoff_texels=4.0)
                self.capture(h, "height")
    """))
  app._doOpenTerrain(fx)
  om = app.outliner_model
  order = app._visibleModelKeys()      # fbm, remap, remap, terr, lpf, capture
  from ork.hypergraph.dflow.terrain.doc import DocNode
  def _deletable(k):
    o = om.object_for_key(k)
    return (isinstance(o, DocNode) and o.clazz_name != "CaptureModule"
            and bool(o.connections))
  t1 = next((i for i, k in enumerate(order) if _deletable(k)), None)
  if t1 is None:
    chain_ok = False
    print("[keys.delete] no deletable node in fixture -> FAIL", flush=True)
  else:
    k1 = order[t1]
    exp1 = order[t1 + 1]
    app._deleteSelected(k1)
    sel1 = app.outliner.selected_key
    # press Delete AGAIN on the auto-selected row: the chain continues
    app._deleteSelected(sel1)
    sel2 = app.outliner.selected_key
    now = set(app._visibleModelKeys())
    chain_ok = (k1 not in now and sel1 == exp1 and sel1 not in now
                and sel2 in now and sel2 != sel1)
    print(f"[keys.delete] del1={k1!r}->sel={sel1!r} (expected {exp1!r})  "
          f"del2={sel1!r}->sel={sel2!r}  both-gone={k1 not in now and sel1 not in now} "
          f"-> {chain_ok}", flush=True)

  # add-module context menu (handler-level): the dropdown SELECTION callback must
  # perform the createItem mutation, select the new node, and record an undo step.
  if chain_ok:
    before = set(app._visibleModelKeys())
    depth0 = app._undo.depth()
    app._onAddMenuSelected(sel2, "/basin_fill")
    new_sel = app.outliner.selected_key
    add_menu_ok = (new_sel not in before
                   and app.outliner_model.object_for_key(new_sel) is not None
                   and app._undo.depth() == depth0 + 1)
    print(f"[keys.addmenu] anchor={sel2!r} -> new={new_sel!r} selected+recorded "
          f"-> {add_menu_ok}", flush=True)
  else:
    add_menu_ok = False

  results = {
    "add_menu_creates_selects_records": add_menu_ok,
    "delete_selects_next":   chain_ok,
    "gamma_luma_shift":      bool(r.get("gamma_shift")),
    "gamma_persists_rebuild": bool(r.get("gamma_persists_rebuild")),
    "envmap_swapped":        bool(r.get("envmap_swapped")),
    "envmap_noncrash":       bool(r.get("envmap_noncrash")),
    "envmap_persists_rebuild": bool(r.get("envmap_persists_rebuild")),
    "tone_persists_rebuild2": bool(r.get("tone_persists_rebuild2")),
    "camera_chords_fallthrough": chords_ok,
    "matmode_wrap_notifies_declared": wrap_ok,
  }
  ok = all(results.values())
  print(f"\n=== terrain editor keys (S1.x) {'PASSED' if ok else 'FAILED'} ===", flush=True)
  for k, v in results.items():
    print(f"    {k}: {'ok' if v else 'FAIL'}", flush=True)
  if r.get("envmap_note"):
    print(f"    (note: {r['envmap_note']})", flush=True)
  print(f"    (gamma delta-mean {r.get('gamma_delta_mean', 0.0):+.4f}; "
        f"E render-LIT verify is windowed)", flush=True)
  sys.exit(0 if ok else 1)


main()
