#!/usr/bin/env ork.python

################################################################################
# test_dflowedit_savedialog — headless gate for the dflow editor's SAVE-via-file-requester
# flow (owner order 2026-07-19: "the save button should open our file requester to state
# where to save"; the bug it closes: a DSL-backed Save silently serialized <name>.orj into
# the CWD — an untracked exprforce.orj landed in the repo root).
#
# HEADLESS + NO WINDOWS by construction: the gate drives the REAL DflowEditor save methods
# (_doSave / _doSaveAs / _performSave) on a bare instance whose _openSaveDialog is a recording
# spy (records the requested defaults + the on_accept callback the dialog would fire), so the
# routing + defaults + per-doc memory are asserted without opening an OS window. The one
# leg that exercises the REAL _openSaveDialog spies at the ezapp.createSecondaryWindow boundary
# (the secondary_window_floating.py pattern) to prove the requester requests floating=True.
#
# What it proves:
#   defaults  : a DSL-backed doc defaults to the ASSET'S dir + '<title>.orj' (Save-As); an
#               .orj-backed doc defaults to its OWN path (dir + basename).
#   write+rt  : dialog accept writes at the chosen path (a bare name gets '.orj'), the file is
#               valid GraphData reflection JSON, and reloads through _load_graphdata with a
#               matching node count.
#   memory    : after a successful save, a second Save re-writes the SAME path with NO dialog;
#               Save-As always dialogs.
#   cancel    : the dialog opens but is not accepted -> NO file written anywhere (cwd + tmp
#               stay clean), no path remembered.
#   unwritable: a bad target FAILS LOUDLY ('save failed' on stderr) without a crash, leaving
#               the per-doc memory untouched.
#   multidoc  : Save targets the ACTIVE tab's doc; two docs keep SEPARATE save targets/memory.
#   floating  : the REAL _openSaveDialog requests a floating (always-on-top) secondary window,
#               still decorated + resizable.
#
# Init recipe = the test_dflowedit.py precedent (full subsystem class registration; always
# coreappexit()). Verdict line: DFLOWEDIT_SAVEDIALOG_RESULT=PASS|FAIL.
################################################################################

import os
os.environ["PYTHONUNBUFFERED"] = "1"

import io
import sys
import json
import glob
import tempfile
import contextlib

# Prefer THIS repo's obt.project/scripts (worktree/lane run shadows the env default), mirroring
# ork.dflow.edit.py so the gate tests the SHELL code sitting next to it.
_SCRIPTS = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                         "..", "..", "..", "..", "obt.project", "scripts"))
if _SCRIPTS not in sys.path[:1]:
  sys.path.insert(0, _SCRIPTS)

from orkengine import core                  # core before lev2
from orkengine import lev2                   # noqa: F401
from orkengine import ecs                    # headless_appinit (full class registration)


################################################################################
# helpers
################################################################################

def _gen_graphdata_orj(dstdir, name="orjdoc"):
  """A minimal GraphData .orj on disk (POOL -> EMITN, 2 modules / 1 edge) — an .orj-backed doc
  for the defaults + reload legs (the exact serialize path the shell's Save re-uses)."""
  from orkengine.core import dataflow as _dflow
  P = lev2.particles
  g = _dflow.GraphData.createShared()
  pool = g.create("POOL", P.Pool)
  emitn = g.create("EMITN", P.NozzleEmitter)
  g.connect(emitn.inputs.pool, pool.outputs.pool)
  path = os.path.join(dstdir, f"{name}.orj")
  with open(path, "w") as f:
    f.write(g.serializeJson())
  return path


def _bare_editor(bindings, sources, focus=0):
  """A DflowEditor with ONLY the save-path attributes set (no engine boot) + a recording spy
  for _openSaveDialog. Exercises the REAL _doSave/_doSaveAs/_performSave routing headlessly."""
  from ork.editor.dflowedit import DflowEditor
  app = DflowEditor.__new__(DflowEditor)
  app._bindings = list(bindings)
  app._sources = list(sources)
  app._focused_index = focus
  app._binding = bindings[focus]
  app._focused_binding = bindings[focus]
  app._save_paths = {}
  app.node_editor = None
  app._dialog_calls = []

  def _spy(title, default_dir, default_name, on_accept):
    # record; DO NOT auto-accept (the test drives acceptance where it wants a write).
    app._dialog_calls.append({"title": title, "dir": default_dir,
                              "name": default_name, "on_accept": on_accept})
  app._openSaveDialog = _spy
  return app


def _focus(app, index):
  app._focused_index = index
  app._binding = app._bindings[index]
  app._focused_binding = app._bindings[index]


def _orj_set(d):
  return set(glob.glob(os.path.join(d, "*.orj")))


################################################################################
# gates
################################################################################

def gate_defaults():
  """DSL-backed -> asset dir + '<title>.orj'; .orj-backed -> its own dir + basename."""
  from ork.editor.dflowedit import _load_particles, _load_graphdata
  from ork.editor import save_target

  b_dsl = _load_particles("exprforce")
  assert b_dsl.dsl_source and b_dsl.dsl_source.endswith(".py"), \
      f"exprforce must be a DSL-backed binding, got dsl_source={b_dsl.dsl_source!r}"
  # save_defaults ignores the opened `source` for DSL docs (uses the resolved dsl_source) — pass
  # the BARE name the user typed to prove the asset-dir default holds regardless.
  ddir, dname = save_target.save_defaults(b_dsl, "exprforce")
  print(f"[gate:defaults] DSL exprforce -> dir={ddir!r} name={dname!r}", flush=True)
  assert ddir == os.path.dirname(str(b_dsl.dsl_source)), \
      f"DSL default dir must be the asset dir, got {ddir!r}"
  assert dname == f"{b_dsl.title}.orj", f"DSL default name must be '<title>.orj', got {dname!r}"
  assert os.path.isdir(ddir), f"DSL default dir must exist (never a bare cwd guess): {ddir!r}"

  # plan_save with no memory -> a dialog plan carrying those defaults.
  plan = save_target.plan_save(b_dsl, "exprforce", None)
  assert plan == ("dialog", ddir, dname), f"no-memory plan must dialog with defaults: {plan!r}"
  # plan_save WITH memory -> resave, no dialog.
  plan2 = save_target.plan_save(b_dsl, "exprforce", "/some/where/x.orj")
  assert plan2 == ("resave", "/some/where/x.orj"), f"memory plan must resave: {plan2!r}"

  tmp = tempfile.mkdtemp(prefix="savedlg_orj_")
  orj = _gen_graphdata_orj(tmp)
  b_orj = _load_graphdata(orj)
  assert not b_orj.dsl_source, "an .orj-backed binding must not carry a dsl_source"
  ddir2, dname2 = save_target.save_defaults(b_orj, orj)
  print(f"[gate:defaults] .orj {os.path.basename(orj)} -> dir={ddir2!r} name={dname2!r}",
        flush=True)
  assert ddir2 == os.path.dirname(orj) and dname2 == os.path.basename(orj), \
      f".orj default must be its own path: dir={ddir2!r} name={dname2!r}"

  print("GATE_DEFAULTS=PASS", flush=True)
  return True


def gate_write_memory():
  """Accept -> write at the chosen path (bare name gets .orj), valid JSON + reload node count;
  second Save re-writes the SAME path with NO dialog; Save-As always dialogs."""
  from ork.editor.dflowedit import _load_particles, _load_graphdata
  b = _load_particles("exprforce")
  n_expected = len(list(b.node_model.nodes()))
  tmp = tempfile.mkdtemp(prefix="savedlg_write_")
  app = _bare_editor([b], ["exprforce"])

  app._doSave()                                   # first Save -> the requester (spy)
  assert len(app._dialog_calls) == 1, "first Save must open the requester exactly once"
  call = app._dialog_calls[0]
  assert call["name"] == f"{b.title}.orj", f"requester name default wrong: {call['name']!r}"

  chosen = os.path.join(tmp, "chosen_graph")      # NO extension -> normalize adds .orj
  call["on_accept"](chosen)                       # simulate the dialog accept
  saved = os.path.abspath(chosen + ".orj")
  assert os.path.isfile(saved), f"accept did not write the chosen .orj: {saved}"
  assert app._save_paths.get(0) == saved, f"accept must remember the save path: {app._save_paths}"

  with open(saved) as f:
    root = json.load(f)                           # valid JSON at all
  cls = (root.get("root", {}).get("object", {}).get("class", "") or "").lower()
  assert "graphdata" in cls, f"saved file is not a GraphData reflection doc: class={cls!r}"
  reloaded = list(_load_graphdata(saved).node_model.nodes())
  print(f"[gate:write] saved={saved} class={cls!r} nodes={len(reloaded)} (expected {n_expected})",
        flush=True)
  assert len(reloaded) == n_expected, \
      f"reload round-trip node count mismatch: {len(reloaded)} != {n_expected}"

  # second Save -> NO dialog (memory), same path REWRITTEN.
  os.remove(saved)
  app._dialog_calls.clear()
  app._doSave()
  assert len(app._dialog_calls) == 0, "a remembered doc must Save with NO dialog"
  assert os.path.isfile(saved), "the remembered Save must rewrite the same path"

  # Save-As always dialogs, even with a remembered path.
  app._dialog_calls.clear()
  app._doSaveAs()
  assert len(app._dialog_calls) == 1, "Save-As must always open the requester"
  assert app._dialog_calls[0]["name"] == f"{b.title}.orj", "Save-As keeps the family default name"

  print("GATE_WRITE_MEMORY=PASS", flush=True)
  return True


def gate_cancel_clean():
  """Cancel = the requester opens but is not accepted -> NO write anywhere (cwd + tmp clean)."""
  from ork.editor.dflowedit import _load_particles
  b = _load_particles("exprforce")
  tmp = tempfile.mkdtemp(prefix="savedlg_cancel_")
  cwd = os.getcwd()
  cwd_before, tmp_before = _orj_set(cwd), _orj_set(tmp)

  app = _bare_editor([b], ["exprforce"])
  app._doSave()                                   # requester opens (spy) — accept NEVER called
  assert len(app._dialog_calls) == 1, "cancel leg must still have opened the requester"
  # (no on_accept invocation == the user pressing CANCEL)

  assert _orj_set(cwd) == cwd_before, "cancel wrote a .orj into the CWD (the exact bug!)"
  assert _orj_set(tmp) == tmp_before, "cancel wrote a .orj into the tmp dir"
  assert app._save_paths == {}, "cancel must not remember any path"
  print(f"[gate:cancel] cwd/tmp unchanged; no path remembered", flush=True)

  print("GATE_CANCEL_CLEAN=PASS", flush=True)
  return True


def gate_unwritable_loud():
  """An unwritable target FAILS LOUDLY without a crash; the per-doc memory stays untouched."""
  from ork.editor.dflowedit import _load_particles
  b = _load_particles("exprforce")
  app = _bare_editor([b], ["exprforce"])
  bad = "/nonexistent_savedlg_dir_xyz/deeper/graph.orj"

  buf = io.StringIO()
  with contextlib.redirect_stdout(buf):
    app._performSave(bad)                         # must NOT raise
  out = buf.getvalue()
  print(f"[gate:unwritable] captured -> {out.strip()!r}", flush=True)
  assert "save failed" in out.lower(), f"an unwritable save must report loudly, got {out!r}"
  assert app._save_paths == {}, "a failed save must not record a path"
  assert not os.path.exists(bad), "a failed save must not have created the file"

  print("GATE_UNWRITABLE_LOUD=PASS", flush=True)
  return True


def gate_multidoc_active_tab():
  """Save targets the ACTIVE tab's doc; two docs keep SEPARATE targets + memory."""
  from ork.editor.dflowedit import _load_particles
  b_a = _load_particles("exprforce")
  b_b = _load_particles("fireball")
  assert b_a.title != b_b.title, "need two distinct-titled DSL docs for the active-tab leg"
  tmp = tempfile.mkdtemp(prefix="savedlg_multi_")
  app = _bare_editor([b_a, b_b], ["exprforce", "fireball"], focus=0)

  app._doSave()                                   # focused == A
  call_a = app._dialog_calls[-1]
  assert call_a["name"] == f"{b_a.title}.orj", f"active-tab A default wrong: {call_a['name']!r}"
  call_a["on_accept"](os.path.join(tmp, "doc_a"))
  assert 0 in app._save_paths and 1 not in app._save_paths, \
      f"A's save must record under index 0 only: {app._save_paths}"

  app._dialog_calls.clear()
  _focus(app, 1)                                  # switch active tab -> B (no memory yet)
  app._doSave()
  call_b = app._dialog_calls[-1]
  assert call_b["name"] == f"{b_b.title}.orj", f"active-tab B default wrong: {call_b['name']!r}"
  call_b["on_accept"](os.path.join(tmp, "doc_b"))
  assert 1 in app._save_paths, "B's save must record under index 1"
  assert app._save_paths[0] != app._save_paths[1], "each doc keeps a SEPARATE save target"
  print(f"[gate:multidoc] targets: {app._save_paths}", flush=True)

  print("GATE_MULTIDOC_ACTIVE_TAB=PASS", flush=True)
  return True


class _StopInit(Exception):
  pass


class _FloatSpy:
  """A fake ezapp whose createSecondaryWindow records the kwargs then short-circuits (no OS
  window built) — the secondary_window_floating.py boundary-spy pattern."""

  def __init__(self):
    self.kwargs = None

  def createSecondaryWindow(self, **kwargs):
    self.kwargs = kwargs
    raise _StopInit()


def gate_floating():
  """The REAL _openSaveDialog requests a floating (always-on-top) requester, still decorated
  + resizable — the owner's 'file requester should also float' addition."""
  from ork.editor.dflowedit import _load_particles, DflowEditor
  b = _load_particles("exprforce")
  app = _bare_editor([b], ["exprforce"])
  spy = _FloatSpy()
  app.ezapp = spy
  try:
    # call the REAL method (bypass the instance spy) — it hits createSecondaryWindow first.
    DflowEditor._openSaveDialog(app, "Save exprforce (.orj)",
                                os.path.expanduser("~"), "exprforce.orj", lambda p: None)
  except _StopInit:
    pass
  assert spy.kwargs is not None, "the requester never called createSecondaryWindow"
  print(f"[gate:floating] createSecondaryWindow kwargs floating={spy.kwargs.get('floating')} "
        f"decorated={spy.kwargs.get('decorated')} resizable={spy.kwargs.get('resizable')}",
        flush=True)
  assert spy.kwargs.get("floating") is True, "the requester must request floating=True"
  assert spy.kwargs.get("decorated") is True and spy.kwargs.get("resizable") is True, \
      "the requester must stay decorated + resizable (a real file dialog window)"

  print("GATE_FLOATING=PASS", flush=True)
  return True


################################################################################

def main(argv):
  ezapp = ecs.headless_appinit(use_subsystems=['opq', 'core', 'gpu', 'lev2'])
  ezapp.mainThreadBegin()
  ezapp.bindGfxToCurrentThread()
  results = []
  try:
    results.append(gate_defaults())
    results.append(gate_write_memory())
    results.append(gate_cancel_clean())
    results.append(gate_unwritable_loud())
    results.append(gate_multidoc_active_tab())
    results.append(gate_floating())
  finally:
    ezapp.mainThreadEnd()
    core.coreappexit()             # ALWAYS exit cleanly (a skipped coreappexit hangs teardown)
  ok = all(results) and len(results) == 6
  print(f"DFLOWEDIT_SAVEDIALOG_RESULT={'PASS' if ok else 'FAIL'}", flush=True)
  return 0 if ok else 1


if __name__ == "__main__":
  sys.exit(main(sys.argv[1:]))
